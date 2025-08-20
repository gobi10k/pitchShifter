/*
  ==============================================================================

    VUMeter.h
    Created: 20 Aug 2025 7:28:00am
    Author:  Jules

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class VUMeter : public juce::Component, public juce::Timer
{
public:
    VUMeter()
    {
        startTimerHz(24);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colours::black);
        g.fillRect(bounds);

        g.setColour(juce::Colours::green);
        const auto scaledY = juce::jmap(level, -60.0f, 6.0f, bounds.getHeight(), 0.0f);
        g.fillRect(bounds.withY(scaledY).withHeight(bounds.getHeight() - scaledY));
    }

    void setLevel(float newLevel)
    {
        level = newLevel;
    }

    void timerCallback() override
    {
        level = std::max(-60.0f, level - 0.5f); // Decay
        repaint();
    }

private:
    float level = -60.0f; // in dB
};
