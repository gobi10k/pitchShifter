/*
  ==============================================================================

    LFODisplay.h
    Created: 20 Aug 2025 7:30:00am
    Author:  Jules

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class LFODisplay : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black);
        g.fillRect(bounds);

        g.setColour(juce::Colours::orange);
        juce::Path p;
        p.startNewSubPath(bounds.getX(), bounds.getCentreY());

        for (int x = 1; x < bounds.getWidth(); ++x)
        {
            float phase = (float)x / bounds.getWidth();
            float y = 0.0f;

            switch (waveform)
            {
                case 0: // Sine
                    y = -std::sin(phase * juce::MathConstants<float>::twoPi);
                    break;
                case 1: // Triangle
                    y = 2.0f * (2.0f * std::abs(phase - 0.5f) - 0.5f);
                    break;
                case 2: // S&H
                    y = snh_values[x % snh_values.size()];
                    break;
            }
            p.lineTo(bounds.getX() + x, bounds.getCentreY() + y * bounds.getHeight() * 0.4f);
        }
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }

    void setWaveform(int newWaveform)
    {
        if (waveform != newWaveform)
        {
            waveform = newWaveform;
            if (waveform == 2) // S&H
            {
                juce::Random r;
                for (auto& val : snh_values)
                {
                    val = r.nextFloat() * 2.0f - 1.0f;
                }
            }
            repaint();
        }
    }

private:
    int waveform = 0;
    std::array<float, 16> snh_values;
};
