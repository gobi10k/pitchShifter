/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// A helper function to create a label for a slider
void setupLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
}

//==============================================================================
PitchShiftAudioProcessorEditor::PitchShiftAudioProcessorEditor (PitchShiftAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    auto& apvts = audioProcessor.getAPVTS();

    for (int i = 0; i < PitchShiftAudioProcessor::NumPresets; ++i)
    {
        presetComboBox.addItem(audioProcessor.getProgramName(i), i + 1);
    }
    addAndMakeVisible(presetComboBox);
    presetComboBox.onChange = [this]
    {
        audioProcessor.setCurrentProgram(presetComboBox.getSelectedId() - 1);
    };

    auto setupSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& name, const juce::String& paramID)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
        addAndMakeVisible(slider);
        setupLabel(label, name);
        addAndMakeVisible(label);
    };

    setupSlider(pitchSlider, pitchLabel, "Pitch", "PITCH");
    pitchAttachment = std::make_unique<SliderAttachment>(apvts, "PITCH", pitchSlider);

    setupSlider(feedbackSlider, feedbackLabel, "Feedback", "FEEDBACK");
    feedbackAttachment = std::make_unique<SliderAttachment>(apvts, "FEEDBACK", feedbackSlider);

    setupSlider(mixSlider, mixLabel, "Mix", "MIX");
    mixAttachment = std::make_unique<SliderAttachment>(apvts, "MIX", mixSlider);

    setupSlider(inputGainSlider, inputGainLabel, "Input", "INPUT_GAIN");
    inputGainAttachment = std::make_unique<SliderAttachment>(apvts, "INPUT_GAIN", inputGainSlider);

    setupSlider(outputGainSlider, outputGainLabel, "Output", "OUTPUT_GAIN");
    outputGainAttachment = std::make_unique<SliderAttachment>(apvts, "OUTPUT_GAIN", outputGainSlider);

    setupSlider(toneCutoffSlider, toneCutoffLabel, "Cutoff", "TONE_CUTOFF");
    toneCutoffAttachment = std::make_unique<SliderAttachment>(apvts, "TONE_CUTOFF", toneCutoffSlider);

    setupSlider(toneResonanceSlider, toneResonanceLabel, "Resonance", "TONE_RESONANCE");
    toneResonanceAttachment = std::make_unique<SliderAttachment>(apvts, "TONE_RESONANCE", toneResonanceSlider);

    setupSlider(noiseSlider, noiseLabel, "Noise", "NOISE");
    noiseAttachment = std::make_unique<SliderAttachment>(apvts, "NOISE", noiseSlider);

    setupSlider(lfoRateSlider, lfoRateLabel, "LFO Rate", "LFO_RATE");
    lfoRateAttachment = std::make_unique<SliderAttachment>(apvts, "LFO_RATE", lfoRateSlider);

    setupSlider(lfoDepthSlider, lfoDepthLabel, "LFO Depth", "LFO_DEPTH");
    lfoDepthAttachment = std::make_unique<SliderAttachment>(apvts, "LFO_DEPTH", lfoDepthSlider);

    toneModeComboBox.addItemList({ "LPF", "BPF", "HPF" }, 1);
    addAndMakeVisible(toneModeComboBox);
    toneModeAttachment = std::make_unique<ComboBoxAttachment>(apvts, "TONE_MODE", toneModeComboBox);

    lfoWaveformComboBox.addItemList({ "Sine", "Triangle", "Sample & Hold" }, 1);
    addAndMakeVisible(lfoWaveformComboBox);
    lfoWaveformAttachment = std::make_unique<ComboBoxAttachment>(apvts, "LFO_WAVEFORM", lfoWaveformComboBox);

    addAndMakeVisible(vuMeter[0]);
    addAndMakeVisible(vuMeter[1]);
    addAndMakeVisible(lfoDisplay);

    startTimerHz(24);

    setSize (700, 450);
}

PitchShiftAudioProcessorEditor::~PitchShiftAudioProcessorEditor()
{
}

//==============================================================================
void PitchShiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromString("#34495e"));
}

void PitchShiftAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    juce::FlexBox topBox;
    topBox.flexDirection = juce::FlexBox::Direction::row;
    topBox.items.add(juce::FlexItem(presetComboBox).withFlex(1.0f).withMargin(5));
    topBox.performLayout(bounds.removeFromTop(40));

    juce::FlexBox mainBox;
    mainBox.flexDirection = juce::FlexBox::Direction::row;

    auto createSliderBox = [&](juce::Slider& slider, juce::Label& label)
    {
        juce::FlexBox box;
        box.flexDirection = juce::FlexBox::Direction::column;
        box.items.add(juce::FlexItem(label).withFlex(0.2f));
        box.items.add(juce::FlexItem(slider).withFlex(0.8f));
        return box;
    };

    juce::FlexBox pitchBox = createSliderBox(pitchSlider, pitchLabel);
    juce::FlexBox feedbackBox = createSliderBox(feedbackSlider, feedbackLabel);
    juce::FlexBox mixBox = createSliderBox(mixSlider, mixLabel);

    juce::FlexBox coreParamsBox;
    coreParamsBox.flexDirection = juce::FlexBox::Direction::row;
    coreParamsBox.items.add(juce::FlexItem(pitchBox).withFlex(1.0f));
    coreParamsBox.items.add(juce::FlexItem(feedbackBox).withFlex(1.0f));
    coreParamsBox.items.add(juce::FlexItem(mixBox).withFlex(1.0f));

    juce::FlexBox toneBox = createSliderBox(toneCutoffSlider, toneCutoffLabel);
    juce::FlexBox resoBox = createSliderBox(toneResonanceSlider, toneResonanceLabel);
    juce::FlexBox noiseBox = createSliderBox(noiseSlider, noiseLabel);

    juce::FlexBox toneAndNoiseBox;
    toneAndNoiseBox.flexDirection = juce::FlexBox::Direction::row;
    toneAndNoiseBox.items.add(juce::FlexItem(toneBox).withFlex(1.0f));
    toneAndNoiseBox.items.add(juce::FlexItem(resoBox).withFlex(1.0f));
    toneAndNoiseBox.items.add(juce::FlexItem(noiseBox).withFlex(1.0f));

    juce::FlexBox lfoRateBox = createSliderBox(lfoRateSlider, lfoRateLabel);
    juce::FlexBox lfoDepthBox = createSliderBox(lfoDepthSlider, lfoDepthLabel);

    juce::FlexBox lfoBox;
    lfoBox.flexDirection = juce::FlexBox::Direction::row;
    lfoBox.items.add(juce::FlexItem(lfoRateBox).withFlex(1.0f));
    lfoBox.items.add(juce::FlexItem(lfoDepthBox).withFlex(1.0f));

    juce::FlexBox leftAndMiddleColumn;
    leftAndMiddleColumn.flexDirection = juce::FlexBox::Direction::column;
    leftAndMiddleColumn.items.add(juce::FlexItem(coreParamsBox).withFlex(1.0f));
    leftAndMiddleColumn.items.add(juce::FlexItem(toneAndNoiseBox).withFlex(1.0f));
    leftAndMiddleColumn.items.add(juce::FlexItem(lfoBox).withFlex(1.0f));

    juce::FlexBox gainAndMeterBox;
    gainAndMeterBox.flexDirection = juce::FlexBox::Direction::row;
    juce::FlexBox inputBox = createSliderBox(inputGainSlider, inputGainLabel);
    juce::FlexBox outputBox = createSliderBox(outputGainSlider, outputGainLabel);
    gainAndMeterBox.items.add(juce::FlexItem(inputBox).withFlex(1.0f));
    gainAndMeterBox.items.add(juce::FlexItem(outputBox).withFlex(1.0f));
    gainAndMeterBox.items.add(juce::FlexItem(vuMeter[0]).withFlex(0.3f).withMargin(5));
    gainAndMeterBox.items.add(juce::FlexItem(vuMeter[1]).withFlex(0.3f).withMargin(5));

    mainBox.items.add(juce::FlexItem(leftAndMiddleColumn).withFlex(3.0f));
    mainBox.items.add(juce::FlexItem(gainAndMeterBox).withFlex(1.5f));

    mainBox.performLayout(bounds);
}

void PitchShiftAudioProcessorEditor::timerCallback()
{
    vuMeter[0].setLevel(audioProcessor.getRMSLevel(0));
    vuMeter[1].setLevel(audioProcessor.getRMSLevel(1));

    auto waveform = audioProcessor.getAPVTS().getRawParameterValue("LFO_WAVEFORM")->load();
    lfoDisplay.setWaveform(static_cast<int>(waveform));

    auto currentPreset = audioProcessor.getCurrentProgram();
    if (currentPreset != lastPresetIndex)
    {
        lastPresetIndex = currentPreset;
        presetComboBox.setSelectedId(currentPreset + 1, juce::dontSendNotification);
    }
}
