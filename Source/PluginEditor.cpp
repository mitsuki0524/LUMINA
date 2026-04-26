#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAEditor::LUMINAEditor(LUMINAProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // カスタムLookAndFeelの適用
    setLookAndFeel(&customLookAndFeel);

    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(grMeter);

    auto setupSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        addAndMakeVisible(s);

        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(13.0f);
        addAndMakeVisible(l);
        };

    setupSlider(crossSliders[0], crossLabels[0], "Cross 1");
    setupSlider(crossSliders[1], crossLabels[1], "Cross 2");
    crossAttachments[0] = std::make_unique<SliderAttachment>(processor.apvts, "CROSS_1", crossSliders[0]);
    crossAttachments[1] = std::make_unique<SliderAttachment>(processor.apvts, "CROSS_2", crossSliders[1]);

    juce::StringArray prefixes = { "B1_", "B2_", "B3_" };
    juce::StringArray paramNames = { "THRESH", "DEPTH", "TONAL", "TRANS" };
    juce::StringArray labelNames = { "Threshold", "Depth", "Tonal", "Transient" };
    juce::StringArray titles = { "LOW BAND", "MID BAND", "HIGH BAND" };

    for (int b = 0; b < 3; ++b) {
        bandTitles[b].setText(titles[b], juce::dontSendNotification);
        bandTitles[b].setJustificationType(juce::Justification::centred);
        bandTitles[b].setFont(juce::Font(16.0f, juce::Font::bold));
        addAndMakeVisible(bandTitles[b]);

        for (int p = 0; p < 4; ++p) {
            setupSlider(bandSliders[b][p], bandLabels[b][p], labelNames[p]);
            bandAttachments[b][p] = std::make_unique<SliderAttachment>(processor.apvts, prefixes[b] + paramNames[p], bandSliders[b][p]);
        }

        bandButtons[b][0].setButtonText("Solo");
        bandButtons[b][1].setButtonText("Delta");
        addAndMakeVisible(bandButtons[b][0]);
        addAndMakeVisible(bandButtons[b][1]);

        bandButtonAttachments[b][0] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "SOLO", bandButtons[b][0]);
        bandButtonAttachments[b][1] = std::make_unique<ButtonAttachment>(processor.apvts, prefixes[b] + "DELTA", bandButtons[b][1]);
    }

    msModeButton.setButtonText("M/S Mode");
    addAndMakeVisible(msModeButton);
    msModeAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "MS_MODE", msModeButton);

    // 余裕を持ったウィンドウサイズ
    setSize(960, 750);
    startTimerHz(30);
}

LUMINAEditor::~LUMINAEditor()
{
    stopTimer();
    setLookAndFeel(nullptr); // メモリリーク防止のため必須
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

    // 中段: クロスオーバーとM/S (高さ 80px)
    auto globalRow = uiArea.removeFromTop(80);

    auto msArea = globalRow.removeFromLeft(120);
    msModeButton.setBounds(msArea.withSizeKeepingCentre(100, 30));

    globalRow.reduce(100, 0); // 中央に寄せるための余白
    auto cross1Area = globalRow.removeFromLeft(globalRow.getWidth() / 2);
    auto cross2Area = globalRow;

    crossLabels[0].setBounds(cross1Area.removeFromTop(20));
    crossSliders[0].setBounds(cross1Area);

    crossLabels[1].setBounds(cross2Area.removeFromTop(20));
    crossSliders[1].setBounds(cross2Area);

    uiArea.removeFromTop(10); // パネルとの間の余白

    // 下段: 3バンドパネル (残りの空間)
    int panelWidth = uiArea.getWidth() / 3;

    for (int b = 0; b < 3; ++b) {
        auto panelBounds = uiArea.removeFromLeft(panelWidth).reduced(8); // 各パネル間の隙間

        bandTitles[b].setBounds(panelBounds.removeFromTop(25));
        panelBounds.removeFromTop(10); // タイトル下の余白

        auto btnRow = panelBounds.removeFromTop(30);
        bandButtons[b][0].setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2).reduced(5, 0));
        bandButtons[b][1].setBounds(btnRow.reduced(5, 0));

        panelBounds.removeFromTop(20);

        // ノブを 2x2 グリッドで配置
        auto topKnobRow = panelBounds.removeFromTop(90);
        auto threshArea = topKnobRow.removeFromLeft(topKnobRow.getWidth() / 2);
        auto depthArea = topKnobRow;

        bandLabels[b][0].setBounds(threshArea.removeFromTop(20));
        bandSliders[b][0].setBounds(threshArea);

        bandLabels[b][1].setBounds(depthArea.removeFromTop(20));
        bandSliders[b][1].setBounds(depthArea);

        panelBounds.removeFromTop(10);

        auto botKnobRow = panelBounds.removeFromTop(90);
        auto tonalArea = botKnobRow.removeFromLeft(botKnobRow.getWidth() / 2);
        auto transArea = botKnobRow;

        bandLabels[b][2].setBounds(tonalArea.removeFromTop(20));
        bandSliders[b][2].setBounds(tonalArea);

        bandLabels[b][3].setBounds(transArea.removeFromTop(20));
        bandSliders[b][3].setBounds(transArea);
    }
}

void LUMINAEditor::timerCallback()
{
    // クロスオーバー値をプロセッサから取得してアナライザーに渡す
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
        repaint();
    }
}