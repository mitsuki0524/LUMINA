#include "HPSSEngine.h"

HPSSEngine::HPSSEngine()
{
}

void HPSSEngine::prepare(int maxBins, int maxHarmonicLen, int maxPercussiveLen)
{
    maxHLen = maxHarmonicLen;
    maxPLen = maxPercussiveLen;
    nBins = maxBins;

    // 履歴バッファの確保 (時間方向の最大長さ * 周波数ビン数)
    history.assign(static_cast<size_t>(maxHLen) * static_cast<size_t>(nBins), 0.0f);
    writeIndex = 0;

    // 計算用ワークスペースの事前確保
    hWorkspace.assign(static_cast<size_t>(maxHLen), 0.0f);
    pWorkspace.assign(static_cast<size_t>(maxPLen), 0.0f);
}

void HPSSEngine::process(const float* currentPower, float* tonalMask, int numBins, float resValue, float blurValue)
{
    // 動的なウィンドウサイズの適用（必ず奇数になるようビット演算、かつ最大値を超えない）
    int currentHLen = juce::jlimit(3, maxHLen, static_cast<int>(resValue) | 1);
    int currentPLen = 31; // Percussive側は周波数解像度に依存するため固定

    // 1. 全体のエネルギーを確認し、無音に近い場合は高速かつ安全に処理
    float totalEnergy = 0.0f;
    for (int i = 0; i < numBins; ++i) totalEnergy += currentPower[i];

    if (totalEnergy < 1e-10f)
    {
        for (int i = 0; i < numBins; ++i)
        {
            tonalMask[i] = 1.0f; // 加工なし
            history[static_cast<size_t>(writeIndex) * static_cast<size_t>(nBins) + static_cast<size_t>(i)] = 0.0f;
        }
        writeIndex = (writeIndex + 1) % maxHLen;
        return;
    }

    // リングバッファへの現在のパワー書き込み
    for (int b = 0; b < numBins; ++b)
    {
        history[static_cast<size_t>(writeIndex) * static_cast<size_t>(nBins) + static_cast<size_t>(b)] = currentPower[b];
    }

    // 演算定数
    const float eps = 1e-12f;
    const float noiseFloor = 1e-9f;
    const bool useBlur = std::abs(blurValue - 1.0f) > 0.01f;

    // 2. 各ビンに対してメジアンフィルタリングを適用
    for (int b = 0; b < numBins; ++b)
    {
        if (currentPower[b] < noiseFloor)
        {
            tonalMask[b] = 1.0f;
            continue;
        }

        // --- Harmonic Median (時間方向のフィルタリング: リングバッファを遡る) ---
        for (int i = 0; i < currentHLen; ++i)
        {
            int readIdx = (writeIndex - i + maxHLen) % maxHLen;
            hWorkspace[static_cast<size_t>(i)] = history[static_cast<size_t>(readIdx) * static_cast<size_t>(nBins) + static_cast<size_t>(b)];
        }
        float hMedian = calculateMedian(hWorkspace, currentHLen);

        // --- Percussive Median (周波数方向のフィルタリング) ---
        int pCount = 0;
        int halfP = currentPLen / 2;
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

        // Blur係数によるコントラストの平滑化 (Pro Parameter)
        if (useBlur) {
            hPow = std::pow(hPow + eps, blurValue);
            pPow = std::pow(pPow + eps, blurValue);
        }

        tonalMask[b] = hPow / (hPow + pPow + eps);
    }

    // リングバッファインデックスの更新
    writeIndex = (writeIndex + 1) % maxHLen;
}

inline float HPSSEngine::calculateMedian(std::vector<float>& workspace, int actualSize)
{
    if (actualSize <= 0) return 0.0f;
    auto target = workspace.begin() + (actualSize / 2);
    std::nth_element(workspace.begin(), target, workspace.begin() + actualSize);
    return *target;
}