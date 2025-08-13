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
        const int blockSize = 512;
        shifter.prepareToPlay(sampleRate, blockSize);

        // Create a sine wave at 440 Hz
        juce::AudioBuffer<float> buffer(1, blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            buffer.setSample(0, i, std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * i / sampleRate));
        }

        // Process with a pitch shift of +12 semitones (up one octave)
        juce::LinearSmoothedValue<float> smoother;
        smoother.reset(sampleRate, 0.0);
        smoother.setTargetValue(12.0f);
        shifter.process(buffer, &smoother);

        // Analyze the output to see if the pitch is now 880 Hz
        // This is a simplified analysis. A more robust test would use a more
        // sophisticated method to find the fundamental frequency.
        juce::dsp::FFT fft(2048);
        juce::AudioBuffer<float> fftBuffer(1, 2048 * 2);
        fftBuffer.clear();
        fftBuffer.copyFrom(0, 0, buffer.getReadPointer(0), blockSize);

        fft.performFrequencyOnlyForwardTransform(fftBuffer.getWritePointer(0));

        float maxLevel = 0.0f;
        int maxIndex = 0;
        for (int i = 0; i < 1024; ++i)
        {
            float level = fft.getMagnitude(i);
            if (level > maxLevel)
            {
                maxLevel = level;
                maxIndex = i;
            }
        }

        float frequency = (float)maxIndex * (float)sampleRate / 2048.0f;
        expectWithinAbsoluteError(frequency, 880.0f, 50.0f); // Allow some error
    }
};

static PitchShifterTest pitchShifterTest;
