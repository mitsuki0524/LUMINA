#include "HPSSEngine.h"

HPSSEngine::HPSSEngine()
{
}

void HPSSEngine::prepare(int maxBins, int harmonicLen, int percussiveLen)
{
    hLen = harmonicLen;
    pLen = percussiveLen;
    nBins = maxBins;

    // 履歴バッファの確保 (時間方向の長さ * 周波数ビン数)
    history.assign(hLen * nBins, 0.0f);
    writeIndex = 0;

    // メジアン計算用ワークスペースの確保
    hWorkspace.resize(hLen);
    pWorkspace.resize(pLen);
}

void HPSSEngine::process(const float* currentPower, float* tonalMask, int numBins)
{
    // 現在のフレームを履歴に書き込み
    float* currentHistoryPos = &history[writeIndex * nBins];
    for (int b = 0; b < numBins; ++b)
    {
        currentHistoryPos[b] = currentPower[b];
    }

    // 各周波数ビンについて処理
    for (int b = 0; b < numBins; ++b)
    {
        // 1. Harmonic Median (時間方向: 横)
        // 過去 hLen フレーム分の同じ周波数ビンを抽出
        for (int i = 0; i < hLen; ++i)
        {
            hWorkspace[i] = history[i * nBins + b];
        }
        float hMedian = calculateMedian(hWorkspace, hLen);

        // 2. Percussive Median (周波数方向: 縦)
        // 現在のフレームの周辺 pLen ビンを抽出
        int pCount = 0;
        int halfP = pLen / 2;
        for (int i = -halfP; i <= halfP; ++i)
        {
            int targetBin = b + i;
            if (targetBin >= 0 && targetBin < numBins)
            {
                pWorkspace[pCount++] = currentPower[targetBin];
            }
        }
        float pMedian = calculateMedian(pWorkspace, pCount);

        // 3. マスクの計算 (Wiener-like Soft Masking)
        // パワーベースで計算 (安定性のために微小値を加算)
        float eps = 1e-9f;
        float hPow = hMedian * hMedian;
        float pPow = pMedian * pMedian;

        tonalMask[b] = hPow / (hPow + pPow + eps);
    }

    // リングバッファのインデックス更新
    writeIndex = (writeIndex + 1) % hLen;
}

inline float HPSSEngine::calculateMedian(std::vector<float>& workspace, int actualSize)
{
    if (actualSize <= 0) return 0.0f;

    auto target = workspace.begin() + (actualSize / 2);
    std::nth_element(workspace.begin(), target, workspace.begin() + actualSize);

    return *target;
}