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

    // Set the fixed pitch ratios for each voice
    voices[0].pitchRatio = 1.0; // BASE voice is always unison
}

void ReshifterAudioProcessor::releaseResources()
{
    delayBuffer.setSize(0, 0);
}

void ReshifterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any excess output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // --- 1. Get Host Transport Information & Update Parameters ---
    playHead = getPlayHead();
    if (!playHead || !playHead->getCurrentPosition(positionInfo))
    {
        return; // Can't sync, do nothing.
    }

    // Update the pitch ratios for the interval voices from the parameters
    voices[1].pitchRatio = std::pow(2.0, indexToSemitones(interval1Pitch->load()) / 12.0);
    voices[2].pitchRatio = std::pow(2.0, indexToSemitones(interval2Pitch->load()) / 12.0);

    // --- 2. Determine Active Stage & Set Voice Gains ---
    int activeStage = 0;
    if (positionInfo.getIsPlaying())
    {
        const double beatsPerBar = positionInfo.getTimeSignature()->numerator;
        const double loopLengthsInBeats[] = { beatsPerBar * 0.5, beatsPerBar, beatsPerBar * 2.0, beatsPerBar * 4.0 };
        const double currentLoopInBeats = loopLengthsInBeats[(int)loopLength->load()];
        const double currentBeatInLoop = fmod(positionInfo.getPpqPosition(), currentLoopInBeats);
        const double positionInLoopNormalized = currentBeatInLoop / currentLoopInBeats;

        const int divisionRatioIndex = (int)divisionRatio->load();
        double baseEnd, int1End;

        if (divisionRatioIndex == 0) { baseEnd = 0.50; int1End = 0.75; }
        else if (divisionRatioIndex == 1) { baseEnd = 0.25; int1End = 0.75; }
        else if (divisionRatioIndex == 2) { baseEnd = 0.25; int1End = 0.50; }
        else { baseEnd = 1.0/3.0; int1End = 2.0/3.0; }

        if (positionInLoopNormalized < baseEnd)         activeStage = 0; // BASE
        else if (positionInLoopNormalized < int1End)    activeStage = 1; // INTERVAL 1
        else                                            activeStage = 2; // INTERVAL 2
    }

    // Set the target gain for each voice. The active voice is 1.0, others are 0.0.
    // The 'glide' parameter controls the crossfade time between them.
    const double glideTime = glide->load();
    for (int i = 0; i < voices.size(); ++i)
    {
        voices[i].setGain((i == activeStage) ? 1.0 : 0.0, glideTime);
    }

    // --- 3. Process Audio ---
    const int bufferSize = buffer.getNumSamples();
    const int delayBufferSize = delayBuffer.getNumSamples();

    // Create a temporary buffer to hold the mixed output of all voices
    juce::AudioBuffer<float> voiceOutputs;
    voiceOutputs.setSize(totalNumInputChannels, bufferSize);
    voiceOutputs.clear();

    // Process each voice
    for (auto& voice : voices)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* outputData = voiceOutputs.getWritePointer(channel);
            const float* delayData = delayBuffer.getReadPointer(channel);
            double currentReadPos = voice.readPosition[channel];

            for (int i = 0; i < bufferSize; ++i)
            {
                // Linear interpolation for reading from the delay line
                auto readPosInt = static_cast<int>(currentReadPos);
                auto readPosNext = (readPosInt + 1) % delayBufferSize;
                auto frac = currentReadPos - readPosInt;
                auto interpolatedSample = (1.0 - frac) * delayData[readPosInt] + frac * delayData[readPosNext];

                // Get the smoothed gain for this sample
                const double currentGain = voice.gain.getNextValue();

                // Add the voice's output, scaled by its gain, to the temporary buffer
                outputData[i] += interpolatedSample * currentGain;

                // Move the read head for this voice according to its pitch ratio
                currentReadPos += voice.pitchRatio;
                if (currentReadPos >= delayBufferSize)
                    currentReadPos -= delayBufferSize;
            }
            voice.readPosition[channel] = currentReadPos;
        }
    }

    // Write the input to the delay buffer and copy the mixed voice output to the main buffer
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // First, write the clean input to the delay buffer for all voices to use next time
        const float* inputData = buffer.getReadPointer(channel);
        for(int i = 0; i < bufferSize; ++i)
            delayBuffer.setSample(channel, (writePosition + i) % delayBufferSize, inputData[i]);

        // Then, copy the processed audio from our temporary buffer to the actual output buffer
        buffer.copyFrom(channel, 0, voiceOutputs, channel, 0, bufferSize);
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
