#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

PluginProcessor::APVTS::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "threshold", 1 }, "Threshold",
        juce::NormalisableRange<float> (-40.0f, 10.0f, 0.1f), -12.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "ratio", 1 }, "Ratio",
        juce::StringArray { "2:1", "4:1", "10:1" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "attack", 1 }, "Attack",
        juce::StringArray { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 5));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "release", 1 }, "Release",
        juce::StringArray { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 4));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "makeup", 1 }, "Makeup",
        juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "hpf", 1 }, "Detector HPF",
        juce::StringArray { "Off", "60 Hz", "90 Hz", "120 Hz", "150 Hz", "200 Hz" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "in", 1 }, "In", true));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
bool PluginProcessor::acceptsMidi() const { return false; }
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }
double PluginProcessor::getTailLengthSeconds() const { return 0.0; }
int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }
void PluginProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "Default";
}
void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;
    envelopeDb = 0.0f;

    hpfPreviousInputL = hpfPreviousInputR = 0.0f;
    hpfPreviousOutputL = hpfPreviousOutputR = 0.0f;

    smoothedMakeupDb.reset (sampleRate, 0.02);
    smoothedMix.reset (sampleRate, 0.02);
    smoothedActive.reset (sampleRate, 0.02);

    smoothedMakeupDb.setCurrentAndTargetValue (
        apvts.getRawParameterValue ("makeup")->load());
    smoothedMix.setCurrentAndTargetValue (
        apvts.getRawParameterValue ("mix")->load() * 0.01f);
    smoothedActive.setCurrentAndTargetValue (
        apvts.getRawParameterValue ("in")->load());
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    const float thresholdDb = apvts.getRawParameterValue ("threshold")->load();
    const int ratioIndex = juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("ratio")->load());
    const int attackIndex = juce::jlimit (0, 5, (int) apvts.getRawParameterValue ("attack")->load());
    const int releaseIndex = juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("release")->load());
    const int hpfIndex = juce::jlimit (0, 5, (int) apvts.getRawParameterValue ("hpf")->load());
    const bool isOn = apvts.getRawParameterValue ("in")->load() > 0.5f;

    constexpr std::array<float, 3> ratios { 2.0f, 4.0f, 10.0f };
    constexpr std::array<float, 6> attacks { 0.0001f, 0.0003f, 0.001f, 0.003f, 0.010f, 0.030f };
    constexpr std::array<float, 5> releases { 0.1f, 0.3f, 0.6f, 1.2f, 0.3f };
    constexpr std::array<float, 6> hpfFrequencies { 0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 200.0f };

    const float ratio = ratios[(size_t) ratioIndex];
    const float attackSeconds = attacks[(size_t) attackIndex];
    const float kneeWidthDb = (ratioIndex == 0 ? 6.0f : ratioIndex == 1 ? 4.5f : 3.0f);
    const float halfKneeDb = kneeWidthDb * 0.5f;
    const float hpfFrequency = hpfFrequencies[(size_t) hpfIndex];

    smoothedMakeupDb.setTargetValue (apvts.getRawParameterValue ("makeup")->load());
    smoothedMix.setTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    smoothedActive.setTargetValue (isOn ? 1.0f : 0.0f);

    const float attackCoefficient =
        std::exp (-1.0f / (float) (currentSampleRate * attackSeconds));

    const float hpfCoefficient = hpfFrequency > 0.0f
        ? std::exp (-2.0f * juce::MathConstants<float>::pi * hpfFrequency
                    / (float) currentSampleRate)
        : 0.0f;

    float maxReductionThisBlock = 0.0f;
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = left[i];
        const float dryR = right[i];

        inputPeak = juce::jmax (inputPeak, std::abs (dryL), std::abs (dryR));

        // High-pass filtering is used only by the compressor detector.
        float detectorL = dryL;
        float detectorR = dryR;

        if (hpfFrequency > 0.0f)
        {
            const float filteredL = hpfCoefficient
                * (hpfPreviousOutputL + dryL - hpfPreviousInputL);
            const float filteredR = hpfCoefficient
                * (hpfPreviousOutputR + dryR - hpfPreviousInputR);

            hpfPreviousInputL = dryL;
            hpfPreviousInputR = dryR;
            hpfPreviousOutputL = filteredL;
            hpfPreviousOutputR = filteredR;

            detectorL = filteredL;
            detectorR = filteredR;
        }
        else
        {
            hpfPreviousInputL = dryL;
            hpfPreviousInputR = dryR;
            hpfPreviousOutputL = dryL;
            hpfPreviousOutputR = dryR;
        }

        // One shared detector keeps the stereo image centered.
        const float linkedPeak = juce::jmax (std::abs (detectorL), std::abs (detectorR));
        const float detectorDb = juce::Decibels::gainToDecibels (linkedPeak, -100.0f);
        const float overThresholdDb = detectorDb - thresholdDb;

        float targetReductionDb = 0.0f;

        if (overThresholdDb <= -halfKneeDb)
        {
            targetReductionDb = 0.0f;
        }
        else if (overThresholdDb >= halfKneeDb)
        {
            targetReductionDb = overThresholdDb * (1.0f - 1.0f / ratio);
        }
        else
        {
            const float x = overThresholdDb + halfKneeDb;
            targetReductionDb =
                (1.0f - 1.0f / ratio) * x * x / (2.0f * kneeWidthDb);
        }

        float releaseSeconds = releases[(size_t) releaseIndex];

        // Auto release adapts to the amount of gain reduction.
        if (releaseIndex == 4)
            releaseSeconds = juce::jlimit (0.10f, 1.10f, 0.10f + envelopeDb * 0.08f);

        const float releaseCoefficient =
            std::exp (-1.0f / (float) (currentSampleRate * releaseSeconds));

        const float coefficient = targetReductionDb > envelopeDb
            ? attackCoefficient
            : releaseCoefficient;

        envelopeDb = coefficient * envelopeDb
                   + (1.0f - coefficient) * targetReductionDb;

        maxReductionThisBlock = juce::jmax (maxReductionThisBlock, envelopeDb);

        const float makeup = smoothedMakeupDb.getNextValue();
        const float mix = smoothedMix.getNextValue();
        const float active = smoothedActive.getNextValue();

        const float compressionGain =
            juce::Decibels::decibelsToGain (makeup - envelopeDb);

        const float wetL = dryL * compressionGain;
        const float wetR = dryR * compressionGain;

        const float mixedL = dryL * (1.0f - mix) + wetL * mix;
        const float mixedR = dryR * (1.0f - mix) + wetR * mix;

        left[i]  = dryL * (1.0f - active) + mixedL * active;
        right[i] = dryR * (1.0f - active) + mixedR * active;

        outputPeak = juce::jmax (outputPeak, std::abs (left[i]), std::abs (right[i]));
    }

    gainReductionDb.store (isOn ? maxReductionThisBlock : 0.0f);
    inputLevelDb.store (juce::Decibels::gainToDecibels (inputPeak, -60.0f));
    outputLevelDb.store (juce::Decibels::gainToDecibels (outputPeak, -60.0f));
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
