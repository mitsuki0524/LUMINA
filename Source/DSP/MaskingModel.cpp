#include "MaskingModel.h"
#include <cmath>

float MaskingModel::hzToBark(float hz)
{
    // Zwicker and Terhardt (1980) の公式
    return 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
}

void MaskingModel::prepare(double sampleRate, int fftSize)
{
    currentSampleRate = sampleRate;
    currentFftSize = fftSize;

    const int numBins = fftSize / 2 + 1;
    barkMap_.assign(static_cast<size_t>(numBins), 0);

    const float binFreq = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    // 各FFTビンが属するBark帯域を事前計算
    for (int i = 0; i < numBins; ++i)
    {
        float hz = static_cast<float>(i) * binFreq;
        float bark = hzToBark(hz);
        int barkIdx = static_cast<int>(std::floor(bark));

        // 0 ~ 23 の範囲に制限
        barkMap_[static_cast<size_t>(i)] = (barkIdx < 0) ? 0 : (barkIdx >= NUM_BARK ? NUM_BARK - 1 : barkIdx);
    }

    // --- Spreading Function (マスキングの広がり) の事前計算 ---
    // これにより processBlock 内での sqrt や複雑な演算を排除します
    for (int m = 0; m < NUM_BARK; ++m)
    {
        for (int k = 0; k < NUM_BARK; ++k)
        {
            // マスカー(m)とマスキー(k)の距離
            float dz = static_cast<float>(k - m);

            // Schröder et al. (1979) による近似式
            // SF(dB) = 15.81 + 7.5*(dz + 0.474) - 17.5*sqrt(1 + (dz + 0.474)^2)
            float arg = dz + 0.474f;
            float sfValue = 15.81f + 7.5f * arg - 17.5f * std::sqrt(1.0f + arg * arg);

            spreadingTable[m][k] = sfValue;
        }
    }
}

std::array<float, MaskingModel::NUM_BARK> MaskingModel::calcBarkPower(const float* powerSpectrum, int numBins) const
{
    std::array<float, NUM_BARK> barkPowerLinear;
    barkPowerLinear.fill(0.0f);

    // パワースペクトルを各Bark帯域に加算
    for (int i = 0; i < numBins; ++i)
    {
        int bandIdx = barkMap_[static_cast<size_t>(i)];
        barkPowerLinear[static_cast<size_t>(bandIdx)] += powerSpectrum[i];
    }

    std::array<float, NUM_BARK> barkPowerDB;
    for (int i = 0; i < NUM_BARK; ++i)
    {
        // log10(0) 回避のため 1e-12f を加算
        barkPowerDB[static_cast<size_t>(i)] = juce::Decibels::gainToDecibels(barkPowerLinear[static_cast<size_t>(i)] + 1e-12f, -120.0f);
    }

    return barkPowerDB;
}

std::array<float, MaskingModel::NUM_BARK> MaskingModel::calcMaskingThreshold(const std::array<float, NUM_BARK>& barkPowerDB) const
{
    std::array<float, NUM_BARK> threshold;
    threshold.fill(-120.0f);

    // 二重ループだが、内部は単なる「加算」と「比較」のみ
    for (int m = 0; m < NUM_BARK; ++m)
    {
        float maskerLevel = barkPowerDB[static_cast<size_t>(m)];

        // 低レベルな帯域はマスキング効果を無視して計算スキップ
        if (maskerLevel < -50.0f) continue;

        const float* sfRow = spreadingTable[m];

        for (int k = 0; k < NUM_BARK; ++k)
        {
            // マスカーのレベルに広がり関数(SF)を足す
            float maskedLevel = maskerLevel + sfRow[k];

            // その帯域における最大マスキングレベルを保持
            if (maskedLevel > threshold[static_cast<size_t>(k)])
            {
                threshold[static_cast<size_t>(k)] = maskedLevel;
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

    // 各帯域で「信号レベルがマスキング閾値をどれだけ超えているか」を確認
    for (int i = 0; i < NUM_BARK; ++i)
    {
        float excess = barkPowerDB[static_cast<size_t>(i)] - maskThreshDB[static_cast<size_t>(i)];
        if (excess > maxExcessDB)
        {
            maxExcessDB = excess;
        }
    }

    // ユーザー設定のThreshold（マイナスの値）との加算
    // 信号が目立っていれば overShoot はプラスの値になる
    float overShoot = maxExcessDB + thresholdDB;

    if (overShoot > 0.0f)
    {
        // 目立ちすぎている量(overShoot)に適用率(depth)をかけ、dBをゲイン係数(0.0~1.0)に変換
        float reductionDB = overShoot * depth;
        return juce::Decibels::decibelsToGain(-reductionDB);
    }

    return 1.0f;
}