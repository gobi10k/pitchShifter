/*
  ==============================================================================

    PitchShifter.cpp
    Created: 20 Aug 2025 7:13:00am
    Author:  Jules

  ==============================================================================
*/

#include "PitchShifter.h"

namespace
{
    // Using a Hann window for constant-power crossfading
    float getCrossfadeWindow(float phase) // phase from 0.0 to 1.0
    {
        return 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * phase));
    }
}

PitchShifter::PitchShifter()
{
}

void PitchShifter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    crossfadePhase = 0.0f;

    juce::dsp::ProcessSpec filterSpec;
    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = spec.maximumBlockSize;
    filterSpec.numChannels = 1; // Each filter processes one channel

    for (int i = 0; i < 2; ++i)
    {
        preFilter[i].prepare(filterSpec);
        bbdVoices[i].prepare(spec);
    }

    setPitch(0.0f); // Initialize pitch ratio, phase increment, and filter cutoff
}

void PitchShifter::setPitch(float pitchInSemitones)
{
    pitchRatio = std::pow(2.0f, pitchInSemitones / 12.0f);
    for (auto& voice : bbdVoices)
    {
        voice.setClockRate(pitchRatio);
    }

    phaseIncrement = pitchRatio / BBD::BBD_SIZE;

    // Update the anti-aliasing filter
    auto cutoffFreq = (sampleRate * pitchRatio) / 4.0;
    // Clamp to Nyquist
    cutoffFreq = std::min(cutoffFreq, sampleRate / 2.0 * 0.99);

    for (int i = 0; i < 2; ++i)
    {
        *preFilter[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoffFreq);
    }
}

float PitchShifter::processSample(float sample, int channel)
{
    // 1. Apply anti-aliasing filter
    float filteredSample = preFilter[channel].processSample(sample);

    // 2. Crossfade logic
    crossfadePhase += phaseIncrement;
    if (crossfadePhase >= 1.0f)
    {
        crossfadePhase -= 1.0f;
        // Reset read pointers on wrap for stability
        int writeIndex = bbdVoices[1].getWriteIndex();
        bbdVoices[1].setReadIndex(writeIndex);
        bbdVoices[0].setReadIndex(writeIndex + BBD::BBD_SIZE / 2.0f);
    }

    float gainA = getCrossfadeWindow(crossfadePhase + 0.5f);
    float gainB = getCrossfadeWindow(crossfadePhase);

    float outA = bbdVoices[0].processSample(filteredSample, channel);
    float outB = bbdVoices[1].processSample(filteredSample, channel);

    return (outA * gainA) + (outB * gainB);
}
