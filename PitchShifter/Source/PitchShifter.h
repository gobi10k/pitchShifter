/*
  ==============================================================================

    PitchShifter.h
    Created: 13 Aug 2025 11:20:00am
    Author:  Jules

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PitchShifter
{
public:
    PitchShifter();
    ~PitchShifter();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void process(juce::AudioBuffer<float>& buffer, juce::LinearSmoothedValue<float>* smoother);

private:
    void processFrame(float pitchShiftRatio);
    void findPeaks(std::vector<int>& peakLocations, const float* magnitudes, int numMagnitudes);
    //==============================================================================
    // STFT variables
    int fftSize;
    int hopSize;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    // Buffers for processing
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    juce::AudioBuffer<float> fftBuffer;

    // Phase vocoder variables
    juce::AudioBuffer<float> lastInputPhase;
    juce::AudioBuffer<float> lastOutputPhase;

    int inputBufferPos;
    int outputBufferPos;
    int samplesInInputBuffer;

    std::vector<int> peakLocations;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShifter)
};
