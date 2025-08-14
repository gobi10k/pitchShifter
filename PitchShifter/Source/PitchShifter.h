/*
  ==============================================================================

    PitchShifter.h
    Created: 14 Aug 2025
    Author:  Enhanced Implementation with Phase Coherence

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <complex>
#include <juce_dsp/juce_dsp.h>

class PitchShifter
{
public:
    PitchShifter();
    ~PitchShifter();

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void process(juce::AudioBuffer<float>& buffer, juce::LinearSmoothedValue<float>* smoother, float mix, float outputGain);
    
    // Quality settings
    void setQuality(int quality); // 0=fast, 1=balanced, 2=high
    void setFormantPreservation(bool enabled);

private:
    //==============================================================================
    // Core STFT processing
    void processFrame(float pitchShiftRatio);
    void performSTFT(const float* input, std::complex<float>* fftData);
    void performISTFT(const std::complex<float>* fftData, float* output);
    
    // Enhanced phase processing with coherence improvement
    void processPhases(std::complex<float>* fftData, float pitchShiftRatio);
    float detectTransient(const std::vector<float>& currentMags);
    void findSpectralPeaks(const float* magnitudes, std::vector<int>& peaks);
    void propagatePhase(std::complex<float>* fftData, const std::vector<int>& peaks, float pitchShiftRatio);
    
    // Improved formant preservation
    void preserveFormants(std::complex<float>* fftData, float pitchShiftRatio);
    void estimateSpectralEnvelope(const float* magnitudes, float* envelope);
    void applySpectralEnvelope(std::complex<float>* fftData, const float* envelope);
    
    // Utility functions
    void applyWindow(float* data, int size, bool useShortWindow = false);
    float calculateInstantaneousFrequency(int bin, float phaseDiff);
    
    //==============================================================================
    // Parameters
    double sampleRate;
    int fftSize;
    int hopSize;
    int overlap;
    
    // Quality settings
    int qualityLevel;
    bool formantPreservationEnabled;
    
    // FFT processing
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> shortWindow; // For transients
    
    // Buffers
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    juce::AudioBuffer<float> overlapBuffer;
    std::vector<std::complex<float>> fftData;
    std::vector<float> magnitudes;
    std::vector<float> phases;
    std::vector<float> spectralEnvelope;
    
    // Enhanced phase tracking
    std::vector<float> lastPhase;
    std::vector<float> lastMagnitudes;
    std::vector<float> phaseAccumulator;
    std::vector<float> lastInputPhase;
    std::vector<float> lastOutputPhase;
    
    // Transient detection
    float lastSpectralFlux;
    bool isTransientFrame;
    
    // Peak detection
    std::vector<int> spectralPeaks;
    std::vector<float> peakMagnitudes;
    
    // Circular buffer management
    int inputBufferPos;
    int outputBufferPos;
    int samplesInInputBuffer;

    // Dry signal delay line
    juce::dsp::DelayLine<float> dryDelay;
    
    // Constants
    static constexpr float PI = juce::MathConstants<float>::pi;
    static constexpr float TWO_PI = 2.0f * PI;
    static constexpr float TRANSIENT_THRESHOLD = 0.5f;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShifter)
};
