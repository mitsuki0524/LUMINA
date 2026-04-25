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
    history.assign(static_cast<size_t>(hLen) * static_cast<size_t>(nBins), 0.0f);
    writeIndex = 0;

    // 計算用ワークスペースの事前確保
    hWorkspace.assign(static_cast<size_t>(hLen), 0.0f);
    pWorkspace.assign(static_cast<size_t>(pLen), 0.0f);
}

void HPSSEngine::process(const float* currentPower, float* tonalMask, int numBins)
{
    // 現在の履歴書き込み位置ポインタを取得
    float* currentHistoryPos = &history[static_cast<size_t>(writeIndex) * static_cast<size_t>(nBins)];

    // 1. 全体のエネルギーを確認し、無音に近い場合は高速かつ安全に処理
    float totalEnergy = 0.0f;
    for (int i = 0; i < numBins; ++i) totalEnergy += currentPower[i];

    if (totalEnergy < 1e-10f)
    {
        for (int i = 0; i < numBins; ++i)
        {
            tonalMask[i] = 1.0f; // 加工なし
            currentHistoryPos[i] = 0.0f;
        }
        writeIndex = (writeIndex + 1) % hLen;
        return;
    }

    // 履歴を現在のパワーで更新
    for (int b = 0; b < numBins; ++b)
    {
        currentHistoryPos[b] = currentPower[b];
    }

    // 演算定数
    const float eps = 1e-12f;
    const float noiseFloor = 1e-9f;

    // 2. 各ビンに対してメジアンフィルタリングを適用
    for (int b = 0; b < numBins; ++b)
    {
        // 信号レベルが低いビンは詳細な計算をスキップして堅牢性を確保
        if (currentPower[b] < noiseFloor)
        {
            tonalMask[b] = 1.0f;
            continue;
        }

        // --- Harmonic Median (時間方向のフィルタリング) ---
        for (int i = 0; i < hLen; ++i)
        {
            hWorkspace[static_cast<size_t>(i)] = history[static_cast<size_t>(i) * static_cast<size_t>(nBins) + static_cast<size_t>(b)];
        }
        float hMedian = calculateMedian(hWorkspace, hLen);

        // --- Percussive Median (周波数方向のフィルタリング) ---
        int pCount = 0;
        int halfP = pLen / 2;
        for (int i = -halfP; i <= halfP; ++i)
        {
            int targetBin = b + i;
            if (targetBin >= 0 && targetBin < numBins)
            {
                pWorkspace[static_cast<size_t>(pCount)] = currentPower[targetBin];
                pCount++;
            }
        }
        float pMedian = calculateMedian(pWorkspace, pCount);

        // --- 分離マスクの算出 (Wienerフィルタベース) ---
        float hPow = hMedian * hMedian;
        float pPow = pMedian * pMedian;
        tonalMask[b] = hPow / (hPow + pPow + eps);
    }

    // リングバッファインデックスの更新
    writeIndex = (writeIndex + 1) % hLen;
}

inline float HPSSEngine::calculateMedian(std::vector<float>& workspace, int actualSize)
{
    if (actualSize <= 0) return 0.0f;

    // C++標準ライブラリを用いた計算量 O(n) の選択アルゴリズム
    auto target = workspace.begin() + (actualSize / 2);
    std::nth_element(workspace.begin(), target, workspace.begin() + actualSize);

    return *target;
}