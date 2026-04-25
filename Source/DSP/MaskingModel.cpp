#include "MaskingModel.h"

float MaskingModel::hzToBark(float hz)
{
    // Barkスケールへの近似変換公式
    return 26.81f * hz / (1960.0f + hz) - 0.53f;
}

void MaskingModel::prepare(double sampleRate, int fftSize)
{
    currentSampleRate = sampleRate;
    currentFftSize = fftSize;

    int numBins = fftSize / 2 + 1;

    // 【DSP Safety】マッピングテーブルの事前確保
    barkMap_.assign(numBins, 0);

    float binFreq = (float)sampleRate / (float)fftSize;

    // 各FFTビンが0〜23のどのBark帯域に属するかを事前計算しておく
    for (int i = 0; i < numBins; ++i)
    {
        float hz = i * binFreq;
        float bark = hzToBark(hz);
        int barkIdx = (int)std::floor(bark);

        // 安全のため 0 ~ 23 の範囲にクランプ
        if (barkIdx < 0) barkIdx = 0;
        if (barkIdx >= NUM_BARK) barkIdx = NUM_BARK - 1;

        barkMap_[i] = barkIdx;
    }
}

std::array<float, MaskingModel::NUM_BARK> MaskingModel::calcBarkPower(const float* powerSpectrum, int numBins) const
{
    std::array<float, NUM_BARK> barkPowerLinear = { 0.0f };

    // 生ポインタと事前計算マップによる超低負荷な帯域集計
    for (int i = 0; i < numBins; ++i)
    {
        int b = barkMap_[i];
        barkPowerLinear[b] += powerSpectrum[i];
    }

    std::array<float, NUM_BARK> barkPowerDB;
    for (int i = 0; i < NUM_BARK; ++i)
    {
        // 0割りと log10(0) を防ぐために 1e-9f を加算してデシベル変換
        barkPowerDB[i] = juce::Decibels::gainToDecibels(barkPowerLinear[i] + 1e-9f, -120.0f);
    }

    return barkPowerDB;
}

std::array<float, MaskingModel::NUM_BARK> MaskingModel::calcMaskingThreshold(const std::array<float, NUM_BARK>& barkPowerDB) const
{
    std::array<float, NUM_BARK> threshold;
    threshold.fill(-120.0f); // ベースノイズフロア設定

    // 各帯域が「マスカー（隠す側）」として、他の帯域「マスキー（隠される側）」に与える影響を計算
    for (int masker = 0; masker < NUM_BARK; ++masker)
    {
        float Lm = barkPowerDB[masker];

        // 音量が小さすぎる帯域はマスキング効果がないためスキップ (CPU負荷削減)
        if (Lm < -20.0f) continue;

        for (int maskee = 0; maskee < NUM_BARK; ++maskee)
        {
            float dz = (float)(maskee - masker);

            // 開発計画書に記載された Spreading Function (広がり関数) の近似式
            float SF = 15.81f + 7.5f * (dz + 0.474f) - 17.5f * std::sqrt(1.0f + (dz + 0.474f) * (dz + 0.474f));
            float contrib = Lm + SF;

            // 複数のマスカーからの影響のうち、最大のものを閾値とする
            if (contrib > threshold[maskee])
            {
                threshold[maskee] = contrib;
            }
        }
    }
    return threshold;
}

float MaskingModel::computeGainReduction(const std::array<float, NUM_BARK>& barkPowerDB,
    const std::array<float, NUM_BARK>& maskThreshDB,
    float thresholdDB,
    float depth) const
{
    float maxExcessDB = 0.0f;

    // 現在の信号がマスキング閾値から「どれだけ耳障りに飛び出しているか(Excess)」の最大値を探す
    for (int i = 0; i < NUM_BARK; ++i)
    {
        float excess = barkPowerDB[i] - maskThreshDB[i];
        if (excess > maxExcessDB)
        {
            maxExcessDB = excess;
        }
    }

    float targetGain = 1.0f;

    // 飛び出し量(maxExcessDB)が、ユーザーが設定したThreshold(負の値)を超えたらリダクション実行
    // 例: maxExcessDBが 10dB で、thresholdDBが -5dB なら、5dBのオーバーシュート
    float overShoot = maxExcessDB + thresholdDB;

    if (overShoot > 0.0f)
    {
        // Depth (0.0 ~ 1.0) に応じてカット量を決定
        float reductionDB = overShoot * depth;
        targetGain = juce::Decibels::decibelsToGain(-reductionDB);
    }

    // 安全のため、0.0 (完全ミュート) から 1.0 (原音そのまま) の範囲に収める
    return juce::jlimit(0.0f, 1.0f, targetGain);
}