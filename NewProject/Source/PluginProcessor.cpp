/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PitchShiftAudioProcessor::PitchShiftAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

PitchShiftAudioProcessor::~PitchShiftAudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PitchShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("PITCH", "Pitch", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("FEEDBACK", "Feedback", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE_CUTOFF", "Tone Cutoff", 20.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE_RESONANCE", "Tone Resonance", 0.1f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("TONE_MODE", "Tone Mode", juce::StringArray { "LPF", "BPF", "HPF" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("NOISE", "Noise", 0.0f, 1.0f, 0.0f));

    // LFO Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LFO_RATE", "LFO Rate", 0.01f, 20.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LFO_DEPTH", "LFO Depth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("LFO_WAVEFORM", "LFO Waveform", juce::StringArray { "Sine", "Triangle", "Sample & Hold" }, 0));

    // Mix and Gain
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("INPUT_GAIN", "Input Gain", -20.0f, 20.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("OUTPUT_GAIN", "Output Gain", -20.0f, 20.0f, 0.0f));


    return { params.begin(), params.end() };
}

void PitchShiftAudioProcessor::loadPreset(int presetIndex)
{
    // A helper lambda to set a parameter's value
    auto setParam = [&](const juce::String& paramID, float value)
    {
        apvts.getParameter(paramID)->setValueNotifyingHost(apvts.getParameter(paramID)->getNormalisableRange().convertTo0to1(value));
    };

    switch (presetIndex)
    {
        case Default:
            // Default state is already defined in createParameterLayout
            break;
        case AnalogDoubler:
            setParam("PITCH", 0.1f); // slight detune
            setParam("MIX", 0.5f);
            setParam("LFO_DEPTH", 0.0f);
            setParam("FEEDBACK", 0.0f);
            break;
        case ChorusEnsemble:
            setParam("PITCH", 0.0f);
            setParam("LFO_RATE", 0.8f);
            setParam("LFO_DEPTH", 0.25f);
            setParam("MIX", 0.6f);
            break;
        case OctaveUp:
            setParam("PITCH", 12.0f);
            setParam("MIX", 0.5f);
            setParam("TONE_CUTOFF", 8000.0f);
            break;
        case Vibrato:
            setParam("PITCH", 0.0f);
            setParam("LFO_RATE", 5.0f);
            setParam("LFO_DEPTH", 0.15f);
            setParam("MIX", 1.0f);
            break;
        case ResonantSwell:
            setParam("PITCH", 7.0f); // A fifth
            setParam("FEEDBACK", 0.8f);
            setParam("TONE_MODE", 1); // BPF
            setParam("TONE_CUTOFF", 1000.0f);
            setParam("TONE_RESONANCE", 0.8f);
            setParam("MIX", 0.7f);
            break;
        case LoFiWarble:
            setParam("PITCH", 0.0f);
            setParam("LFO_RATE", 0.2f);
            setParam("LFO_DEPTH", 0.4f);
            setParam("LFO_WAVEFORM", 2); // S&H
            setParam("NOISE", 0.1f);
            setParam("TONE_CUTOFF", 4000.0f);
            break;
        default:
            break;
    }
}


juce::AudioProcessorValueTreeState& PitchShiftAudioProcessor::getAPVTS()
{
    return apvts;
}

float PitchShiftAudioProcessor::getRMSLevel(int channel) const
{
    jassert(channel == 0 || channel == 1);
    return rmsLevel[channel];
}

const juce::String PitchShiftAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PitchShiftAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PitchShiftAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PitchShiftAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PitchShiftAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PitchShiftAudioProcessor::getNumPrograms()
{
    return Presets::NumPresets;
}

int PitchShiftAudioProcessor::getCurrentProgram()
{
    // This could be improved to return the index of the current preset
    return 0;
}

void PitchShiftAudioProcessor::setCurrentProgram (int index)
{
    loadPreset(index);
}

const juce::String PitchShiftAudioProcessor::getProgramName (int index)
{
    switch (index)
    {
        case Default: return "Default";
        case AnalogDoubler: return "Analog Doubler";
        case ChorusEnsemble: return "Chorus Ensemble";
        case OctaveUp: return "Octave Up";
        case Vibrato: return "Vibrato";
        case ResonantSwell: return "Resonant Swell";
        case LoFiWarble: return "Lo-Fi Warble";
        default: return "Unknown";
    }
}

void PitchShiftAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    // Not implemented
}

//==============================================================================
void PitchShiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    pitchShifter[0].prepare(spec);
    pitchShifter[1].prepare(spec);

    juce::dsp::ProcessSpec filterSpec;
    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = samplesPerBlock;
    filterSpec.numChannels = 1;

    for (int i = 0; i < 2; ++i)
    {
        feedbackFilter[i].prepare(filterSpec);
        *feedbackFilter[i].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 5000.0f);
        lastFeedbackOutput[i] = 0.0f;

        postFilter[i].prepare(filterSpec);
        noiseFilter[i].prepare(filterSpec);
    }

    lfoPhase = 0.0f;
}

void PitchShiftAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PitchShiftAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PitchShiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get Gain and Mix parameters
    auto inputGainDb = apvts.getRawParameterValue("INPUT_GAIN")->load();
    auto outputGainDb = apvts.getRawParameterValue("OUTPUT_GAIN")->load();
    auto mix = apvts.getRawParameterValue("MIX")->load();

    float inputGain = juce::Decibels::decibelsToGain(inputGainDb);
    float outputGain = juce::Decibels::decibelsToGain(outputGainDb);

    // Get other parameters
    auto lfoRate = apvts.getRawParameterValue("LFO_RATE")->load();
    auto lfoDepth = apvts.getRawParameterValue("LFO_DEPTH")->load();
    auto lfoWaveform = static_cast<int>(apvts.getRawParameterValue("LFO_WAVEFORM")->load());
    auto pitch = apvts.getRawParameterValue("PITCH")->load();
    auto feedback = apvts.getRawParameterValue("FEEDBACK")->load();
    auto toneCutoff = apvts.getRawParameterValue("TONE_CUTOFF")->load();
    auto toneResonance = apvts.getRawParameterValue("TONE_RESONANCE")->load();
    auto toneMode = apvts.getRawParameterValue("TONE_MODE")->load();
    auto noiseLevel = apvts.getRawParameterValue("NOISE")->load();

    double sampleRate = getSampleRate();
    float lfoPhaseInc = lfoRate / sampleRate;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // LFO Generation
            lfoPhase += lfoPhaseInc;
            if (lfoPhase >= 1.0f)
            {
                lfoPhase -= 1.0f;
                // New random value for S&H on phase wrap
                lfo_sh_value = random.nextFloat() * 2.0f - 1.0f;
            }

            float lfoSample = 0.0f;
            switch (lfoWaveform)
            {
                case 0: // Sine
                    lfoSample = std::sin(lfoPhase * juce::MathConstants<float>::twoPi);
                    break;
                case 1: // Triangle
                    lfoSample = 4.0f * std::abs(lfoPhase - 0.5f) - 1.0f;
                    break;
                case 2: // Sample & Hold
                    lfoSample = lfo_sh_value;
                    break;
            }

            // Processing
            float modulatedPitch = pitch + lfoSample * lfoDepth * 24.0f;
            pitchShifter[channel].setPitch(modulatedPitch);

            float pitchRatio = std::pow(2.0f, modulatedPitch / 12.0f);

            if (toneMode == 0) *postFilter[channel].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, toneCutoff, toneResonance);
            else if (toneMode == 1) *postFilter[channel].coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, toneCutoff, toneResonance);
            else *postFilter[channel].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, toneCutoff, toneResonance);

            auto noiseCutoff = std::min((sampleRate * pitchRatio) / 2.0, sampleRate / 2.0 * 0.99);
            *noiseFilter[channel].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, noiseCutoff);

            float drySample = channelData[sample] * inputGain;

            float feedbackSample = feedbackFilter[channel].processSample(lastFeedbackOutput[channel]);
            float inputToShifter = drySample + feedbackSample * feedback;

            float wetSample = pitchShifter[channel].processSample(inputToShifter, channel);
            wetSample = postFilter[channel].processSample(wetSample);

            float noise = (random.nextFloat() * 2.0f - 1.0f);
            float filteredNoise = noiseFilter[channel].processSample(noise);
            wetSample += filteredNoise * noiseLevel * 0.1f;

            lastFeedbackOutput[channel] = wetSample;

            float mixedSample = (drySample * (1.0f - mix)) + (wetSample * mix);

            channelData[sample] = mixedSample * outputGain;
        }
    }

    // Calculate RMS for VU meter
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        rmsLevel[channel] = juce::Decibels::gainToDecibels(buffer.getRMSLevel(channel, 0, buffer.getNumSamples()));
    }
}

//==============================================================================
bool PitchShiftAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PitchShiftAudioProcessor::createEditor()
{
    return new PitchShiftAudioProcessorEditor (*this);
}

//==============================================================================
void PitchShiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PitchShiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchShiftAudioProcessor();
}
