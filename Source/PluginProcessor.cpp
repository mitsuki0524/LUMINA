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

    // --- Global Pro Params ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "PRO_MODE", 1 }, "Pro Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "OVERSAMPLING", 1 }, "Oversampling", juce::StringArray{ "Off", "2x", "4x" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "LOOKAHEAD", 1 }, "Lookahead (ms)", 0.0f, 10.0f, 0.0f));

    // ⚡ デフォルトは現在のハードコーディング値（200Hz, 5000Hz）と完全に一致
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "WIDTH_CROSS_1", 1 }, "Width Low Freq", 20.0f, 1000.0f, 200.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "WIDTH_CROSS_2", 1 }, "Width High Freq", 1000.0f, 20000.0f, 5000.0f));

    // --- Normal Global Params ---
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

        // ⚡ --- Band Pro Params --- (デフォルトは現在のDSPと完全に一致)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TAME_SHARP", 1 }, nm + "Tame Sharp", 0.1f, 10.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "TAME_SPEED", 1 }, nm + "Tame Speed", 0.1f, 0.99f, 0.92f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "ATTACK_M", 1 }, nm + "Attack M", 0.0f, 100.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "ATTACK_S", 1 }, nm + "Attack S", 0.0f, 100.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "RELEASE_M", 1 }, nm + "Release M", 0.0f, 500.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "RELEASE_S", 1 }, nm + "Release S", 0.0f, 500.0f, 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "HPSS_BLUR", 1 }, nm + "HPSS Blur", 0.1f, 2.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "HPSS_RES", 1 }, nm + "HPSS Res", 3.0f, 65.0f, 17.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ pfx + "LINK_AMT", 1 }, nm + "Link Amt", 0.0f, 1.0f, 1.0f));
    }

    return { params.begin(), params.end() };
}

void LUMINAProcessor::updateParamCache()
{
    cache.proMode = apvts.getRawParameterValue("PRO_MODE");
    cache.oversampling = apvts.getRawParameterValue("OVERSAMPLING");
    cache.lookahead = apvts.getRawParameterValue("LOOKAHEAD");
    cache.widthCross1 = apvts.getRawParameterValue("WIDTH_CROSS_1");
    cache.widthCross2 = apvts.getRawParameterValue("WIDTH_CROSS_2");

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
        cache.bands[i].tame = apvts.getRawParameterValue(prefixes[i] + "TAME");
        cache.bands[i].width = apvts.getRawParameterValue(prefixes[i] + "WIDTH");
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

        cache.bands[i].tameSharp = apvts.getRawParameterValue(prefixes[i] + "TAME_SHARP");
        cache.bands[i].tameSpeed = apvts.getRawParameterValue(prefixes[i] + "TAME_SPEED");
        cache.bands[i].attackM = apvts.getRawParameterValue(prefixes[i] + "ATTACK_M");
        cache.bands[i].attackS = apvts.getRawParameterValue(prefixes[i] + "ATTACK_S");
        cache.bands[i].releaseM = apvts.getRawParameterValue(prefixes[i] + "RELEASE_M");
        cache.bands[i].releaseS = apvts.getRawParameterValue(prefixes[i] + "RELEASE_S");
        cache.bands[i].hpssBlur = apvts.getRawParameterValue(prefixes[i] + "HPSS_BLUR");
        cache.bands[i].hpssRes = apvts.getRawParameterValue(prefixes[i] + "HPSS_RES");
        cache.bands[i].linkAmt = apvts.getRawParameterValue(prefixes[i] + "LINK_AMT");
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

    dryDelayBuffer.setSize(2, mFftSize + samplesPerBlock + 1024);
    dryDelayBuffer.clear();
    delayWritePosition = 0;

    analyzerCore.prepare(sampleRate, mFftSize, hopSize);
    widthEngine.prepare(sampleRate, samplesPerBlock);

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
        tameGainsWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);

        spectralEngines[ch].setProcessingCallback([this, ch, numBins](float* magnitudes, int nBins)
            {
                auto& hpss = hpssEngines[ch];
                auto& onset = onsetDetectors[ch];
                auto& masking = maskingModels[ch];
                float* power = powerWorkspaces[ch].data();
                float* tonalMask = tonalMaskWorkspaces[ch].data();
                float* binGains = binGainsWorkspaces[ch].data();
                float* binTameGains = tameGainsWorkspaces[ch].data();

                float normFactor = 2.0f / static_cast<float>(mFftSize);
                std::vector<float> rawMags(nBins, 0.0f);
                for (int i = 0; i < nBins; ++i) {
                    rawMags[i] = magnitudes[i] * normFactor;
                }

                juce::FloatVectorOperations::multiply(power, magnitudes, 1.0f / static_cast<float>(mFftSize), nBins);
                juce::FloatVectorOperations::multiply(power, power, magnitudes, nBins);
                juce::FloatVectorOperations::multiply(power, power, 1.0f / static_cast<float>(mFftSize), nBins);

                if (ch == 0) analyzerCore.processSpectrumFrame(power, nBins);

                hpss.process(power, tonalMask, nBins);
                bool isOnset = onset.detectOnset(power, nBins);
                auto barkPower = masking.calcBarkPower(power, nBins);
                auto maskThresh = masking.calcMaskingThreshold(barkPower);
                auto tonalRatio = masking.calcBarkTonalRatio(tonalMask, nBins);

                const bool isMsMode = cache.msMode->load() > 0.5f;
                const bool isSide = (isMsMode && ch == 1);
                const float cross1 = std::max(20.0f, cache.cross1->load());
                const float cross2 = std::max(20.0f, cache.cross2->load());

                std::array<BandParams, 3> bands{};
                bool anySolo = false;

                // ⚡ Pro パラメータを安全に読み込み（複雑な時間軸処理は将来のフェーズで完全統合）
                float tameSharp[3], hpssBlur[3], hpssRes[3], atk[3], rel[3], linkAmt[3];

                for (int b = 0; b < 3; ++b) {
                    bool isLinked = cache.bands[b].link->load() > 0.5f;
                    bands[b].tame = cache.bands[b].tame->load();

                    if (isSide && !isLinked) {
                        bands[b].threshold = cache.bands[b].threshS->load();
                        bands[b].depth = cache.bands[b].depthS->load();
                        bands[b].tonalShift = juce::Decibels::decibelsToGain(cache.bands[b].tonalS->load());
                        bands[b].transShift = juce::Decibels::decibelsToGain(cache.bands[b].transS->load());
                        atk[b] = cache.bands[b].attackS->load();
                        rel[b] = cache.bands[b].releaseS->load();
                    }
                    else {
                        bands[b].threshold = cache.bands[b].threshM->load();
                        bands[b].depth = cache.bands[b].depthM->load();
                        bands[b].tonalShift = juce::Decibels::decibelsToGain(cache.bands[b].tonalM->load());
                        bands[b].transShift = juce::Decibels::decibelsToGain(cache.bands[b].transM->load());
                        atk[b] = cache.bands[b].attackM->load();
                        rel[b] = cache.bands[b].releaseM->load();
                    }
                    bands[b].isBypass = cache.bands[b].bypass->load() > 0.5f;
                    bands[b].isSolo = cache.bands[b].solo->load() > 0.5f;
                    bands[b].isDelta = cache.bands[b].delta->load() > 0.5f;
                    if (bands[b].isSolo) anySolo = true;

                    tameSharp[b] = cache.bands[b].tameSharp->load();
                    hpssBlur[b] = cache.bands[b].hpssBlur->load();
                    hpssRes[b] = cache.bands[b].hpssRes->load();
                    linkAmt[b] = cache.bands[b].linkAmt->load();
                    juce::ignoreUnused(hpssBlur[b], hpssRes[b], atk[b], rel[b], linkAmt[b]); // 今回のフェーズでの未使用警告回避
                }

                // ⚡ Tame Speed (Envelope Alpha) - ゼロ位相の安定性のためMidバンドのSpeedを共有
                float smoothAlpha = cache.bands[1].tameSpeed->load();

                std::vector<float> env(static_cast<size_t>(nBins), 0.0f);
                env[0] = rawMags[0];
                for (int i = 1; i < nBins; ++i) {
                    env[i] = env[i - 1] * smoothAlpha + rawMags[i] * (1.0f - smoothAlpha);
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
                    float hz = std::max(1.0f, static_cast<float>(i) * bFreq);
                    int barkIdx = binToBarkMap[i];
                    float tRatio = tonalRatio[barkIdx];

                    float wLow = 1.0f, wMid = 0.0f, wHigh = 0.0f;
                    float logHz = std::log2(hz);

                    float logC1_L = std::log2(cross1 / transRatio);
                    float logC1_U = std::log2(cross1 * transRatio);
                    if (logHz <= logC1_L) { wLow = 1.0f; wMid = 0.0f; }
                    else if (logHz >= logC1_U) { wLow = 0.0f; wMid = 1.0f; }
                    else {
                        float t = (logHz - logC1_L) / (logC1_U - logC1_L);
                        float mix = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                        wLow = 1.0f - mix; wMid = mix;
                    }

                    float logC2_L = std::log2(cross2 / transRatio);
                    float logC2_U = std::log2(cross2 * transRatio);
                    if (logHz > logC2_L) {
                        if (logHz >= logC2_U) { wMid = 0.0f; wHigh = 1.0f; }
                        else {
                            float t = (logHz - logC2_L) / (logC2_U - logC2_L);
                            float mix = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
                            wHigh = wMid * mix; wMid = wMid * (1.0f - mix);
                        }
                    }

                    float effTame0 = bands[0].isBypass ? 0.0f : bands[0].tame;
                    float effTame1 = bands[1].isBypass ? 0.0f : bands[1].tame;
                    float effTame2 = bands[2].isBypass ? 0.0f : bands[2].tame;
                    float currentTame = (effTame0 * wLow) + (effTame1 * wMid) + (effTame2 * wHigh);

                    // ⚡ Tame Sharpness の完璧な統合
                    float currentTameSharp = (tameSharp[0] * wLow) + (tameSharp[1] * wMid) + (tameSharp[2] * wHigh);
                    float protectedTame = currentTame * tRatio * currentTameSharp;

                    float tGain = 1.0f;
                    if (protectedTame > 0.0f && rawMags[i] > env[i] && env[i] > 1e-12f) {
                        float peakRatio = rawMags[i] / env[i];
                        tGain = 1.0f / (1.0f + (peakRatio - 1.0f) * protectedTame);
                    }
                    binTameGains[i] = tGain;

                    float dGain0 = 1.0f, dGain1 = 1.0f, dGain2 = 1.0f;
                    float pureReductions[3] = { 1.0f, 1.0f, 1.0f };

                    auto calcDynEQ = [&](int b, float& dGain, float& pureRed) {
                        if (bands[b].isBypass) return;
                        float excess = barkPower[barkIdx] - maskThresh[barkIdx] - bands[b].threshold;
                        if (excess > 0.0f && !isOnset) {
                            float reductionDB = excess * bands[b].depth * tRatio;
                            pureRed = juce::Decibels::decibelsToGain(-reductionDB);
                        }
                        float shiftMult = (bands[b].tonalShift * tRatio) + (bands[b].transShift * (1.0f - tRatio));
                        dGain = pureRed * shiftMult;
                        };
                    calcDynEQ(0, dGain0, pureReductions[0]);
                    calcDynEQ(1, dGain1, pureReductions[1]);
                    calcDynEQ(2, dGain2, pureReductions[2]);

                    float totalGain0 = tGain * dGain0;
                    float totalGain1 = tGain * dGain1;
                    float totalGain2 = tGain * dGain2;

                    if (bands[0].isDelta) totalGain0 = 1.0f - totalGain0;
                    if (bands[1].isDelta) totalGain1 = 1.0f - totalGain1;
                    if (bands[2].isDelta) totalGain2 = 1.0f - totalGain2;

                    if (anySolo) {
                        if (!bands[0].isSolo) totalGain0 = 0.0f;
                        if (!bands[1].isSolo) totalGain1 = 0.0f;
                        if (!bands[2].isSolo) totalGain2 = 0.0f;
                    }

                    float finalGain = (totalGain0 * wLow) + (totalGain1 * wMid) + (totalGain2 * wHigh);

                    magnitudes[i] *= finalGain;
                    binGains[i] = finalGain;

                    float blendedReduction = (pureReductions[0] * wLow) + (pureReductions[1] * wMid) + (pureReductions[2] * wHigh);
                    if (blendedReduction < barkGR[barkIdx]) barkGR[barkIdx] = blendedReduction;
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
                        float sumPost = 0.0f;
                        float sumPre = 0.0f;
                        float minTame = 1.0f;

                        int startIdx = i * binsPerUI;
                        for (int j = 0; j < binsPerUI; ++j) {
                            int idx = startIdx + j;
                            if (idx < nBins) {
                                sumPre += rawMags[idx] * rawMags[idx];
                                float postMag = rawMags[idx] * binGains[idx];
                                sumPost += postMag * postMag;
                                if (binTameGains[idx] < minTame) minTame = binTameGains[idx];
                            }
                        }
                        frame.unprocessedSpectrum[i] = std::sqrt(sumPre / binsPerUI + 1e-12f);
                        frame.magnitudeSpectrum[i] = std::sqrt(sumPost / binsPerUI + 1e-12f);
                        frame.tameSpectrum[i] = minTame;
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

    // ⚡ Global Pro 設定の読み込み準備（UIと状態を同期）
    int oversamplingSetting = static_cast<int>(cache.oversampling->load());
    float lookaheadMs = cache.lookahead->load();
    juce::ignoreUnused(oversamplingSetting, lookaheadMs);

    const bool isMsMode = cache.msMode->load() > 0.5f;
    if (isMsMode) msEncoder.encodeMidSide(buffer);

    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        spectralEngines[ch].process(data, data, numSamples);
    }

    if (isMsMode) msEncoder.decodeMidSide(buffer);

    // ⚡ Width エンジンの独立クロスオーバー適用
    float wLowFreq = cache.widthCross1->load();
    float wHighFreq = cache.widthCross2->load();
    widthEngine.setCutoffs(wLowFreq, wHighFreq);

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
    int latency = spectralEngines[0].getLatencySamples();
    int delayBufferSize = dryDelayBuffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch) {
        const float* dryData = inputCopyBuffer.getReadPointer(ch);
        float* wetData = buffer.getWritePointer(ch);
        float* delayData = dryDelayBuffer.getWritePointer(ch);

        int tempWritePos = delayWritePosition;
        int readPos = tempWritePos - latency;
        if (readPos < 0) readPos += delayBufferSize;

        for (int i = 0; i < numSamples; ++i) {
            delayData[tempWritePos] = dryData[i];
            float delayedDry = delayData[readPos];

            if (wetRatio < 1.0f) {
                wetData[i] = delayedDry * (1.0f - wetRatio) + wetData[i] * wetRatio;
            }

            tempWritePos++;
            if (tempWritePos >= delayBufferSize) tempWritePos = 0;

            readPos++;
            if (readPos >= delayBufferSize) readPos = 0;
        }

        if (ch == numChannels - 1) {
            delayWritePosition = tempWritePos;
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