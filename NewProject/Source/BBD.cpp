/*
  ==============================================================================

    BBD.cpp
    Created: 20 Aug 2025 7:10:00am
    Author:  Jules

  ==============================================================================
*/

#include "BBD.h"

BBD::BBD()
{
}

void BBD::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    delayLine.setSize(spec.numChannels, BBD_SIZE);
    delayLine.clear();
    writeIndex = 0;
    readIndex = 0.0f;
}

float BBD::processSample(float sample, int channel)
{
    // 1. Write the input sample to the delay line
    delayLine.setSample(channel, writeIndex, sample);

    // 2. Calculate the delayed sample using linear interpolation
    int readIndexInt = static_cast<int>(readIndex);
    int readIndexNext = (readIndexInt + 1) % BBD_SIZE;
    float fraction = readIndex - readIndexInt;

    float sample1 = delayLine.getSample(channel, readIndexInt);
    float sample2 = delayLine.getSample(channel, readIndexNext);

    float outputSample = sample1 + fraction * (sample2 - sample1);

    // 3. Increment write index
    writeIndex = (writeIndex + 1) % BBD_SIZE;

    // 4. Increment read index based on clock rate
    readIndex += clockRate;
    if (readIndex >= BBD_SIZE)
        readIndex -= BBD_SIZE;

    return outputSample;
}

void BBD::setClockRate(float newClockRate)
{
    // Add constraints if necessary, e.g. positive clock rate
    clockRate = newClockRate;
}

void BBD::setReadIndex(float newReadIndex)
{
    readIndex = newReadIndex;
    if (readIndex >= BBD_SIZE)
        readIndex -= BBD_SIZE;
    else if (readIndex < 0)
        readIndex += BBD_SIZE;
}

int BBD::getWriteIndex() const
{
    return writeIndex;
}

float BBD::getReadIndex() const
{
    return readIndex;
}
