// ==========================================
// Source/PluginEditor.h
// ==========================================
#ifndef LUMINA_PLUGINEDITOR_H
#define LUMINA_PLUGINEDITOR_H

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
    void setBandTab(int bandIndex, bool showSide);
    void updateAutoBandUI();

    LUMINAProcessor& processor;
    AbletonLookAndFeel customLookAndFeel;

    SpectrumAnalyzer spectrumAnalyzer;
    GRMeter grMeter;

    // --- Global Controls ---
    juce::ToggleButton msModeButton;
    juce::ToggleButton autoLevelButton;

    juce::Label autoLevelValueLabel;
    juce::TextButton autoLevelCommitBtn;

    juce::ComboBox autoBandTimeCombo;
    juce::TextButton autoBandButton;
    juce::ProgressBar autoBandProgress;

    // --- Master Section ---
    juce::Label masterTitle;
    juce::Slider masterInSlider;
    juce::Label masterInLabel;
    juce::Slider masterOutSlider;
    juce::Label masterOutLabel;
    juce::Slider masterDryWetSlider;
    juce::Label masterDryWetLabel;

    // --- Band UI Components ---
    juce::Label bandTitles[3];
    juce::TextButton tabMid[3];
    juce::TextButton tabSide[3];
    juce::ToggleButton bandLink[3];

    juce::ToggleButton bandBypass[3];
    juce::ToggleButton bandSolo[3];
    juce::ToggleButton bandDelta[3];

    // ⚡ 砂時計用スライダー (Tame / Width)
    juce::Slider bandTame[3];
    juce::Label bandTameLabel[3];
    juce::Slider bandWidth[3];
    juce::Label bandWidthLabel[3];

    juce::Slider bandSlidersM[3][4];
    juce::Label bandLabelsM[3][4];
    juce::Slider bandSlidersS[3][4];
    juce::Label bandLabelsS[3][4];

    // --- APVTS Attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> msModeAttachment;
    std::unique_ptr<ButtonAttachment> autoLevelAttachment;
    std::unique_ptr<ButtonAttachment> autoBandAttachment;
    std::unique_ptr<ComboBoxAttachment> autoBandTimeAttachment;

    std::unique_ptr<SliderAttachment> masterInAttachment;
    std::unique_ptr<SliderAttachment> masterOutAttachment;
    std::unique_ptr<SliderAttachment> masterDryWetAttachment;

    // ⚡ Tame / Width アタッチメント
    std::unique_ptr<SliderAttachment> bandTameAttachments[3];
    std::unique_ptr<SliderAttachment> bandWidthAttachments[3];

    std::unique_ptr<SliderAttachment> bandAttachmentsM[3][4];
    std::unique_ptr<SliderAttachment> bandAttachmentsS[3][4];

    std::unique_ptr<ButtonAttachment> bandBypassAttachments[3];
    std::unique_ptr<ButtonAttachment> bandSoloAttachments[3];
    std::unique_ptr<ButtonAttachment> bandDeltaAttachments[3];
    std::unique_ptr<ButtonAttachment> bandLinkAttachments[3];

    AnalysisFrame latestFrame{};
    bool currentTabIsSide[3] = { false, false, false };
    double progressValue = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAEditor)
};

#endif // LUMINA_PLUGINEDITOR_H