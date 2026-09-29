#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

class NorthstarMasteringAudioProcessor final : public juce::AudioProcessor
{
public:
    NorthstarMasteringAudioProcessor();
    ~NorthstarMasteringAudioProcessor() override = default;
    using juce::AudioProcessor::processBlock;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void requestAnalysis();
    bool isAnalysisRunning() const noexcept { return analysisRunning.load(); }
    float getAnalysisProgress() const noexcept { return analysisProgress.load(); }
    float getLoudnessEstimate() const noexcept { return loudnessEstimate.load(); }
    float getPeakDb() const noexcept { return peakDb.load(); }
    float getLearnedLowDb() const noexcept { return learnedLowDb.load(); }
    float getLearnedHighDb() const noexcept { return learnedHighDb.load(); }
    float getLearnedCompression() const noexcept { return learnedCompression.load(); }
    bool hasAnalysis() const noexcept { return analysisComplete.load(); }

    void copySpectrum(std::array<float, 48>& destination) const noexcept;

    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    void updateEqFilters(float lowDb, float bodyDb, float highDb);
    void finishAnalysis();
    static float toDb(float value) noexcept;

    double currentSampleRate = 44100.0;
    std::array<std::array<Filter, 2>, 3> eqFilters;
    std::array<std::array<Filter, 2>, 2> loudnessFilters;
    std::array<std::atomic<float>, 48> spectrumBins {};
    juce::dsp::FFT spectrumFft { 11 };
    juce::dsp::WindowingFunction<float> spectrumWindow {
        2048, juce::dsp::WindowingFunction<float>::hann, false
    };
    std::array<float, 2048> spectrumFifo {};
    std::array<float, 4096> spectrumWork {};
    std::array<float, 2> analysisLowState {};
    std::array<float, 2> analysisHighState {};
    int spectrumFifoPosition = 0;
    float lastLowDb = 999.0f;
    float lastBodyDb = 999.0f;
    float lastHighDb = 999.0f;

    std::atomic<bool> analysisRequested { false };
    std::atomic<bool> analysisRunning { false };
    std::atomic<bool> analysisComplete { false };
    std::atomic<float> analysisProgress { 0.0f };
    std::atomic<float> loudnessEstimate { -60.0f };
    std::atomic<float> peakDb { -60.0f };
    std::atomic<float> learnedLowDb { 0.0f };
    std::atomic<float> learnedBodyDb { 0.0f };
    std::atomic<float> learnedHighDb { 0.0f };
    std::atomic<float> learnedCompression { 0.0f };
    std::atomic<float> learnedInputLufs { -18.0f };

    bool collectingAnalysis = false;
    int64_t analyzedSamples = 0;
    double analysisEnergy = 0.0;
    double analysisLowEnergy = 0.0;
    double analysisBodyEnergy = 0.0;
    double analysisHighEnergy = 0.0;
    float envelope = 0.0f;
    float smoothedPeak = -60.0f;
    float smoothedLoudness = -60.0f;
    int spectrumUpdateCountdown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NorthstarMasteringAudioProcessor)
};