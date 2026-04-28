// ==========================================
// Source/PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAEditor::LUMINAEditor(LUMINAProcessor& p)
    : AudioProcessorEditor(&p),
    processor(p),
    spectrumAnalyzer(p.apvts),
    autoBandProgress(progressValue)
{
    setLookAndFeel(&customLookAndFeel);

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

    addAndMakeVisible(autoLevelValueLabel);
    autoLevelValueLabel.setJustificationType(juce::Justification::centred);
    autoLevelValueLabel.setFont(14.0f);
    autoLevelValueLabel.setText("Diff: 0.0 dB", juce::dontSendNotification);

    autoLevelCommitBtn.setButtonText("Commit");
    addAndMakeVisible(autoLevelCommitBtn);
    autoLevelCommitBtn.onClick = [this] {
        float currentGain = processor.analyzerCore.getMatchingGain();
        float deltaDB = juce::Decibels::gainToDecibels(currentGain);
        float currentOut = processor.apvts.getRawParameterValue("MASTER_OUT")->load();
        float newOut = juce::jlimit(-24.0f, 24.0f, currentOut + deltaDB);

        if (auto* param = processor.apvts.getParameter("MASTER_OUT"))
            param->setValueNotifyingHost(param->convertTo0to1(newOut));

        if (auto* param = processor.apvts.getParameter("AUTO_LEVEL"))
            param->setValueNotifyingHost(0.0f);
        };

    // ⚡ Pro Mode Button
    proModeButton.setButtonText("Professional");
    addAndMakeVisible(proModeButton);
    proModeAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "PRO_MODE", proModeButton);

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

    auto setupRotary = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 14);
        addChildComponent(s);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(12.0f);
        addChildComponent(l);
        };

    // --- Master & Global Pro Section ---
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

    oversamplingCombo.addItemList({ "Off", "2x", "4x" }, 1);
    oversamplingCombo.setJustificationType(juce::Justification::centred);
    addChildComponent(oversamplingCombo);
    oversamplingLabel.setText("Oversampling", juce::dontSendNotification);
    oversamplingLabel.setJustificationType(juce::Justification::centred);
    oversamplingLabel.setFont(12.0f);
    addChildComponent(oversamplingLabel);
    oversamplingAttachment = std::make_unique<ComboBoxAttachment>(processor.apvts, "OVERSAMPLING", oversamplingCombo);

    setupRotary(lookaheadSlider, lookaheadLabel, "Lookahead");
    lookaheadAttachment = std::make_unique<SliderAttachment>(processor.apvts, "LOOKAHEAD", lookaheadSlider);

    setupRotary(widthCross1Slider, widthCross1Label, "Width X1");
    widthCross1Attachment = std::make_unique<SliderAttachment>(processor.apvts, "WIDTH_CROSS_1", widthCross1Slider);

    setupRotary(widthCross2Slider, widthCross2Label, "Width X2");
    widthCross2Attachment = std::make_unique<SliderAttachment>(processor.apvts, "WIDTH_CROSS_2", widthCross2Slider);

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

        bandBypass[b].setButtonText("Bypass");
        bandSolo[b].setButtonText("Solo");
        bandDelta[b].setButtonText("Delta");

        bandBypass[b].setClickingTogglesState(true);
        bandSolo[b].setClickingTogglesState(true);
        bandDelta[b].setClickingTogglesState(true);

        addAndMakeVisible(bandBypass[b]);
        addAndMakeVisible(bandSolo[b]);
        addAndMakeVisible(bandDelta[b]);

        bandBypassAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "BYPASS", bandBypass[b]);
        bandSoloAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "SOLO", bandSolo[b]);
        bandDeltaAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "DELTA", bandDelta[b]);

        bandTame[b].setSliderStyle(juce::Slider::LinearHorizontal);
        bandTame[b].setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 16);
        addAndMakeVisible(bandTame[b]);
        bandTameLabel[b].setText("Tame", juce::dontSendNotification);
        bandTameLabel[b].setFont(13.0f);
        addAndMakeVisible(bandTameLabel[b]);
        bandTameAttachments[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "TAME", bandTame[b]);

        bandWidth[b].setSliderStyle(juce::Slider::LinearHorizontal);
        bandWidth[b].setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 16);
        addAndMakeVisible(bandWidth[b]);
        bandWidthLabel[b].setText("Width", juce::dontSendNotification);
        bandWidthLabel[b].setFont(13.0f);
        addAndMakeVisible(bandWidthLabel[b]);
        bandWidthAttachments[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "WIDTH", bandWidth[b]);

        for (int p = 0; p < 4; ++p) {
            setupRotary(bandSlidersM[b][p], bandLabelsM[b][p], labelNames[p] + " (M)");
            bandAttachmentsM[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_M", bandSlidersM[b][p]);

            setupRotary(bandSlidersS[b][p], bandLabelsS[b][p], labelNames[p] + " (S)");
            bandAttachmentsS[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_S", bandSlidersS[b][p]);
        }

        // ⚡ Pro Knobs Setup
        setupRotary(proTameSharp[b], proTameSharpLabel[b], "Sharpness");
        proTameSharpAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "TAME_SHARP", proTameSharp[b]);

        setupRotary(proTameSpeed[b], proTameSpeedLabel[b], "Tame Spd");
        proTameSpeedAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "TAME_SPEED", proTameSpeed[b]);

        setupRotary(proAttackM[b], proAttackMLabel[b], "Attack (M)");
        proAttackMAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "ATTACK_M", proAttackM[b]);

        setupRotary(proAttackS[b], proAttackSLabel[b], "Attack (S)");
        proAttackSAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "ATTACK_S", proAttackS[b]);

        setupRotary(proReleaseM[b], proReleaseMLabel[b], "Release (M)");
        proReleaseMAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "RELEASE_M", proReleaseM[b]);

        setupRotary(proReleaseS[b], proReleaseSLabel[b], "Release (S)");
        proReleaseSAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "RELEASE_S", proReleaseS[b]);

        setupRotary(proHpssBlur[b], proHpssBlurLabel[b], "HPSS Blur");
        proHpssBlurAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "HPSS_BLUR", proHpssBlur[b]);

        setupRotary(proHpssRes[b], proHpssResLabel[b], "HPSS Res");
        proHpssResAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "HPSS_RES", proHpssRes[b]);

        setupRotary(proLinkAmt[b], proLinkAmtLabel[b], "Link Amt");
        proLinkAmtAtt[b] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + "LINK_AMT", proLinkAmt[b]);

        setBandTab(b, false);
    }

    setSize(1000, 750);
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

    bool isPro = processor.apvts.getRawParameterValue("PRO_MODE")->load() > 0.5f;

    for (int p = 0; p < 4; ++p) {
        bandSlidersM[bandIndex][p].setVisible(!isPro && !showSide);
        bandLabelsM[bandIndex][p].setVisible(!isPro && !showSide);
        bandSlidersS[bandIndex][p].setVisible(!isPro && showSide);
        bandLabelsS[bandIndex][p].setVisible(!isPro && showSide);
    }

    proAttackM[bandIndex].setVisible(isPro && !showSide);
    proAttackMLabel[bandIndex].setVisible(isPro && !showSide);
    proAttackS[bandIndex].setVisible(isPro && showSide);
    proAttackSLabel[bandIndex].setVisible(isPro && showSide);

    proReleaseM[bandIndex].setVisible(isPro && !showSide);
    proReleaseMLabel[bandIndex].setVisible(isPro && !showSide);
    proReleaseS[bandIndex].setVisible(isPro && showSide);
    proReleaseSLabel[bandIndex].setVisible(isPro && showSide);
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
        autoBandButton.setEnabled(true);
        autoBandButton.setButtonText("Retrigger");
        autoBandProgress.setTextToDisplay("Optimized");
    }
}

void LUMINAEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
    auto bounds = getLocalBounds().toFloat();
    float masterX = bounds.getWidth() - 140.0f;
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawVerticalLine(static_cast<int>(masterX), 300.0f, bounds.getHeight());
}

void LUMINAEditor::resized()
{
    auto bounds = getLocalBounds();

    auto visArea = bounds.removeFromTop(300);
    spectrumAnalyzer.setBounds(visArea.reduced(10));
    grMeter.setBounds(visArea.reduced(10));

    auto masterArea = bounds.removeFromRight(140).reduced(10);
    auto mainArea = bounds.reduced(15);

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
    masterDryWetSlider.setBounds(masterArea.removeFromTop(75));

    // ⚡ Global Pro Settings (Master Section)
    masterArea.removeFromTop(10);
    oversamplingLabel.setBounds(masterArea.removeFromTop(15));
    oversamplingCombo.setBounds(masterArea.removeFromTop(24));
    masterArea.removeFromTop(5);
    lookaheadLabel.setBounds(masterArea.removeFromTop(15));
    lookaheadSlider.setBounds(masterArea.removeFromTop(60));

    auto globalRow = mainArea.removeFromTop(40);
    msModeButton.setBounds(globalRow.removeFromLeft(90).withSizeKeepingCentre(80, 30));
    globalRow.removeFromLeft(10);

    autoLevelButton.setBounds(globalRow.removeFromLeft(90).withSizeKeepingCentre(80, 30));
    autoLevelValueLabel.setBounds(globalRow.removeFromLeft(70).withSizeKeepingCentre(60, 20));
    autoLevelCommitBtn.setBounds(globalRow.removeFromLeft(70).withSizeKeepingCentre(60, 24));

    // ⚡ Pro Mode Button Layout
    proModeButton.setBounds(globalRow.removeFromLeft(90).withSizeKeepingCentre(80, 24));

    auto bandBtnArea = globalRow.removeFromRight(90);
    autoBandButton.setBounds(bandBtnArea.withSizeKeepingCentre(80, 30));
    globalRow.removeFromRight(10);
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

        auto btnRow = panelBounds.removeFromTop(30);
        int btnW = btnRow.getWidth() / 3;
        bandBypass[b].setBounds(btnRow.removeFromLeft(btnW).reduced(2, 0));
        bandSolo[b].setBounds(btnRow.removeFromLeft(btnW).reduced(2, 0));
        bandDelta[b].setBounds(btnRow.reduced(2, 0));

        panelBounds.removeFromTop(15);

        // --- Normal Mode Layout (Overlapped) ---
        auto normArea = panelBounds;
        auto tameArea = normArea.removeFromTop(20);
        bandTameLabel[b].setBounds(tameArea.removeFromLeft(40));
        bandTame[b].setBounds(tameArea);
        normArea.removeFromTop(15);

        auto topKnobRow = normArea.removeFromTop(75);
        auto threshArea = topKnobRow.removeFromLeft(topKnobRow.getWidth() / 2);
        auto depthArea = topKnobRow;
        bandLabelsM[b][0].setBounds(threshArea.removeFromTop(20)); bandSlidersM[b][0].setBounds(threshArea);
        bandLabelsS[b][0].setBounds(bandLabelsM[b][0].getBounds()); bandSlidersS[b][0].setBounds(bandSlidersM[b][0].getBounds());
        bandLabelsM[b][1].setBounds(depthArea.removeFromTop(20)); bandSlidersM[b][1].setBounds(depthArea);
        bandLabelsS[b][1].setBounds(bandLabelsM[b][1].getBounds()); bandSlidersS[b][1].setBounds(bandSlidersM[b][1].getBounds());

        normArea.removeFromTop(5);
        auto botKnobRow = normArea.removeFromTop(75);
        auto tonalArea = botKnobRow.removeFromLeft(botKnobRow.getWidth() / 2);
        auto transArea = botKnobRow;
        bandLabelsM[b][2].setBounds(tonalArea.removeFromTop(20)); bandSlidersM[b][2].setBounds(tonalArea);
        bandLabelsS[b][2].setBounds(bandLabelsM[b][2].getBounds()); bandSlidersS[b][2].setBounds(bandSlidersM[b][2].getBounds());
        bandLabelsM[b][3].setBounds(transArea.removeFromTop(20)); bandSlidersM[b][3].setBounds(transArea);
        bandLabelsS[b][3].setBounds(bandLabelsM[b][3].getBounds()); bandSlidersS[b][3].setBounds(bandSlidersM[b][3].getBounds());

        normArea.removeFromTop(15);
        auto widthArea = normArea.removeFromTop(20);
        bandWidthLabel[b].setBounds(widthArea.removeFromLeft(40));
        bandWidth[b].setBounds(widthArea);

        // ⚡ --- Pro Mode Layout (Overlapped) 3x3 Grid ---
        auto proArea = panelBounds;
        int proKnobH = 75;
        int proW3 = proArea.getWidth() / 3;

        auto pRow1 = proArea.removeFromTop(proKnobH);
        auto r1a1 = pRow1.removeFromLeft(proW3);
        proTameSharpLabel[b].setBounds(r1a1.removeFromTop(15)); proTameSharp[b].setBounds(r1a1);
        auto r1a2 = pRow1.removeFromLeft(proW3);
        proTameSpeedLabel[b].setBounds(r1a2.removeFromTop(15)); proTameSpeed[b].setBounds(r1a2);
        auto r1a3 = pRow1;
        proAttackMLabel[b].setBounds(r1a3.removeFromTop(15)); proAttackSLabel[b].setBounds(proAttackMLabel[b].getBounds());
        proAttackM[b].setBounds(r1a3); proAttackS[b].setBounds(proAttackM[b].getBounds());

        proArea.removeFromTop(5);
        auto pRow2 = proArea.removeFromTop(proKnobH);
        auto r2a1 = pRow2.removeFromLeft(proW3);
        proReleaseMLabel[b].setBounds(r2a1.removeFromTop(15)); proReleaseSLabel[b].setBounds(proReleaseMLabel[b].getBounds());
        proReleaseM[b].setBounds(r2a1); proReleaseS[b].setBounds(proReleaseM[b].getBounds());
        auto r2a2 = pRow2.removeFromLeft(proW3);
        proHpssBlurLabel[b].setBounds(r2a2.removeFromTop(15)); proHpssBlur[b].setBounds(r2a2);
        auto r2a3 = pRow2;
        proHpssResLabel[b].setBounds(r2a3.removeFromTop(15)); proHpssRes[b].setBounds(r2a3);

        proArea.removeFromTop(5);
        auto pRow3 = proArea.removeFromTop(proKnobH);
        auto r3a1 = pRow3.removeFromLeft(proW3);
        proLinkAmtLabel[b].setBounds(r3a1.removeFromTop(15)); proLinkAmt[b].setBounds(r3a1);

        if (b == 0) {
            auto r3a2 = pRow3.removeFromLeft(proW3);
            widthCross1Label.setBounds(r3a2.removeFromTop(15)); widthCross1Slider.setBounds(r3a2);
        }
        else if (b == 2) {
            auto r3a2 = pRow3.removeFromLeft(proW3);
            widthCross2Label.setBounds(r3a2.removeFromTop(15)); widthCross2Slider.setBounds(r3a2);
        }
    }
}

void LUMINAEditor::timerCallback()
{
    updateAutoBandUI();

    float matchGain = processor.analyzerCore.getMatchingGain();
    float matchDB = juce::Decibels::gainToDecibels(matchGain);
    juce::String sign = (matchDB >= 0.0f && matchDB > 0.05f) ? "+" : "";
    autoLevelValueLabel.setText("Diff: " + sign + juce::String(matchDB, 1) + " dB", juce::dontSendNotification);

    bool isAutoLevelOn = processor.apvts.getRawParameterValue("AUTO_LEVEL")->load() > 0.5f;
    autoLevelCommitBtn.setEnabled(isAutoLevelOn);

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

    bool isMSMode = processor.apvts.getRawParameterValue("MS_MODE")->load() > 0.5f;
    bool isPro = processor.apvts.getRawParameterValue("PRO_MODE")->load() > 0.5f;
    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };

    // ⚡ Global Pro Visibility
    oversamplingCombo.setVisible(isPro);
    oversamplingLabel.setVisible(isPro);
    lookaheadSlider.setVisible(isPro);
    lookaheadLabel.setVisible(isPro);
    widthCross1Slider.setVisible(isPro); widthCross1Label.setVisible(isPro);
    widthCross2Slider.setVisible(isPro); widthCross2Label.setVisible(isPro);

    for (int b = 0; b < 3; ++b) {
        bool isBypass = processor.apvts.getRawParameterValue(prefixes[b] + "BYPASS")->load() > 0.5f;
        float alpha = isBypass ? 0.25f : 1.0f;

        bandTame[b].setAlpha(alpha); bandTameLabel[b].setAlpha(alpha);
        bandWidth[b].setAlpha(alpha); bandWidthLabel[b].setAlpha(alpha);
        proTameSharp[b].setAlpha(alpha); proTameSharpLabel[b].setAlpha(alpha);
        proTameSpeed[b].setAlpha(alpha); proTameSpeedLabel[b].setAlpha(alpha);
        proHpssBlur[b].setAlpha(alpha);  proHpssBlurLabel[b].setAlpha(alpha);
        proHpssRes[b].setAlpha(alpha);   proHpssResLabel[b].setAlpha(alpha);
        proLinkAmt[b].setAlpha(alpha);   proLinkAmtLabel[b].setAlpha(alpha);

        // Visibility Toggles between Normal and Pro Mode
        bandTame[b].setVisible(!isPro); bandTameLabel[b].setVisible(!isPro);
        bandWidth[b].setVisible(!isPro); bandWidthLabel[b].setVisible(!isPro);
        proTameSharp[b].setVisible(isPro); proTameSharpLabel[b].setVisible(isPro);
        proTameSpeed[b].setVisible(isPro); proTameSpeedLabel[b].setVisible(isPro);
        proHpssBlur[b].setVisible(isPro);  proHpssBlurLabel[b].setVisible(isPro);
        proHpssRes[b].setVisible(isPro);   proHpssResLabel[b].setVisible(isPro);
        proLinkAmt[b].setVisible(isPro);   proLinkAmtLabel[b].setVisible(isPro);

        for (int p = 0; p < 4; ++p) {
            bandSlidersM[b][p].setAlpha(alpha); bandLabelsM[b][p].setAlpha(alpha);
            bandSlidersS[b][p].setAlpha(alpha); bandLabelsS[b][p].setAlpha(alpha);

            bandSlidersM[b][p].setVisible(!isPro && !currentTabIsSide[b]);
            bandLabelsM[b][p].setVisible(!isPro && !currentTabIsSide[b]);
            bandSlidersS[b][p].setVisible(!isPro && currentTabIsSide[b]);
            bandLabelsS[b][p].setVisible(!isPro && currentTabIsSide[b]);
        }

        proAttackM[b].setAlpha(alpha); proAttackMLabel[b].setAlpha(alpha);
        proAttackS[b].setAlpha(alpha); proAttackSLabel[b].setAlpha(alpha);
        proReleaseM[b].setAlpha(alpha); proReleaseMLabel[b].setAlpha(alpha);
        proReleaseS[b].setAlpha(alpha); proReleaseSLabel[b].setAlpha(alpha);

        proAttackM[b].setVisible(isPro && !currentTabIsSide[b]);
        proAttackMLabel[b].setVisible(isPro && !currentTabIsSide[b]);
        proAttackS[b].setVisible(isPro && currentTabIsSide[b]);
        proAttackSLabel[b].setVisible(isPro && currentTabIsSide[b]);

        proReleaseM[b].setVisible(isPro && !currentTabIsSide[b]);
        proReleaseMLabel[b].setVisible(isPro && !currentTabIsSide[b]);
        proReleaseS[b].setVisible(isPro && currentTabIsSide[b]);
        proReleaseSLabel[b].setVisible(isPro && currentTabIsSide[b]);

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