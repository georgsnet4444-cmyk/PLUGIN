#pragma once

#include "PluginProcessor.h"

class NorthstarMasteringAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     private juce::Timer
{
public:
    explicit NorthstarMasteringAudioProcessorEditor(NorthstarMasteringAudioProcessor&);
    ~NorthstarMasteringAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Meter : public juce::Component
    {
    public:
        void setValue(float value) { level = value; repaint(); }
        void paint(juce::Graphics&) override;
    private:
        float level = 0.0f;
    };

    class Spectrum : public juce::Component
    {
    public:
        void setValues(const std::array<float, 48>& values)
        {
            bins = values;
            repaint();
        }
        void paint(juce::Graphics&) override;
    private:
        std::array<float, 48> bins {};
    };

    void timerCallback() override;
    void styleSlider(juce::Slider&, const juce::String& suffix);

    NorthstarMasteringAudioProcessor& processor;
    juce::Label brandLabel;
    juce::Label statusLabel;
    juce::Label loudnessLabel;
    juce::Label peakLabel;
    juce::Label targetLabel;
    juce::Label amountLabel;
    juce::Label inputLabel;
    juce::Label widthLabel;
    juce::Label ceilingLabel;
    juce::TextButton analyzeButton { "ANALYZE & MASTER" };
    juce::TextButton toneButton { "TONE" };
    juce::TextButton dynamicsButton { "DYNAMICS" };
    juce::TextButton limiterButton { "LIMITER" };
    juce::TextButton bypassButton { "BYPASS" };
    juce::ComboBox styleSelector;
    juce::Slider targetSlider;
    juce::Slider amountSlider;
    juce::Slider inputSlider;
    juce::Slider widthSlider;
    juce::Slider ceilingSlider;
    Meter inputMeter;
    Meter outputMeter;
    Spectrum spectrum;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> targetAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> ceilingAttachment;
    std::unique_ptr<ComboAttachment> styleAttachment;
    std::unique_ptr<ButtonAttachment> toneAttachment;
    std::unique_ptr<ButtonAttachment> dynamicsAttachment;
    std::unique_ptr<ButtonAttachment> limiterAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NorthstarMasteringAudioProcessorEditor)
};