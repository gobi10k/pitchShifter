/*
  ==============================================================================

    PitchShifter.h
    Created: 20 Aug 2025 7:13:00am
    Author:  Jules

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "BBD.h"

class PitchShifter
{
public:
    PitchShifter();

    void prepare(const juce::dsp::ProcessSpec& spec);
    float processSample(float sample, int channel);
    void setPitch(float pitchInSemitones);

private:
    juce::dsp::IIR::Filter<float> preFilter[2];

    static constexpr int NUM_BBD_VOICES = 2;
    BBD bbdVoices[NUM_BBD_VOICES];

    float pitchRatio = 1.0f;

    double sampleRate = 44100.0;
    float crossfadePhase = 0.0f;
    float phaseIncrement = 0.0f;
};
