/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define SOUNDTOUCH_FLOAT_SAMPLES 1
#include "SoundTouch.h"

//==============================================================================
/**
*/

// A dedicated struct to hold the state of a single pitch-shifting voice.
struct PitchShiftVoice
{
    soundtouch::SoundTouch soundTouch;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPitch;
    double sampleRate = 44100.0;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        soundTouch.setSampleRate(sampleRate);
        soundTouch.setChannels(spec.numChannels);
        gain.reset(sampleRate, 0.05);
        smoothedPitch.reset(sampleRate, 0.05); // 50ms pitch smoothing
    }

    void setPitch(float semitones)
    {
        if (std::abs(semitones - smoothedPitch.getTargetValue()) > 0.01f)
        {
            soundTouch.clear();
            smoothedPitch.setTargetValue(semitones);
        }
    }

    void setGain(float targetGain, float smoothTime)
    {
        gain.reset(sampleRate, smoothTime);
        gain.setTargetValue(targetGain);
    }

    void update()
    {
        soundTouch.setPitchSemiTones(smoothedPitch.getNextValue());
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
    double currentBpm = 120.0;
    double freeRunningPpq = 0.0;

    // Pointers to the parameters in the APVTS for quick access.
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* manualTempo = nullptr;
    std::atomic<float>* interval1Pitch = nullptr;
    std::atomic<float>* interval2Pitch = nullptr;
    std::atomic<float>* loopLength = nullptr;
    std::atomic<float>* divisionRatio = nullptr;
    std::atomic<float>* glide = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReshifterAudioProcessor)
};
