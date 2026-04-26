#ifndef LUMINA_PLUGINEDITOR_H
#define LUMINA_PLUGINEDITOR_H

// JuceHeader.h を直接モジュールと標準ライブラリに置き換え
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "PluginProcessor.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/GRMeter.h"
#include "GUI/AbletonLookAndFeel.h"

class LUMINAEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    LUMINAEditor(LUMINAProcessor&);
    ~LUMINAEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // 指定したバンドの M/S タブ表示を切り替えるヘルパー関数
    void setBandTab(int bandIndex, bool showSide);

    LUMINAProcessor& processor;
    AbletonLookAndFeel customLookAndFeel;

    SpectrumAnalyzer spectrumAnalyzer;
    GRMeter grMeter;

    // --- Band UI Components ---
    juce::Label bandTitles[3];

    // M/S 切り替えタブとリンクボタン
    juce::TextButton tabMid[3];
    juce::TextButton tabSide[3];
    juce::ToggleButton bandLink[3];

    // Solo / Delta ボタン
    juce::ToggleButton bandButtons[3][2];

    // Mid (Stereo) 用のスライダーとラベル
    juce::Slider bandSlidersM[3][4];
    juce::Label bandLabelsM[3][4];

    // Side 用のスライダーとラベル
    juce::Slider bandSlidersS[3][4];
    juce::Label bandLabelsS[3][4];

    juce::ToggleButton msModeButton;

    // --- APVTS Attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> bandAttachmentsM[3][4];
    std::unique_ptr<SliderAttachment> bandAttachmentsS[3][4];

    std::unique_ptr<ButtonAttachment> bandButtonAttachments[3][2];
    std::unique_ptr<ButtonAttachment> bandLinkAttachments[3];
    std::unique_ptr<ButtonAttachment> msModeAttachment;

    AnalysisFrame latestFrame{};

    // 現在どちらのタブを表示しているかの状態保持
    bool currentTabIsSide[3] = { false, false, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAEditor)
};

#endif // LUMINA_PLUGINEDITOR_H