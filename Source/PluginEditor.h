#ifndef LUMINA_PLUGINEDITOR_H
#define LUMINA_PLUGINEDITOR_H

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/GRMeter.h"

/**
 * @class LUMINAEditor
 * @brief LUMINAプラグインのユーザーインターフェースを管理するクラス。
 */
class LUMINAEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    LUMINAEditor(LUMINAProcessor&);
    ~LUMINAEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    /**
     * @brief タイマーによる描画更新処理（30fps）
     */
    void timerCallback() override;

    // プロセッサへの参照
    LUMINAProcessor& processor;

    // --- 視覚化コンポーネント ---
    SpectrumAnalyzer spectrumAnalyzer;
    GRMeter grMeter; // ここでの宣言が C2065 エラーの解決に必須です

    // --- GUIコントロール ---
    juce::Slider thresholdSlider, depthSlider, attackSlider, releaseSlider;
    juce::Label thresholdLabel, depthLabel, attackLabel, releaseLabel;
    juce::ToggleButton msModeButton, linearPhaseButton;

    // --- パラメータ・アタッチメント ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> depthAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<ButtonAttachment> msModeAttachment;
    std::unique_ptr<ButtonAttachment> linearPhaseAttachment;

    // 最新の解析データ保持用
    AnalysisFrame latestFrame;

    // LEDの明るさ管理
    float onsetGlow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAEditor)
};

#endif // LUMINA_PLUGINEDITOR_H