#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAProcessor::LUMINAProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    updateParamCache();
}

LUMINAProcessor::~LUMINAProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout LUMINAProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "CROSS_1", 1 }, "Crossover 1",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 250.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "CROSS_2", 1 }, "Crossover 2",
        juce::NormalisableRange<float>(1000.0f, 20000.0f, 1.0f, 0.3f), 4000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "MS_MODE", 1 }, "M/S Mode", false,
        juce::AudioParameterBoolAttributes()));

    juce::StringArray bandPrefixes = { "B1_", "B2_", "B3_" };
    juce::StringArray bandNames = { "Low ", "Mid ", "High " };

    for (int i = 0; i < 3; ++i)
    {
        juce::String pfx = bandPrefixes[i];
        juce::String nm = bandNames[i];

        // --- Mid / Stereo Parameters ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "THRESH_M", 1 }, nm + "Thresh M",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "DEPTH_M", 1 }, nm + "Depth M",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
            juce::AudioParameterFloatAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TONAL_M", 1 }, nm + "Tonal M",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TRANS_M", 1 }, nm + "Trans M",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        // --- Side Parameters ---
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "THRESH_S", 1 }, nm + "Thresh S",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "DEPTH_S", 1 }, nm + "Depth S",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
            juce::AudioParameterFloatAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TONAL_S", 1 }, nm + "Tonal S",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TRANS_S", 1 }, nm + "Trans S",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        // --- Band Controls ---
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "SOLO", 1 }, nm + "Solo", false,
            juce::AudioParameterBoolAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "DELTA", 1 }, nm + "Delta", false,
            juce::AudioParameterBoolAttributes()));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "LINK", 1 }, nm + "Link M/S", true,
            juce::AudioParameterBoolAttributes()));
    }

    return { params.begin(), params.end() };
}

void LUMINAProcessor::updateParamCache()
{
    cache.cross1 = apvts.getRawParameterValue("CROSS_1");
    cache.cross2 = apvts.getRawParameterValue("CROSS_2");
    cache.msMode = apvts.getRawParameterValue("MS_MODE");

    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };
    for (int i = 0; i < 3; ++i) {
        cache.bands[i].threshM = apvts.getRawParameterValue(prefixes[i] + "THRESH_M");
        cache.bands[i].depthM = apvts.getRawParameterValue(prefixes[i] + "DEPTH_M");
        cache.bands[i].tonalM = apvts.getRawParameterValue(prefixes[i] + "TONAL_M");
        cache.bands[i].transM = apvts.getRawParameterValue(prefixes[i] + "TRANS_M");
        cache.bands[i].threshS = apvts.getRawParameterValue(prefixes[i] + "THRESH_S");
        cache.bands[i].depthS = apvts.getRawParameterValue(prefixes[i] + "DEPTH_S");
        cache.bands[i].tonalS = apvts.getRawParameterValue(prefixes[i] + "TONAL_S");
        cache.bands[i].transS = apvts.getRawParameterValue(prefixes[i] + "TRANS_S");
        cache.bands[i].solo = apvts.getRawParameterValue(prefixes[i] + "SOLO");
        cache.bands[i].delta = apvts.getRawParameterValue(prefixes[i] + "DELTA");
        cache.bands[i].link = apvts.getRawParameterValue(prefixes[i] + "LINK");
    }
}

const juce::String LUMINAProcessor::getName() const { return JucePlugin_Name; }
bool LUMINAProcessor::acceptsMidi() const { return false; }
bool LUMINAProcessor::producesMidi() const { return false; }
bool LUMINAProcessor::isMidiEffect() const { return false; }
double LUMINAProcessor::getTailLengthSeconds() const { return 0.0; }
int LUMINAProcessor::getNumPrograms() { return 1; }
int LUMINAProcessor::getCurrentProgram() { return 0; }
void LUMINAProcessor::setCurrentProgram(int index) {}
const juce::String LUMINAProcessor::getProgramName(int index) { return {}; }
void LUMINAProcessor::changeProgramName(int index, const juce::String& newName) {}

void LUMINAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int fftSize = 4096;
    const int numBins = fftSize / 2 + 1;
    const float overlapRatio = 0.75f;

    binToBarkMap.assign(static_cast<size_t>(numBins), 0);
    const float binFreq = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    for (int i = 0; i < numBins; ++i)
    {
        float hz = static_cast<float>(i) * binFreq;
        float bark = 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
        int barkIdx = static_cast<int>(std::floor(bark));
        binToBarkMap[i] = (barkIdx < 0) ? 0 : (barkIdx >= 24 ? 23 : barkIdx);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        spectralEngines[ch].prepare(sampleRate, fftSize, overlapRatio);
        hpssEngines[ch].prepare(numBins, 17, 31);
        maskingModels[ch].prepare(sampleRate, fftSize);
        onsetDetectors[ch].prepare(sampleRate, numBins);

        powerWorkspaces[ch].assign(static_cast<size_t>(numBins), 0.0f);
        tonalMaskWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);
        binGainsWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);

        spectralEngines[ch].setProcessingCallback([this, ch, fftSize, sampleRate](float* magnitudes, int numBins)
            {
                auto& hpss = hpssEngines[ch];
                auto& onset = onsetDetectors[ch];
                auto& masking = maskingModels[ch];
                float* power = powerWorkspaces[ch].data();
                float* tonalMask = tonalMaskWorkspaces[ch].data();
                float* binGains = binGainsWorkspaces[ch].data();

                // ⚡ SIMD最適化: パワー計算
                float normFactor = 1.0f / static_cast<float>(fftSize);
                juce::FloatVectorOperations::multiply(power, magnitudes, normFactor, numBins);
                juce::FloatVectorOperations::multiply(power, power, magnitudes, numBins);
                juce::FloatVectorOperations::multiply(power, power, normFactor, numBins);

                hpss.process(power, tonalMask, numBins);
                bool isOnset = onset.detectOnset(power, numBins);

                auto barkPower = masking.calcBarkPower(power, numBins);
                auto maskThresh = masking.calcMaskingThreshold(barkPower);
                auto tonalRatio = masking.calcBarkTonalRatio(tonalMask, numBins);

                const bool isMsMode = cache.msMode->load() > 0.5f;
                const bool isSide = (isMsMode && ch == 1);
                const float cross1 = cache.cross1->load();
                const float cross2 = cache.cross2->load();

                std::array<BandParams, 3> bands;
                bool anySolo = false;

                // キャッシュから現在のチャンネル（M/S）に適したパラメータを読み込む
                for (int b = 0; b < 3; ++b) {
                    bool isLinked = cache.bands[b].link->load() > 0.5f;

                    if (isSide && !isLinked) {
                        bands[b].threshold = cache.bands[b].threshS->load();
                        bands[b].depth = cache.bands[b].depthS->load();
                        bands[b].tonalShift = juce::Decibels::decibelsToGain(cache.bands[b].tonalS->load());
                        bands[b].transShift = juce::Decibels::decibelsToGain(cache.bands[b].transS->load());
                    }
                    else {
                        bands[b].threshold = cache.bands[b].threshM->load();
                        bands[b].depth = cache.bands[b].depthM->load();
                        bands[b].tonalShift = juce::Decibels::decibelsToGain(cache.bands[b].tonalM->load());
                        bands[b].transShift = juce::Decibels::decibelsToGain(cache.bands[b].transM->load());
                    }

                    bands[b].isSolo = cache.bands[b].solo->load() > 0.5f;
                    bands[b].isDelta = cache.bands[b].delta->load() > 0.5f;
                    if (bands[b].isSolo) anySolo = true;
                }

                const float binFreq = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
                const float transRatio = 1.414f;

                std::array<float, 24> barkGR;
                barkGR.fill(1.0f);

                for (int i = 0; i < numBins; ++i)
                {
                    float hz = static_cast<float>(i) * binFreq;
                    int barkIdx = binToBarkMap[i];
                    float tRatio = tonalRatio[barkIdx];

                    float pureReductionGains[3];
                    float targetGains[3];

                    for (int b = 0; b < 3; ++b) {
                        float excess = barkPower[barkIdx] - maskThresh[barkIdx] - bands[b].threshold;
                        float reductionGain = 1.0f;

                        if (excess > 0.0f && !isOnset) {
                            float reductionDB = excess * bands[b].depth * tRatio;
                            reductionGain = juce::Decibels::decibelsToGain(-reductionDB);
                        }

                        pureReductionGains[b] = reductionGain;
                        float shiftMultiplier = (bands[b].tonalShift * tRatio) + (bands[b].transShift * (1.0f - tRatio));
                        targetGains[b] = reductionGain * shiftMultiplier;
                    }

                    float wLow = 1.0f, wMid = 0.0f, wHigh = 0.0f;

                    float c1_L = cross1 / transRatio;
                    float c1_U = cross1 * transRatio;
                    if (hz < c1_L) {
                        wLow = 1.0f; wMid = 0.0f;
                    }
                    else if (hz > c1_U) {
                        wLow = 0.0f; wMid = 1.0f;
                    }
                    else {
                        float t = (hz - c1_L) / (c1_U - c1_L);
                        float mix = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                        wLow = 1.0f - mix;
                        wMid = mix;
                    }

                    float c2_L = cross2 / transRatio;
                    float c2_U = cross2 * transRatio;
                    if (hz > c2_L) {
                        if (hz > c2_U) {
                            wMid = 0.0f; wHigh = 1.0f;
                        }
                        else {
                            float t = (hz - c2_L) / (c2_U - c2_L);
                            float mix = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                            wHigh = wMid * mix;
                            wMid = wMid * (1.0f - mix);
                        }
                    }

                    float blendedReduction = (pureReductionGains[0] * wLow) + (pureReductionGains[1] * wMid) + (pureReductionGains[2] * wHigh);
                    if (blendedReduction < barkGR[barkIdx]) {
                        barkGR[barkIdx] = blendedReduction;
                    }

                    float originalMag = magnitudes[i];
                    float finalGain = 0.0f;

                    if (anySolo) {
                        float soloGain = 0.0f;
                        if (bands[0].isSolo) soloGain += (bands[0].isDelta ? (1.0f - targetGains[0]) : targetGains[0]) * wLow;
                        if (bands[1].isSolo) soloGain += (bands[1].isDelta ? (1.0f - targetGains[1]) : targetGains[1]) * wMid;
                        if (bands[2].isSolo) soloGain += (bands[2].isDelta ? (1.0f - targetGains[2]) : targetGains[2]) * wHigh;

                        finalGain = soloGain;
                    }
                    else {
                        float g0 = bands[0].isDelta ? (1.0f - targetGains[0]) : targetGains[0];
                        float g1 = bands[1].isDelta ? (1.0f - targetGains[1]) : targetGains[1];
                        float g2 = bands[2].isDelta ? (1.0f - targetGains[2]) : targetGains[2];

                        finalGain = (g0 * wLow) + (g1 * wMid) + (g2 * wHigh);
                    }

                    magnitudes[i] = originalMag * finalGain;
                    binGains[i] = finalGain;
                }

                // GUIへ送るためのデータ更新 (Channel 0をマスターとして描画)
                if (ch == 0) {
                    AnalysisFrame frame;
                    frame.isOnset = isOnset;
                    frame.barkPower = barkPower;

                    for (int i = 0; i < 24; ++i) {
                        frame.barkGainReduction[i] = barkGR[i];
                    }

                    const int binsPerUI = numBins / 512;
                    for (int i = 0; i < 512; ++i) {
                        float sumP = 0.0f;
                        int startIdx = i * binsPerUI;
                        for (int j = 0; j < binsPerUI; ++j) {
                            int idx = startIdx + j;
                            if (idx < numBins) {
                                float rawMag = (anySolo || bands[0].isDelta || bands[1].isDelta || bands[2].isDelta)
                                    ? magnitudes[idx]
                                    : (magnitudes[idx] / (binGains[idx] + 1e-6f));
                                sumP += rawMag * rawMag;
                            }
                        }
                        frame.magnitudeSpectrum[static_cast<size_t>(i)] = std::sqrt(sumP / static_cast<float>(binsPerUI) + 1e-12f);
                    }
                    analysisFifo.push(frame);
                }
            });
    }
    setLatencySamples(spectralEngines[0].getLatencySamples());
}

void LUMINAProcessor::releaseResources() {}

bool LUMINAProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void LUMINAProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    const bool isMsMode = cache.msMode->load() > 0.5f;

    if (isMsMode) msEncoder.encodeMidSide(buffer);

    if (buffer.getNumChannels() > 0) {
        float* chData0 = buffer.getWritePointer(0);
        spectralEngines[0].process(chData0, chData0, numSamples);
    }

    if (buffer.getNumChannels() > 1) {
        float* chData1 = buffer.getWritePointer(1);
        spectralEngines[1].process(chData1, chData1, numSamples);
    }

    if (isMsMode) msEncoder.decodeMidSide(buffer);
}

bool LUMINAProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* LUMINAProcessor::createEditor() { return new LUMINAEditor(*this); }

void LUMINAProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LUMINAProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LUMINAProcessor(); }