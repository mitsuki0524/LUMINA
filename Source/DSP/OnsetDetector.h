#pragma once

#include <JuceHeader.h>
#include <vector>

/**
 * @class OnsetDetector
 * @brief スペクトルフラックスを用いて音の立ち上がり（トランジェント）を検出するクラス。
 * * 規約遵守:
 * - メモリの動的確保を排除。
 * - 生ポインタとシンプルなループによる極低負荷な設計。
 */
class OnsetDetector
{
public:
    OnsetDetector();
    ~OnsetDetector() = default;

    /**
     * @brief 初期化とバッファの事前確保
     * @param sampleRate サンプルレート
     * @param maxBins FFTの最大ビン数 (例: 2049)
     */
    void prepare(double sampleRate, int maxBins);

    /**
     * @brief 最新のスペクトルからオンセットを検出する
     * @param currentPower 現在のフレームのパワースペクトル [numBins]
     * @param numBins 処理するビン数
     * @return オンセットが検出された場合 true
     */
    bool detectOnset(const float* currentPower, int numBins);

    /**
     * @brief 検出感度の設定 (UIのパラメータ等から後日操作可能にする用)
     * @param sensitivity 感度 (0.0 ~ 1.0 など)
     */
    void setSensitivity(float sensitivity);

private:
    // 過去1フレーム分のパワースペクトルを保持するバッファ
    std::vector<float> prevPower;

    // 動的スレッショルド（フラックスの移動平均）計算用
    float smoothedFlux = 0.0f;

    // 感度調整用パラメータ
    float thresholdMultiplier = 1.5f;
    float noiseFloorOffset = 1e-5f;

    // 連続検知を防ぐためのクールダウン (単位: フレーム)
    int cooldownFrames = 0;
    int currentCooldown = 0;

    double currentSampleRate = 44100.0;

    JU_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OnsetDetector)
};