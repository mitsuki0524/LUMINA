#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 * @class MaskingModel
 * @brief 人間の聴覚特性(Barkスケール)に基づくマスキング閾値を計算するクラス。
 * * 規約遵守:
 * - 動的メモリ確保を排除し、固定長配列 (std::array<float, 24>) を用いて高速化。
 */
class MaskingModel
{
public:
    static constexpr int NUM_BARK = 24;

    MaskingModel() = default;
    ~MaskingModel() = default;

    /**
     * @brief バッファの事前確保とマッピングテーブルの生成
     */
    void prepare(double sampleRate, int fftSize);

    /**
     * @brief FFTのパワースペクトルを24のBark帯域パワー(dB)に変換
     * @param powerSpectrum FFTのパワースペクトル配列
     * @param numBins 処理するビン数
     * @return Bark帯域ごとのパワー (dB)
     */
    std::array<float, NUM_BARK> calcBarkPower(const float* powerSpectrum, int numBins) const;

    /**
     * @brief Barkパワーから、他帯域へのマスキング閾値(dB)を計算
     */
    std::array<float, NUM_BARK> calcMaskingThreshold(const std::array<float, NUM_BARK>& barkPowerDB) const;

    /**
     * @brief マスキング閾値とユーザー設定に基づいてゲインリダクション量(0.0~1.0)を算出
     */
    float computeGainReduction(const std::array<float, NUM_BARK>& barkPowerDB,
        const std::array<float, NUM_BARK>& maskThreshDB,
        float thresholdDB,
        float depth) const;

private:
    // 周波数(Hz)からBark値への変換式
    static float hzToBark(float hz);

    // 各FFTビンがどのBark帯域に属するかの事前計算マッピング [numBins]
    std::vector<int> barkMap_;

    double currentSampleRate = 44100.0;
    int currentFftSize = 4096;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaskingModel)
};