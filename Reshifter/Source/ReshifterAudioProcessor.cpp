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

    voices[0].setPitch(0.0f);
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

    // --- 1. Update Parameters ---
    voices[1].setPitch(indexToSemitones(interval1Pitch->load()));
    voices[2].setPitch(indexToSemitones(interval2Pitch->load()));

    // --- 2. Determine Active Stage & Set Voice Gains ---
    auto currentMode = (int)mode->load();
    int activeStage = 0;
    auto position = playHead != nullptr ? playHead->getPosition() : juce::nullopt;

    if (currentMode == 1) { currentBpm = manualTempo->load(); }
    else if (position && position->getBpm()) { currentBpm = *position->getBpm(); }

    if (currentMode < 2) // Sync or Manual Mode
    {
        double ppq = 0.0;
        if (position && position->getIsPlaying() && position->getPpqPosition()) {
            ppq = *position->getPpqPosition();
        } else {
            freeRunningPpq += bufferSize * (currentBpm / (getSampleRate() * 60.0));
            ppq = freeRunningPpq;
        }

        double beatsPerBar = (position && position->getTimeSignature()) ? (double)position->getTimeSignature()->numerator : 4.0;
        const double loopLengthsInBeats[] = { beatsPerBar * 0.5, beatsPerBar, beatsPerBar * 2.0, beatsPerBar * 4.0 };
        const double currentLoopInBeats = loopLengthsInBeats[(int)loopLength->load()];
        if (currentLoopInBeats > 0) freeRunningPpq = fmod(freeRunningPpq, currentLoopInBeats);

        const double positionInLoopNormalized = fmod(ppq, currentLoopInBeats) / currentLoopInBeats;
        const int divisionRatioIndex = (int)divisionRatio->load();
        double baseEnd, int1End;

        if (divisionRatioIndex == 0) { baseEnd = 0.50; int1End = 0.75; }
        else if (divisionRatioIndex == 1) { baseEnd = 0.25; int1End = 0.75; }
        else if (divisionRatioIndex == 2) { baseEnd = 0.25; int1End = 0.50; }
        else { baseEnd = 1.0/3.0; int1End = 2.0/3.0; }

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

    // --- 4. Process Audio ---
    const int delayBufferSize = delayBuffer.getNumSamples();
    const float delayTimeSecs = 0.25f;
    int delayInSamples = static_cast<int>(delayTimeSecs * getSampleRate());

    juce::AudioBuffer<float> delayedAudio;
    delayedAudio.setSize(totalNumInputChannels, bufferSize);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        int readPos = (writePosition - delayInSamples + delayBufferSize) % delayBufferSize;
        const float* delayData = delayBuffer.getReadPointer(channel);
        float* delayedAudioData = delayedAudio.getWritePointer(channel);
        for (int i = 0; i < bufferSize; ++i)
        {
            delayedAudioData[i] = delayData[readPos];
            readPos = (readPos + 1) % delayBufferSize;
        }
    }

    juce::AudioBuffer<float> interleavedInput;
    interleavedInput.setSize(1, bufferSize * totalNumInputChannels);
    float* interleavedPtr = interleavedInput.getWritePointer(0);

    for (int i = 0; i < bufferSize; ++i)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            interleavedPtr[i * totalNumInputChannels + channel] = delayedAudio.getSample(channel, i);
        }
    }

    buffer.clear();
    juce::AudioBuffer<float> interleavedOutput;
    interleavedOutput.setSize(1, bufferSize * totalNumInputChannels);

    for (auto& voice : voices)
    {
        voice.update();
        voice.soundTouch.putSamples(interleavedInput.getReadPointer(0), bufferSize);

        int numSamplesReceived = 0;
        do
        {
            numSamplesReceived = voice.soundTouch.receiveSamples(interleavedOutput.getWritePointer(0), bufferSize);

            for (int i = 0; i < numSamplesReceived; ++i)
            {
                float gain = voice.gain.getNextValue();
                for (int channel = 0; channel < totalNumInputChannels; ++channel)
                {
                    float sample = interleavedOutput.getSample(0, i * totalNumInputChannels + channel);
                    buffer.addSample(channel, i, sample * gain);
                }
            }
        } while (numSamplesReceived != 0);
    }

    // Write clean input to delay buffer
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* inputData = cleanInput.getReadPointer(channel);
        for(int i = 0; i < bufferSize; ++i)
            delayBuffer.setSample(channel, (writePosition + i) % delayBufferSize, inputData[i]);
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
