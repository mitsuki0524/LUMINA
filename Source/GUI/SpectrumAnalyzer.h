// ==========================================
// Source/GUI/SpectrumAnalyzer.h
// ==========================================
#ifndef LUMINA_SPECTRUMANALYZER_H
#define LUMINA_SPECTRUMANALYZER_H

// JuceHeader.h を直接モジュールに置き換え
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>
#include "AnalysisFifo.h"

class SpectrumAnalyzer : public juce::Component
{
public:
    // APVTSへの参照を受け取り、直接パラメータを更新できるようにする
    SpectrumAnalyzer(juce::AudioProcessorValueTreeState& vts);
    ~SpectrumAnalyzer() override;

    void updateFrame(const AnalysisFrame& frame);
    void setCrossovers(float c1, float c2);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // --- マウスイベントのオーバーライド ---
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    AnalysisFrame currentFrame;

    // ⚡ 追加: マスキング閾値の平滑化バッファ
    std::array<float, 24> currentMaskingThreshold{};

    float cross1 = 250.0f;
    float cross2 = 4000.0f;

    // マウス操作状態
    int draggingCrossIndex = -1; // -1: なし, 0: cross1, 1: cross2
    float currentDragFreq = 0.0f;
    bool isDragging = false;

    // 座標と周波数の相互変換ヘルパー
    float getFreqFromX(float x, float width) const;
    float getXFromFreq(float freq, float width) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

#endif // LUMINA_SPECTRUMANALYZER_H