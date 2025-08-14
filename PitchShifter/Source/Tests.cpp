/*
  ==============================================================================

    Tests.cpp
    Created: 14 Aug 2025
    Author:  Modern Implementation Tests

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PitchShifter.h"

class ModernPitchShifterTest : public juce::UnitTest
{
public:
    ModernPitchShifterTest() : juce::UnitTest("Modern Pitch Shifter Test") {}

    void runTest() override
    {
        beginTest("Basic Pitch Shifting Test");
        testBasicPitchShifting();
        
        beginTest("Quality Levels Test");
        testQualityLevels();
        
        beginTest("Formant Preservation Test");
        testFormantPreservation();
        
        beginTest("Real-time Performance Test");
        testRealTimePerformance();
        
        beginTest("Edge Cases Test");
        testEdgeCases();
    }

private:
    void testBasicPitchShifting()
    {
        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        
        shifter.prepareToPlay(sampleRate, blockSize);
        
        // Create a sine wave at 440 Hz
        juce::AudioBuffer<float> buffer(1, blockSize);
        float phase = 0.0f;
        float increment = 2.0f * juce::MathConstants<float>::pi * 440.0f / (float)sampleRate;
        
        for (int i = 0; i < blockSize; ++i)
        {
            buffer.setSample(0, i, std::sin(phase));
            phase += increment;
        }
        
        // Test +12 semitone shift (up one octave)
        juce::LinearSmoothedValue<float> smoother;
        smoother.reset(sampleRate, 0.0);
        smoother.setTargetValue(12.0f);
        
        // Process several blocks to allow the algorithm to stabilize
        for (int block = 0; block < 10; ++block)
        {
            shifter.process(buffer, &smoother, 1.0f, 1.0f); // Add gain parameters
        }
        
        // Check that output contains energy (basic sanity check)
        float rms = 0.0f;
        for (int i = 0; i < blockSize; ++i)
        {
            float sample = buffer.getSample(0, i);
            rms += sample * sample;
        }
        rms = std::sqrt(rms / blockSize);
        
        expect(rms > 0.01f, "Output should contain significant energy");
        expect(rms < 2.0f, "Output should not be excessively loud");
    }
    
    void testQualityLevels()
    {
        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        
        // Test all quality levels
        for (int quality = 0; quality <= 2; ++quality)
        {
            shifter.setQuality(quality);
            shifter.prepareToPlay(sampleRate, blockSize);
            
            juce::AudioBuffer<float> buffer(1, blockSize);
            
            // Create a more complex signal (sawtooth wave)
            for (int i = 0; i < blockSize; ++i)
            {
                float t = (float)i / sampleRate;
                float fundamental = 220.0f; // A3
                float phase = std::fmod(t * fundamental, 1.0f);
                buffer.setSample(0, i, 2.0f * phase - 1.0f); // Sawtooth
            }
            
            juce::LinearSmoothedValue<float> smoother;
            smoother.reset(sampleRate, 0.0);
            smoother.setTargetValue(7.0f); // +7 semitones
            
            // Process multiple blocks
            for (int block = 0; block < 5; ++block)
            {
                shifter.process(buffer, &smoother, 1.0f, 1.0f); // Add gain parameters
            }
            
            // Check output is reasonable
            float maxSample = buffer.getMagnitude(0, 0, blockSize);
            expect(maxSample < 5.0f, "Quality level " + juce::String(quality) + " should not produce excessive output");
        }
    }
    
    void testFormantPreservation()
    {
        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        
        shifter.prepareToPlay(sampleRate, blockSize);
        
        // Test with formant preservation enabled
        shifter.setFormantPreservation(true);
        
        juce::AudioBuffer<float> bufferWithFormants(1, blockSize);
        createVocalLikeSignal(bufferWithFormants, sampleRate);
        
        juce::LinearSmoothedValue<float> smoother;
        smoother.reset(sampleRate, 0.0);
        smoother.setTargetValue(-5.0f); // Down 5 semitones
        
        for (int block = 0; block < 8; ++block)
        {
            shifter.process(bufferWithFormants, &smoother, 1.0f, 1.0f); // Add gain parameters
        }
        
        // Test without formant preservation
        shifter.setFormantPreservation(false);
        shifter.prepareToPlay(sampleRate, blockSize); // Reset state
        
        juce::AudioBuffer<float> bufferWithoutFormants(1, blockSize);
        createVocalLikeSignal(bufferWithoutFormants, sampleRate);
        
        smoother.reset(sampleRate, 0.0);
        smoother.setTargetValue(-5.0f);
        
        for (int block = 0; block < 8; ++block)
        {
            shifter.process(bufferWithoutFormants, &smoother, 1.0f, 1.0f); // Add gain parameters
        }
        
        // Both should produce valid output
        float rmsWithFormants = bufferWithFormants.getRMSLevel(0, 0, blockSize);
        float rmsWithoutFormants = bufferWithoutFormants.getRMSLevel(0, 0, blockSize);
        
        expect(rmsWithFormants > 0.001f, "Formant preservation should produce output");
        expect(rmsWithoutFormants > 0.001f, "Non-formant processing should produce output");
    }
    
    void testRealTimePerformance()
    {
        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 256; // Typical real-time block size
        
        shifter.setQuality(1); // Balanced quality
        shifter.prepareToPlay(sampleRate, blockSize);
        
        juce::AudioBuffer<float> buffer(1, blockSize);
        juce::LinearSmoothedValue<float> smoother;
        smoother.reset(sampleRate, 0.1); // 100ms glide
        
        // Measure processing time
        auto startTime = juce::Time::getHighResolutionTicks();
        
        const int numBlocks = 100;
        for (int block = 0; block < numBlocks; ++block)
        {
            // Create varying input signal
            createTestSignal(buffer, sampleRate, block);
            
            // Vary pitch shift over time
            float pitch = 12.0f * std::sin(block * 0.1f); // ±12 semitones
            smoother.setTargetValue(pitch);
            
            shifter.process(buffer, &smoother, 1.0f, 1.0f); // Add gain parameters
        }
        
        auto endTime = juce::Time::getHighResolutionTicks();
        auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds(endTime - startTime);
        auto audioSeconds = (numBlocks * blockSize) / sampleRate;
        auto realTimeRatio = elapsedSeconds / audioSeconds;
        
        expect(realTimeRatio < 0.5, "Processing should be efficient enough for real-time use");
        logMessage("Real-time ratio: " + juce::String(realTimeRatio, 3));
    }
    
    void testEdgeCases()
    {
        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        
        shifter.prepareToPlay(sampleRate, blockSize);
        
        juce::AudioBuffer<float> buffer(1, blockSize);
        juce::LinearSmoothedValue<float> smoother;
        smoother.reset(sampleRate, 0.0);
        
        // Test extreme pitch shifts
        std::vector<float> extremeShifts = {-24.0f, -12.0f, 0.0f, 12.0f, 24.0f};
        
        for (float pitchShift : extremeShifts)
        {
            smoother.setTargetValue(pitchShift);
            
            // Test with silence
            buffer.clear();
            shifter.process(buffer, &smoother, 1.0f, 1.0f); // Add gain parameters
            expect(!buffer.hasBeenCleared(), "Should handle silence without crashing");
            
            // Test with very loud signal
            buffer.clear();
            for (int i = 0; i < blockSize; ++i)
            {
                buffer.setSample(0, i, (i % 2 == 0) ? 1.0f : -1.0f); // Square wave
            }
            shifter.process(buffer, &smoother, 1.0f, 1.0f); // Add gain parameters
            
            float maxOutput = buffer.getMagnitude(0, 0, blockSize);
            expect(maxOutput < 10.0f, "Should handle loud input without excessive output at " +
                   juce::String(pitchShift) + " semitones");
        }
        
        // Test rapid pitch changes
        for (int i = 0; i < 50; ++i)
        {
            float randomPitch = juce::Random::getSystemRandom().nextFloat() * 48.0f - 24.0f;
            smoother.setTargetValue(randomPitch);
            
            createTestSignal(buffer, sampleRate, i);
            shifter.process(buffer, &smoother, 1.0f, 1.0f); // Add gain parameters
        }
        
        expect(true, "Should handle rapid pitch changes without crashing");
    }
    
    void createVocalLikeSignal(juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        // Create a signal with harmonic content similar to vocals
        const float fundamental = 200.0f; // Typical male voice fundamental
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float t = i / (float)sampleRate;
            float sample = 0.0f;
            
            // Add harmonics with formant-like emphasis
            for (int harmonic = 1; harmonic <= 8; ++harmonic)
            {
                float freq = fundamental * harmonic;
                float amplitude = 1.0f / harmonic;
                
                // Simulate formant peaks around 800Hz and 1200Hz
                if (freq > 700.0f && freq < 900.0f) amplitude *= 3.0f;
                if (freq > 1100.0f && freq < 1300.0f) amplitude *= 2.5f;
                
                sample += amplitude * std::sin(2.0f * juce::MathConstants<float>::pi * freq * t);
            }
            
            buffer.setSample(0, i, sample * 0.1f); // Scale down
        }
    }
    
    void createTestSignal(juce::AudioBuffer<float>& buffer, double sampleRate, int blockIndex)
    {
        // Create a test signal with varying characteristics
        float baseFreq = 440.0f + 100.0f * std::sin(blockIndex * 0.05f);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float t = (blockIndex * buffer.getNumSamples() + i) / (float)sampleRate;
            float sample = std::sin(2.0f * juce::MathConstants<float>::pi * baseFreq * t);
            
            // Add some harmonic content
            sample += 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * baseFreq * 2.0f * t);
            sample += 0.1f * std::sin(2.0f * juce::MathConstants<float>::pi * baseFreq * 3.0f * t);
            
            buffer.setSample(0, i, sample * 0.3f);
        }
    }
};

static ModernPitchShifterTest modernPitchShifterTest;
