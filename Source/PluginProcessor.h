#ifndef LUMINA_PLUGINPROCESSOR_H
#define LUMINA_PLUGINPROCESSOR_H

#include <JuceHeader.h>
#include "DSP/MultiResSTFT.h"
#include "DSP/HPSSEngine.h"
#include "DSP/MaskingModel.h"
#include "DSP/OnsetDetector.h"
#include "DSP/MSEncoder.h"
#include "GUI/AnalysisFifo.h"

/**
 * 【循環参照の物理的遮断】
 * PluginEditor.h はここでは絶対にインクルードしないでください。
 * 代わりに、以下の前方宣言を使用します。
 */
class LUMINAEditor;

/**
 * @class LUMINAProcessor
 * @brief LUMINAプラグインのオーディオ処理およびパラメータ管理を統括するクラス。
 */
class LUMINAProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    LUMINAProcessor();
    ~LUMINAProcessor() override;

    // --- オーディオ処理ライフサイクル ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    /**
     * @brief DAWから渡されるオーディオ信号の入出力レイアウトをチェック
     */
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    /**
     * @brief リアルタイム・オーディオ・レンダリング・ループ
     */
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // --- エディタ（GUI）管理 ---
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // --- プラグイン情報 ---
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // --- プログラム（プリセット）管理 ---
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // --- 状態の保存と復元（APVTSをバイナリ化） ---
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- 公開メンバ（外部からのアクセス用） ---

    // パラメータ管理（APVTS: Audio Processor Value Tree State）
    juce::AudioProcessorValueTreeState apvts;

    // パラメータ定義用スタティックメソッド
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // GUIスレッドへの解析データ転送用 FIFO
    AnalysisFifo analysisFifo;

private:
    // --- DSP 演算モジュール群 ---
    MSEncoder     msEncoder;     // M/S 変換器
    MultiResSTFT  stft;          // マルチ解像度短時間フーリエ変換
    HPSSEngine    hpss;          // 成分分離（Harmonic/Percussive）
    MaskingModel  maskingModel;  // 心理音響マスキングモデル
    OnsetDetector onsetDetector; // オンセット（アタック）検出

    // --- オーディオ・スムージング ---
    // パラメータ変更や解析結果による音量変化でのジッパーノイズを防止
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmooth;

    // --- 内部バッファ ---
    // processBlock 内での動的メモリ確保 (std::vector::resize等) を避けるため
    // prepareToPlay で事前に最大サイズを確保します。
    std::vector<float> tonalMaskWorkspace;

    // 解析タイミングおよび周波数計算用のキャッシュ
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAProcessor)
};

#endif // LUMINA_PLUGINPROCESSOR_H