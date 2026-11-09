#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (900, 480);
    setResizable (false, false);

    const auto teal = juce::Colour::fromRGB (60, 220, 190);
    const auto cream = juce::Colour::fromRGB (245, 238, 220);
    const auto dark = juce::Colour::fromRGB (22, 29, 42);
    const auto panel = juce::Colour::fromRGB (34, 44, 60);
    const auto orange = juce::Colour::fromRGB (255, 157, 70);

    auto setupSlider = [&] (juce::Slider& slider, const juce::String& suffix,
                            int decimalPlaces)
    {
        addAndMakeVisible (slider);
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 22);
        slider.setNumDecimalPlacesToDisplay (decimalPlaces);
        slider.setTextValueSuffix (suffix);
        slider.setColour (juce::Slider::rotarySliderFillColourId, teal);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, panel);
        slider.setColour (juce::Slider::thumbColourId, orange);
        slider.setColour (juce::Slider::textBoxTextColourId, cream);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, dark);
        slider.setColour (juce::Slider::textBoxOutlineColourId, panel);
    };

    setupSlider (thresholdSlider, " dB", 1);
    setupSlider (makeupSlider, " dB", 1);
    setupSlider (mixSlider, " %", 0);

    ratioBox.addItemList (juce::StringArray { "2:1", "4:1", "10:1" }, 1);
    attackBox.addItemList (
        juce::StringArray { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 1);
    releaseBox.addItemList (
        juce::StringArray { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 1);
    hpfBox.addItemList (
        juce::StringArray { "Off", "60 Hz", "90 Hz", "120 Hz", "150 Hz", "200 Hz" }, 1);

    for (auto* box : { &ratioBox, &attackBox, &releaseBox, &hpfBox })
    {
        addAndMakeVisible (*box);
        box->setJustificationType (juce::Justification::centred);
        box->setColour (juce::ComboBox::backgroundColourId, dark);
        box->setColour (juce::ComboBox::textColourId, cream);
        box->setColour (juce::ComboBox::outlineColourId, teal);
        box->setColour (juce::ComboBox::arrowColourId, teal);
    }

    const std::array<juce::String, 7> names {
        "THRESHOLD", "RATIO", "ATTACK", "RELEASE",
        "MAKEUP", "MIX", "SIDECHAIN HPF"
    };

    for (size_t i = 0; i < controlLabels.size(); ++i)
    {
        addAndMakeVisible (controlLabels[i]);
        controlLabels[i].setText (names[i], juce::dontSendNotification);
        controlLabels[i].setJustificationType (juce::Justification::centred);
        controlLabels[i].setColour (juce::Label::textColourId, cream);
        controlLabels[i].setFont (juce::Font (12.0f, juce::Font::bold));
    }

    addAndMakeVisible (inButton);
    inButton.setColour (juce::ToggleButton::textColourId, cream);
    inButton.setColour (juce::ToggleButton::tickColourId, teal);
    inButton.setColour (juce::ToggleButton::tickDisabledColourId, panel);

    thresholdAttachment = std::make_unique<SliderAttachment>(
        processorRef.apvts, "threshold", thresholdSlider);
    ratioAttachment = std::make_unique<ComboBoxAttachment>(
        processorRef.apvts, "ratio", ratioBox);
    attackAttachment = std::make_unique<ComboBoxAttachment>(
        processorRef.apvts, "attack", attackBox);
    releaseAttachment = std::make_unique<ComboBoxAttachment>(
        processorRef.apvts, "release", releaseBox);
    makeupAttachment = std::make_unique<SliderAttachment>(
        processorRef.apvts, "makeup", makeupSlider);
    mixAttachment = std::make_unique<SliderAttachment>(
        processorRef.apvts, "mix", mixSlider);
    hpfAttachment = std::make_unique<ComboBoxAttachment>(
        processorRef.apvts, "hpf", hpfBox);
    inAttachment = std::make_unique<ButtonAttachment>(
        processorRef.apvts, "in", inButton);

    startTimerHz (30);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto background = juce::Colour::fromRGB (22, 29, 42);
    const auto panel = juce::Colour::fromRGB (34, 44, 60);
    const auto cream = juce::Colour::fromRGB (245, 238, 220);
    const auto teal = juce::Colour::fromRGB (60, 220, 190);
    const auto orange = juce::Colour::fromRGB (255, 157, 70);

    g.fillAll (background);

    auto panelArea = getLocalBounds().toFloat().reduced (14.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (panelArea, 18.0f);

    g.setColour (teal);
    g.fillRoundedRectangle (juce::Rectangle<float> (28.0f, 25.0f, 7.0f, 60.0f), 3.0f);

    g.setColour (cream);
    g.setFont (juce::Font (14.0f, juce::Font::bold));
    g.drawText ("NOPTIK AUDIO LABS", 48, 24, 300, 22, juce::Justification::left);

    g.setFont (juce::Font (28.0f, juce::Font::bold));
    g.drawText ("GLUE GREASE", 48, 44, 360, 38, juce::Justification::left);

    g.setColour (teal);
    g.setFont (juce::Font (12.0f));
    g.drawText ("STEREO BUS COMPRESSOR", 50, 80, 300, 20, juce::Justification::left);

    // Gain-reduction meter
    const float reduction = juce::jlimit (0.0f, 15.0f,
                                          processorRef.getGainReductionDb());
    const float meterX = (float) getWidth() - 71.0f;
    const float meterY = 169.0f;
    const float meterWidth = 24.0f;
    const float meterHeight = 205.0f;

    g.setColour (cream);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("GR", (int) meterX - 10, 137, 44, 22, juce::Justification::centred);
    g.drawText (juce::String (reduction, 1) + " dB",
                (int) meterX - 28, 380, 62, 22, juce::Justification::centred);

    g.setColour (background);
    g.fillRoundedRectangle (meterX, meterY, meterWidth, meterHeight, 5.0f);

    const float fillHeight = meterHeight * (reduction / 15.0f);
    g.setColour (reduction > 8.0f ? orange : teal);
    g.fillRoundedRectangle (meterX + 4.0f,
                            meterY + meterHeight - fillHeight,
                            meterWidth - 8.0f,
                            fillHeight,
                            3.0f);

    g.setColour (cream.withAlpha (0.5f));
    g.drawRoundedRectangle (meterX, meterY, meterWidth, meterHeight, 5.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (112);
    area.removeFromBottom (28);

    auto controlsArea = area;
    controlsArea.removeFromRight (66);

    const int columnWidth = controlsArea.getWidth() / 7;
    const int labelY = controlsArea.getY();
    const int controlY = labelY + 28;
    const int controlHeight = controlsArea.getHeight() - 30;

    for (int i = 0; i < 7; ++i)
    {
        const int x = controlsArea.getX() + i * columnWidth;
        controlLabels[(size_t) i].setBounds (x, labelY, columnWidth, 24);
    }

    thresholdSlider.setBounds (controlsArea.getX(), controlY,
                               columnWidth, controlHeight);
    ratioBox.setBounds (controlsArea.getX() + columnWidth + 8,
                        controlY + 70, columnWidth - 16, 42);
    attackBox.setBounds (controlsArea.getX() + columnWidth * 2 + 8,
                         controlY + 70, columnWidth - 16, 42);
    releaseBox.setBounds (controlsArea.getX() + columnWidth * 3 + 8,
                          controlY + 70, columnWidth - 16, 42);
    makeupSlider.setBounds (controlsArea.getX() + columnWidth * 4,
                            controlY, columnWidth, controlHeight);
    mixSlider.setBounds (controlsArea.getX() + columnWidth * 5,
                         controlY, columnWidth, controlHeight);
    hpfBox.setBounds (controlsArea.getX() + columnWidth * 6 + 6,
                      controlY + 70, columnWidth - 12, 42);

    inButton.setBounds (getWidth() - 190, 47, 140, 30);
}

void PluginEditor::timerCallback()
{
    repaint();
}
