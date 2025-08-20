/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "PitchShifter.h"

//==============================================================================
/**
*/
class PitchShiftAudioProcessor  : public juce::AudioProcessor
{
public:
    enum Presets
    {
        Default,
        AnalogDoubler,
        ChorusEnsemble,
        OctaveUp,
        Vibrato,
        ResonantSwell,
        LoFiWarble,
        NumPresets
    };

    //==============================================================================
    PitchShiftAudioProcessor();
    ~PitchShiftAudioProcessor() override;

    juce::AudioProcessorValueTreeState& getAPVTS();
    float getRMSLevel(int channel) const;

    void loadPreset(int presetIndex);

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

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    PitchShifter pitchShifter[2];

    // Feedback Path
    float lastFeedbackOutput[2] { 0.0f, 0.0f };
    juce::dsp::IIR::Filter<float> feedbackFilter[2];

    // Post Filter
    juce::dsp::IIR::Filter<float> postFilter[2];

    // Noise
    juce::Random random;
    juce::dsp::IIR::Filter<float> noiseFilter[2];

    // LFO
    juce::dsp::LFO<float> lfo;
    juce::dsp::LFO<float> lfo_sh_clock;
    float lfo_sh_value = 0.0f;

    // VU Meter
    float rmsLevel[2] { -60.0f, -60.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessor)
};
