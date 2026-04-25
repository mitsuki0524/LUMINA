#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LUMINAProcessor::LUMINAProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

LUMINAProcessor::~LUMINAProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LUMINAProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "THRESHOLD", 1 }, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "DEPTH", 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.5f), 10.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "RELEASE", 1 }, "Release",
        juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.5f), 50.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "LINEAR_PHASE", 1 }, "Linear Phase", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "MS_MODE", 1 }, "M/S Mode", false));

    return { params.begin(), params.end() };
}

//==============================================================================
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

//==============================================================================
void LUMINAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    stft.prepare(sampleRate);

    const int maxBins = 4096 / 2 + 1;
    hpss.prepare(maxBins, 17, 31);
    maskingModel.prepare(sampleRate, 4096);
    onsetDetector.prepare(sampleRate, maxBins);

    tonalMaskWorkspace.assign(static_cast<size_t>(maxBins), 1.0f);

    // オーディオスレッドでのメモリ再確保を防ぐため、余裕を持たせたバッファを事前確保
    monoAnalysisBuffer.assign(8192, 0.0f);

    gainSmooth.reset(sampleRate, 0.023); // 約23msの平滑化
    gainSmooth.setCurrentAndTargetValue(1.0f);

    setLatencySamples(stft.getLatencySamples());
}

void LUMINAProcessor::releaseResources()
{
}

bool LUMINAProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

//==============================================================================
void LUMINAProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // RMS ゲートによる不要な解析のスキップ
    float rms = buffer.getRMSLevel(0, 0, numSamples);
    if (rms < 1e-6f)
    {
        gainSmooth.setTargetValue(1.0f);
        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) data[i] *= gainSmooth.getNextValue();
        }
        return;
    }

    const bool isMsMode = *apvts.getRawParameterValue("MS_MODE") > 0.5f;
    const float threshold = *apvts.getRawParameterValue("THRESHOLD");
    const float depth = *apvts.getRawParameterValue("DEPTH");

    if (isMsMode) msEncoder.encodeMidSide(buffer);

    // --- 解析用バッファの合成 (ステレオリンク または Mid抽出) ---
    float* analysisPtr = monoAnalysisBuffer.data();
    const float* ch0 = buffer.getReadPointer(0);

    if (totalNumInputChannels > 1) {
        const float* ch1 = buffer.getReadPointer(1);
        for (int i = 0; i < numSamples; ++i) {
            // LとRの平均をとり、位相と波形を安全に維持したまま両方のピークを捉える
            analysisPtr[i] = (ch0[i] + ch1[i]) * 0.5f;
        }
    }
    else {
        for (int i = 0; i < numSamples; ++i) {
            analysisPtr[i] = ch0[i];
        }
    }

    bool newFrameReady = stft.process(analysisPtr, numSamples);

    if (newFrameReady)
    {
        stft.updateMergedPower();
        const int numBins = stft.bands[0].fftSize / 2 + 1;
        const float* currentPower = stft.getMergedPowerPointer();

        // HPSS分離とトランジェント検知
        hpss.process(currentPower, tonalMaskWorkspace.data(), numBins);
        bool isOnset = onsetDetector.detectOnset(currentPower, numBins);

        // 心理音響計算とTonal Ratioの集約
        auto barkPower = maskingModel.calcBarkPower(currentPower, numBins);
        auto maskThresh = maskingModel.calcMaskingThreshold(barkPower);
        auto tonalRatio = maskingModel.calcBarkTonalRatio(tonalMaskWorkspace.data(), numBins);

        AnalysisFrame frame;
        frame.isOnset = isOnset;
        frame.barkPower = barkPower;

        float maxReductionDB = 0.0f;

        // 帯域別ゲインリダクションの計算
        for (int i = 0; i < 24; ++i)
        {
            float excess = barkPower[i] - maskThresh[i] + threshold;

            if (excess > 0.0f && !isOnset)
            {
                // 超過量に深さと調波比率を掛け、打撃音（tonalRatio=0近傍）を保護
                float bandReductionDB = excess * depth * tonalRatio[i];

                if (bandReductionDB > maxReductionDB) {
                    maxReductionDB = bandReductionDB;
                }
                frame.barkGainReduction[i] = juce::Decibels::decibelsToGain(-bandReductionDB);
            }
            else
            {
                frame.barkGainReduction[i] = 1.0f;
            }
        }

        // 最もリダクションの強い帯域に合わせて全体のターゲットゲインを設定
        float targetGain = juce::Decibels::decibelsToGain(-maxReductionDB);
        gainSmooth.setTargetValue(targetGain);

        // UI表示用のスペクトル縮小
        const int binsPerUI = numBins / 512;
        for (int i = 0; i < 512; ++i) {
            float sumP = 0.0f;
            int startIdx = i * binsPerUI;
            for (int j = 0; j < binsPerUI; ++j) {
                int idx = startIdx + j;
                if (idx < numBins) sumP += currentPower[idx];
            }
            frame.magnitudeSpectrum[static_cast<size_t>(i)] = std::sqrt(sumP / static_cast<float>(binsPerUI) + 1e-12f);
        }

        analysisFifo.push(frame);
    }

    // --- ゲインリダクションの適用 ---
    if (isMsMode && totalNumOutputChannels > 1)
    {
        // M/Sモード: Mid(ch0)にのみゲインを適用し、Side(ch1)はそのまま通過させる
        float* midData = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i) {
            midData[i] *= gainSmooth.getNextValue();
        }
    }
    else
    {
        // ステレオ/モノラルモード: 全チャンネルに同じゲインを適用（ステレオリンク）
        if (totalNumOutputChannels == 1) {
            float* chData = buffer.getWritePointer(0);
            for (int i = 0; i < numSamples; ++i) {
                chData[i] *= gainSmooth.getNextValue();
            }
        }
        else {
            float* leftData = buffer.getWritePointer(0);
            float* rightData = buffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i) {
                float g = gainSmooth.getNextValue();
                leftData[i] *= g;
                rightData[i] *= g;
            }
        }
    }

    if (isMsMode) msEncoder.decodeMidSide(buffer);
}

//==============================================================================
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

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LUMINAProcessor(); }