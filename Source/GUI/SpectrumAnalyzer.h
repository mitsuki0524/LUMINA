#pragma once

#include <JuceHeader.h>
#include "AnalysisFifo.h"

/**
 * @class SpectrumAnalyzer
 * @brief 対数周波数スケールで高解像度スペクトルとBark帯域パワーを描画するコンポーネント。
 */
class SpectrumAnalyzer : public juce::Component
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /**
     * @brief PluginEditorから最新のフレームを受け取って内部状態を更新
     */
    void updateFrame(const AnalysisFrame& newFrame);

private:
    // 周波数(Hz)をX座標に変換（対数スケール）
    float mapToLogX(float freqHz, float width) const;
    // 振幅(Linear)をY座標に変換（デシベルスケール）
    float mapToLogY(float magnitude, float height) const;
    // Bark帯域(0~23)の代表周波数を取得（簡易版）
    float getBarkCenterFreq(int barkIndex) const;

    AnalysisFrame currentFrame;

    // X軸の事前計算用配列（512ビン）
    std::vector<float> binFrequencies;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};