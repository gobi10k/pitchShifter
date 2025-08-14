/*
  ==============================================================================

    Tests.cpp
    Created: 13 Aug 2025 3:45:00pm
    Author:  Jules

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PitchShifter.h"

class PitchShifterTest : public juce::UnitTest
{
public:
    PitchShifterTest() : juce::UnitTest("Pitch Shifter Test") {}

    void runTest() override
    {
        beginTest("Pitch Shifter Test");

        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 1024; // Use a larger block size for more harmonics
        shifter.prepareToPlay(sampleRate, blockSize);

        // Create a sawtooth wave at 220 Hz
        juce::AudioBuffer<float> buffer(1, blockSize);
        float phase = 0.0f;
        float increment = 2.0f * juce::MathConstants<float>::pi * 220.0f / (float)sampleRate;
        for (int i = 0; i < blockSize; ++i)
        {
            buffer.setSample(0, i, phase / juce::MathConstants<float>::pi - 1.0f);
            phase += increment;
            if (phase >= juce::MathConstants<float>::pi)
                phase -= 2.0f * juce::MathConstants<float>::pi;
        }

        // Process with a pitch shift of +12 semitones (up one octave)
        juce::LinearSmoothedValue<float> smoother;
        smoother.reset(sampleRate, 0.0);
        smoother.setTargetValue(12.0f);
        shifter.process(buffer, &smoother);

        // Analyze the output to see if the pitch is now 440 Hz
        juce::dsp::FFT fft(2048);
        juce::AudioBuffer<float> fftBuffer(1, 2048 * 2);
        fftBuffer.clear();
        fftBuffer.copyFrom(0, 0, buffer.getReadPointer(0), blockSize);

        fft.performFrequencyOnlyForwardTransform(fftBuffer.getWritePointer(0));

        float maxLevel = 0.0f;
        int maxIndex = 0;
        auto* magnitudes = fftBuffer.getReadPointer(0);
        for (int i = 0; i < 1024; ++i)
        {
            float level = magnitudes[i];
            if (level > maxLevel)
            {
                maxLevel = level;
                maxIndex = i;
            }
        }

        float frequency = (float)maxIndex * (float)sampleRate / 2048.0f;
        expectWithinAbsoluteError(frequency, 440.0f, 50.0f); // Allow some error
    }
};

static PitchShifterTest pitchShifterTest;
