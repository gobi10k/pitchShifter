// This block contains the entire file content, but I'm only showing the changed function
// for brevity in my thought process. The tool will overwrite the whole file.

// ... (rest of the file is as it was) ...

void PitchShifterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get current parameter values from APVTS
    float pitch = *apvts.getRawParameterValue("PITCH");
    float glide = *apvts.getRawParameterValue("GLIDE");
    float mix = *apvts.getRawParameterValue("MIX");
    float outputGainDB = *apvts.getRawParameterValue("OUTPUT_GAIN");
    float outputGain = juce::Decibels::decibelsToGain(outputGainDB);

    // The new BBD algorithm doesn't have quality/formant params from the old FFT-based one.
    // We only need to handle glide time updates for the wet path smoother.
    if (glide != lastGlide)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float rampLength = glide > 0.0f ? glide / 1000.0f : 0.001f;
            smoothedWetPitch[channel]->reset(getSampleRate(), rampLength);
        }
        lastGlide = glide;
    }

    // Set the target pitch for the wet path's smoother
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        smoothedWetPitch[channel]->setTargetValue(pitch);
    }

    // Create copies of the input buffer for parallel dry and wet processing
    juce::AudioBuffer<float> dryBuffer(buffer);
    juce::AudioBuffer<float> wetBuffer(buffer);

    // Process each channel in parallel
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // 1. Process the Dry Path (0 pitch shift)
        auto* dryChannelData = dryBuffer.getWritePointer(channel);
        juce::AudioBuffer<float> dryChannelBuffer(&dryChannelData, 1, buffer.getNumSamples());
        dryShifters[channel]->process(dryChannelBuffer, 1.0f); // pitchRatio = 1.0 means no shift

        // 2. Process the Wet Path (UI-controlled pitch shift)
        auto* wetChannelData = wetBuffer.getWritePointer(channel);
        juce::AudioBuffer<float> wetChannelBuffer(&wetChannelData, 1, buffer.getNumSamples());
        float wetPitch = smoothedWetPitch[channel]->getNextValue();
        float wetPitchRatio = std::pow(2.0f, wetPitch / 12.0f);
        wetShifters[channel]->process(wetChannelBuffer, wetPitchRatio);
    }

    // 3. Mix the parallel signals back into the main output buffer
    const float wetGainBoost = 1.41f; // ~3dB boost for the wet signal
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* outputData = buffer.getWritePointer(channel);
        const auto* dryData = dryBuffer.getReadPointer(channel);
        const auto* wetData = wetBuffer.getReadPointer(channel);

        // Use an equal-power crossfade for a perceptually smooth blend
        const float dryMix = std::cos(mix * juce::MathConstants<float>::pi * 0.5f);
        const float wetMix = std::sin(mix * juce::MathConstants<float>::pi * 0.5f);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float drySample = dryData[sample];
            const float wetSample = wetData[sample] * wetGainBoost;
            outputData[sample] = (drySample * dryMix + wetSample * wetMix) * outputGain;
        }
    }
}
// ... (rest of the file is as it was) ...
