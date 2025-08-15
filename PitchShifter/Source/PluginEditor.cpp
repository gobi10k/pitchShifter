/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PitchShifterAudioProcessorEditor::PitchShifterAudioProcessorEditor (PitchShifterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Pitch Slider
    pitchSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    pitchSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    pitchSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(pitchSlider);
    
    pitchLabel.setText("Pitch", juce::dontSendNotification);
    pitchLabel.setJustificationType(juce::Justification::centred);
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(pitchLabel);
    
    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "PITCH", pitchSlider);

    // Glide Slider
    glideSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    glideSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    glideSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(glideSlider);
    
    glideLabel.setText("Glide", juce::dontSendNotification);
    glideLabel.setJustificationType(juce::Justification::centred);
    glideLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(glideLabel);
    
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "GLIDE", glideSlider);

    // Mix Slider
    mixSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    mixSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(mixSlider);
    
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centred);
    mixLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(mixLabel);
    
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "MIX", mixSlider);

    // Output Gain Slider
    outputGainSlider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    outputGainSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    addAndMakeVisible(outputGainSlider);
    
    outputGainLabel.setText("Output Gain", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(outputGainLabel);
    
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "OUTPUT_GAIN", outputGainSlider);

    // Set window size
    setSize (400, 350); // Reduced height
}

PitchShifterAudioProcessorEditor::~PitchShifterAudioProcessorEditor()
{
}

//==============================================================================
void PitchShifterAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Modern dark theme background
    juce::ColourGradient gradient(juce::Colour(0xff1a1a1a), 0, 0,
                                  juce::Colour(0xff2d2d2d), 0, getHeight(), false);
    g.setGradientFill(gradient);
    g.fillAll();
    
    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Arial", 20.0f, juce::Font::bold));
    g.drawText("Pitch Shifter", 0, 10, getWidth(), 30, juce::Justification::centred);
}

void PitchShifterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(50); // Space for title
    area.reduce(20, 20); // Add some padding around the controls

    auto topRow = area.removeFromTop(area.getHeight() / 2);
    auto bottomRow = area;
    bottomRow.removeFromTop(20); // spacing

    auto topLeft = topRow.removeFromLeft(topRow.getWidth() / 2);
    auto topRight = topRow;
    topRight.removeFromLeft(20);

    auto bottomLeft = bottomRow.removeFromLeft(bottomRow.getWidth() / 2);
    auto bottomRight = bottomRow;
    bottomRight.removeFromLeft(20);

    // Pitch
    pitchLabel.setBounds(topLeft.removeFromTop(20));
    pitchSlider.setBounds(topLeft.reduced(10));

    // Glide
    glideLabel.setBounds(topRight.removeFromTop(20));
    glideSlider.setBounds(topRight.reduced(10));

    // Mix
    mixLabel.setBounds(bottomLeft.removeFromTop(20));
    mixSlider.setBounds(bottomLeft.reduced(10));

    // Output Gain
    outputGainLabel.setBounds(bottomRight.removeFromTop(20));
    outputGainSlider.setBounds(bottomRight.reduced(10));
}
