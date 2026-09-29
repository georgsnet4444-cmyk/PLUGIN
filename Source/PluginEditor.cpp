#include "PluginEditor.h"

namespace
{
const juce::Colour background { 0xff101317 };
const juce::Colour panel { 0xff181d23 };
const juce::Colour border { 0xff2b333d };
const juce::Colour text { 0xffeaf0f4 };
const juce::Colour muted { 0xff7c8996 };
const juce::Colour accent { 0xff67e0c2 };
}

NorthstarMasteringAudioProcessorEditor::NorthstarMasteringAudioProcessorEditor(
    NorthstarMasteringAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 530);
    setResizable(true, true);
    setResizeLimits(660, 490, 1080, 720);

    brandLabel.setText("NORTHSTAR", juce::dontSendNotification);
    brandLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    brandLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(brandLabel);

    statusLabel.setText("MASTERING ASSISTANT", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    statusLabel.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(statusLabel);

    for (auto* button : { &analyzeButton, &toneButton, &dynamicsButton, &limiterButton, &bypassButton })
    {
        button->setColour(juce::TextButton::buttonColourId, panel);
        button->setColour(juce::TextButton::textColourOffId, text);
        button->setColour(juce::TextButton::textColourOnId, background);
        button->setColour(juce::TextButton::buttonOnColourId, accent);
        button->setClickingTogglesState(button != &analyzeButton);
        button->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible(*button);
    }
    toneButton.setToggleState(true, juce::dontSendNotification);
    dynamicsButton.setToggleState(true, juce::dontSendNotification);
    limiterButton.setToggleState(true, juce::dontSendNotification);
    bypassButton.setToggleState(false, juce::dontSendNotification);
    analyzeButton.setColour(juce::TextButton::buttonColourId, accent);
    analyzeButton.setColour(juce::TextButton::textColourOffId, background);
    analyzeButton.onClick = [this] { processor.requestAnalysis(); };

    const auto addLabel = [this](juce::Label& label, const juce::String& title)
    {
        label.setText(title, juce::dontSendNotification);
        label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        label.setColour(juce::Label::textColourId, muted);
        addAndMakeVisible(label);
    };
    addLabel(targetLabel, "TARGET");
    addLabel(amountLabel, "AMOUNT");
    addLabel(inputLabel, "INPUT");
    addLabel(widthLabel, "STEREO WIDTH");
    addLabel(ceilingLabel, "CEILING");
    addLabel(loudnessLabel, juce::String::fromUTF8("\xe2\x80\x94 LUFS"));
    addLabel(peakLabel, juce::String::fromUTF8("PEAK \xe2\x80\x94 dB"));
    loudnessLabel.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
    loudnessLabel.setColour(juce::Label::textColourId, text);
    peakLabel.setColour(juce::Label::textColourId, accent);
    peakLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

    styleSlider(targetSlider, " LUFS");
    styleSlider(amountSlider, "%");
    styleSlider(inputSlider, " dB");
    styleSlider(widthSlider, "%");
    styleSlider(ceilingSlider, " dB");

    styleSelector.addItem("BALANCED", 1);
    styleSelector.addItem("WARM", 2);
    styleSelector.addItem("PUNCH", 3);
    styleSelector.setColour(juce::ComboBox::backgroundColourId, panel);
    styleSelector.setColour(juce::ComboBox::textColourId, text);
    styleSelector.setColour(juce::ComboBox::outlineColourId, border);
    addAndMakeVisible(styleSelector);

    inputMeter.setName("Input level");
    outputMeter.setName("Output level");
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(spectrum);

    targetAttachment = std::make_unique<SliderAttachment>(processor.parameters, "target", targetSlider);
    amountAttachment = std::make_unique<SliderAttachment>(processor.parameters, "amount", amountSlider);
    inputAttachment = std::make_unique<SliderAttachment>(processor.parameters, "input", inputSlider);
    widthAttachment = std::make_unique<SliderAttachment>(processor.parameters, "width", widthSlider);
    ceilingAttachment = std::make_unique<SliderAttachment>(processor.parameters, "ceiling", ceilingSlider);
    styleAttachment = std::make_unique<ComboAttachment>(processor.parameters, "style", styleSelector);
    toneAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "eqOn", toneButton);
    dynamicsAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "dynamicsOn", dynamicsButton);
    limiterAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "limiterOn", limiterButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "bypass", bypassButton);
    startTimerHz(20);
}

void NorthstarMasteringAudioProcessorEditor::styleSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 20);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, border);
    slider.setColour(juce::Slider::thumbColourId, text);
    slider.setColour(juce::Slider::textBoxTextColourId, text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);
}

void NorthstarMasteringAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(panel);
    g.fillRoundedRectangle(18.0f, 70.0f, static_cast<float>(getWidth() - 36), 238.0f, 10.0f);
    g.fillRoundedRectangle(18.0f, 322.0f, static_cast<float>(getWidth() - 36), 188.0f, 10.0f);
    g.setColour(border);
    g.drawRoundedRectangle(18.0f, 70.0f, static_cast<float>(getWidth() - 36), 238.0f, 10.0f, 1.0f);
    g.drawRoundedRectangle(18.0f, 322.0f, static_cast<float>(getWidth() - 36), 188.0f, 10.0f, 1.0f);
    g.setColour(muted);
    g.setFont(10.0f);
    g.drawText("LEARN A SHORT PASS, THEN APPLY A RESTRAINED MASTER",
               34, 82, getWidth() - 335, 16, juce::Justification::centredLeft);
    g.drawText("SIGNAL", 30, 330, 66, 16, juce::Justification::centredLeft);
    g.drawText("LIVE SPECTRUM", 112, 330, 200, 16, juce::Justification::centredLeft);
    g.setColour(border);
    g.drawHorizontalLine(306, 34.0f, static_cast<float>(getWidth() - 34));
    g.setColour(muted.withAlpha(0.72f));
    g.setFont(9.5f);
    g.drawText("INPUT", 25, 365, 52, 12, juce::Justification::centred);
    g.drawText("OUTPUT", 25, 456, 52, 12, juce::Justification::centred);
    g.drawText("40", 111, 478, 30, 13, juce::Justification::centredLeft);
    g.drawText("250", 195, 478, 35, 13, juce::Justification::centred);
    g.drawText("1k", 263, 478, 30, 13, juce::Justification::centred);
    g.drawText("8k", 324, 478, 30, 13, juce::Justification::centredRight);
    g.drawText("20k", 354, 478, 35, 13, juce::Justification::centredRight);
}

void NorthstarMasteringAudioProcessorEditor::resized()
{
    const auto w = getWidth();
    brandLabel.setBounds(22, 16, 170, 32);
    statusLabel.setBounds(23, 43, 220, 17);
    analyzeButton.setBounds(w - 210, 18, 185, 38);
    loudnessLabel.setBounds(w - 190, 76, 160, 29);
    peakLabel.setBounds(w - 190, 105, 160, 19);
    inputMeter.setBounds(44, 390, 15, 63);
    outputMeter.setBounds(44, 468, 15, 29);
    spectrum.setBounds(108, 352, juce::jmax(260, w - 144), 120);

    const auto columnWidth = static_cast<float>(w - 56) / 5.0f;
    const std::array<juce::Component*, 5> controls {
        &targetSlider, &amountSlider, &inputSlider, &widthSlider, &ceilingSlider
    };
    const std::array<juce::Label*, 5> labels {
        &targetLabel, &amountLabel, &inputLabel, &widthLabel, &ceilingLabel
    };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        const auto center = 28.0f + columnWidth * (static_cast<float>(i) + 0.5f);
        controls[i]->setBounds(static_cast<int>(center - 54.0f), 133, 108, 148);
        labels[i]->setBounds(static_cast<int>(center - 53.0f), 115, 106, 18);
    }
    const auto rowY = 282;
    toneButton.setBounds(34, rowY, 92, 30);
    dynamicsButton.setBounds(126, rowY, 112, 30);
    limiterButton.setBounds(238, rowY, 92, 30);
    styleSelector.setBounds(w - 226, rowY, 132, 30);
    bypassButton.setBounds(w - 88, rowY, 54, 30);
}

void NorthstarMasteringAudioProcessorEditor::timerCallback()
{
    std::array<float, 48> bins {};
    processor.copySpectrum(bins);
    spectrum.setValues(bins);
    inputMeter.setValue(juce::jlimit(0.0f, 1.0f, (processor.getLoudnessEstimate() + 48.0f) / 48.0f));
    outputMeter.setValue(juce::jlimit(0.0f, 1.0f, (processor.getPeakDb() + 36.0f) / 36.0f));

    const auto lufs = processor.getLoudnessEstimate();
    loudnessLabel.setText(juce::String(lufs, 1) + " LUFS", juce::dontSendNotification);
    peakLabel.setText("PEAK  " + juce::String(processor.getPeakDb(), 1) + " dBFS",
                      juce::dontSendNotification);

    if (processor.isAnalysisRunning())
    {
        analyzeButton.setButtonText("ANALYZING  " +
            juce::String(static_cast<int>(processor.getAnalysisProgress() * 100.0f)) + "%");
    }
    else if (processor.hasAnalysis())
    {
        analyzeButton.setButtonText("RE-ANALYZE");
    }
    else
    {
        analyzeButton.setButtonText("ANALYZE & MASTER");
    }
}

void NorthstarMasteringAudioProcessorEditor::Meter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(border);
    g.fillRoundedRectangle(bounds, 4.0f);
    const auto height = bounds.getHeight() * juce::jlimit(0.0f, 1.0f, level);
    g.setColour(level > 0.88f ? juce::Colour(0xffff806f) : accent);
    g.fillRoundedRectangle(bounds.removeFromBottom(height).reduced(1.0f), 3.0f);
}

void NorthstarMasteringAudioProcessorEditor::Spectrum::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff12171c));
    g.fillRoundedRectangle(bounds, 6.0f);

    for (int i = 0; i < 4; ++i)
    {
        const auto y = bounds.getY() + bounds.getHeight() * (static_cast<float>(i + 1) / 5.0f);
        g.setColour(border.withAlpha(0.65f));
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    juce::Path path;
    const auto step = bounds.getWidth() / static_cast<float>(bins.size() - 1);
    for (size_t i = 0; i < bins.size(); ++i)
    {
        const auto x = bounds.getX() + step * static_cast<float>(i);
        const auto level = juce::jlimit(0.0f, 1.0f, bins[i]);
        const auto y = bounds.getBottom() - 7.0f - level * (bounds.getHeight() - 18.0f);
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    juce::Path area = path;
    area.lineTo(bounds.getRight(), bounds.getBottom());
    area.lineTo(bounds.getX(), bounds.getBottom());
    area.closeSubPath();
    g.setColour(accent.withAlpha(0.09f));
    g.fillPath(area);
    g.setColour(accent);
    g.strokePath(path, juce::PathStrokeType(1.6f));
}