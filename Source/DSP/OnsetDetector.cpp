#include "OnsetDetector.h"

OnsetDetector::OnsetDetector()
{
}

void OnsetDetector::prepare(double sampleRate, int maxBins)
{
    currentSampleRate = sampleRate;

    // 【DSP Safety】過去フレームの記録用バッファを最大サイズで事前確保
    prevPower.assign(maxBins, 0.0f);

    smoothedFlux = 0.0f;

    // クールダウンフレーム数の設定 (例: STFTのフレームが約10msとして、3フレーム=30ms休止)
    cooldownFrames = 3;
    currentCooldown = 0;
}

void OnsetDetector::setSensitivity(float sensitivity)
{
    // sensitivity が高いほど、multiplierが下がり検知しやすくなる設計
    // (将来的にGUIパラメータと連動させるための準備)
    thresholdMultiplier = juce::jmap(sensitivity, 0.0f, 1.0f, 2.5f, 1.1f);
}

bool OnsetDetector::detectOnset(const float* currentPower, int numBins)
{
    float currentFlux = 0.0f;

    // 【DSP Safety】生ポインタアクセスによる高速なスペクトルフラックス計算
    for (int b = 0; b < numBins; ++b)
    {
        // 1フレーム前との差分を計算
        float diff = currentPower[b] - prevPower[b];

        // エネルギーが増加している(立ち上がっている)成分のみを足し合わせる (Half-wave rectification)
        if (diff > 0.0f)
        {
            currentFlux += diff;
        }

        // 次のフレーム計算のために現在のパワーを保存
        prevPower[b] = currentPower[b];
    }

    bool isOnset = false;

    // クールダウン中は検知をスキップ (トランジェントの二重検知防止)
    if (currentCooldown > 0)
    {
        currentCooldown--;
    }
    else
    {
        // 動的スレッショルドの判定
        // 現在のフラックスが「過去の平均フラックスの X 倍 + ノイズフロア」を超えたらアタックとみなす
        float threshold = (smoothedFlux * thresholdMultiplier) + noiseFloorOffset;

        if (currentFlux > threshold)
        {
            isOnset = true;
            currentCooldown = cooldownFrames; // クールダウン開始
        }
    }

    // 次のフレームのための動的スレッショルド更新 (Leaky Integrator / 低域通過フィルタ)
    // 係数 0.8 / 0.2 は過去の履歴をどの程度引きずるかのウェイト
    smoothedFlux = (smoothedFlux * 0.8f) + (currentFlux * 0.2f);

    return isOnset;
}