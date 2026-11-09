#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class PluginProcessor : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // The editor uses these to show its meters and connect its controls.
    APVTS apvts;
    float getGainReductionDb() const { return gainReductionDb.load(); }
    float getInputLevelDb() const    { return inputLevelDb.load(); }
    float getOutputLevelDb() const   { return outputLevelDb.load(); }

private:
    static APVTS::ParameterLayout createParameterLayout();

    double currentSampleRate = 44100.0;

    // Linked stereo compressor detector state.
    float envelopeDb = 0.0f;

    // Detector high-pass filter state.
    float hpfPreviousInputL = 0.0f, hpfPreviousInputR = 0.0f;
    float hpfPreviousOutputL = 0.0f, hpfPreviousOutputR = 0.0f;

    juce::SmoothedValue<float> smoothedMakeupDb;
    juce::SmoothedValue<float> smoothedMix;
    juce::SmoothedValue<float> smoothedActive;

    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> inputLevelDb { -60.0f };
    std::atomic<float> outputLevelDb { -60.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
