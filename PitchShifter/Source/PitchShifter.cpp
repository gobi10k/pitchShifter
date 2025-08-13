/*
  ==============================================================================

    PitchShifter.cpp
    Created: 13 Aug 2025 11:20:00am
    Author:  Jules

  ==============================================================================
*/

#include "PitchShifter.h"

// This is a basic phase vocoder implementation. For a true "studio-grade"
// pitch shifter, more advanced techniques could be used to mitigate artifacts,
// such as:
// - Transient detection and preservation to avoid smearing of percussive sounds.
// - Phase locking to reduce phasiness in the output.
// - More sophisticated overlap-add techniques.
// However, this implementation serves as a good starting point.

PitchShifter::PitchShifter() : fftSize(2048), hopSize(512), fft(fftSize), window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
}

PitchShifter::~PitchShifter()
{
}

void PitchShifter::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize buffers
    inputBuffer.setSize(1, fftSize);
    outputBuffer.setSize(1, fftSize);
    fftBuffer.setSize(1, fftSize * 2); // Complex buffer

    lastInputPhase.setSize(1, fftSize / 2 + 1);
    lastOutputPhase.setSize(1, fftSize / 2 + 1);

    inputBuffer.clear();
    outputBuffer.clear();
    fftBuffer.clear();
    lastInputPhase.clear();
    lastOutputPhase.clear();

    inputBufferPos = 0;
    outputBufferPos = 0;

    samplesInInputBuffer = 0;
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer, juce::LinearSmoothedValue<float>* smoother)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            // Get a sample from the output buffer
            const float outSample = outputBuffer.getSample(0, outputBufferPos);
            // Overwrite the oldest sample in the output buffer
            outputBuffer.setSample(0, outputBufferPos, 0.0f);
            // Write the output sample to the main buffer
            channelData[i] = outSample;

            // Increment the output buffer position
            outputBufferPos = (outputBufferPos + 1) % fftSize;

            // Add the input sample to our input buffer
            inputBuffer.setSample(0, inputBufferPos, channelData[i]);
            inputBufferPos = (inputBufferPos + 1) % fftSize;
            samplesInInputBuffer++;

            // If we have enough samples for a hop, process a frame
            if (samplesInInputBuffer >= hopSize)
            {
                float pitchShift = smoother->getNextValue();
                float pitchShiftRatio = pow(2.0f, pitchShift / 12.0f);
                processFrame(pitchShiftRatio);
                samplesInInputBuffer -= hopSize;
            }
        }
    }
}

void PitchShifter::processFrame(float pitchShiftRatio)
{
    // Copy input buffer to FFT buffer and apply window
    for (int i = 0; i < fftSize; ++i)
    {
        int circularBufferIndex = (inputBufferPos + i) % fftSize;
        fftBuffer.setSample(0, i * 2, inputBuffer.getSample(0, circularBufferIndex) * window.getSample(i));
        fftBuffer.setSample(0, i * 2 + 1, 0.0f);
    }

    // Perform FFT
    fft.perform(fftBuffer.getWritePointer(0), fftBuffer.getWritePointer(0), false);

    for (int i = 0; i < fftSize / 2 + 1; ++i)
    {
        // Get magnitude and phase
        float magnitude = std::abs(std::complex<float>(fftBuffer.getSample(0, i * 2), fftBuffer.getSample(0, i * 2 + 1)));
        float phase = std::arg(std::complex<float>(fftBuffer.getSample(0, i * 2), fftBuffer.getSample(0, i * 2 + 1)));

        // Calculate phase difference
        float phaseDifference = phase - lastInputPhase.getSample(0, i);
        lastInputPhase.setSample(0, i, phase);

        // Calculate frequency deviation
        float freqDev = phaseDifference - (float)i * 2.0f * juce::MathConstants<float>::pi * (float)hopSize / (float)fftSize;
        freqDev = fmod(freqDev + juce::MathConstants<float>::pi, 2.0f * juce::MathConstants<float>::pi) - juce::MathConstants<float>::pi;

        // Calculate true frequency
        float trueFreq = (float)i * 2.0f * juce::MathConstants<float>::pi / (float)fftSize + freqDev / (float)hopSize;

        // Calculate new phase
        float newPhase = lastOutputPhase.getSample(0, i) + trueFreq * (float)hopSize * pitchShiftRatio;
        lastOutputPhase.setSample(0, i, newPhase);

        // Convert back to complex
        std::complex<float> newComplex = std::polar(magnitude, newPhase);
        fftBuffer.setSample(0, i * 2, newComplex.real());
        fftBuffer.setSample(0, i * 2 + 1, newComplex.imag());
    }

    // Perform inverse FFT
    fft.perform(fftBuffer.getWritePointer(0), fftBuffer.getWritePointer(0), true);

    // Overlap-add to output buffer
    for (int i = 0; i < fftSize; ++i)
    {
        int circularBufferIndex = (outputBufferPos + i) % fftSize;
        outputBuffer.addSample(0, circularBufferIndex, fftBuffer.getSample(0, i * 2) * window.getSample(i));
    }
}
