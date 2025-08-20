/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VUMeter.h"
#include "LFODisplay.h"

//==============================================================================
/**
*/
class PitchShiftAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    PitchShiftAudioProcessorEditor (PitchShiftAudioProcessor&);
    ~PitchShiftAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    PitchShiftAudioProcessor& audioProcessor;

    // UI Components
    juce::Slider pitchSlider;
    juce::Slider feedbackSlider;
    juce::Slider mixSlider;
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::Slider toneCutoffSlider;
    juce::Slider toneResonanceSlider;
    juce::Slider noiseSlider;
    juce::Slider lfoRateSlider;
    juce::Slider lfoDepthSlider;

    juce::ComboBox toneModeComboBox;
    juce::ComboBox lfoWaveformComboBox;
    juce::ComboBox presetComboBox;

    VUMeter vuMeter[2];
    LFODisplay lfoDisplay;

    juce::Label pitchLabel, feedbackLabel, mixLabel, inputGainLabel, outputGainLabel,
                toneCutoffLabel, toneResonanceLabel, noiseLabel, lfoRateLabel, lfoDepthLabel;

    int lastPresetIndex = -1;

    // Attachments
    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> feedbackAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> inputGainAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;
    std::unique_ptr<SliderAttachment> toneCutoffAttachment;
    std::unique_ptr<SliderAttachment> toneResonanceAttachment;
    std::unique_ptr<SliderAttachment> noiseAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAttachment;
    std::unique_ptr<SliderAttachment> lfoDepthAttachment;

    std::unique_ptr<ComboBoxAttachment> toneModeAttachment;
    std::unique_ptr<ComboBoxAttachment> lfoWaveformAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessorEditor)
};
