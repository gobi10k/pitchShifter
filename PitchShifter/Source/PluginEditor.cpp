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
    pitchSlider.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
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

    // Quality ComboBox
    qualityComboBox.addItem("Fast", 1);
    qualityComboBox.addItem("Balanced", 2);
    qualityComboBox.addItem("High Quality", 3);
    qualityComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    qualityComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2a2a));
    addAndMakeVisible(qualityComboBox);

    qualityLabel.setText("Quality", juce::dontSendNotification);
    qualityLabel.setJustificationType(juce::Justification::centred);
    qualityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(qualityLabel);

    qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), "QUALITY", qualityComboBox);

    // Formant Preservation Toggle
    formantToggle.setButtonText("Formant Preserve");
    formantToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    formantToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff00ff00));
    addAndMakeVisible(formantToggle);

    formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "FORMANT_PRESERVATION", formantToggle);

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
    setSize (400, 500);
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
    g.drawText("Modern Pitch Shifter", 0, 10, getWidth(), 30, juce::Justification::centred);
    
    // Version info
    g.setFont(juce::Font("Arial", 12.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText("High-Quality Real-Time Processing", 0, 35, getWidth(), 20, juce::Justification::centred);

    // Section separators
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(20, 70, getWidth() - 20, 70, 1.0f);
    g.drawLine(20, 280, getWidth() - 20, 280, 1.0f);
}

void PitchShifterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(60); // Space for title

    // Main pitch control section
    auto pitchSection = area.removeFromTop(180);
    pitchSection.reduce(20, 10);

    // Pitch slider takes most of the space
    auto pitchArea = pitchSection.removeFromLeft(120);
    pitchLabel.setBounds(pitchArea.removeFromTop(20));
    pitchSlider.setBounds(pitchArea);

    // Glide control next to pitch
    pitchSection.removeFromLeft(20); // spacing
    auto glideArea = pitchSection.removeFromLeft(100);
    glideLabel.setBounds(glideArea.removeFromTop(20));
    glideSlider.setBounds(glideArea.reduced(10));

    area.removeFromTop(20); // spacing

    // Quality and formant section
    auto controlSection = area.removeFromTop(100);
    controlSection.reduce(30, 10);

    // Quality selection
    auto qualityArea = controlSection.removeFromTop(45);
    qualityLabel.setBounds(qualityArea.removeFromTop(20));
    qualityComboBox.setBounds(qualityArea.reduced(0, 2));

    controlSection.removeFromTop(10); // spacing

    // Formant preservation toggle
    formantToggle.setBounds(controlSection.removeFromTop(30));

    area.removeFromTop(20); // spacing

    // Mix and Output Gain section
    auto gainSection = area.removeFromTop(120);
    gainSection.reduce(20, 10);

    // Mix control
    auto mixArea = gainSection.removeFromLeft(150);
    mixLabel.setBounds(mixArea.removeFromTop(20));
    mixSlider.setBounds(mixArea.reduced(10));

    gainSection.removeFromLeft(10); // spacing

    // Output gain
    auto outputGainArea = gainSection.removeFromLeft(150);
    outputGainLabel.setBounds(outputGainArea.removeFromTop(20));
    outputGainSlider.setBounds(outputGainArea.reduced(10));
}
