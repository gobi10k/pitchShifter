/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/

// A dedicated struct to hold the state of a single pitch-shifting voice.
struct PitchShiftVoice
{
    // Each voice has its own independent read position in the shared delay buffer.
    std::vector<double> readPosition;

    // Each voice has its own smoothed gain, allowing for smooth crossfades.
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> gain;

    // The pitch ratio for this voice (e.g., 1.0 for unison, 1.5 for a fifth up).
    double pitchRatio = 1.0;
    double sampleRate = 44100.0;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        readPosition.resize(spec.numChannels);
        std::fill(readPosition.begin(), readPosition.end(), 0.0);
        gain.reset(sampleRate, 0.05);
    }

    void setGain(double targetGain, double smoothTime)
    {
        gain.reset(sampleRate, smoothTime);
        gain.setTargetValue(targetGain);
    }
};


class ReshifterAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ReshifterAudioProcessor();
    ~ReshifterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // A shared delay buffer for all voices to read from.
    juce::AudioBuffer<float> delayBuffer;
    int writePosition { 0 };

    // An array of three voices, one for each sequencer stage (BASE, INT1, INT2).
    std::array<PitchShiftVoice, 3> voices;

    // Sequencer variables
    juce::AudioPlayHead* playHead = nullptr;

    // Pointers to the parameters in the APVTS for quick access.
    std::atomic<float>* interval1Pitch = nullptr;
    std::atomic<float>* interval2Pitch = nullptr;
    std::atomic<float>* loopLength = nullptr;
    std::atomic<float>* divisionRatio = nullptr;
    std::atomic<float>* glide = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReshifterAudioProcessor)
};
