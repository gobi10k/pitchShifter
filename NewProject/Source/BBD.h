/*
  ==============================================================================

    BBD.h
    Created: 20 Aug 2025 7:10:00am
    Author:  Jules

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>

class BBD
{
public:
    static constexpr int BBD_SIZE = 512;

    BBD();

    void prepare(const juce::dsp::ProcessSpec& spec);
    float processSample(float sample, int channel);
    void setClockRate(float newClockRate);

    void setReadIndex(float newReadIndex);
    int getWriteIndex() const;
    float getReadIndex() const;

private:
    juce::AudioBuffer<float> delayLine;
    int writeIndex { 0 };
    float readIndex { 0.0f };

    float clockRate { 1.0f }; // As a factor of the main sample rate
    double sampleRate { 44100.0 };
};
