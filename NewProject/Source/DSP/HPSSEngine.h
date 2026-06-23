#ifndef LUMINA_HPSSEngine_H
#define LUMINA_HPSSEngine_H

#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include <cmath>

/**
 * @class HPSSEngine
 * @brief Harmonic-Percussive Source Separation (HPSS) エンジン。
 * 時間方向と周波数方向のメジアンフィルタを用いて、持続音と打撃音の分離マスクを生成します。
 * Proパラメータ（Res, Blur）のリアルタイム操作を想定し、バッファは事前最大確保されます。
 */
class HPSSEngine
{
public:
    HPSSEngine();
    ~HPSSEngine() = default;

    /**
     * @brief 解析バッファの初期化と領域確保（DSP Safetyのため最大サイズで事前確保）
     * @param maxBins 最大周波数ビン数
     * @param maxHarmonicLen 時間方向の最大メジアン窓幅
     * @param maxPercussiveLen 周波数方向の最大メジアン窓幅
     */
    void prepare(int maxBins, int maxHarmonicLen = 65, int maxPercussiveLen = 65);

    /**
     * @brief 最新のパワースペクトルから分離マスクを算出
     * @param currentPower 現在のフレームのパワースペクトル [numBins]
     * @param tonalMask 結果を格納する配列 (0.0~1.0) [numBins]
     * @param numBins 処理するビン数
     * @param resValue 時間方向の解像度 (HPSS Res)
     * @param blurValue マスク境界の平滑化係数 (HPSS Blur)
     */
    void process(const float* currentPower, float* tonalMask, int numBins, float resValue = 17.0f, float blurValue = 1.0f);

private:
    /**
     * @brief 高速なメジアン（中央値）計算の実装
     */
    inline float calculateMedian(std::vector<float>& workspace, int actualSize);

    // 最大パラメータ設定
    int maxHLen = 65;
    int maxPLen = 65;
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