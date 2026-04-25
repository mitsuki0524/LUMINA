#ifndef LUMINA_MaskingModel_H
#define LUMINA_MaskingModel_H

#include <JuceHeader.h>
#include <vector>
#include <array>

/**
 * @class MaskingModel
 * @brief 人間の聴覚特性（Bark尺度）に基づく心理音響マスキング閾値を計算するクラス。
 * * 信号間の相互マスキングをシミュレートし、耳障りな成分（閾値を超えたピーク）を特定します。
 * 計算負荷を抑えるため、Spreading Functionの結果をLookup Tableとして保持します。
 */
class MaskingModel
{
public:
    static constexpr int NUM_BARK = 24;

    MaskingModel() = default;
    ~MaskingModel() = default;

    /**
     * @brief 内部バッファの初期化とSpreading Functionの事前計算
     * @param sampleRate システムのサンプリングレート
     * @param fftSize 処理に使用するFFTサイズ
     */
    void prepare(double sampleRate, int fftSize);

    /**
     * @brief 各FFTビンを24のBark帯域に集約し、パワー（dB）を算出
     * @param powerSpectrum FFTのパワースペクトル [numBins]
     * @param numBins 処理するビン数
     */
    std::array<float, NUM_BARK> calcBarkPower(const float* powerSpectrum, int numBins) const;

    /**
     * @brief 各帯域のパワーから相互マスキング閾値（dB）を計算
     * @param barkPowerDB calcBarkPowerで得られた各帯域のdBレベル
     */
    std::array<float, NUM_BARK> calcMaskingThreshold(const std::array<float, NUM_BARK>& barkPowerDB) const;

    /**
     * @brief マスキング閾値に基づき、最終的なゲインリダクション量を決定
     * @param barkPowerDB 現在の帯域レベル
     * @param maskThreshDB 算出されたマスキング閾値
     * @param thresholdDB ユーザー設定のスレッショルド
     * @param depth ユーザー設定の適用量
     */
    float computeGainReduction(const std::array<float, NUM_BARK>& barkPowerDB,
        const std::array<float, NUM_BARK>& maskThreshDB,
        float thresholdDB,
        float depth) const;

private:
    /**
     * @brief 周波数(Hz)からBark値への変換
     */
    static float hzToBark(float hz);

    // FFTビンインデックスからBark帯域インデックスへのマッピング
    std::vector<int> barkMap_;

    double currentSampleRate = 44100.0;
    int currentFftSize = 4096;

    // 事前計算されたマスキング曲線の Lookup Table [maskerIdx][maskeeIdx]
    float spreadingTable[NUM_BARK][NUM_BARK];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaskingModel)
};

#endif // LUMINA_MaskingModel_H