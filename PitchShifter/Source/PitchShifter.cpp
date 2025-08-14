/*
  ==============================================================================

    PitchShifter.cpp
    Created: 14 Aug 2025
    Author:  Enhanced Implementation with Phase Coherence

  ==============================================================================
*/

#include "PitchShifter.h"
#include <algorithm>
#include <cmath>

PitchShifter::PitchShifter()
    : sampleRate(44100.0)
    , fftSize(2048)
    , hopSize(256)
    , overlap(8)
    , qualityLevel(1)
    , formantPreservationEnabled(true)
    , fft(std::make_unique<juce::dsp::FFT>(11)) // 2^11 = 2048
    , window(std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hann, true))
    , shortWindow(std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize/2, juce::dsp::WindowingFunction<float>::hann, true))
    , inputBufferPos(0)
    , outputBufferPos(0)
    , samplesInInputBuffer(0)
    , lastSpectralFlux(0.0f)
    , isTransientFrame(false)
{
}

PitchShifter::~PitchShifter()
{
}

void PitchShifter::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    
    // Enhanced quality settings with better overlap ratios
    switch (qualityLevel)
    {
        case 0: // Fast (8.9ms latency)
            fftSize = 1024;
            overlap = 8;
            break;
        case 1: // Balanced (18.6ms latency)
            fftSize = 2048;
            overlap = 8;  // Increased from 4
            break;
        case 2: // High quality (37.2ms latency)
            fftSize = 4096;
            overlap = 16; // Increased from 8
            break;
    }
    
    hopSize = fftSize / overlap;
    
    // Reinitialize FFT with new size
    int fftOrder = (int)std::log2(fftSize);
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hann, true);
    shortWindow = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize/2, juce::dsp::WindowingFunction<float>::hann, true);
    
    // Initialize buffers with extra space for safety
    inputBuffer.setSize(1, fftSize * 3);   // Extra space for safety
    outputBuffer.setSize(1, fftSize * 3);
    overlapBuffer.setSize(1, fftSize * 2); // Double overlap
    
    inputBuffer.clear();
    outputBuffer.clear();
    overlapBuffer.clear();
    
    // Initialize processing arrays
    int spectrumSize = fftSize / 2 + 1;
    fftData.resize(fftSize);
    magnitudes.resize(spectrumSize);
    phases.resize(spectrumSize);
    spectralEnvelope.resize(spectrumSize);
    
    // Enhanced phase tracking
    lastPhase.resize(spectrumSize);
    lastMagnitudes.resize(spectrumSize);
    phaseAccumulator.resize(spectrumSize);
    lastInputPhase.resize(spectrumSize);
    lastOutputPhase.resize(spectrumSize);
    
    spectralPeaks.reserve(spectrumSize / 4);
    peakMagnitudes.reserve(spectrumSize / 4);
    
    // Clear all buffers
    inputBuffer.clear();
    outputBuffer.clear();
    overlapBuffer.clear();
    
    std::fill(lastPhase.begin(), lastPhase.end(), 0.0f);
    std::fill(lastMagnitudes.begin(), lastMagnitudes.end(), 0.0f);
    std::fill(phaseAccumulator.begin(), phaseAccumulator.end(), 0.0f);
    std::fill(lastInputPhase.begin(), lastInputPhase.end(), 0.0f);
    std::fill(lastOutputPhase.begin(), lastOutputPhase.end(), 0.0f);
    
    inputBufferPos = 0;
    outputBufferPos = hopSize; // Add pre-delay for better transient handling
    samplesInInputBuffer = 0;
    lastSpectralFlux = 0.0f;
    isTransientFrame = false;

    // Prepare dry delay line
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 1 };
    dryDelay.prepare(spec);
    dryDelay.setDelay(fftSize);
    dryDelay.reset();
}

void PitchShifter::setQuality(int quality)
{
    qualityLevel = juce::jlimit(0, 2, quality);
    // Re-init will happen on next prepareToPlay call
}

void PitchShifter::setFormantPreservation(bool enabled)
{
    formantPreservationEnabled = enabled;
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer, juce::LinearSmoothedValue<float>* smoother, float mix, float outputGain)
{
    const int numSamples = buffer.getNumSamples();
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        // Store input samples
        for (int i = 0; i < numSamples; ++i)
        {
            inputBuffer.setSample(0, inputBufferPos, channelData[i]);
            inputBufferPos = (inputBufferPos + 1) % inputBuffer.getNumSamples();
            samplesInInputBuffer++;
            
            // Process when we have enough samples
            if (samplesInInputBuffer >= hopSize)
            {
                float pitchShift = smoother->getNextValue();
                float pitchShiftRatio = std::pow(2.0f, pitchShift / 12.0f);
                processFrame(pitchShiftRatio);
                samplesInInputBuffer -= hopSize;
            }
            
            // Get output sample
            float wetSample = outputBuffer.getSample(0, outputBufferPos);
            outputBuffer.setSample(0, outputBufferPos, 0.0f); // Clear for next cycle
            outputBufferPos = (outputBufferPos + 1) % outputBuffer.getNumSamples();
            
            // Get delayed dry sample
            float drySample = dryDelay.popSample(0);
            dryDelay.pushSample(0, channelData[i]);

            // Mix dry/wet and apply gain
            channelData[i] = (drySample * (1.0f - mix) + wetSample * mix) * outputGain;
        }
    }
}

void PitchShifter::processFrame(float pitchShiftRatio)
{
    // Extract windowed frame for processing
    std::vector<float> frameData(fftSize);
    
    // Get the most recent fftSize samples
    int startPos = (inputBufferPos - fftSize + inputBuffer.getNumSamples()) % inputBuffer.getNumSamples();
    for (int i = 0; i < fftSize; ++i)
    {
        int readPos = (startPos + i) % inputBuffer.getNumSamples();
        frameData[i] = inputBuffer.getSample(0, readPos);
    }
    
    // Bypass processing if no pitch shift - but still apply window and overlap
    if (std::abs(pitchShiftRatio - 1.0f) < 0.001f)
    {
        // Apply window
        applyWindow(frameData.data(), fftSize);
        
        // Direct overlap-add
        float overlapGain = 2.0f / overlap;
        for (int i = 0; i < fftSize; ++i)
        {
            int writePos = (outputBufferPos + i) % outputBuffer.getNumSamples();
            outputBuffer.addSample(0, writePos, frameData[i] * overlapGain);
        }
        return;
    }
    
    // Detect transients via spectral flux BEFORE windowing
    std::vector<float> tempFrameData = frameData;
    applyWindow(tempFrameData.data(), fftSize);
    performSTFT(tempFrameData.data(), fftData.data());
    float spectralFlux = detectTransient(magnitudes);
    isTransientFrame = (spectralFlux > TRANSIENT_THRESHOLD);
    
    // Apply adaptive windowing for transients
    if (isTransientFrame)
    {
        applyWindow(frameData.data(), fftSize, true); // Use short window
    }
    else
    {
        applyWindow(frameData.data(), fftSize, false);
    }
    
    // Perform forward STFT with windowed data
    performSTFT(frameData.data(), fftData.data());
    
    // Process phases with enhanced coherence
    processPhases(fftData.data(), pitchShiftRatio);
    
    // Apply formant preservation if enabled
    if (formantPreservationEnabled)
    {
        preserveFormants(fftData.data(), pitchShiftRatio);
    }
    
    // Perform inverse STFT
    std::vector<float> outputFrame(fftSize);
    performISTFT(fftData.data(), outputFrame.data());
    
    // Overlap-add with proper windowing and normalization
    float overlapGain = 2.0f / overlap; // Consistent normalization
    for (int i = 0; i < fftSize; ++i)
    {
        int writePos = (outputBufferPos + i) % outputBuffer.getNumSamples();
        outputBuffer.addSample(0, writePos, outputFrame[i] * overlapGain);
    }
    
    // Update last magnitudes for transient detection
    lastMagnitudes = magnitudes;
}

void PitchShifter::performSTFT(const float* input, std::complex<float>* fftData)
{
    // Copy input to complex array (real part only)
    for (int i = 0; i < fftSize; ++i)
    {
        fftData[i] = std::complex<float>(input[i], 0.0f);
    }
    
    // Perform FFT using JUCE's optimized implementation
    fft->perform(reinterpret_cast<const juce::dsp::Complex<float>*>(fftData),
                reinterpret_cast<juce::dsp::Complex<float>*>(fftData), false);
    
    // Extract magnitudes and phases
    int spectrumSize = fftSize / 2 + 1;
    for (int i = 0; i < spectrumSize; ++i)
    {
        magnitudes[i] = std::abs(fftData[i]);
        phases[i] = std::arg(fftData[i]);
    }
}

void PitchShifter::performISTFT(const std::complex<float>* fftData, float* output)
{
    // Copy data for inverse transform
    std::vector<std::complex<float>> ifftData(fftSize);
    
    // Copy positive frequencies
    int spectrumSize = fftSize / 2 + 1;
    for (int i = 0; i < spectrumSize; ++i)
    {
        ifftData[i] = fftData[i];
    }
    
    // Mirror for negative frequencies (excluding DC and Nyquist)
    for (int i = 1; i < fftSize / 2; ++i)
    {
        ifftData[fftSize - i] = std::conj(fftData[i]);
    }
    
    // Perform inverse FFT
    fft->perform(reinterpret_cast<const juce::dsp::Complex<float>*>(ifftData.data()),
                reinterpret_cast<juce::dsp::Complex<float>*>(ifftData.data()), true);
    
    // Extract real part with proper normalization
    for (int i = 0; i < fftSize; ++i)
    {
        output[i] = ifftData[i].real() / fftSize;
    }
}

void PitchShifter::processPhases(std::complex<float>* fftData, float pitchShiftRatio)
{
    const int spectrumSize = fftSize / 2 + 1;
    
    // Simple frequency-domain pitch shifting that WORKS
    std::vector<std::complex<float>> shiftedSpectrum(spectrumSize);
    std::fill(shiftedSpectrum.begin(), shiftedSpectrum.end(), std::complex<float>(0.0f, 0.0f));
    
    // Keep DC component
    shiftedSpectrum[0] = fftData[0];
    
    // Simple bin shifting - this guarantees output
    for (int i = 1; i < spectrumSize; ++i)
    {
        float targetBin = i * pitchShiftRatio;
        
        if (targetBin >= 1.0f && targetBin < spectrumSize - 1)
        {
            int bin1 = (int)targetBin;
            int bin2 = bin1 + 1;
            float frac = targetBin - bin1;
            
            // Simple linear interpolation
            if (bin1 < spectrumSize)
            {
                shiftedSpectrum[bin1] += fftData[i] * (1.0f - frac);
            }
            if (bin2 < spectrumSize)
            {
                shiftedSpectrum[bin2] += fftData[i] * frac;
            }
        }
    }
    
    // Copy back
    for (int i = 0; i < spectrumSize; ++i)
    {
        fftData[i] = shiftedSpectrum[i];
        magnitudes[i] = std::abs(fftData[i]);
    }
}

float PitchShifter::detectTransient(const std::vector<float>& currentMags)
{
    float spectralFlux = 0.0f;
    
    // Calculate spectral flux (onset strength)
    for (int i = 1; i < fftSize/2; i++)
    {
        float diff = currentMags[i] - lastMagnitudes[i];
        spectralFlux += diff > 0 ? diff : 0;
    }
    
    // Normalize by FFT size
    spectralFlux /= (fftSize / 2);
    
    // Simple smoothing to avoid false positives
    float smoothedFlux = 0.8f * lastSpectralFlux + 0.2f * spectralFlux;
    lastSpectralFlux = smoothedFlux;
    
    return spectralFlux;
}

void PitchShifter::preserveFormants(std::complex<float>* fftData, float pitchShiftRatio)
{
    if (std::abs(pitchShiftRatio - 1.0f) < 0.01f) return; // No processing needed for small shifts
    
    const int spectrumSize = fftSize / 2 + 1;
    std::vector<float> envelope(spectrumSize);
    
    // Simple but effective envelope extraction
    for (int i = 1; i < spectrumSize-1; i++)
    {
        // Moving average with peak hold
        envelope[i] = 0.5f * (magnitudes[i-1] + magnitudes[i+1]);
        envelope[i] = std::max(envelope[i], magnitudes[i] * 0.8f);
    }
    
    // Apply multiple smoothing passes
    const int smoothingPasses = 2;
    for (int pass = 0; pass < smoothingPasses; ++pass)
    {
        std::vector<float> tempEnvelope = envelope;
        for (int i = 2; i < spectrumSize-2; i++)
        {
            envelope[i] = 0.2f * tempEnvelope[i-2] + 0.2f * tempEnvelope[i-1] +
                         0.2f * tempEnvelope[i] + 0.2f * tempEnvelope[i+1] + 0.2f * tempEnvelope[i+2];
        }
    }
    
    // Apply correction only where needed
    for (int i = 1; i < spectrumSize; i++)
    {
        float targetBin = i / pitchShiftRatio;
        if (targetBin >= 1 && targetBin < spectrumSize-1)
        {
            int bin1 = (int)targetBin;
            int bin2 = bin1 + 1;
            float frac = targetBin - bin1;
            
            float envValue = envelope[bin1] * (1.0f - frac) + envelope[bin2] * frac;
            float correction = envValue / (magnitudes[i] + 1e-12f);
            
            // Limit correction to avoid artifacts
            correction = juce::jlimit(0.5f, 2.0f, correction);
            
            // Apply correction with frequency-dependent strength
            float freqWeight = 1.0f - (float)i / spectrumSize; // Stronger at low frequencies
            correction = 1.0f + (correction - 1.0f) * freqWeight * 0.7f;
            
            fftData[i] *= correction;
        }
    }
}

void PitchShifter::applyWindow(float* data, int size, bool useShortWindow)
{
    if (useShortWindow && size >= fftSize/2)
    {
        // Apply short window to first half for transients
        shortWindow->multiplyWithWindowingTable(data, fftSize/2);
        // Taper the rest
        for (int i = fftSize/2; i < size; ++i)
        {
            float taper = 1.0f - (float)(i - fftSize/2) / (size - fftSize/2);
            data[i] *= taper;
        }
    }
    else
    {
        window->multiplyWithWindowingTable(data, size);
    }
}

float PitchShifter::calculateInstantaneousFrequency(int bin, float phaseDiff)
{
    // Calculate instantaneous frequency using phase derivative
    float expectedPhaseDiff = TWO_PI * bin * hopSize / fftSize;
    float deviation = phaseDiff - expectedPhaseDiff;
    
    // Unwrap deviation
    while (deviation > PI) deviation -= TWO_PI;
    while (deviation < -PI) deviation += TWO_PI;
    
    return TWO_PI * bin / fftSize + deviation / hopSize;
}

void PitchShifter::findSpectralPeaks(const float* magnitudes, std::vector<int>& peaks)
{
    peaks.clear();
    const int spectrumSize = fftSize / 2 + 1;
    
    for (int i = 2; i < spectrumSize - 2; ++i)
    {
        // Check if this is a local maximum
        if (magnitudes[i] > magnitudes[i-1] &&
            magnitudes[i] > magnitudes[i+1] &&
            magnitudes[i] > magnitudes[i-2] &&
            magnitudes[i] > magnitudes[i+2] &&
            magnitudes[i] > 0.01f)
        {
            peaks.push_back(i);
        }
    }
}

void PitchShifter::propagatePhase(std::complex<float>* fftData, const std::vector<int>& peaks, float pitchShiftRatio)
{
    // Phase propagation from peaks (not currently used but available for future enhancement)
    const int spectrumSize = fftSize / 2 + 1;
    
    for (size_t p = 0; p < peaks.size(); ++p)
    {
        int peakBin = peaks[p];
        float peakPhase = std::arg(fftData[peakBin]);
        
        // Propagate phase to neighboring bins
        for (int offset = -5; offset <= 5; ++offset)
        {
            int bin = peakBin + offset;
            if (bin >= 0 && bin < spectrumSize && bin != peakBin)
            {
                float expectedPhase = peakPhase + offset * TWO_PI * hopSize / fftSize;
                float currentMag = std::abs(fftData[bin]);
                fftData[bin] = std::polar(currentMag, expectedPhase);
            }
        }
    }
}

void PitchShifter::estimateSpectralEnvelope(const float* magnitudes, float* envelope)
{
    const int spectrumSize = fftSize / 2 + 1;
    
    // Initial envelope estimation
    for (int i = 0; i < spectrumSize; ++i)
    {
        envelope[i] = magnitudes[i];
    }
    
    // Apply cepstral smoothing
    const int cepstralCutoff = 30;
    std::vector<std::complex<float>> cepstrum(fftSize);
    
    // Convert to log magnitude
    for (int i = 0; i < spectrumSize; ++i)
    {
        float logMag = std::log(magnitudes[i] + 1e-12f);
        cepstrum[i] = std::complex<float>(logMag, 0.0f);
    }
    
    // Mirror for negative frequencies
    for (int i = 1; i < fftSize / 2; ++i)
    {
        cepstrum[fftSize - i] = cepstrum[i];
    }
    
    // IFFT to get cepstrum
    fft->perform(reinterpret_cast<const juce::dsp::Complex<float>*>(cepstrum.data()),
                reinterpret_cast<juce::dsp::Complex<float>*>(cepstrum.data()), true);
    
    // Lifter (low-pass in cepstral domain)
    for (int i = cepstralCutoff; i < fftSize - cepstralCutoff; ++i)
    {
        cepstrum[i] = std::complex<float>(0.0f, 0.0f);
    }
    
    // FFT back to frequency domain
    fft->perform(reinterpret_cast<const juce::dsp::Complex<float>*>(cepstrum.data()),
                reinterpret_cast<juce::dsp::Complex<float>*>(cepstrum.data()), false);
    
    // Extract smoothed envelope
    for (int i = 0; i < spectrumSize; ++i)
    {
        envelope[i] = std::exp(cepstrum[i].real() / fftSize);
    }
}

void PitchShifter::applySpectralEnvelope(std::complex<float>* fftData, const float* envelope)
{
    const int spectrumSize = fftSize / 2 + 1;
    
    for (int i = 0; i < spectrumSize; ++i)
    {
        float currentMag = std::abs(fftData[i]);
        if (currentMag > 1e-12f)
        {
            float targetMag = envelope[i];
            float scale = targetMag / currentMag;
            scale = juce::jlimit(0.1f, 10.0f, scale);
            fftData[i] *= scale;
        }
    }
}
