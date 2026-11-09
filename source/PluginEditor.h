#pragma once

#include "PluginProcessor.h"
#include <array>

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment =
        juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;

    PluginProcessor& processorRef;

    juce::Slider thresholdSlider;
    juce::Slider makeupSlider;
    juce::Slider mixSlider;

    juce::ComboBox ratioBox;
    juce::ComboBox attackBox;
    juce::ComboBox releaseBox;
    juce::ComboBox hpfBox;

    juce::ToggleButton inButton { "COMP IN" };
    std::array<juce::Label, 7> controlLabels;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> makeupAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    std::unique_ptr<ComboBoxAttachment> ratioAttachment;
    std::unique_ptr<ComboBoxAttachment> attackAttachment;
    std::unique_ptr<ComboBoxAttachment> releaseAttachment;
    std::unique_ptr<ComboBoxAttachment> hpfAttachment;

    std::unique_ptr<ButtonAttachment> inAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
