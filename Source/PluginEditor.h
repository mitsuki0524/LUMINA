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

    LUMINAProcessor& processor;
    AbletonLookAndFeel customLookAndFeel; // カスタムUIのインスタンス

    SpectrumAnalyzer spectrumAnalyzer;
    GRMeter grMeter;

    juce::Slider crossSliders[2];
    juce::Label crossLabels[2];

    juce::Slider bandSliders[3][4];
    juce::Label bandLabels[3][4];
    juce::ToggleButton bandButtons[3][2];
    juce::Label bandTitles[3];

    juce::ToggleButton msModeButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> crossAttachments[2];
    std::unique_ptr<SliderAttachment> bandAttachments[3][4];
    std::unique_ptr<ButtonAttachment> bandButtonAttachments[3][2];
    std::unique_ptr<ButtonAttachment> msModeAttachment;

    AnalysisFrame latestFrame{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAEditor)
};

#endif