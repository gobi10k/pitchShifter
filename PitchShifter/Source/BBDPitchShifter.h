/*
  ==============================================================================

    BBDPitchShifter.h
    Created: 14 Aug 2025
    A pitch shifter based on a Bucket Brigade Device (BBD) model.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class BBDPitchShifter
{
public:
    BBDPitchShifter();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::AudioBuffer<float>& buffer, float pitchRatio);
    void reset();

private:
    // Core components
    juce::AudioBuffer<float> delayBuffer;
    juce::dsp::StateVariableTPTFilter<float> filter;

    // BBD parameters
    static constexpr float maxDelayTime = 2.0f; // Maximum delay in seconds
    int delayBufferSize;
    float sampleRate;

    // Dual-tap resampling state
    float readPointerA;
    float readPointerB;
    float writePointer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDPitchShifter)
};
