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
            const float inSample = channelData[i];
            const float outSample = outputBuffer.getSample(0, outputBufferPos);

            channelData[i] = outSample;

            outputBuffer.setSample(0, outputBufferPos, 0.0f);
            outputBufferPos = (outputBufferPos + 1) % fftSize;

            inputBuffer.setSample(0, inputBufferPos, inSample);
            inputBufferPos = (inputBufferPos + 1) % fftSize;
            samplesInInputBuffer++;

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
    // Copy input buffer to a temporary buffer for windowing
    juce::AudioBuffer<float> windowedInput(1, fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        int circularBufferIndex = (inputBufferPos + i) % fftSize;
        windowedInput.setSample(0, i, inputBuffer.getSample(0, circularBufferIndex));
    }

    // Apply window
    window.multiplyWithWindowingTable(windowedInput.getWritePointer(0), fftSize);

    // Copy to complex buffer for FFT
    for (int i = 0; i < fftSize; ++i)
    {
        fftBuffer.setSample(0, i * 2, windowedInput.getSample(0, i));
        fftBuffer.setSample(0, i * 2 + 1, 0.0f);
    }

    // Perform FFT
    fft.perform(reinterpret_cast<const juce::dsp::Complex<float>*>(fftBuffer.getReadPointer(0)),
                reinterpret_cast<juce::dsp::Complex<float>*>(fftBuffer.getWritePointer(0)),
                false);

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
    fft.perform(reinterpret_cast<const juce::dsp::Complex<float>*>(fftBuffer.getReadPointer(0)),
                reinterpret_cast<juce::dsp::Complex<float>*>(fftBuffer.getWritePointer(0)),
                true);

    // Apply window to IFFT output
    juce::AudioBuffer<float> windowedOutput(1, fftSize);
     for (int i = 0; i < fftSize; ++i)
    {
        windowedOutput.setSample(0, i, fftBuffer.getSample(0, i * 2));
    }
    window.multiplyWithWindowingTable(windowedOutput.getWritePointer(0), fftSize);

    // Overlap-add to output buffer
    for (int i = 0; i < fftSize; ++i)
    {
        int circularBufferIndex = (outputBufferPos + i) % fftSize;
        outputBuffer.addSample(0, circularBufferIndex, windowedOutput.getSample(0, i));
    }
}
