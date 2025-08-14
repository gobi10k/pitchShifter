/*
  ==============================================================================

    BBDPitchShifter.cpp
    Created: 14 Aug 2025
    A pitch shifter based on a Bucket Brigade Device (BBD) model.

  ==============================================================================
*/

#include "BBDPitchShifter.h"

BBDPitchShifter::BBDPitchShifter()
    : delayBufferSize(0), sampleRate(44100.0),
      readPointerA(0.0f), readPointerB(0.0f), writePointer(0.0f)
{
}

void BBDPitchShifter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = (float)spec.sampleRate;
    delayBufferSize = (int)(maxDelayTime * sampleRate);

    delayBuffer.setSize(1, delayBufferSize);
    delayBuffer.clear();

    filter.prepare(spec);
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setCutoffFrequency(10000.0f);
    filter.setResonance(0.707f);

    reset();
}

void BBDPitchShifter::reset()
{
    delayBuffer.clear();
    filter.reset();

    writePointer = 0.0f;
    readPointerA = 0.0f;
    // Stagger the second tap by half the buffer size
    readPointerB = (float)delayBufferSize / 2.0f;
}

void BBDPitchShifter::process(juce::AudioBuffer<float>& buffer, float pitchRatio)
{
    auto* channelData = buffer.getWritePointer(0);
    const int numSamples = buffer.getNumSamples();
    const float grainSize = (float)delayBufferSize;
    const float fadeTime = grainSize * 0.1f;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Write input to the manual delay buffer
        delayBuffer.setSample(0, (int)writePointer, channelData[i]);

        // 2. Calculate gain for each tap based on its window position
        float gainA = 1.0f;
        float posInGrainA = fmod(readPointerA, grainSize);
        if (posInGrainA < fadeTime) gainA = posInGrainA / fadeTime;
        else if (posInGrainA > grainSize - fadeTime) gainA = (grainSize - posInGrainA) / fadeTime;

        float gainB = 1.0f;
        float posInGrainB = fmod(readPointerB, grainSize);
        if (posInGrainB < fadeTime) gainB = posInGrainB / fadeTime;
        else if (posInGrainB > grainSize - fadeTime) gainB = (grainSize - posInGrainB) / fadeTime;

        // 3. Read from both taps with linear interpolation
        int readPosA1 = (int)readPointerA;
        int readPosA2 = (readPosA1 + 1) % delayBufferSize;
        float fracA = readPointerA - readPosA1;
        float sampleA1 = delayBuffer.getSample(0, readPosA1);
        float sampleA2 = delayBuffer.getSample(0, readPosA2);
        float sampleA = (sampleA1 * (1.0f - fracA) + sampleA2 * fracA) * gainA;

        int readPosB1 = (int)readPointerB;
        int readPosB2 = (readPosB1 + 1) % delayBufferSize;
        float fracB = readPointerB - readPosB1;
        float sampleB1 = delayBuffer.getSample(0, readPosB1);
        float sampleB2 = delayBuffer.getSample(0, readPosB2);
        float sampleB = (sampleB1 * (1.0f - fracB) + sampleB2 * fracB) * gainB;

        // 4. Sum the crossfaded signals
        float outputSample = sampleA + sampleB;

        // 5. Apply BBD-style low-pass filter
        float cutoff = 10000.0f / pitchRatio;
        filter.setCutoffFrequency(juce::jlimit(1000.0f, 12000.0f, cutoff));
        outputSample = filter.processSample(0, outputSample);

        // 6. Write to output buffer
        channelData[i] = outputSample;

        // 7. Advance pointers, wrapping around the buffer
        writePointer = fmod(writePointer + 1.0f, grainSize);
        readPointerA = fmod(readPointerA + pitchRatio, grainSize);
        readPointerB = fmod(readPointerB + pitchRatio, grainSize);
    }
}
