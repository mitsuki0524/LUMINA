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

LUMINAProcessor::~LUMINAProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LUMINAProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "THRESHOLD", "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f, "dB"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEPTH", "Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ATTACK", "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.5f), 10.0f, "ms"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RELEASE", "Release",
        juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.5f), 50.0f, "ms"));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "LINEAR_PHASE", "Linear Phase", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "MS_MODE", "M/S Mode", false));

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

    // 全DSPモジュールの初期化 (最大解像度の 4096 を基準に設定)
    stft.prepare(sampleRate);
    int maxBins = 4096 / 2 + 1; // 2049 bins

    hpss.prepare(maxBins, 17, 31);
    maskingModel.prepare(sampleRate, 4096);
    onsetDetector.prepare(sampleRate, maxBins);

    tonalMaskWorkspace.assign(maxBins, 1.0f);

    gainSmooth.reset(sampleRate, 0.02);
    gainSmooth.setCurrentAndTargetValue(1.0f);

    setLatencySamples(stft.getLatencySamples());
}

void LUMINAProcessor::releaseResources() {}

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

    // パラメータの取得
    bool isMsMode = *apvts.getRawParameterValue("MS_MODE") > 0.5f;
    float threshold = *apvts.getRawParameterValue("THRESHOLD");
    float depth = *apvts.getRawParameterValue("DEPTH");

    // 1. M/S モードならエンコード
    if (isMsMode) msEncoder.encodeMidSide(buffer);

    // 解析には Mid (または L) チャンネルを使用
    const float* analysisData = buffer.getReadPointer(0);

    // 2. STFT分析
    stft.process(analysisData, numSamples);

    // 高解像度(Low)バンドのパワースペクトルを取得
    int numBins = stft.bands[0].fftSize / 2 + 1;
    const float* currentPower = stft.bands[0].power.data();

    // 3. トーナル/トランジェント分離マスクの計算
    hpss.process(currentPower, tonalMaskWorkspace.data(), numBins);

    // 4. オンセット(アタック)検出
    bool isOnset = onsetDetector.detectOnset(currentPower, numBins);

    // 5. 心理音響モデルによるマスキング計算
    auto barkPower = maskingModel.calcBarkPower(currentPower, numBins);
    auto maskThresh = maskingModel.calcMaskingThreshold(barkPower);

    // 6. リダクション量の決定 (アタック検出時はリダクションを1.0に固定して保護)
    float targetGain = isOnset ? 1.0f : maskingModel.computeGainReduction(barkPower, maskThresh, threshold, depth);

    // 7. スムージングしながらゲインを適用
    // アタック・リリース時間の適用は今後の拡張とし、今回は固定のスムージングで適用
    gainSmooth.setTargetValue(targetGain);

    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        float* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            channelData[i] *= gainSmooth.getNextValue();
        }
    }

    // 8. M/S デコード
    if (isMsMode) msEncoder.decodeMidSide(buffer);

    // 9. UIへ分析データを送信 (Lock-free)
    AnalysisFrame frame;
    frame.isOnset = isOnset;
    frame.barkPower = barkPower;
    // (スペクトル縮小などのUI向け処理はUI実装時に追加)
    analysisFifo.push(frame);
}

//==============================================================================
bool LUMINAProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* LUMINAProcessor::createEditor() { return new juce::GenericAudioProcessorEditor(*this); }
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