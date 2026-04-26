#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAEditor::LUMINAEditor(LUMINAProcessor& p)
    : AudioProcessorEditor(&p),
    processor(p),
    spectrumAnalyzer(p.apvts)
{
    setLookAndFeel(&customLookAndFeel);

    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(grMeter);

    // GRMeterをマウスクリックに対して透過させる（STEP 1 の修正）
    grMeter.setInterceptsMouseClicks(false, false);

    auto setupSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        addChildComponent(s); // addAndMakeVisibleの代わりにaddChild（初期状態は非表示）

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
        // --- Titles ---
        bandTitles[b].setText(titles[b], juce::dontSendNotification);
        bandTitles[b].setJustificationType(juce::Justification::centred);
        bandTitles[b].setFont(juce::Font(16.0f, juce::Font::bold));
        addAndMakeVisible(bandTitles[b]);

        // --- Tabs & Link ---
        tabMid[b].setButtonText("Mid/St");
        tabSide[b].setButtonText("Side");
        bandLink[b].setButtonText("Link");

        addAndMakeVisible(tabMid[b]);
        addAndMakeVisible(tabSide[b]);
        addAndMakeVisible(bandLink[b]);

        // タブ切り替えのイベントハンドラ
        tabMid[b].onClick = [this, b] { setBandTab(b, false); };
        tabSide[b].onClick = [this, b] { setBandTab(b, true); };

        bandLinkAttachments[b] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "LINK", bandLink[b]);

        // --- Sliders M & S ---
        for (int p = 0; p < 4; ++p) {
            // Mid / Stereo
            setupSlider(bandSlidersM[b][p], bandLabelsM[b][p], labelNames[p] + " (M)");
            bandAttachmentsM[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_M", bandSlidersM[b][p]);

            // Side
            setupSlider(bandSlidersS[b][p], bandLabelsS[b][p], labelNames[p] + " (S)");
            bandAttachmentsS[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p] + "_S", bandSlidersS[b][p]);
        }

        // --- Buttons ---
        bandButtons[b][0].setButtonText("Solo");
        bandButtons[b][1].setButtonText("Delta");
        addAndMakeVisible(bandButtons[b][0]);
        addAndMakeVisible(bandButtons[b][1]);

        bandButtonAttachments[b][0] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "SOLO", bandButtons[b][0]);
        bandButtonAttachments[b][1] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "DELTA", bandButtons[b][1]);

        // 初期状態は Mid タブを表示
        setBandTab(b, false);
    }

    msModeButton.setButtonText("M/S Mode");
    addAndMakeVisible(msModeButton);
    msModeAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "MS_MODE", msModeButton);

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

    // タブの色（見た目）を更新
    tabMid[bandIndex].setColour(juce::TextButton::buttonColourId, showSide ? juce::Colour::fromString("FF333333") : juce::Colour::fromString("FF764DFF"));
    tabSide[bandIndex].setColour(juce::TextButton::buttonColourId, showSide ? juce::Colour::fromString("FF764DFF") : juce::Colour::fromString("FF333333"));

    // スライダーの表示・非表示を切り替え
    for (int p = 0; p < 4; ++p) {
        bandSlidersM[bandIndex][p].setVisible(!showSide);
        bandLabelsM[bandIndex][p].setVisible(!showSide);

        bandSlidersS[bandIndex][p].setVisible(showSide);
        bandLabelsS[bandIndex][p].setVisible(showSide);
    }
}

void LUMINAEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void LUMINAEditor::resized()
{
    auto bounds = getLocalBounds();

    // 上部: アナライザー (高さ 300px)
    auto visArea = bounds.removeFromTop(300);
    spectrumAnalyzer.setBounds(visArea.reduced(10));
    grMeter.setBounds(visArea.reduced(10));

    auto uiArea = bounds.reduced(15);

    auto globalRow = uiArea.removeFromTop(40);
    auto msArea = globalRow.removeFromLeft(120);
    msModeButton.setBounds(msArea.withSizeKeepingCentre(100, 30));

    uiArea.removeFromTop(10);

    int panelWidth = uiArea.getWidth() / 3;

    for (int b = 0; b < 3; ++b) {
        auto panelBounds = uiArea.removeFromLeft(panelWidth).reduced(8);

        bandTitles[b].setBounds(panelBounds.removeFromTop(25));
        panelBounds.removeFromTop(5);

        // --- Tabs & Link Row ---
        auto tabRow = panelBounds.removeFromTop(25);
        tabMid[b].setBounds(tabRow.removeFromLeft(60));
        tabRow.removeFromLeft(5); // 余白
        tabSide[b].setBounds(tabRow.removeFromLeft(60));
        tabRow.removeFromLeft(15); // 余白
        bandLink[b].setBounds(tabRow);

        panelBounds.removeFromTop(15);

        // --- Solo & Delta Row ---
        auto btnRow = panelBounds.removeFromTop(30);
        bandButtons[b][0].setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2).reduced(5, 0));
        bandButtons[b][1].setBounds(btnRow.reduced(5, 0));

        panelBounds.removeFromTop(20);

        // --- Top Knobs (Thresh & Depth) ---
        auto topKnobRow = panelBounds.removeFromTop(90);
        auto threshArea = topKnobRow.removeFromLeft(topKnobRow.getWidth() / 2);
        auto depthArea = topKnobRow;

        // Thresh 座標設定 (MidとSideを完全に重ねる)
        auto tAreaM = threshArea;
        bandLabelsM[b][0].setBounds(tAreaM.removeFromTop(20));
        bandSlidersM[b][0].setBounds(tAreaM);
        bandLabelsS[b][0].setBounds(bandLabelsM[b][0].getBounds());
        bandSlidersS[b][0].setBounds(bandSlidersM[b][0].getBounds());

        // Depth 座標設定
        auto dAreaM = depthArea;
        bandLabelsM[b][1].setBounds(dAreaM.removeFromTop(20));
        bandSlidersM[b][1].setBounds(dAreaM);
        bandLabelsS[b][1].setBounds(bandLabelsM[b][1].getBounds());
        bandSlidersS[b][1].setBounds(bandSlidersM[b][1].getBounds());

        panelBounds.removeFromTop(10);

        // --- Bottom Knobs (Tonal & Trans) ---
        auto botKnobRow = panelBounds.removeFromTop(90);
        auto tonalArea = botKnobRow.removeFromLeft(botKnobRow.getWidth() / 2);
        auto transArea = botKnobRow;

        // Tonal 座標設定
        auto toAreaM = tonalArea;
        bandLabelsM[b][2].setBounds(toAreaM.removeFromTop(20));
        bandSlidersM[b][2].setBounds(toAreaM);
        bandLabelsS[b][2].setBounds(bandLabelsM[b][2].getBounds());
        bandSlidersS[b][2].setBounds(bandSlidersM[b][2].getBounds());

        // Trans 座標設定
        auto trAreaM = transArea;
        bandLabelsM[b][3].setBounds(trAreaM.removeFromTop(20));
        bandSlidersM[b][3].setBounds(trAreaM);
        bandLabelsS[b][3].setBounds(bandLabelsM[b][3].getBounds());
        bandSlidersS[b][3].setBounds(bandSlidersM[b][3].getBounds());
    }
}

void LUMINAEditor::timerCallback()
{
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
}