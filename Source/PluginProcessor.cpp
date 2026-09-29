#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int analysisDurationSeconds = 4;
constexpr float safeFloor = 1.0e-9f;

juce::AudioParameterFloatAttributes withLabel(const juce::String& label)
{
    return juce::AudioParameterFloatAttributes().withLabel(label);
}
}

NorthstarMasteringAudioProcessor::NorthstarMasteringAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "NORTHSTAR_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
NorthstarMasteringAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<Float>(
        juce::ParameterID { "target", 1 }, "Target", juce::NormalisableRange<float>(-18.0f, -8.0f, 0.1f), -14.0f,
        withLabel("LUFS")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "input", 1 }, "Input", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        withLabel("dB")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "amount", 1 }, "Amount", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 72.0f,
        withLabel("%")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "width", 1 }, "Width", juce::NormalisableRange<float>(0.0f, 150.0f, 0.1f), 100.0f,
        withLabel("%")));
    layout.add(std::make_unique<Float>(
        juce::ParameterID { "ceiling", 1 }, "Ceiling", juce::NormalisableRange<float>(-1.5f, 0.0f, 0.01f), -1.0f,
        withLabel("dBFS")));
    layout.add(std::make_unique<Choice>(
        juce::ParameterID { "style", 1 }, "Style",
        juce::StringArray { "Balanced", "Warm", "Punch" }, 0));
    layout.add(std::make_unique<Bool>(juce::ParameterID { "eqOn", 1 }, "Tone", true));
    layout.add(std::make_unique<Bool>(juce::ParameterID { "dynamicsOn", 1 }, "Dynamics", true));
    layout.add(std::make_unique<Bool>(juce::ParameterID { "limiterOn", 1 }, "Limiter", true));
    layout.add(std::make_unique<Bool>(juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

bool NorthstarMasteringAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output)
        return false;
    return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();
}

void NorthstarMasteringAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    envelope = 0.0f;
    smoothedPeak = -60.0f;
    smoothedLoudness = -60.0f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 8192;
    spec.numChannels = 1;

    for (auto& band : eqFilters)
        for (auto& filter : band)
        {
            filter.prepare(spec);
            filter.reset();
        }

    for (auto& stage : loudnessFilters)
        for (auto& filter : stage)
        {
            filter.prepare(spec);
            filter.reset();
        }

    const auto shelfFrequency = juce::jmin(1681.974f, static_cast<float>(sampleRate * 0.40));
    const auto highPassFrequency = juce::jmin(38.135f, static_cast<float>(sampleRate * 0.20));
    for (size_t channel = 0; channel < 2; ++channel)
    {
        loudnessFilters[0][channel].coefficients =
            juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, highPassFrequency, 0.5f);
        loudnessFilters[1][channel].coefficients =
            juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                sampleRate, shelfFrequency, 0.707f, juce::Decibels::decibelsToGain(3.999f));
    }

    analysisLowState.fill(0.0f);
    analysisHighState.fill(0.0f);
    spectrumFifo.fill(0.0f);
    spectrumWork.fill(0.0f);
    spectrumFifoPosition = 0;
    lastLowDb = 999.0f;
    lastBodyDb = 999.0f;
    lastHighDb = 999.0f;
    updateEqFilters(0.0f, 0.0f, 0.0f);
    analysisRunning.store(false);
    analysisProgress.store(0.0f);
    analysisComplete.store(false);
    collectingAnalysis = false;
}

void NorthstarMasteringAudioProcessor::releaseResources()
{
}

void NorthstarMasteringAudioProcessor::requestAnalysis()
{
    analysisRequested.store(true);
}

float NorthstarMasteringAudioProcessor::toDb(float value) noexcept
{
    return 20.0f * std::log10(std::max(value, safeFloor));
}

void NorthstarMasteringAudioProcessor::updateEqFilters(float lowDb, float bodyDb, float highDb)
{
    if (std::abs(lastLowDb - lowDb) < 0.005f
        && std::abs(lastBodyDb - bodyDb) < 0.005f
        && std::abs(lastHighDb - highDb) < 0.005f)
        return;

    const auto sr = currentSampleRate;
    const auto safeFrequency = [sr](float hz)
    {
        return juce::jlimit(10.0f, static_cast<float>(sr * 0.45), hz);
    };

    const std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>, 3> coefficients {
        juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, safeFrequency(135.0f), 0.707f,
                                                         juce::Decibels::decibelsToGain(lowDb)),
        juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, safeFrequency(520.0f), 0.8f,
                                                            juce::Decibels::decibelsToGain(bodyDb)),
        juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, safeFrequency(8200.0f), 0.707f,
                                                           juce::Decibels::decibelsToGain(highDb))
    };

    for (size_t band = 0; band < coefficients.size(); ++band)
        for (auto& filter : eqFilters[band])
            filter.coefficients = coefficients[band];

    lastLowDb = lowDb;
    lastBodyDb = bodyDb;
    lastHighDb = highDb;
}

void NorthstarMasteringAudioProcessor::finishAnalysis()
{
    const auto samples = std::max<int64_t>(1, analyzedSamples);
    const auto totalEnergy = std::max(analysisLowEnergy + analysisBodyEnergy + analysisHighEnergy, 1.0e-12);
    const auto lowShare = static_cast<float>(analysisLowEnergy / totalEnergy);
    const auto bodyShare = static_cast<float>(analysisBodyEnergy / totalEnergy);
    const auto highShare = static_cast<float>(analysisHighEnergy / totalEnergy);

    const auto correction = [](float targetShare, float observedShare, float scale)
    {
        const auto ratio = targetShare / std::max(observedShare, 0.03f);
        return juce::jlimit(-2.5f, 2.5f, 10.0f * std::log10(ratio) * scale);
    };

    const auto lowDb = correction(0.25f, lowShare, 0.58f);
    const auto bodyDb = correction(0.55f, bodyShare, 0.34f);
    const auto highDb = correction(0.20f, highShare, 0.50f);

    const auto meanEnergy = std::max(analysisEnergy / static_cast<double>(samples), 1.0e-12);
    const auto inputLufs = static_cast<float>(-0.691 + 10.0 * std::log10(meanEnergy));
    learnedLowDb.store(lowDb);
    learnedBodyDb.store(bodyDb);
    learnedHighDb.store(highDb);
    learnedInputLufs.store(inputLufs);

    const auto peakToAverage = smoothedPeak - inputLufs;
    const auto compression = juce::jlimit(0.0f, 1.0f, (peakToAverage - 10.0f) / 12.0f);
    learnedCompression.store(compression);

    analysisProgress.store(1.0f);
    analysisRunning.store(false);
    analysisComplete.store(true);
    collectingAnalysis = false;
    updateEqFilters(lowDb, bodyDb, highDb);
}

void NorthstarMasteringAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();
    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    if (analysisRequested.exchange(false))
    {
        collectingAnalysis = true;
        analyzedSamples = 0;
        analysisEnergy = 0.0;
        analysisLowEnergy = 0.0;
        analysisBodyEnergy = 0.0;
        analysisHighEnergy = 0.0;
        analysisProgress.store(0.0f);
        analysisComplete.store(false);
        analysisRunning.store(true);
    }

    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin(inputChannels, 2);
    const auto amount = parameters.getRawParameterValue("amount")->load() * 0.01f;
    const auto inputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("input")->load());
    const auto width = parameters.getRawParameterValue("width")->load() * 0.01f;
    const auto targetLufs = parameters.getRawParameterValue("target")->load();
    const auto style = static_cast<int>(parameters.getRawParameterValue("style")->load());
    const bool toneOn = parameters.getRawParameterValue("eqOn")->load() > 0.5f;
    const bool dynamicsOn = parameters.getRawParameterValue("dynamicsOn")->load() > 0.5f;
    const bool limiterOn = parameters.getRawParameterValue("limiterOn")->load() > 0.5f;
    const bool bypass = parameters.getRawParameterValue("bypass")->load() > 0.5f;

    auto lowDb = analysisComplete.load() ? learnedLowDb.load() : 0.0f;
    auto bodyDb = analysisComplete.load() ? learnedBodyDb.load() : 0.0f;
    auto highDb = analysisComplete.load() ? learnedHighDb.load() : 0.0f;
    if (style == 1)
    {
        lowDb += 0.8f * amount;
        highDb -= 0.35f * amount;
    }
    else if (style == 2)
    {
        lowDb += 0.15f * amount;
        highDb += 0.65f * amount;
    }
    lowDb *= amount;
    bodyDb *= amount;
    highDb *= amount;
    updateEqFilters(lowDb, bodyDb, highDb);

    const auto attackCoeff = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.010f));
    const auto releaseCoeff = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.160f));
    const auto lowCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 180.0f / static_cast<float>(currentSampleRate));
    const auto highCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 2600.0f / static_cast<float>(currentSampleRate));

    double blockEnergy = 0.0;
    float blockPeak = 0.0f;

    const auto learnedMakeup = analysisComplete.load()
        ? juce::jlimit(-6.0f, 6.0f, (targetLufs - learnedInputLufs.load()) * 0.72f)
        : juce::jlimit(-4.0f, 4.0f, (targetLufs - loudnessEstimate.load()) * 0.35f);
    const auto outputGain = juce::Decibels::decibelsToGain(learnedMakeup * amount);
    const auto baseThreshold = -12.0f - learnedCompression.load() * 4.0f;
    const auto threshold = baseThreshold + (style == 1 ? 1.0f : 0.0f) - (style == 2 ? 1.0f : 0.0f);
    const auto ratio = 1.0f + amount * (style == 2 ? 1.2f : 1.8f);
    const auto ceilingGain = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue("ceiling")->load());
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float linkedPeak = 0.0f;
        float channelSamples[2] {};
        float weightedEnergy = 0.0f;
        float lowEnergy = 0.0f;
        float bodyEnergy = 0.0f;
        float highEnergy = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto channelIndex = static_cast<size_t>(channel);
            auto* data = buffer.getWritePointer(channel);
            auto inputSample = data[sample] * inputGain;
            channelSamples[channel] = inputSample;
            linkedPeak = juce::jmax(linkedPeak, std::abs(inputSample));

            auto weighted = loudnessFilters[0][channelIndex].processSample(inputSample);
            weighted = loudnessFilters[1][channelIndex].processSample(weighted);
            weightedEnergy += weighted * weighted;

            const auto lowState = lowCoeff * analysisLowState[static_cast<size_t>(channel)]
                + (1.0f - lowCoeff) * inputSample;
            const auto highState = highCoeff * analysisHighState[static_cast<size_t>(channel)]
                + (1.0f - highCoeff) * inputSample;
            analysisLowState[static_cast<size_t>(channel)] = lowState;
            analysisHighState[static_cast<size_t>(channel)] = highState;
            const auto lowBand = lowState;
            const auto highBand = inputSample - highState;
            const auto bodyBand = inputSample - lowBand - highBand;
            lowEnergy += lowBand * lowBand;
            bodyEnergy += bodyBand * bodyBand;
            highEnergy += highBand * highBand;

            auto output = eqFilters[0][channelIndex].processSample(inputSample);
            output = eqFilters[1][channelIndex].processSample(output);
            output = eqFilters[2][channelIndex].processSample(output);
            if (!toneOn)
                output = inputSample;
            channelSamples[channel] = output;

            blockPeak = juce::jmax(blockPeak, std::abs(output));
        }

        if (channels == 2)
        {
            const auto mid = (channelSamples[0] + channelSamples[1]) * 0.5f;
            const auto side = (channelSamples[0] - channelSamples[1]) * 0.5f * width;
            channelSamples[0] = mid + side;
            channelSamples[1] = mid - side;
        }

        const auto detector = juce::jmax(linkedPeak, safeFloor);
        envelope = detector > envelope
            ? attackCoeff * envelope + (1.0f - attackCoeff) * detector
            : releaseCoeff * envelope + (1.0f - releaseCoeff) * detector;
        const auto envelopeDb = toDb(envelope);
        const auto overThreshold = envelopeDb - threshold;
        const auto reductionDb = dynamicsOn && overThreshold > 0.0f
            ? overThreshold * (1.0f - 1.0f / ratio) * amount
            : 0.0f;
        const auto compressionGain = juce::Decibels::decibelsToGain(-reductionDb);
        blockEnergy += weightedEnergy;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto output = channelSamples[channel] * compressionGain * outputGain;
            if (limiterOn)
            {
                const auto absOutput = std::abs(output);
                if (absOutput > ceilingGain)
                    output = std::copysign(ceilingGain + (absOutput - ceilingGain) * 0.04f, output);
                output = juce::jlimit(-ceilingGain, ceilingGain, output);
            }
            else
            {
                output = juce::jlimit(-1.0f, 1.0f, output);
            }
            if (!bypass)
                buffer.getWritePointer(channel)[sample] = output;
        }

        if (collectingAnalysis)
        {
            analysisEnergy += weightedEnergy;
            analysisLowEnergy += lowEnergy / static_cast<double>(juce::jmax(1, channels));
            analysisBodyEnergy += bodyEnergy / static_cast<double>(juce::jmax(1, channels));
            analysisHighEnergy += highEnergy / static_cast<double>(juce::jmax(1, channels));
            ++analyzedSamples;
        }

        spectrumFifo[static_cast<size_t>(spectrumFifoPosition++)] =
            channels == 2 ? (channelSamples[0] + channelSamples[1]) * 0.5f : channelSamples[0];
        if (spectrumFifoPosition == static_cast<int>(spectrumFifo.size()))
        {
            std::copy(spectrumFifo.begin(), spectrumFifo.end(), spectrumWork.begin());
            spectrumWindow.multiplyWithWindowingTable(spectrumWork.data(), spectrumFifo.size());
            spectrumFft.performFrequencyOnlyForwardTransform(spectrumWork.data());

            std::array<float, 48> magnitudes {};
            std::array<int, 48> counts {};
            for (int bin = 1; bin <= 1024; ++bin)
            {
                const auto frequency = static_cast<float>(bin) * static_cast<float>(currentSampleRate)
                    / static_cast<float>(spectrumFifo.size());
                if (frequency < 20.0f || frequency > 20000.0f)
                    continue;
                const auto position = juce::jlimit(0, 47, static_cast<int>(
                    std::log(frequency / 20.0f) / std::log(1000.0f) * 48.0f));
                magnitudes[static_cast<size_t>(position)] += spectrumWork[static_cast<size_t>(bin)];
                ++counts[static_cast<size_t>(position)];
            }

            for (size_t i = 0; i < magnitudes.size(); ++i)
            {
                const auto average = counts[i] > 0
                    ? magnitudes[i] / static_cast<float>(counts[i])
                    : 0.0f;
                const auto db = 20.0f * std::log10(std::max(average / 2048.0f, 1.0e-6f));
                const auto normalized = juce::jlimit(0.0f, 1.0f, (db + 72.0f) / 60.0f);
                spectrumBins[i].store(0.68f * spectrumBins[i].load() + 0.32f * normalized);
            }
            spectrumFifoPosition = 0;
        }
    }

    const auto energyPerSample = static_cast<float>(blockEnergy / juce::jmax(1, numSamples));
    const auto currentLufs = -0.691f + 10.0f * std::log10(std::max(energyPerSample, 1.0e-12f));
    smoothedLoudness = smoothedLoudness < -59.0f
        ? currentLufs
        : 0.88f * smoothedLoudness + 0.12f * currentLufs;
    const auto peakNow = toDb(blockPeak);
    smoothedPeak = smoothedPeak < -59.0f ? peakNow : 0.84f * smoothedPeak + 0.16f * peakNow;
    loudnessEstimate.store(smoothedLoudness);
    peakDb.store(peakNow);

    if (collectingAnalysis)
    {
        analysisProgress.store(juce::jlimit(0.0f, 1.0f,
            static_cast<float>(analyzedSamples) /
            static_cast<float>(currentSampleRate * analysisDurationSeconds)));
        if (analyzedSamples >= static_cast<int64_t>(currentSampleRate * analysisDurationSeconds))
            finishAnalysis();
    }

}

void NorthstarMasteringAudioProcessor::copySpectrum(std::array<float, 48>& destination) const noexcept
{
    for (size_t i = 0; i < destination.size(); ++i)
        destination[i] = spectrumBins[i].load();
}

juce::AudioProcessorEditor* NorthstarMasteringAudioProcessor::createEditor()
{
    return new NorthstarMasteringAudioProcessorEditor(*this);
}

void NorthstarMasteringAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    auto state = parameters.copyState();
    state.setProperty("learnedLowDb", learnedLowDb.load(), nullptr);
    state.setProperty("learnedBodyDb", learnedBodyDb.load(), nullptr);
    state.setProperty("learnedHighDb", learnedHighDb.load(), nullptr);
    state.setProperty("learnedInputLufs", learnedInputLufs.load(), nullptr);
    state.setProperty("learnedCompression", learnedCompression.load(), nullptr);
    state.setProperty("analysisComplete", analysisComplete.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void NorthstarMasteringAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml(*xml);
            learnedLowDb.store(static_cast<float>(state.getProperty("learnedLowDb", 0.0f)));
            learnedBodyDb.store(static_cast<float>(state.getProperty("learnedBodyDb", 0.0f)));
            learnedHighDb.store(static_cast<float>(state.getProperty("learnedHighDb", 0.0f)));
            learnedInputLufs.store(static_cast<float>(state.getProperty("learnedInputLufs", -18.0f)));
            learnedCompression.store(static_cast<float>(state.getProperty("learnedCompression", 0.0f)));
            const auto complete = static_cast<bool>(state.getProperty("analysisComplete", false));
            analysisComplete.store(complete);
            parameters.replaceState(state);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NorthstarMasteringAudioProcessor();
}