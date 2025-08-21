/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "ReshifterAudioProcessor.h"
#include "ReshifterAudioProcessorEditor.h"

//==============================================================================
// Helper function to map choice index to semitone value
int indexToSemitones(int index)
{
    switch (index)
    {
        case 0: return -24; case 1: return -19; case 2: return -17;
        case 3: return -12; case 4: return -7;  case 5: return -5;
        case 6: return 0;   case 7: return 5;   case 8: return 7;
        case 9: return 12;  case 10: return 17; case 11: return 19;
        case 12: return 24;
        default: return 0;
    }
}


ReshifterAudioProcessor::ReshifterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    mode = apvts.getRawParameterValue("MODE");
    manualTempo = apvts.getRawParameterValue("MANUAL_TEMPO");
    interval1Pitch = apvts.getRawParameterValue("INTERVAL1_PITCH");
    interval2Pitch = apvts.getRawParameterValue("INTERVAL2_PITCH");
    loopLength = apvts.getRawParameterValue("LOOP_LENGTH");
    divisionRatio = apvts.getRawParameterValue("DIVISION_RATIO");
    glide = apvts.getRawParameterValue("GLIDE");
}

ReshifterAudioProcessor::~ReshifterAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ReshifterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("MODE", "Mode", juce::StringArray{"Sync", "Manual", "Pitch"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MANUAL_TEMPO", "Manual Tempo", juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f));

    juce::StringArray intervalChoices { "-24", "-19", "-17", "-12", "-7", "-5", "0", "+5", "+7", "+12", "+17", "+19", "+24" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("INTERVAL1_PITCH", "Interval 1 Pitch", intervalChoices, 6));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("INTERVAL2_PITCH", "Interval 2 Pitch", intervalChoices, 8));

    juce::StringArray loopLengthChoices { "1/2 bar", "1 bar", "2 bars", "4 bars" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("LOOP_LENGTH", "Loop Length", loopLengthChoices, 1));

    juce::StringArray divisionRatioChoices { "50/25/25", "25/50/25", "25/25/50", "33/33/33" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("DIVISION_RATIO", "Division Ratio", divisionRatioChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("GLIDE", "Glide", juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f), 0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
void ReshifterAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };

    const double maxLoopDurationSeconds = (4.0 * 4.0 * 60.0) / 40.0;
    const int maxDelayBufferSize = static_cast<int>((maxLoopDurationSeconds + 2.0) * spec.sampleRate);

    delayBuffer.setSize(spec.numChannels, maxDelayBufferSize);
    delayBuffer.clear();

    for (auto& voice : voices)
        voice.prepare(spec);

    voices[0].pitchRatio = 1.0;

    filter.prepare(spec);
    filter.setCutoffFrequencyHz(8000.0f); // BBD-style filtering
    filter.setResonance(0.0f);
    filter.setMode(juce::dsp::LadderFilter<float>::Mode::LPF24);
}

void ReshifterAudioProcessor::releaseResources()
{
}

void ReshifterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int bufferSize = buffer.getNumSamples();

    juce::AudioBuffer<float> cleanInput;
    cleanInput.makeCopyOf(buffer);

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, bufferSize);

    // --- 1. Update Parameters & Get Host Info ---
    if (playHead == nullptr)
        playHead = getPlayHead();

    voices[1].pitchRatio = std::pow(2.0, (double)indexToSemitones(interval1Pitch->load()) / 12.0);
    voices[2].pitchRatio = std::pow(2.0, (double)indexToSemitones(interval2Pitch->load()) / 12.0);

    // --- 2. Determine Sequencer State (BPM and Position) ---
    auto currentMode = (int)mode->load();
    int activeStage = 0;
    auto position = playHead != nullptr ? playHead->getPosition() : juce::nullopt;

    if (currentMode == 1) { currentBpm = manualTempo->load(); }
    else if (position && position->getBpm()) { currentBpm = *position->getBpm(); }

    bool isPlaying = position && position->getIsPlaying();
    if (isPlaying && !wasPlaying)
    {
        freeRunningPpq = 0.0; // Reset free-running counter on playback start
    }
    wasPlaying = isPlaying;

    if (currentMode < 2) // Sync or Manual Mode
    {
        double ppq = 0.0;
        if (isPlaying && position->getPpqPosition()) {
            ppq = *position->getPpqPosition();
        } else {
            double beatsPerSample = currentBpm / (getSampleRate() * 60.0);
            freeRunningPpq += bufferSize * beatsPerSample;
            ppq = freeRunningPpq;
        }

        double beatsPerBar = (position && position->getTimeSignature()) ? (double)position->getTimeSignature()->numerator : 4.0;
        const double loopLengthsInBeats[] = { beatsPerBar * 0.5, beatsPerBar, beatsPerBar * 2.0, beatsPerBar * 4.0 };
        const double currentLoopInBeats = loopLengthsInBeats[(int)loopLength->load()];
        if (currentLoopInBeats > 0) freeRunningPpq = fmod(freeRunningPpq, currentLoopInBeats);

        const double positionInLoopNormalized = fmod(ppq, currentLoopInBeats) / currentLoopInBeats;
        const int divisionRatioIndex = static_cast<int>(divisionRatio->load() + 0.5f);

        double baseEnd, int1End;
        if (divisionRatioIndex == 0)      { baseEnd = 0.50; int1End = 0.75; }
        else if (divisionRatioIndex == 1) { baseEnd = 0.25; int1End = 0.75; }
        else if (divisionRatioIndex == 2) { baseEnd = 0.25; int1End = 0.50; }
        else                              { baseEnd = 1.0/3.0; int1End = 2.0/3.0; }

        if (positionInLoopNormalized < baseEnd) activeStage = 0;
        else if (positionInLoopNormalized < int1End) activeStage = 1;
        else activeStage = 2;
    } else { // Pitch Mode
        activeStage = 1;
    }

    const float glideTime = (currentMode == 2) ? 0.01f : glide->load();
    for (int i = 0; i < voices.size(); ++i)
    {
        voices[i].setGain((i == activeStage) ? 1.0f : 0.0f, glideTime);
    }

    // --- 3. Process Audio (Original Resampling Method) ---
    const int delayBufferSize = delayBuffer.getNumSamples();
    juce::AudioBuffer<float> voiceOutputs;
    voiceOutputs.setSize(totalNumInputChannels, bufferSize);
    voiceOutputs.clear();

    for (auto& voice : voices)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* outputData = voiceOutputs.getWritePointer(channel);
            const float* delayData = delayBuffer.getReadPointer(channel);
            double currentReadPos = voice.readPosition[channel];

            for (int i = 0; i < bufferSize; ++i)
            {
                auto readPosInt = static_cast<int>(currentReadPos);
                auto readPosNext = (readPosInt + 1) % delayBufferSize;
                auto frac = currentReadPos - readPosInt;
                auto interpolatedSample = (1.0 - frac) * delayData[readPosInt] + frac * delayData[readPosNext];

                // BBD-style saturation
                interpolatedSample = std::tanh(interpolatedSample * 1.2f);

                const float currentGain = voice.gain.getNextValue();
                outputData[i] += interpolatedSample * currentGain;

                currentReadPos += voice.pitchRatio;
                if (currentReadPos >= delayBufferSize)
                    currentReadPos -= delayBufferSize;
            }
            voice.readPosition[channel] = currentReadPos;
        }
    }

    // BBD-style filtering
    filter.process(juce::dsp::ProcessContextReplacing<float>(voiceOutputs));

    buffer.makeCopyOf(voiceOutputs);

    // --- 4. Write clean input to delay buffer ---
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* inputData = cleanInput.getReadPointer(channel);
        for(int i = 0; i < bufferSize; ++i)
        {
            int pos = (writePosition + i) % delayBufferSize;
            // Bounds check to prevent potential crashes
            if (pos >= 0 && pos < delayBufferSize) {
                delayBuffer.setSample(channel, pos, inputData[i]);
            }
        }
    }

    writePosition = (writePosition + bufferSize) % delayBufferSize;
}


//==============================================================================
// Unchanged methods below...
bool ReshifterAudioProcessor::isMidiEffect() const { return false; }
double ReshifterAudioProcessor::getTailLengthSeconds() const { return 4.0; }
const juce::String ReshifterAudioProcessor::getName() const { return JucePlugin_Name; }
bool ReshifterAudioProcessor::acceptsMidi() const { return false; }
bool ReshifterAudioProcessor::producesMidi() const { return false; }
int ReshifterAudioProcessor::getNumPrograms() { return 1; }
int ReshifterAudioProcessor::getCurrentProgram() { return 0; }
void ReshifterAudioProcessor::setCurrentProgram (int index) { }
const juce::String ReshifterAudioProcessor::getProgramName (int index) { return {}; }
void ReshifterAudioProcessor::changeProgramName (int index, const juce::String& newName) { }
#ifndef JucePlugin_PreferredChannelConfigurations
bool ReshifterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}
#endif
bool ReshifterAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* ReshifterAudioProcessor::createEditor() { return new ReshifterAudioProcessorEditor (*this); }
void ReshifterAudioProcessor::getStateInformation (juce::MemoryBlock& destData) { auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml()); copyXmlToBinary (*xml, destData); }
void ReshifterAudioProcessor::setStateInformation (const void* data, int sizeInBytes) { std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes)); if (xmlState.get() != nullptr) if (xmlState->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xmlState)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReshifterAudioProcessor(); }
