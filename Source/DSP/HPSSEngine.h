#ifndef LUMINA_HPSSEngine_H
#define LUMINA_HPSSEngine_H

#include <JuceHeader.h>
#include <vector>
#include <algorithm>

/**
 * @class HPSSEngine
 * @brief Harmonic-Percussive Source Separation (HPSS) エンジン。
 * 時間方向と周波数方向のメジアンフィルタを用いて、持続音と打撃音の分離マスクを生成します。
 */
class HPSSEngine
{
public:
    HPSSEngine();
    ~HPSSEngine() = default;

    /**
     * @brief 解析バッファの初期化と領域確保
     * @param maxBins 最大周波数ビン数
     * @param harmonicLen 時間方向のメジアン窓幅 (奇数)
     * @param percussiveLen 周波数方向のメジアン窓幅 (奇数)
     */
    void prepare(int maxBins, int harmonicLen = 17, int percussiveLen = 31);

    /**
     * @brief 最新のパワースペクトルから分離マスクを算出
     * @param currentPower 現在のフレームのパワースペクトル [numBins]
     * @param tonalMask 結果を格納する配列 (0.0~1.0) [numBins]
     * @param numBins 処理するビン数
     */
    void process(const float* currentPower, float* tonalMask, int numBins);

private:
    /**
     * @brief 高速なメジアン（中央値）計算の実装
     */
    inline float calculateMedian(std::vector<float>& workspace, int actualSize);

    // パラメータ
    int hLen = 17;
    int pLen = 31;
    int nBins = 0;

    // 解析用履歴管理 (リングバッファ)
    int writeIndex = 0;
    std::vector<float> history;

    // 計算用ワークスペース（processBlock内でのメモリ確保を回避）
    std::vector<float> hWorkspace;
    std::vector<float> pWorkspace;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HPSSEngine)
};

#endif // LUMINA_HPSSEngine_H