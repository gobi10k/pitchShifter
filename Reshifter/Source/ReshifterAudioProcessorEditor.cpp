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
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "MODE", modeSelector);
    manualTempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "MANUAL_TEMPO", manualTempoSlider);
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "GLIDE", glideSlider);
    interval1PitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "INTERVAL1_PITCH", interval1PitchSelector);
    interval2PitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "INTERVAL2_PITCH", interval2PitchSelector);
    divisionRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "DIVISION_RATIO", divisionRatioSelector);

    // --- Sliders ---
    addAndMakeVisible(glideSlider);
    glideSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    glideSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    addAndMakeVisible(manualTempoSlider);
    manualTempoSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    manualTempoSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);

    // --- Combo Boxes ---
    addAndMakeVisible(modeSelector);
    addAndMakeVisible(interval1PitchSelector);
    addAndMakeVisible(interval2PitchSelector);
    addAndMakeVisible(divisionRatioSelector);

    // --- Buttons ---
    addAndMakeVisible(loopLengthButton);
    loopLengthButton.addListener(this);

    // --- Labels ---
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
    };

    setupLabel(modeLabel, "Mode");
    setupLabel(manualTempoLabel, "Tempo");
    setupLabel(glideLabel, "Glide");
    setupLabel(interval1Label, "Interval 1");
    setupLabel(interval2Label, "Interval 2");
    setupLabel(divisionLabel, "Division");
    setupLabel(loopLengthLabel, "Loop Length");

    setSize (400, 300);
    startTimerHz(30);
}

ReshifterAudioProcessorEditor::~ReshifterAudioProcessorEditor()
{
    stopTimer();
    loopLengthButton.removeListener(this);
}

//==============================================================================
void ReshifterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

void ReshifterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    auto row = bounds.removeFromTop(70);
    modeSelector.setBounds(row.removeFromLeft(100));
    manualTempoSlider.setBounds(row.removeFromLeft(100));
    glideSlider.setBounds(row.removeFromLeft(100));

    auto labelRow = bounds.removeFromTop(20);
    modeLabel.setBounds(labelRow.removeFromLeft(100));
    manualTempoLabel.setBounds(labelRow.removeFromLeft(100));
    glideLabel.setBounds(labelRow.removeFromLeft(100));

    bounds.removeFromTop(20);

    row = bounds.removeFromTop(70);
    interval1PitchSelector.setBounds(row.removeFromLeft(100));
    interval2PitchSelector.setBounds(row.removeFromLeft(100));
    divisionRatioSelector.setBounds(row.removeFromLeft(100));

    labelRow = bounds.removeFromTop(20);
    interval1Label.setBounds(labelRow.removeFromLeft(100));
    interval2Label.setBounds(labelRow.removeFromLeft(100));
    divisionLabel.setBounds(labelRow.removeFromLeft(100));

    bounds.removeFromTop(20);

    auto buttonBounds = bounds.removeFromTop(50).reduced(0, 10);
    loopLengthLabel.setBounds(buttonBounds.removeFromLeft(100));
    loopLengthButton.setBounds(buttonBounds);
}


void ReshifterAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &loopLengthButton)
    {
        auto* parameter = audioProcessor.apvts.getParameter("LOOP_LENGTH");
        if (parameter == nullptr) return;

        auto range = parameter->getNormalisableRange();
        int numChoices = range.end - range.start + 1; // Assuming choices are 0, 1, 2...
        if (numChoices <= 1) return;

        float currentValue = parameter->getValue();
        int currentIndex = static_cast<int>(currentValue * (numChoices - 1) + 0.5f);

        int nextIndex = (currentIndex + 1) % numChoices;

        float nextValueNormalized = (float)nextIndex / (float)(numChoices - 1);

        parameter->setValueNotifyingHost(nextValueNormalized);
    }
}

void ReshifterAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
}

void ReshifterAudioProcessorEditor::timerCallback()
{
    auto* parameter = audioProcessor.apvts.getParameter("LOOP_LENGTH");
    if (parameter == nullptr) return;

    float currentValue = parameter->getValue();
    juce::String newText = parameter->getText(currentValue, 0);

    if (loopLengthButton.getButtonText() != newText)
    {
        loopLengthButton.setButtonText(newText);
    }
}
