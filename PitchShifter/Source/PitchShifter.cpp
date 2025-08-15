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
    filter.setCutoffFrequency(15000.0f); // Higher cutoff
    filter.setResonance(0.5f);          // Less resonance

    reset();
}

void PitchShifter::reset()
{
    delayBuffer.clear();
    filter.reset();

    const float grainSize = sampleRate * 0.01f;
    writePointer = 0.0f;
    readPointerA = writePointer - grainSize;
    readPointerB = writePointer - grainSize / 2.0f;
}

float PitchShifter::interpolatedRead(float readPos)
{
    int readPosInt = static_cast<int>(readPos);
    float frac = readPos - readPosInt;

    // Ensure buffer indices are wrapped correctly and are positive
    int readPos1 = (readPosInt + delayBufferSize) % delayBufferSize;
    int readPos2 = (readPos1 + 1) % delayBufferSize;

    float sample1 = delayBuffer.getSample(0, readPos1);
    float sample2 = delayBuffer.getSample(0, readPos2);

    return sample1 * (1.0f - frac) + sample2 * frac;
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer, float pitchRatio)
{
    auto* channelData = buffer.getWritePointer(0);
    const int numSamples = buffer.getNumSamples();
    const float grainSize = sampleRate * 0.01f; // 10ms grains

    for (int i = 0; i < numSamples; ++i)
    {
        // Write input to delay buffer
        float inputSample = channelData[i];
        delayBuffer.setSample(0, (int)writePointer, inputSample);

        // Calculate crossfade gains
        float phaseA = fmod(readPointerA - writePointer + delayBufferSize, grainSize) / grainSize;
        float gainA = std::sin(phaseA * juce::MathConstants<float>::pi);

        float phaseB = fmod(readPointerB - writePointer + delayBufferSize, grainSize) / grainSize;
        float gainB = std::sin(phaseB * juce::MathConstants<float>::pi);

        // Read with interpolation
        float sampleA = interpolatedRead(readPointerA) * gainA;
        float sampleB = interpolatedRead(readPointerB) * gainB;

        // Sum outputs
        float outputSample = sampleA + sampleB;

        // Apply gentle filtering
        float cutoff = juce::jmap(pitchRatio, 0.5f, 2.0f, 8000.0f, 18000.0f);
        filter.setCutoffFrequency(cutoff);
        outputSample = filter.processSample(0, outputSample);

        // Write output
        channelData[i] = outputSample;

        // Advance pointers
        writePointer = fmod(writePointer + 1.0f, (float)delayBufferSize);
        readPointerA = fmod(readPointerA + pitchRatio, (float)delayBufferSize);
        readPointerB = fmod(readPointerB + pitchRatio, (float)delayBufferSize);
    }
}
