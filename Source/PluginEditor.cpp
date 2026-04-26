#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAEditor::LUMINAEditor(LUMINAProcessor& p)
    : AudioProcessorEditor(&p),
    processor(p),
    spectrumAnalyzer(p.apvts),
    autoBandProgress(progressValue) // ⚡ プログレスバーをメンバー変数と連動
{
    setLookAndFeel(&customLookAndFeel);

    // --- Global Visualization ---
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(grMeter);
    grMeter.setInterceptsMouseClicks(false, false);

    // --- Global Controls (STEP 3) ---
    msModeButton.setButtonText("M/S Mode");
    addAndMakeVisible(msModeButton);
    msModeAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "MS_MODE", msModeButton);

    autoLevelButton.setButtonText("Auto Level");
    addAndMakeVisible(autoLevelButton);
    autoLevelAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "AUTO_LEVEL", autoLevelButton);

    autoBandButton.setButtonText("Auto Band");
    addAndMakeVisible(autoBandButton);
    // ⚡ ボタンクリックで APVTS の AUTO_BAND パラメータを ON にする
    autoBandButton.onClick = [this] {
        if (auto* param = processor.apvts.getParameter("AUTO_BAND"))
            param->setValueNotifyingHost(1.0f);
        };

    addAndMakeVisible(autoBandProgress);
    autoBandProgress.setPercentageDisplay(true);
    autoBandProgress.setTextToDisplay("Ready");

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

        for (int p = 0; p < 4; ++p) {
            setupSlider(bandSlidersM[b][p], bandLabelsM[b][p], labelNames[p] + " (M)");
            bandAttachmentsM[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_M", bandSlidersM[b][p]);

            setupSlider(bandSlidersS[b][p], bandLabelsS[b][p], labelNames[p] + " (S)");
            bandAttachmentsS[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_S", bandSlidersS[b][p]);
        }

        bandButtons[b][0].setButtonText("Solo");
        bandButtons[b][1].setButtonText("Delta");
        addAndMakeVisible(bandButtons[b][0]);
        addAndMakeVisible(bandButtons[b][1]);

        bandButtonAttachments[b][0] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "SOLO", bandButtons[b][0]);
        bandButtonAttachments[b][1] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "DELTA", bandButtons[b][1]);

        setBandTab(b, false);
    }

    setSize(960, 750);
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
        autoBandProgress.setTextToDisplay("Capturing Spectrum");
    }
    else if (state == AnalyzerCore::State::Done) {
        autoBandButton.setEnabled(true);
        autoBandButton.setButtonText("Done!");
        autoBandProgress.setTextToDisplay("Optimized");
    }
}

void LUMINAEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void LUMINAEditor::resized()
{
    auto bounds = getLocalBounds();

    // 上部: アナライザー
    auto visArea = bounds.removeFromTop(300);
    spectrumAnalyzer.setBounds(visArea.reduced(10));
    grMeter.setBounds(visArea.reduced(10));

    auto uiArea = bounds.reduced(15);

    // --- Global Controls Row (STEP 3 Layout) ---
    auto globalRow = uiArea.removeFromTop(40);

    // M/S & Auto Level
    msModeButton.setBounds(globalRow.removeFromLeft(100).withSizeKeepingCentre(90, 30));
    globalRow.removeFromLeft(10);
    autoLevelButton.setBounds(globalRow.removeFromLeft(100).withSizeKeepingCentre(90, 30));

    // Auto Band & Progress (右側に配置)
    auto progressArea = globalRow.removeFromRight(200);
    autoBandProgress.setBounds(progressArea.withSizeKeepingCentre(190, 20));
    globalRow.removeFromRight(10);
    autoBandButton.setBounds(globalRow.removeFromRight(100).withSizeKeepingCentre(90, 30));

    uiArea.removeFromTop(10);

    int panelWidth = uiArea.getWidth() / 3;

    for (int b = 0; b < 3; ++b) {
        auto panelBounds = uiArea.removeFromLeft(panelWidth).reduced(8);

        bandTitles[b].setBounds(panelBounds.removeFromTop(25));
        panelBounds.removeFromTop(5);

        auto tabRow = panelBounds.removeFromTop(25);
        tabMid[b].setBounds(tabRow.removeFromLeft(60));
        tabRow.removeFromLeft(5);
        tabSide[b].setBounds(tabRow.removeFromLeft(60));
        tabRow.removeFromLeft(15);
        bandLink[b].setBounds(tabRow);

        panelBounds.removeFromTop(15);

        auto btnRow = panelBounds.removeFromTop(30);
        bandButtons[b][0].setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2).reduced(5, 0));
        bandButtons[b][1].setBounds(btnRow.reduced(5, 0));

        panelBounds.removeFromTop(20);

        // --- Knobs Layout ---
        auto topKnobRow = panelBounds.removeFromTop(90);
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

        auto botKnobRow = panelBounds.removeFromTop(90);
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
    // --- 解析UIの更新 ---
    updateAutoBandUI();

    // --- アナライザーの更新 ---
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

    // --- M/S & Link の動的制御 (STEP 2 UX改善ロジック) ---
    bool isMSMode = processor.apvts.getRawParameterValue("MS_MODE")->load() > 0.5f;
    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };

    for (int b = 0; b < 3; ++b) {
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