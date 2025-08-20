/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "ReshifterAudioProcessor.h"
#include "ReshifterAudioProcessorEditor.h"

//==============================================================================
ReshifterAudioProcessorEditor::ReshifterAudioProcessorEditor (ReshifterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // --- Parameter Attachments ---
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "GLIDE", glideSlider);
    interval1PitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "INTERVAL1_PITCH", interval1PitchSelector);
    interval2PitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "INTERVAL2_PITCH", interval2PitchSelector);
    divisionRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "DIVISION_RATIO", divisionRatioSelector);

    // --- Sliders ---
    addAndMakeVisible(glideSlider);
    glideSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    glideSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    // --- Combo Boxes ---
    addAndMakeVisible(interval1PitchSelector);
    addAndMakeVisible(interval2PitchSelector);
    addAndMakeVisible(divisionRatioSelector);

    // --- Buttons ---
    addAndMakeVisible(loopLengthButton);
    loopLengthButton.setButtonText("Tap");
    loopLengthButton.addListener(this);

    // --- Labels ---
    addAndMakeVisible(glideLabel);
    glideLabel.setText("Glide", juce::dontSendNotification);
    glideLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(interval1Label);
    interval1Label.setText("Interval 1", juce::dontSendNotification);
    interval1Label.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(interval2Label);
    interval2Label.setText("Interval 2", juce::dontSendNotification);
    interval2Label.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(divisionLabel);
    divisionLabel.setText("Division", juce::dontSendNotification);
    divisionLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(loopLengthLabel);
    loopLengthLabel.setText("Loop Length", juce::dontSendNotification);
    loopLengthLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(loopLengthValueLabel);
    loopLengthValueLabel.setJustificationType(juce::Justification::centred);
    // Set initial value for the loop length label
    auto& loopLengthParam = *audioProcessor.apvts.getParameter("LOOP_LENGTH");
    loopLengthValueLabel.setText(loopLengthParam.getText(loopLengthParam.getValue(), 0), juce::dontSendNotification);


    setSize (300, 400);
}

ReshifterAudioProcessorEditor::~ReshifterAudioProcessorEditor()
{
    loopLengthButton.removeListener(this);
}

//==============================================================================
void ReshifterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

void ReshifterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    auto topRow = bounds.removeFromTop(100);
    glideSlider.setBounds(topRow.removeFromLeft(100));
    interval1PitchSelector.setBounds(topRow.removeFromLeft(100));
    interval2PitchSelector.setBounds(topRow.removeFromLeft(100));

    auto labelRow = bounds.removeFromTop(20);
    glideLabel.setBounds(labelRow.removeFromLeft(100));
    interval1Label.setBounds(labelRow.removeFromLeft(100));
    interval2Label.setBounds(labelRow.removeFromLeft(100));

    bounds.removeFromTop(20); // spacing

    auto middleRow = bounds.removeFromTop(50);
    divisionRatioSelector.setBounds(middleRow.removeFromLeft(150));
    divisionLabel.setBounds(middleRow.translated(0, 25));

    bounds.removeFromTop(20); // spacing

    auto bottomRow = bounds.removeFromTop(100);
    loopLengthButton.setBounds(bottomRow.getCentreX() - 50, bottomRow.getY(), 100, 100);
    loopLengthLabel.setBounds(loopLengthButton.getX(), loopLengthButton.getY() - 20, 100, 20);
    loopLengthValueLabel.setBounds(loopLengthButton.getX(), loopLengthButton.getBottom(), 100, 20);
}


void ReshifterAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &loopLengthButton)
    {
        auto* parameter = audioProcessor.apvts.getParameter("LOOP_LENGTH");
        jassert(parameter != nullptr);

        int currentValue = parameter->getValue();
        int numChoices = parameter->getNumSteps();
        int nextValue = (currentValue + 1) % numChoices;

        parameter->setValueNotifyingHost(nextValue);

        loopLengthValueLabel.setText(parameter->getText(nextValue, 0), juce::dontSendNotification);
    }
}

void ReshifterAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    // Can be used for custom logic if needed in the future
}
