// ==========================================
// Source/PluginProcessor.cpp
// ==========================================
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

    auto hzRange = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "CROSS_1", 1 }, "Crossover 1", hzRange, 250.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "CROSS_2", 1 }, "Crossover 2", hzRange, 4000.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "MS_MODE", 1 }, "M/S Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "MASTER_IN", 1 }, "Input Gain", -24.0f, 24.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "MASTER_OUT", 1 }, "Output Gain", -24.0f, 24.0f, 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "MASTER_WET", 1 }, "Dry/Wet", 0.0f, 1.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "AUTO_BAND_TIME", 1 }, "Auto Band Time",
        juce::StringArray{ "3s", "10s", "30s" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "AUTO_LEVEL", 1 }, "Auto Level", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "AUTO_BAND", 1 }, "Auto Band Trigger", false));

    juce::StringArray bandPrefixes = { "B1_", "B2_", "B3_" };
    juce::StringArray bandNames = { "Low ", "Mid ", "High " };

    for (int i = 0; i < 3; ++i)
    {
        juce::String pfx = bandPrefixes[i];
        juce::String nm = bandNames[i];

        // ⚡ 追加: TAME & WIDTH
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TAME", 1 }, nm + "Tame", 0.0f, 1.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "WIDTH", 1 }, nm + "Width", 0.0f, 2.0f, 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "THRESH_M", 1 }, nm + "Thresh M", -60.0f, 0.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "DEPTH_M", 1 }, nm + "Depth M", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TONAL_M", 1 }, nm + "Tonal M", -24.0f, 24.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TRANS_M", 1 }, nm + "Trans M", -24.0f, 24.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "THRESH_S", 1 }, nm + "Thresh S", -60.0f, 0.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "DEPTH_S", 1 }, nm + "Depth S", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TONAL_S", 1 }, nm + "Tonal S", -24.0f, 24.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TRANS_S", 1 }, nm + "Trans S", -24.0f, 24.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "BYPASS", 1 }, nm + "Bypass", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "SOLO", 1 }, nm + "Solo", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "DELTA", 1 }, nm + "Delta", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ pfx + "LINK", 1 }, nm + "Link", true));
    }

    return { params.begin(), params.end() };
}

void LUMINAProcessor::updateParamCache()
{
    cache.cross1 = apvts.getRawParameterValue("CROSS_1");
    cache.cross2 = apvts.getRawParameterValue("CROSS_2");
    cache.msMode = apvts.getRawParameterValue("MS_MODE");
    cache.autoLevel = apvts.getRawParameterValue("AUTO_LEVEL");
    cache.autoBandTrigger = apvts.getRawParameterValue("AUTO_BAND");

    cache.masterInGain = apvts.getRawParameterValue("MASTER_IN");
    cache.masterOutGain = apvts.getRawParameterValue("MASTER_OUT");
    cache.masterDryWet = apvts.getRawParameterValue("MASTER_WET");
    cache.autoBandTime = apvts.getRawParameterValue("AUTO_BAND_TIME");

    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };
    for (int i = 0; i < 3; ++i) {
        cache.bands[i].tame = apvts.getRawParameterValue(prefixes[i] + "TAME");   // ⚡ 追加
        cache.bands[i].width = apvts.getRawParameterValue(prefixes[i] + "WIDTH"); // ⚡ 追加
        cache.bands[i].threshM = apvts.getRawParameterValue(prefixes[i] + "THRESH_M");
        cache.bands[i].depthM = apvts.getRawParameterValue(prefixes[i] + "DEPTH_M");
        cache.bands[i].tonalM = apvts.getRawParameterValue(prefixes[i] + "TONAL_M");
        cache.bands[i].transM = apvts.getRawParameterValue(prefixes[i] + "TRANS_M");
        cache.bands[i].threshS = apvts.getRawParameterValue(prefixes[i] + "THRESH_S");
        cache.bands[i].depthS = apvts.getRawParameterValue(prefixes[i] + "DEPTH_S");
        cache.bands[i].tonalS = apvts.getRawParameterValue(prefixes[i] + "TONAL_S");
        cache.bands[i].transS = apvts.getRawParameterValue(prefixes[i] + "TRANS_S");

        cache.bands[i].bypass = apvts.getRawParameterValue(prefixes[i] + "BYPASS");
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
    mSampleRate = sampleRate;

    if (sampleRate >= 176400.0)      mFftSize = 16384;
    else if (sampleRate >= 88200.0)  mFftSize = 8192;
    else                             mFftSize = 4096;

    const int numBins = mFftSize / 2 + 1;
    const int hopSize = mFftSize / 4;

    inputCopyBuffer.setSize(2, samplesPerBlock);
    analyzerCore.prepare(sampleRate, mFftSize, hopSize);
    widthEngine.prepare(sampleRate, samplesPerBlock); // ⚡ 追加

    binToBarkMap.assign(static_cast<size_t>(numBins), 0);
    const float binFreq = static_cast<float>(sampleRate) / static_cast<float>(mFftSize);
    for (int i = 0; i < numBins; ++i)
    {
        float hz = static_cast<float>(i) * binFreq;
        float bark = 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
        int barkIdx = static_cast<int>(std::floor(bark));
        binToBarkMap[i] = (barkIdx < 0) ? 0 : (barkIdx >= 24 ? 23 : barkIdx);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        spectralEngines[ch].prepare(sampleRate, mFftSize, 0.75f);
        hpssEngines[ch].prepare(numBins, 17, 31);
        maskingModels[ch].prepare(sampleRate, mFftSize);
        onsetDetectors[ch].prepare(sampleRate, numBins);

        powerWorkspaces[ch].assign(static_cast<size_t>(numBins), 0.0f);
        tonalMaskWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);
        binGainsWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);

        spectralEngines[ch].setProcessingCallback([this, ch, numBins](float* magnitudes, int nBins)
            {
                auto& hpss = hpssEngines[ch];
                auto& onset = onsetDetectors[ch];
                auto& masking = maskingModels[ch];
                float* power = powerWorkspaces[ch].data();
                float* tonalMask = tonalMaskWorkspaces[ch].data();
                float* binGains = binGainsWorkspaces[ch].data();

                float normFactor = 1.0f / static_cast<float>(mFftSize);
                juce::FloatVectorOperations::multiply(power, magnitudes, normFactor, nBins);
                juce::FloatVectorOperations::multiply(power, power, magnitudes, nBins);
                juce::FloatVectorOperations::multiply(power, power, normFactor, nBins);

                if (ch == 0) analyzerCore.processSpectrumFrame(power, nBins);

                hpss.process(power, tonalMask, nBins);
                bool isOnset = onset.detectOnset(power, nBins);
                auto barkPower = masking.calcBarkPower(power, nBins);
                auto maskThresh = masking.calcMaskingThreshold(barkPower);
                auto tonalRatio = masking.calcBarkTonalRatio(tonalMask, nBins);

                const bool isMsMode = cache.msMode->load() > 0.5f;
                const bool isSide = (isMsMode && ch == 1);
                const float cross1 = cache.cross1->load();
                const float cross2 = cache.cross2->load();

                std::array<BandParams, 3> bands{};
                bool anySolo = false;

                for (int b = 0; b < 3; ++b) {
                    bool isLinked = cache.bands[b].link->load() > 0.5f;
                    bands[b].tame = cache.bands[b].tame->load(); // ⚡

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
                    bands[b].isBypass = cache.bands[b].bypass->load() > 0.5f;
                    bands[b].isSolo = cache.bands[b].solo->load() > 0.5f;
                    bands[b].isDelta = cache.bands[b].delta->load() > 0.5f;
                    if (bands[b].isSolo) anySolo = true;
                }

                // ⚡ MICRO: Tame (Resonance Suppression) 用のゼロ位相エンベロープ構築
                std::vector<float> env(static_cast<size_t>(nBins), 0.0f);
                float smoothAlpha = 0.92f;
                env[0] = magnitudes[0];
                for (int i = 1; i < nBins; ++i) {
                    env[i] = env[i - 1] * smoothAlpha + magnitudes[i] * (1.0f - smoothAlpha);
                }
                for (int i = nBins - 2; i >= 0; --i) {
                    env[i] = env[i + 1] * smoothAlpha + env[i] * (1.0f - smoothAlpha);
                }

                const float bFreq = static_cast<float>(this->mSampleRate) / static_cast<float>(mFftSize);
                const float transRatio = 1.414f;
                std::array<float, 24> barkGR;
                barkGR.fill(1.0f);

                for (int i = 0; i < nBins; ++i)
                {
                    float hz = static_cast<float>(i) * bFreq;
                    int barkIdx = binToBarkMap[i];
                    float tRatio = tonalRatio[barkIdx];

                    float pureReductionGains[3] = { 1.0f, 1.0f, 1.0f };
                    float targetGains[3] = { 1.0f, 1.0f, 1.0f };

                    for (int b = 0; b < 3; ++b) {
                        if (bands[b].isBypass) {
                            pureReductionGains[b] = 1.0f;
                            targetGains[b] = 1.0f;
                            continue;
                        }

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
                    if (hz < c1_L) { wLow = 1.0f; wMid = 0.0f; }
                    else if (hz > c1_U) { wLow = 0.0f; wMid = 1.0f; }
                    else {
                        float t = (hz - c1_L) / (c1_U - c1_L);
                        float mix = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                        wLow = 1.0f - mix; wMid = mix;
                    }

                    float c2_L = cross2 / transRatio;
                    float c2_U = cross2 * transRatio;
                    if (hz > c2_L) {
                        if (hz > c2_U) { wMid = 0.0f; wHigh = 1.0f; }
                        else {
                            float t = (hz - c2_L) / (c2_U - c2_L);
                            float mix = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                            wHigh = wMid * mix; wMid = wMid * (1.0f - mix);
                        }
                    }

                    float originalMag = magnitudes[i];
                    float finalGain = 1.0f;

                    // ⚡ TAME 処理 (Sootheアルゴリズム)
                    float currentTame = (bands[0].tame * wLow) + (bands[1].tame * wMid) + (bands[2].tame * wHigh);
                    if (currentTame > 0.0f && originalMag > env[i] && env[i] > 1e-12f) {
                        float peakRatio = originalMag / env[i];
                        float tameGain = 1.0f / (1.0f + (peakRatio - 1.0f) * currentTame * 3.0f);
                        finalGain *= tameGain;
                        originalMag *= tameGain; // クリーンな音を次の処理に渡す
                    }

                    float blendedReduction = (pureReductionGains[0] * wLow) + (pureReductionGains[1] * wMid) + (pureReductionGains[2] * wHigh);
                    if (blendedReduction < barkGR[barkIdx]) barkGR[barkIdx] = blendedReduction;

                    if (anySolo) {
                        float soloGain = 0.0f;
                        if (bands[0].isSolo) soloGain += (bands[0].isDelta ? (1.0f - targetGains[0]) : targetGains[0]) * wLow;
                        if (bands[1].isSolo) soloGain += (bands[1].isDelta ? (1.0f - targetGains[1]) : targetGains[1]) * wMid;
                        if (bands[2].isSolo) soloGain += (bands[2].isDelta ? (1.0f - targetGains[2]) : targetGains[2]) * wHigh;
                        finalGain *= soloGain;
                    }
                    else {
                        float g0 = bands[0].isDelta ? (1.0f - targetGains[0]) : targetGains[0];
                        float g1 = bands[1].isDelta ? (1.0f - targetGains[1]) : targetGains[1];
                        float g2 = bands[2].isDelta ? (1.0f - targetGains[2]) : targetGains[2];
                        finalGain *= (g0 * wLow) + (g1 * wMid) + (g2 * wHigh);
                    }
                    magnitudes[i] = originalMag * finalGain;
                    binGains[i] = finalGain;
                }

                if (ch == 0) {
                    handleAutoBandResult();
                    AnalysisFrame frame;
                    frame.isOnset = isOnset;
                    frame.barkPower = barkPower;
                    for (int i = 0; i < 24; ++i) frame.barkGainReduction[i] = barkGR[i];

                    int soloBandIdx = -1;
                    if (bands[0].isSolo) soloBandIdx = 0;
                    else if (bands[1].isSolo) soloBandIdx = 1;
                    else if (bands[2].isSolo) soloBandIdx = 2;
                    frame.activeSoloBand = soloBandIdx;

                    const int uiBins = 512;
                    const int binsPerUI = nBins / uiBins;
                    for (int i = 0; i < uiBins; ++i) {
                        float sumP = 0.0f;
                        int startIdx = i * binsPerUI;
                        for (int j = 0; j < binsPerUI; ++j) {
                            int idx = startIdx + j;
                            if (idx < nBins) {
                                float rawMag = (anySolo || bands[0].isDelta || bands[1].isDelta || bands[2].isDelta)
                                    ? magnitudes[idx] : (magnitudes[idx] / (binGains[idx] + 1e-6f));
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

void LUMINAProcessor::handleAutoBandResult()
{
    if (analyzerCore.getAutoBandState() == AnalyzerCore::State::Done) {
        float c1 = analyzerCore.getProposedCross1();
        float c2 = analyzerCore.getProposedCross2();
        apvts.getParameter("CROSS_1")->setValueNotifyingHost(apvts.getParameter("CROSS_1")->convertTo0to1(c1));
        apvts.getParameter("CROSS_2")->setValueNotifyingHost(apvts.getParameter("CROSS_2")->convertTo0to1(c2));
        apvts.getRawParameterValue("AUTO_BAND")->store(0.0f);
        analyzerCore.resetToIdle();
    }
}

void LUMINAProcessor::releaseResources() { inputCopyBuffer.setSize(0, 0); }

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
    const int numChannels = buffer.getNumChannels();

    float inGain = juce::Decibels::decibelsToGain(cache.masterInGain->load());
    buffer.applyGain(inGain);

    for (int ch = 0; ch < numChannels; ++ch) {
        inputCopyBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    if (cache.autoBandTrigger->load() > 0.5f) {
        auto state = analyzerCore.getAutoBandState();
        if (state == AnalyzerCore::State::Idle || state == AnalyzerCore::State::Done) {
            float timeChoice = cache.autoBandTime->load();
            float timeSec = (timeChoice == 0.0f) ? 3.0f : ((timeChoice == 1.0f) ? 10.0f : 30.0f);
            analyzerCore.startAutoBand(timeSec);
        }
    }

    // --- FFT Processing (Tame & Dynamics) ---
    const bool isMsMode = cache.msMode->load() > 0.5f;
    if (isMsMode) msEncoder.encodeMidSide(buffer);

    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        spectralEngines[ch].process(data, data, numSamples);
    }

    if (isMsMode) msEncoder.decodeMidSide(buffer);

    // ⚡ 追加: Stereo Width Processing (時間領域の絶対周波数処理)
    // 常にL/Rドメインで処理されるため、MS/Stereoモードを問わず完璧に動作します
    float wLow = cache.bands[0].width->load();
    float wMid = cache.bands[1].width->load();
    float wHigh = cache.bands[2].width->load();
    if (std::abs(wLow - 1.0f) > 0.01f || std::abs(wMid - 1.0f) > 0.01f || std::abs(wHigh - 1.0f) > 0.01f) {
        widthEngine.processStereoWidth(buffer, wLow, wMid, wHigh);
    }

    analyzerCore.setAutoLevelActive(cache.autoLevel->load() > 0.5f);
    analyzerCore.processAudioBlock(inputCopyBuffer.getArrayOfReadPointers(),
        buffer.getArrayOfWritePointers(),
        numChannels, numSamples);

    float wetRatio = cache.masterDryWet->load();
    if (wetRatio < 1.0f) {
        for (int ch = 0; ch < numChannels; ++ch) {
            const float* dryData = inputCopyBuffer.getReadPointer(ch);
            float* wetData = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                wetData[i] = dryData[i] * (1.0f - wetRatio) + wetData[i] * wetRatio;
            }
        }
    }

    float outGain = juce::Decibels::decibelsToGain(cache.masterOutGain->load());
    buffer.applyGain(outGain);
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