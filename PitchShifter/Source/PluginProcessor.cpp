/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PitchShifterAudioProcessor::PitchShifterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    :
#endif
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

PitchShifterAudioProcessor::~PitchShifterAudioProcessor()
{
}

//==============================================================================
const juce::String PitchShifterAudioProcessor::getName() const
{
    return "Pitch Shifter";
}

bool PitchShifterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PitchShifterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PitchShifterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PitchShifterAudioProcessor::getTailLengthSeconds() const
{
    return 0.2; // Corresponds to new maxDelayTime
}

int PitchShifterAudioProcessor::getNumPrograms()
{
    return 1;
}

int PitchShifterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PitchShifterAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PitchShifterAudioProcessor::getProgramName (int index)
{
    return {};
}

void PitchShifterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void PitchShifterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;

    pitchShifters.clear();
    dryDelayLines.clear();
    smoothedPitch.clear();

    for (int i = 0; i < getTotalNumOutputChannels(); ++i)
    {
        pitchShifters.add(new PitchShifter());
        pitchShifters[i]->prepare(spec);

        dryDelayLines.add(new juce::dsp::DelayLine<float>());
        // Set delay to half the BBD buffer size for averaging
        dryDelayLines[i]->prepare(spec);
        dryDelayLines[i]->setDelay(sampleRate * 0.2f * 0.5f);

        smoothedPitch.add(new juce::LinearSmoothedValue<float>());
        float glideTime = *apvts.getRawParameterValue("GLIDE") / 1000.0f;
        smoothedPitch[i]->reset(sampleRate, glideTime > 0.0f ? glideTime : 0.001f);
    }

    lastGlide = *apvts.getRawParameterValue("GLIDE");
}

void PitchShifterAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PitchShifterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PitchShifterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float pitch = *apvts.getRawParameterValue("PITCH");
    float glide = *apvts.getRawParameterValue("GLIDE");
    float mix = *apvts.getRawParameterValue("MIX");
    float outputGainDB = *apvts.getRawParameterValue("OUTPUT_GAIN");
    float outputGain = juce::Decibels::decibelsToGain(outputGainDB);

    if (glide != lastGlide)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float rampLength = glide > 0.0f ? glide / 1000.0f : 0.001f;
            smoothedPitch[channel]->reset(getSampleRate(), rampLength);
        }
        lastGlide = glide;
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        smoothedPitch[channel]->setTargetValue(pitch);
    }

    juce::AudioBuffer<float> wetBuffer(buffer);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = wetBuffer.getWritePointer(channel);
        juce::AudioBuffer<float> wetChannelBuffer(&channelData, 1, buffer.getNumSamples());
        float currentPitch = smoothedPitch[channel]->getNextValue();
        float pitchRatio = std::pow(2.0f, currentPitch / 12.0f);
        pitchShifters[channel]->process(wetChannelBuffer, pitchRatio);
    }

    const float wetGainBoost = 1.41f;
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        const auto* wetData = wetBuffer.getReadPointer(channel);

        const float dryMix = std::cos(mix * juce::MathConstants<float>::pi * 0.5f);
        const float wetMix = std::sin(mix * juce::MathConstants<float>::pi * 0.5f);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float dryInput = channelData[sample];
            dryDelayLines[channel]->pushSample(0, dryInput);
            const float drySample = dryDelayLines[channel]->popSample(0);

            const float wetSample = wetData[sample] * wetGainBoost;

            channelData[sample] = (drySample * dryMix + wetSample * wetMix) * outputGain;
        }
    }
}

//==============================================================================
bool PitchShifterAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PitchShifterAudioProcessor::createEditor()
{
    return new PitchShifterAudioProcessorEditor (*this);
}

//==============================================================================
void PitchShifterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PitchShifterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchShifterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "PITCH", "Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String(value >= 0 ? "+" : "") + juce::String(value, 2) + " st";
        }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "GLIDE", "Glide",
        juce::NormalisableRange<float>(0.0f, 1000.0f, 1.0f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String((int)value) + " ms";
        }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "MIX", "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f, // Default to fully wet
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String((int)(value * 100)) + "%";
        }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "OUTPUT_GAIN", "Output Gain",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f),
        0.0f, // Default to 0dB
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String(value, 1) + " dB";
        }));

    return layout;
}

juce::AudioProcessorValueTreeState& PitchShifterAudioProcessor::getAPVTS()
{
    return apvts;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchShifterAudioProcessor();
}
