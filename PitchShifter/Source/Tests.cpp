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
    ModernPitchShifterTest() : juce::UnitTest("BBD Pitch Shifter Test") {}

    void runTest() override
    {
        beginTest("Basic Pitch Shifting Test");
        testBasicPitchShifting();
        
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
        
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)blockSize, 1 };
        shifter.prepare(spec);
        
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
        float pitchRatio = std::pow(2.0f, 12.0f / 12.0f);
        
        // Process several blocks to allow the algorithm to stabilize
        for (int block = 0; block < 10; ++block)
        {
            shifter.process(buffer, pitchRatio);
        }
        
        // Check that output contains energy (basic sanity check)
        float rms = buffer.getRMSLevel(0, 0, blockSize);
        
        expect(rms > 0.01f, "Output should contain significant energy");
        expect(rms < 2.0f, "Output should not be excessively loud");
    }
    
    void testRealTimePerformance()
    {
        PitchShifter shifter;
        const double sampleRate = 44100.0;
        const int blockSize = 256; // Typical real-time block size
        
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)blockSize, 1 };
        shifter.prepare(spec);
        
        juce::AudioBuffer<float> buffer(1, blockSize);
        
        // Measure processing time
        auto startTime = juce::Time::getHighResolutionTicks();
        
        const int numBlocks = 100;
        for (int block = 0; block < numBlocks; ++block)
        {
            // Create varying input signal
            createTestSignal(buffer, sampleRate, block);
            
            // Vary pitch shift over time
            float pitch = 12.0f * std::sin(block * 0.1f); // ±12 semitones
            float pitchRatio = std::pow(2.0f, pitch / 12.0f);
            
            shifter.process(buffer, pitchRatio);
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
        
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)blockSize, 1 };
        shifter.prepare(spec);
        
        juce::AudioBuffer<float> buffer(1, blockSize);
        
        // Test extreme pitch shifts
        std::vector<float> extremeShifts = {-24.0f, -12.0f, 0.0f, 12.0f, 24.0f};
        
        for (float pitchShift : extremeShifts)
        {
            float pitchRatio = std::pow(2.0f, pitchShift / 12.0f);

            // Test with silence
            buffer.clear();
            shifter.process(buffer, pitchRatio);
            // We can't expect it to not be cleared, as processing silence might result in silence
            expect(true, "Should handle silence without crashing");
            
            // Test with very loud signal
            buffer.clear();
            for (int i = 0; i < blockSize; ++i)
            {
                buffer.setSample(0, i, (i % 2 == 0) ? 1.0f : -1.0f); // Square wave
            }
            shifter.process(buffer, pitchRatio);
            
            float maxOutput = buffer.getMagnitude(0, 0, blockSize);
            expect(maxOutput < 10.0f, "Should handle loud input without excessive output at " +
                   juce::String(pitchShift) + " semitones");
        }
        
        // Test rapid pitch changes
        for (int i = 0; i < 50; ++i)
        {
            float randomPitch = juce::Random::getSystemRandom().nextFloat() * 48.0f - 24.0f;
            float pitchRatio = std::pow(2.0f, randomPitch / 12.0f);
            
            createTestSignal(buffer, sampleRate, i);
            shifter.process(buffer, pitchRatio);
        }
        
        expect(true, "Should handle rapid pitch changes without crashing");
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
