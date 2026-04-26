#ifndef LUMINA_PLUGINEDITOR_H
#define LUMINA_PLUGINEDITOR_H

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/GRMeter.h"

class LUMINAEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    LUMINAEditor(LUMINAProcessor&);
    ~LUMINAEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LUMINAProcessor& processor;

    SpectrumAnalyzer spectrumAnalyzer;
    GRMeter grMeter;

    juce::Slider thresholdSlider, depthSlider, attackSlider, releaseSlider;
    juce::Label thresholdLabel, depthLabel, attackLabel, releaseLabel;
    juce::ToggleButton msModeButton, linearPhaseButton;

    // 追加: Delta Listen ボタン
    juce::TextButton deltaListenButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> depthAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<ButtonAttachment> msModeAttachment;
    std::unique_ptr<ButtonAttachment> linearPhaseAttachment;

    // 追加: Delta Listen のアタッチメント
    std::unique_ptr<ButtonAttachment> deltaListenAttachment;

    AnalysisFrame latestFrame;
    float onsetGlow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAEditor)
};

#endif // LUMINA_PLUGINEDITOR_H