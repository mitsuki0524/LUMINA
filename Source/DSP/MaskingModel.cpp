#include "MaskingModel.h"
#include <cmath>

float MaskingModel::hzToBark(float hz)
{
    return 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
}

void MaskingModel::prepare(double sampleRate, int fftSize)
{
    currentSampleRate = sampleRate;
    currentFftSize = fftSize;

    const int numBins = fftSize / 2 + 1;
    barkMap_.assign(static_cast<size_t>(numBins), 0);

    const float binFreq = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    for (int i = 0; i < numBins; ++i)
    {
        float hz = static_cast<float>(i) * binFreq;
        float bark = hzToBark(hz);
        int barkIdx = static_cast<int>(std::floor(bark));
        barkMap_[static_cast<size_t>(i)] = (barkIdx < 0) ? 0 : (barkIdx >= NUM_BARK ? NUM_BARK - 1 : barkIdx);
    }

    for (int m = 0; m < NUM_BARK; ++m)
    {
        for (int k = 0; k < NUM_BARK; ++k)
        {
            float dz = static_cast<float>(k - m);
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

    for (int i = 0; i < numBins; ++i)
    {
        int bandIdx = barkMap_[static_cast<size_t>(i)];
        barkPowerLinear[static_cast<size_t>(bandIdx)] += powerSpectrum[i];
    }

    std::array<float, NUM_BARK> barkPowerDB;
    for (int i = 0; i < NUM_BARK; ++i)
    {
        barkPowerDB[static_cast<size_t>(i)] = juce::Decibels::gainToDecibels(barkPowerLinear[static_cast<size_t>(i)] + 1e-12f, -120.0f);
    }

    return barkPowerDB;
}

std::array<float, MaskingModel::NUM_BARK> MaskingModel::calcMaskingThreshold(const std::array<float, NUM_BARK>& barkPowerDB) const
{
    std::array<float, NUM_BARK> threshold;
    threshold.fill(-120.0f);

    for (int m = 0; m < NUM_BARK; ++m)
    {
        float maskerLevel = barkPowerDB[static_cast<size_t>(m)];
        if (maskerLevel < -50.0f) continue;

        const float* sfRow = spreadingTable[m];

        for (int k = 0; k < NUM_BARK; ++k)
        {
            float maskedLevel = maskerLevel + sfRow[k];
            if (maskedLevel > threshold[static_cast<size_t>(k)])
            {
                threshold[static_cast<size_t>(k)] = maskedLevel;
            }
        }
    }
    return threshold;
}

std::array<float, MaskingModel::NUM_BARK> MaskingModel::calcBarkTonalRatio(const float* tonalMask, int numBins) const
{
    std::array<float, NUM_BARK> barkTonalSum;
    std::array<int, NUM_BARK> barkCount;
    barkTonalSum.fill(0.0f);
    barkCount.fill(0);

    // 高解像度のマスク値をBark帯域ごとに積算
    for (int i = 0; i < numBins; ++i)
    {
        int bandIdx = barkMap_[static_cast<size_t>(i)];
        barkTonalSum[static_cast<size_t>(bandIdx)] += tonalMask[i];
        barkCount[static_cast<size_t>(bandIdx)] += 1;
    }

    std::array<float, NUM_BARK> barkTonalRatio;
    for (int i = 0; i < NUM_BARK; ++i)
    {
        if (barkCount[static_cast<size_t>(i)] > 0)
        {
            // 帯域内の平均値を算出 (0.0=完全打撃音, 1.0=完全持続音)
            barkTonalRatio[static_cast<size_t>(i)] = barkTonalSum[static_cast<size_t>(i)] / static_cast<float>(barkCount[static_cast<size_t>(i)]);
        }
        else
        {
            barkTonalRatio[static_cast<size_t>(i)] = 1.0f; // 安全対策
        }
    }

    return barkTonalRatio;
}