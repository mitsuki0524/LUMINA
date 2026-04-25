#pragma once

#include <JuceHeader.h>
#include <vector>
#include <algorithm>

/**
 * @class HPSSEngine
 * @brief 時間軸と周波数軸のメジアンフィルタを用いてトーナル/トランジェント成分を分離するクラス。
 * * 規約遵守:
 * - processBlock内での動的メモリ確保(new/std::vector::resize)は行いません。
 * - std::nth_elementを用いた高速なメジアン計算を実装。
 */
class HPSSEngine
{
public:
    HPSSEngine();
    ~HPSSEngine() = default;

    /**
     * @brief バッファの確保とパラメータの初期化
     * @param maxBins FFTのビン数 (例: 4096ptなら2049)
     * @param harmonicLen 時間方向のメジアン窓幅 (奇数推奨)
     * @param percussiveLen 周波数方向のメジアン窓幅 (奇数推奨)
     */
    void prepare(int maxBins, int harmonicLen = 17, int percussiveLen = 31);

    /**
     * @brief 最新のスペクトルフレームを処理し、トーナル成分のマスクを出力
     * @param currentPower 現在のフレームのパワースペクトル [numBins]
     * @param tonalMask 結果を格納するバッファ (0.0~1.0) [numBins]
     * @param numBins 処理するビン数
     */
    void process(const float* currentPower, float* tonalMask, int numBins);

private:
    /**
     * @brief ワークスペース内のデータから中央値を算出 (std::nth_elementを使用)
     */
    inline float calculateMedian(std::vector<float>& workspace, int actualSize);

    // パラメータ
    int hLen = 17;
    int pLen = 31;
    int nBins = 0;

    // 履歴管理
    int writeIndex = 0;
    // 2次元履歴バッファを1次元で保持 [hLen * maxBins]
    std::vector<float> history;

    // 計算用ワークスペース (毎フレームのメモリ確保を避けるため)
    std::vector<float> hWorkspace;
    std::vector<float> pWorkspace;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HPSSEngine)
};