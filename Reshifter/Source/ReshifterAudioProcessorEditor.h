/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include "ReshifterAudioProcessor.h"

//==============================================================================
/**
*/
class ReshifterAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Button::Listener, private juce::ComboBox::Listener
{
public:
    ReshifterAudioProcessorEditor (ReshifterAudioProcessor&);
    ~ReshifterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBox) override;

    ReshifterAudioProcessor& audioProcessor;

    juce::Slider glideSlider, manualTempoSlider;
    juce::ComboBox modeSelector, interval1PitchSelector, interval2PitchSelector, divisionRatioSelector;
    juce::TextButton loopLengthButton;
    juce::Label glideLabel, manualTempoLabel, modeLabel, interval1Label, interval2Label, divisionLabel, loopLengthLabel, loopLengthValueLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment, manualTempoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment, interval1PitchAttachment, interval2PitchAttachment, divisionRatioAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReshifterAudioProcessorEditor)
};
