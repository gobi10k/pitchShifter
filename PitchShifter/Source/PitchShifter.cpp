/*
  ==============================================================================

    PitchShifter.cpp
    A pitch shifter based on a Bucket Brigade Device (BBD) model.

  ==============================================================================
*/

#include "PitchShifter.h"

PitchShifter::PitchShifter()
    : delayBufferSize(0), sampleRate(44100.0),
      readPointerA(0.0f), readPointerB(0.0f), writePointer(0.0f)
{
}

void PitchShifter::prepare(const juce::dsp::ProcessSpec& spec)
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

void PitchShifter::reset()
{
    delayBuffer.clear();
    filter.reset();

    writePointer = 0.0f;
    // Stagger the read pointers by half a grain size
    const float grainSize = sampleRate * 0.05f; // 50ms grain
    readPointerA = 0.0f;
    readPointerB = grainSize / 2.0f;
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer, float pitchRatio)
{
    auto* channelData = buffer.getWritePointer(0);
    const int numSamples = buffer.getNumSamples();
    const float grainSize = sampleRate * 0.05f; // 50ms grains

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Write input to the manual delay buffer
        delayBuffer.setSample(0, (int)writePointer, channelData[i]);

        // 2. Calculate gain for each tap using a sine window for equal-power crossfade
        float phaseA = fmod(readPointerA, grainSize) / grainSize;
        float gainA = std::sin(phaseA * juce::MathConstants<float>::pi);

        float phaseB = fmod(readPointerB, grainSize) / grainSize;
        float gainB = std::sin(phaseB * juce::MathConstants<float>::pi);

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

        // 4. Sum the crossfaded signals. Normalization is implicitly handled by the sine windows.
        float outputSample = sampleA + sampleB;

        // 5. Apply BBD-style low-pass filter
        float cutoff = 10000.0f / pitchRatio;
        filter.setCutoffFrequency(juce::jlimit(1000.0f, 12000.0f, cutoff));
        outputSample = filter.processSample(0, outputSample);

        // 6. Write to output buffer
        channelData[i] = outputSample;

        // 7. Advance pointers, wrapping around the main buffer
        writePointer = fmod(writePointer + 1.0f, (float)delayBufferSize);
        readPointerA = fmod(readPointerA + pitchRatio, (float)delayBufferSize);
        readPointerB = fmod(readPointerB + pitchRatio, (float)delayBufferSize);
    }
}
