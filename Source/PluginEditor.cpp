#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAEditor::LUMINAEditor(LUMINAProcessor& p)
    : AudioProcessorEditor(&p),
    processor(p),
    spectrumAnalyzer(p.apvts),
    autoBandProgress(progressValue)
{
    setLookAndFeel(&customLookAndFeel);

    // --- Visualizers ---
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(grMeter);
    grMeter.setInterceptsMouseClicks(false, false);

    // --- Global Controls ---
    msModeButton.setButtonText("M/S Mode");
    addAndMakeVisible(msModeButton);
    msModeAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "MS_MODE", msModeButton);

    autoLevelButton.setButtonText("Auto Level");
    addAndMakeVisible(autoLevelButton);
    autoLevelAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "AUTO_LEVEL", autoLevelButton);

    // ⚡ 追加: Auto Level Commit 関連のUIセットアップ
    addAndMakeVisible(autoLevelValueLabel);
    autoLevelValueLabel.setJustificationType(juce::Justification::centred);
    autoLevelValueLabel.setFont(14.0f);
    autoLevelValueLabel.setText("Δ: 0.0 dB", juce::dontSendNotification);

    autoLevelCommitBtn.setButtonText("Commit");
    addAndMakeVisible(autoLevelCommitBtn);
    autoLevelCommitBtn.onClick = [this] {
        // 現在の補正ゲインを取得してdBに変換
        float currentGain = processor.analyzerCore.getMatchingGain();
        float deltaDB = juce::Decibels::gainToDecibels(currentGain);

        // 現在の Master Out Gain に加算 (-24 ~ +24の範囲にクランプ)
        float currentOut = processor.apvts.getRawParameterValue("MASTER_OUT")->load();
        float newOut = juce::jlimit(-24.0f, 24.0f, currentOut + deltaDB);

        if (auto* param = processor.apvts.getParameter("MASTER_OUT"))
            param->setValueNotifyingHost(param->convertTo0to1(newOut));

        // 適用したらAuto LevelをOFFにする
        if (auto* param = processor.apvts.getParameter("AUTO_LEVEL"))
            param->setValueNotifyingHost(0.0f);
        };

    // ⚡ Auto Band Time Combo
    autoBandTimeCombo.addItemList({ "3s", "10s", "30s" }, 1);
    autoBandTimeCombo.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(autoBandTimeCombo);
    autoBandTimeAttachment = std::make_unique<ComboBoxAttachment>(processor.apvts, "AUTO_BAND_TIME", autoBandTimeCombo);

    autoBandButton.setButtonText("Auto Band");
    addAndMakeVisible(autoBandButton);
    autoBandButton.onClick = [this] {
        if (auto* param = processor.apvts.getParameter("AUTO_BAND"))
            param->setValueNotifyingHost(1.0f);
        };

    addAndMakeVisible(autoBandProgress);
    autoBandProgress.setPercentageDisplay(true);
    autoBandProgress.setTextToDisplay("Ready");

    // --- Master Section ⚡ ---
    auto setupMasterSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name, bool isVertical) {
        if (isVertical) {
            s.setSliderStyle(juce::Slider::LinearVertical);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        }
        else {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        }
        addAndMakeVisible(s);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(12.0f);
        addAndMakeVisible(l);
        };

    masterTitle.setText("MASTER", juce::dontSendNotification);
    masterTitle.setJustificationType(juce::Justification::centred);
    masterTitle.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(masterTitle);

    setupMasterSlider(masterInSlider, masterInLabel, "In Gain", true);
    masterInAttachment = std::make_unique<SliderAttachment>(processor.apvts, "MASTER_IN", masterInSlider);

    setupMasterSlider(masterOutSlider, masterOutLabel, "Out Gain", true);
    masterOutAttachment = std::make_unique<SliderAttachment>(processor.apvts, "MASTER_OUT", masterOutSlider);

    setupMasterSlider(masterDryWetSlider, masterDryWetLabel, "Dry/Wet", false);
    masterDryWetAttachment = std::make_unique<SliderAttachment>(processor.apvts, "MASTER_WET", masterDryWetSlider);

    // --- Band UI Setup ---
    auto setupSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        addChildComponent(s);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(13.0f);
        addChildComponent(l);
        };

    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };
    juce::StringArray paramNames = { "THRESH", "DEPTH", "TONAL", "TRANS" };
    juce::StringArray labelNames = { "Threshold", "Depth", "Tonal", "Transient" };
    juce::StringArray titles = { "LOW BAND", "MID BAND", "HIGH BAND" };

    for (int b = 0; b < 3; ++b) {
        bandTitles[b].setText(titles[b], juce::dontSendNotification);
        bandTitles[b].setJustificationType(juce::Justification::centred);
        bandTitles[b].setFont(juce::Font(16.0f, juce::Font::bold));
        addAndMakeVisible(bandTitles[b]);

        tabMid[b].setButtonText("Mid/St");
        tabSide[b].setButtonText("Side");
        bandLink[b].setButtonText("Link");
        addAndMakeVisible(tabMid[b]);
        addAndMakeVisible(tabSide[b]);
        addAndMakeVisible(bandLink[b]);

        tabMid[b].onClick = [this, b] { setBandTab(b, false); };
        tabSide[b].onClick = [this, b] { setBandTab(b, true); };
        bandLinkAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "LINK", bandLink[b]);

        // ⚡ Bypass / Solo / Delta
        bandBypass[b].setButtonText("Bypass");
        bandSolo[b].setButtonText("Solo");
        bandDelta[b].setButtonText("Delta");

        // ToggleButtonのスタイルをプッシュボタン風にする
        bandBypass[b].setClickingTogglesState(true);
        bandSolo[b].setClickingTogglesState(true);
        bandDelta[b].setClickingTogglesState(true);

        addAndMakeVisible(bandBypass[b]);
        addAndMakeVisible(bandSolo[b]);
        addAndMakeVisible(bandDelta[b]);

        bandBypassAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "BYPASS", bandBypass[b]);
        bandSoloAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "SOLO", bandSolo[b]);
        bandDeltaAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "DELTA", bandDelta[b]);

        for (int p = 0; p < 4; ++p) {
            setupSlider(bandSlidersM[b][p], bandLabelsM[b][p], labelNames[p] + " (M)");
            bandAttachmentsM[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_M", bandSlidersM[b][p]);

            setupSlider(bandSlidersS[b][p], bandLabelsS[b][p], labelNames[p] + " (S)");
            bandAttachmentsS[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_S", bandSlidersS[b][p]);
        }
        setBandTab(b, false);
    }

    setSize(1000, 750); // マスター追加に伴い横幅を少し拡張
    startTimerHz(30);
}

LUMINAEditor::~LUMINAEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void LUMINAEditor::setBandTab(int bandIndex, bool showSide)
{
    currentTabIsSide[bandIndex] = showSide;
    tabMid[bandIndex].setColour(juce::TextButton::buttonColourId, showSide ? juce::Colour::fromString("FF333333") : juce::Colour::fromString("FF764DFF"));
    tabSide[bandIndex].setColour(juce::TextButton::buttonColourId, showSide ? juce::Colour::fromString("FF764DFF") : juce::Colour::fromString("FF333333"));

    for (int p = 0; p < 4; ++p) {
        bandSlidersM[bandIndex][p].setVisible(!showSide);
        bandLabelsM[bandIndex][p].setVisible(!showSide);
        bandSlidersS[bandIndex][p].setVisible(showSide);
        bandLabelsS[bandIndex][p].setVisible(showSide);
    }
}

void LUMINAEditor::updateAutoBandUI()
{
    auto state = processor.analyzerCore.getAutoBandState();
    progressValue = processor.analyzerCore.getAutoBandProgress();

    if (state == AnalyzerCore::State::Idle) {
        autoBandButton.setEnabled(true);
        autoBandButton.setButtonText("Auto Band");
        autoBandProgress.setTextToDisplay("Ready");
    }
    else if (state == AnalyzerCore::State::WaitingForSignal) {
        autoBandButton.setEnabled(false);
        autoBandButton.setButtonText("Waiting...");
        autoBandProgress.setTextToDisplay("Signal Needed");
    }
    else if (state == AnalyzerCore::State::Analyzing) {
        autoBandButton.setEnabled(false);
        autoBandButton.setButtonText("Analyzing...");
        autoBandProgress.setTextToDisplay("Capturing...");
    }
    else if (state == AnalyzerCore::State::Done) {
        autoBandButton.setEnabled(true); // ⚡ 再トリガーのために有効化
        autoBandButton.setButtonText("Retrigger");
        autoBandProgress.setTextToDisplay("Optimized");
    }
}

void LUMINAEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));

    // マスターセクションとの境界線を引く
    auto bounds = getLocalBounds().toFloat();
    float masterX = bounds.getWidth() - 140.0f;
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawVerticalLine(static_cast<int>(masterX), 300.0f, bounds.getHeight());
}

void LUMINAEditor::resized()
{
    auto bounds = getLocalBounds();

    // 1. Top Area (Visualizer) - 全幅使用
    auto visArea = bounds.removeFromTop(300);
    spectrumAnalyzer.setBounds(visArea.reduced(10));
    grMeter.setBounds(visArea.reduced(10));

    // 2. 左右のパネル分割 (Main Area:Master Area = 860:140)
    auto masterArea = bounds.removeFromRight(140).reduced(10);
    auto mainArea = bounds.reduced(15);

    // --- Master Area Layout ---
    masterTitle.setBounds(masterArea.removeFromTop(30));
    masterArea.removeFromTop(10);

    auto faderArea = masterArea.removeFromTop(180);
    auto inArea = faderArea.removeFromLeft(faderArea.getWidth() / 2);
    auto outArea = faderArea;

    masterInLabel.setBounds(inArea.removeFromBottom(20));
    masterInSlider.setBounds(inArea);

    masterOutLabel.setBounds(outArea.removeFromBottom(20));
    masterOutSlider.setBounds(outArea);

    masterArea.removeFromTop(20);
    masterDryWetLabel.setBounds(masterArea.removeFromTop(20));
    masterDryWetSlider.setBounds(masterArea.removeFromTop(80));

    // --- Main Area Layout (Global + Bands) ---
    auto globalRow = mainArea.removeFromTop(40);

    msModeButton.setBounds(globalRow.removeFromLeft(90).withSizeKeepingCentre(80, 30));
    globalRow.removeFromLeft(10);

    // ⚡ 修正: Auto Level, Label, Commit を横に並べる
    autoLevelButton.setBounds(globalRow.removeFromLeft(90).withSizeKeepingCentre(80, 30));
    autoLevelValueLabel.setBounds(globalRow.removeFromLeft(70).withSizeKeepingCentre(60, 20));
    autoLevelCommitBtn.setBounds(globalRow.removeFromLeft(70).withSizeKeepingCentre(60, 24));

    auto bandBtnArea = globalRow.removeFromRight(90);
    autoBandButton.setBounds(bandBtnArea.withSizeKeepingCentre(80, 30));
    globalRow.removeFromRight(10);
    // ...続く (autoBandProgress等の配置)

    autoBandProgress.setBounds(globalRow.removeFromRight(150).withSizeKeepingCentre(140, 20));
    globalRow.removeFromRight(10);

    autoBandTimeCombo.setBounds(globalRow.removeFromRight(60).withSizeKeepingCentre(60, 24));

    mainArea.removeFromTop(10);
    int panelWidth = mainArea.getWidth() / 3;

    for (int b = 0; b < 3; ++b) {
        auto panelBounds = mainArea.removeFromLeft(panelWidth).reduced(8);

        bandTitles[b].setBounds(panelBounds.removeFromTop(25));
        panelBounds.removeFromTop(5);

        auto tabRow = panelBounds.removeFromTop(25);
        tabMid[b].setBounds(tabRow.removeFromLeft(60));
        tabRow.removeFromLeft(5);
        tabSide[b].setBounds(tabRow.removeFromLeft(60));
        tabRow.removeFromLeft(10);
        bandLink[b].setBounds(tabRow.removeFromLeft(60));

        panelBounds.removeFromTop(10);

        // ⚡ Bypass / Solo / Delta Row
        auto btnRow = panelBounds.removeFromTop(30);
        int btnW = btnRow.getWidth() / 3;
        bandBypass[b].setBounds(btnRow.removeFromLeft(btnW).reduced(2, 0));
        bandSolo[b].setBounds(btnRow.removeFromLeft(btnW).reduced(2, 0));
        bandDelta[b].setBounds(btnRow.reduced(2, 0));

        panelBounds.removeFromTop(20);

        // --- Knobs Layout ---
        auto topKnobRow = panelBounds.removeFromTop(80);
        auto threshArea = topKnobRow.removeFromLeft(topKnobRow.getWidth() / 2);
        auto depthArea = topKnobRow;

        bandLabelsM[b][0].setBounds(threshArea.removeFromTop(20));
        bandSlidersM[b][0].setBounds(threshArea);
        bandLabelsS[b][0].setBounds(bandLabelsM[b][0].getBounds());
        bandSlidersS[b][0].setBounds(bandSlidersM[b][0].getBounds());

        bandLabelsM[b][1].setBounds(depthArea.removeFromTop(20));
        bandSlidersM[b][1].setBounds(depthArea);
        bandLabelsS[b][1].setBounds(bandLabelsM[b][1].getBounds());
        bandSlidersS[b][1].setBounds(bandSlidersM[b][1].getBounds());

        panelBounds.removeFromTop(10);

        auto botKnobRow = panelBounds.removeFromTop(80);
        auto tonalArea = botKnobRow.removeFromLeft(botKnobRow.getWidth() / 2);
        auto transArea = botKnobRow;

        bandLabelsM[b][2].setBounds(tonalArea.removeFromTop(20));
        bandSlidersM[b][2].setBounds(tonalArea);
        bandLabelsS[b][2].setBounds(bandLabelsM[b][2].getBounds());
        bandSlidersS[b][2].setBounds(bandSlidersM[b][2].getBounds());

        bandLabelsM[b][3].setBounds(transArea.removeFromTop(20));
        bandSlidersM[b][3].setBounds(transArea);
        bandLabelsS[b][3].setBounds(bandLabelsM[b][3].getBounds());
        bandSlidersS[b][3].setBounds(bandSlidersM[b][3].getBounds());
    }
}

void LUMINAEditor::timerCallback()
{
    updateAutoBandUI();

    // ⚡ 追加: Auto Levelの数値表示更新
    float matchGain = processor.analyzerCore.getMatchingGain();
    float matchDB = juce::Decibels::gainToDecibels(matchGain);
    juce::String sign = (matchDB >= 0.0f && matchDB > 0.05f) ? "+" : "";
    autoLevelValueLabel.setText("Δ: " + sign + juce::String(matchDB, 1) + " dB", juce::dontSendNotification);

    bool isAutoLevelOn = processor.apvts.getRawParameterValue("AUTO_LEVEL")->load() > 0.5f;
    autoLevelCommitBtn.setEnabled(isAutoLevelOn); // ONの時だけCommit可能

    // ...以下既存のコード
    float c1 = processor.apvts.getRawParameterValue("CROSS_1")->load();
    float c2 = processor.apvts.getRawParameterValue("CROSS_2")->load();
    spectrumAnalyzer.setCrossovers(c1, c2);

    AnalysisFrame frame;
    bool hasNewData = false;
    while (processor.analysisFifo.pop(frame)) {
        latestFrame = frame;
        hasNewData = true;
    }
    if (hasNewData) {
        spectrumAnalyzer.updateFrame(latestFrame);
        grMeter.updateFrame(latestFrame);
        repaint();
    }

    // --- UX Control Logic ---
    bool isMSMode = processor.apvts.getRawParameterValue("MS_MODE")->load() > 0.5f;
    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };

    for (int b = 0; b < 3; ++b) {
        // ⚡ Bypassによるグレーアウト処理
        bool isBypass = processor.apvts.getRawParameterValue(prefixes[b] + "BYPASS")->load() > 0.5f;
        float alpha = isBypass ? 0.25f : 1.0f;

        for (int p = 0; p < 4; ++p) {
            bandSlidersM[b][p].setAlpha(alpha);
            bandLabelsM[b][p].setAlpha(alpha);
            bandSlidersS[b][p].setAlpha(alpha);
            bandLabelsS[b][p].setAlpha(alpha);
        }

        if (!isMSMode) {
            tabMid[b].setButtonText("Stereo");
            tabSide[b].setVisible(false);
            bandLink[b].setVisible(false);
            if (currentTabIsSide[b]) setBandTab(b, false);
        }
        else {
            tabMid[b].setButtonText("Mid");
            tabSide[b].setVisible(true);
            bandLink[b].setVisible(true);

            bool isLinked = processor.apvts.getRawParameterValue(prefixes[b] + "LINK")->load() > 0.5f;
            tabSide[b].setEnabled(!isLinked);
            if (isLinked && currentTabIsSide[b]) setBandTab(b, false);
        }
    }
}