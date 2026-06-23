#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

/**
 * @class MaskingModel
 * @brief 人間の聴覚特性（Bark尺度）に基づく心理音響マスキング閾値を計算するクラス。
 */
class MaskingModel
{
public:
    static constexpr int NUM_BARK = 24;

    MaskingModel() = default;
    ~MaskingModel() = default;

    /**
     * @brief 内部バッファの初期化とSpreading Functionの事前計算
     */
    void prepare(double sampleRate, int fftSize);

    /**
     * @brief 各FFTビンを24のBark帯域に集約し、パワー（dB）を算出
     */
    std::array<float, NUM_BARK> calcBarkPower(const float* powerSpectrum, int numBins) const;

    /**
     * @brief 各帯域のパワーから相互マスキング閾値（dB）を計算
     */
    std::array<float, NUM_BARK> calcMaskingThreshold(const std::array<float, NUM_BARK>& barkPowerDB) const;

    /**
     * @brief HPSSによる高解像度マスクを24のBark帯域に集約し、調波比率（0.0~1.0）を算出する
     * @param tonalMask 持続音成分の比率を示す配列 [numBins]
     * @param numBins 処理するビン数
     * @return 各Bark帯域の調波比率（1.0に近いほど持続音、0.0に近いほど打撃音）
     */
    std::array<float, NUM_BARK> calcBarkTonalRatio(const float* tonalMask, int numBins) const;

private:
    static float hzToBark(float hz);

    std::vector<int> barkMap_;

    double currentSampleRate = 44100.0;
    int currentFftSize = 4096;

    float spreadingTable[NUM_BARK][NUM_BARK];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaskingModel)
};