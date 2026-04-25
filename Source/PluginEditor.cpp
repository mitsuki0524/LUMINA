#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LUMINAEditor::LUMINAEditor(LUMINAProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // --- コンポーネントの可視化 ---
    addAndMakeVisible(spectrumAnalyzer);
    addAndMakeVisible(grMeter);

    // --- スライダーのセットアップヘルパー ---
    auto setupSlider = [this](juce::Slider& s, juce::Label& l, const juce::String& name) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        addAndMakeVisible(s);

        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.attachToComponent(&s, false);
        addAndMakeVisible(l);
        };

    setupSlider(thresholdSlider, thresholdLabel, "Threshold");
    setupSlider(depthSlider, depthLabel, "Depth");
    setupSlider(attackSlider, attackLabel, "Attack");
    setupSlider(releaseSlider, releaseLabel, "Release");

    msModeButton.setButtonText("M/S Mode");
    addAndMakeVisible(msModeButton);

    linearPhaseButton.setButtonText("Linear Phase");
    addAndMakeVisible(linearPhaseButton);

    // --- APVTS との接続 ---
    thresholdAttachment = std::make_unique<SliderAttachment>(processor.apvts, "THRESHOLD", thresholdSlider);
    depthAttachment = std::make_unique<SliderAttachment>(processor.apvts, "DEPTH", depthSlider);
    attackAttachment = std::make_unique<SliderAttachment>(processor.apvts, "ATTACK", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(processor.apvts, "RELEASE", releaseSlider);
    msModeAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "MS_MODE", msModeButton);
    linearPhaseAttachment = std::make_unique<ButtonAttachment>(processor.apvts, "LINEAR_PHASE", linearPhaseButton);

    // ウィンドウサイズ
    setSize(600, 450);

    // 描画更新タイマー（30Hz）
    startTimerHz(30);
}

LUMINAEditor::~LUMINAEditor()
{
    stopTimer();
}

//==============================================================================
void LUMINAEditor::paint(juce::Graphics& g)
{
    // 背景
    g.fillAll(juce::Colour(0xff121212));

    auto bounds = getLocalBounds();
    auto visualizerArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.6f));

    // セパレーターライン
    g.setColour(juce::Colours::grey.withAlpha(0.2f));
    g.drawHorizontalLine(visualizerArea.getBottom(), 0.0f, static_cast<float>(getWidth()));

    // --- オンセット LED の描画 ---
    float ledX = static_cast<float>(getWidth()) - 30.0f;
    float ledY = static_cast<float>(getHeight()) - 30.0f;
    float ledSize = 14.0f;

    g.setColour(juce::Colour(0xff2A2A2A));
    g.fillEllipse(ledX, ledY, ledSize, ledSize);

    if (onsetGlow > 0.0f)
    {
        g.setColour(juce::Colours::orange.withAlpha(onsetGlow));
        g.fillEllipse(ledX, ledY, ledSize, ledSize);

        g.setColour(juce::Colours::orange.withAlpha(onsetGlow * 0.4f));
        g.fillEllipse(ledX - 3.0f, ledY - 3.0f, ledSize + 6.0f, ledSize + 6.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(14.0f);
    g.drawText("Transient Protect", static_cast<int>(ledX - 130.0f), static_cast<int>(ledY), 120, static_cast<int>(ledSize), juce::Justification::centredRight, false);
}

void LUMINAEditor::resized()
{
    auto bounds = getLocalBounds();

    // 視覚化エリア
    auto visualizerArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.6f));
    spectrumAnalyzer.setBounds(visualizerArea.reduced(10));
    grMeter.setBounds(visualizerArea.reduced(10)); // スペクトラムと同じ位置に重ねる

    // コントロールエリア
    auto controlArea = bounds.reduced(10);
    auto sliderArea = controlArea.removeFromTop(static_cast<int>(controlArea.getHeight() * 0.7f));
    auto knobWidth = sliderArea.getWidth() / 4;

    thresholdSlider.setBounds(sliderArea.removeFromLeft(knobWidth).reduced(5));
    depthSlider.setBounds(sliderArea.removeFromLeft(knobWidth).reduced(5));
    attackSlider.setBounds(sliderArea.removeFromLeft(knobWidth).reduced(5));
    releaseSlider.setBounds(sliderArea.removeFromLeft(knobWidth).reduced(5));

    auto buttonArea = controlArea;
    msModeButton.setBounds(buttonArea.removeFromLeft(100).reduced(2));
    linearPhaseButton.setBounds(buttonArea.removeFromLeft(120).reduced(2));
}

void LUMINAEditor::timerCallback()
{
    AnalysisFrame frame;
    bool hasNewData = false;

    // プロセッサの FIFO から最新データを取得
    while (processor.analysisFifo.pop(frame))
    {
        latestFrame = frame;
        hasNewData = true;
    }

    if (hasNewData)
    {
        spectrumAnalyzer.updateFrame(latestFrame);
        grMeter.updateFrame(latestFrame);

        // LED の発光減衰
        if (latestFrame.isOnset)
        {
            onsetGlow = 1.0f;
        }
        else
        {
            onsetGlow -= 0.1f;
            if (onsetGlow < 0.0f) onsetGlow = 0.0f;
        }

        repaint();
    }
}