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
    return "Modern PitchShifter";
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
    // Return latency based on current quality setting
    if (apvts.getRawParameterValue("QUALITY"))
    {
        int quality = (int)*apvts.getRawParameterValue("QUALITY");
        switch (quality)
        {
            case 0: return 0.009; // ~9ms for fast mode
            case 1: return 0.019; // ~19ms for balanced mode
            case 2: return 0.037; // ~37ms for high quality mode
            default: return 0.02;
        }
    }
    return 0.02; // Default
}

int PitchShifterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
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
    spec.numChannels = getTotalNumOutputChannels();

    dryShifters.clear();
    wetShifters.clear();
    smoothedWetPitch.clear();

    for (int i = 0; i < spec.numChannels; ++i)
    {
        // Initialize Dry Shifter (for 0-shift processing)
        dryShifters.add(new PitchShifter());
        int quality = (int)*apvts.getRawParameterValue("QUALITY");
        dryShifters[i]->setQuality(quality);
        dryShifters[i]->setFormantPreservation(*apvts.getRawParameterValue("FORMANT_PRESERVATION") > 0.5f);
        dryShifters[i]->prepareToPlay(sampleRate, samplesPerBlock);

        // Initialize Wet Shifter (for variable pitch processing)
        wetShifters.add(new PitchShifter());
        wetShifters[i]->setQuality(quality);
        wetShifters[i]->setFormantPreservation(*apvts.getRawParameterValue("FORMANT_PRESERVATION") > 0.5f);
        wetShifters[i]->prepareToPlay(sampleRate, samplesPerBlock);

        // Smoother for the wet path pitch
        smoothedWetPitch.add(new juce::LinearSmoothedValue<float>());
        float glideTime = *apvts.getRawParameterValue("GLIDE") / 1000.0f;
        smoothedWetPitch[i]->reset(sampleRate, glideTime > 0.0f ? glideTime : 0.001f);
    }
    
    // Initialize parameter tracking
    lastGlide = *apvts.getRawParameterValue("GLIDE");
    lastQuality = (int)*apvts.getRawParameterValue("QUALITY");
    lastFormantPreservation = *apvts.getRawParameterValue("FORMANT_PRESERVATION") > 0.5f;
}

void PitchShifterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PitchShifterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
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

    // Clear any output channels that didn't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get current parameter values
    float pitch = *apvts.getRawParameterValue("PITCH");
    float glide = *apvts.getRawParameterValue("GLIDE");
    int quality = (int)*apvts.getRawParameterValue("QUALITY");
    bool formantPreservation = *apvts.getRawParameterValue("FORMANT_PRESERVATION") > 0.5f;
    float mix = *apvts.getRawParameterValue("MIX");
    float outputGainDB = *apvts.getRawParameterValue("OUTPUT_GAIN");
    float outputGain = juce::Decibels::decibelsToGain(outputGainDB);

    // Update glide time if changed
    if (glide != lastGlide)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float rampLength = glide > 0.0f ? glide / 1000.0f : 0.001f;
            smoothedWetPitch[channel]->reset(getSampleRate(), rampLength);
        }
        lastGlide = glide;
    }
    
    // Update quality if changed - requires re-initialization for all shifters
    if (quality != lastQuality)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            dryShifters[channel]->setQuality(quality);
            dryShifters[channel]->prepareToPlay(getSampleRate(), buffer.getNumSamples());
            dryShifters[channel]->setFormantPreservation(formantPreservation);

            wetShifters[channel]->setQuality(quality);
            wetShifters[channel]->prepareToPlay(getSampleRate(), buffer.getNumSamples());
            wetShifters[channel]->setFormantPreservation(formantPreservation);
        }
        lastQuality = quality;
    }
    
    // Update formant preservation if changed
    if (formantPreservation != lastFormantPreservation)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            dryShifters[channel]->setFormantPreservation(formantPreservation);
            wetShifters[channel]->setFormantPreservation(formantPreservation);
        }
        lastFormantPreservation = formantPreservation;
    }

    // Set target pitch for the wet path
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        smoothedWetPitch[channel]->setTargetValue(pitch);
    }

    // Create copies of the buffer for parallel processing
    juce::AudioBuffer<float> dryBuffer(buffer);
    juce::AudioBuffer<float> wetBuffer(buffer);

    // Process each channel in parallel
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // 1. Process Dry Path (0 pitch shift)
        auto* dryChannelData = dryBuffer.getWritePointer(channel);
        juce::AudioBuffer<float> dryChannelBuffer(&dryChannelData, 1, dryBuffer.getNumSamples());
        juce::LinearSmoothedValue<float> drySmoother;
        drySmoother.setCurrentAndTargetValue(0.0f); // Hardcoded 0 shift
        dryShifters[channel]->process(dryChannelBuffer, &drySmoother);

        // 2. Process Wet Path (UI-controlled pitch shift)
        auto* wetChannelData = wetBuffer.getWritePointer(channel);
        juce::AudioBuffer<float> wetChannelBuffer(&wetChannelData, 1, wetBuffer.getNumSamples());
        wetShifters[channel]->process(wetChannelBuffer, smoothedWetPitch[channel]);
    }

    // 3. Mix the parallel signals
    const float wetGainBoost = 1.41f; // ~3dB boost for the wet signal
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* outputData = buffer.getWritePointer(channel);
        const auto* dryData = dryBuffer.getReadPointer(channel);
        const auto* wetData = wetBuffer.getReadPointer(channel);

        // Using an equal-power crossfade for a smoother blend
        const float dryMix = std::cos(mix * juce::MathConstants<float>::pi * 0.5f);
        const float wetMix = std::sin(mix * juce::MathConstants<float>::pi * 0.5f);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float drySample = dryData[sample];
            float wetSample = wetData[sample] * wetGainBoost;
            outputData[sample] = (drySample * dryMix + wetSample * wetMix) * outputGain;
        }
    }
}

//==============================================================================
bool PitchShifterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PitchShifterAudioProcessor::createEditor()
{
    return new PitchShifterAudioProcessorEditor (*this);
}

//==============================================================================
void PitchShifterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PitchShifterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchShifterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Pitch control (-24 to +24 semitones)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "PITCH", "Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String(value >= 0 ? "+" : "") + juce::String(value, 2) + " st";
        }));

    // Glide time (0 to 1000ms)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "GLIDE", "Glide",
        juce::NormalisableRange<float>(0.0f, 1000.0f, 1.0f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String((int)value) + " ms";
        }));

    // Quality setting (0=Fast, 1=Balanced, 2=High)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "QUALITY", "Quality",
        juce::StringArray{"Fast", "Balanced", "High Quality"},
        1)); // Default to Balanced

    // Formant preservation toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "FORMANT_PRESERVATION", "Formant Preserve",
        true)); // Default enabled

    // Bypass gain - removed, not needed
    // Mix control instead (0 = dry, 1 = wet)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "MIX", "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f, // Default to fully wet
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return juce::String((int)(value * 100)) + "%";
        }));

    // Output gain - normal range
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
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchShifterAudioProcessor();
}
