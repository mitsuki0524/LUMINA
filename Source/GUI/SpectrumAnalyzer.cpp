// ==========================================
// Source/GUI/SpectrumAnalyzer.cpp
// ==========================================
#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer(juce::AudioProcessorValueTreeState& vts)
    : apvts(vts), cross1(250.0f), cross2(4000.0f)
{
    setOpaque(true);
}

SpectrumAnalyzer::~SpectrumAnalyzer() {}

void SpectrumAnalyzer::updateFrame(const AnalysisFrame& frame)
{
    currentFrame = frame;
}

void SpectrumAnalyzer::setCrossovers(float c1, float c2)
{
    // ドラッグ中でなければ値を更新（ドラッグ中のガタつき防止）
    if (!isDragging) {
        cross1 = c1;
        cross2 = c2;
    }
}

float SpectrumAnalyzer::getXFromFreq(float freq, float width) const
{
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float normFreq = (std::log10(freq) - std::log10(minFreq)) / (std::log10(maxFreq) - std::log10(minFreq));
    return juce::jlimit(0.0f, width, normFreq * width);
}

float SpectrumAnalyzer::getFreqFromX(float x, float width) const
{
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float normFreq = juce::jlimit(0.0f, 1.0f, x / width);
    return minFreq * std::pow(maxFreq / minFreq, normFreq);
}

void SpectrumAnalyzer::mouseDown(const juce::MouseEvent& e)
{
    float width = static_cast<float>(getWidth());
    float x1 = getXFromFreq(cross1, width);
    float x2 = getXFromFreq(cross2, width);
    float mouseX = e.position.x;
    float tolerance = 10.0f; // ドラッグ判定の遊び（ピクセル）

    if (std::abs(mouseX - x1) < tolerance) {
        draggingCrossIndex = 0;
        isDragging = true;
    }
    else if (std::abs(mouseX - x2) < tolerance) {
        draggingCrossIndex = 1;
        isDragging = true;
    }
    else {
        draggingCrossIndex = -1;
        isDragging = false;
    }
}

void SpectrumAnalyzer::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging || draggingCrossIndex == -1) return;

    float width = static_cast<float>(getWidth());
    float mouseX = juce::jlimit(0.0f, width, e.position.x);
    float freq = getFreqFromX(mouseX, width);

    // クロスオーバーが逆転しないように制限をかける
    if (draggingCrossIndex == 0) {
        freq = juce::jlimit(20.0f, cross2 - 10.0f, freq);
        cross1 = freq;
        if (auto* param = apvts.getParameter("CROSS_1"))
            param->setValueNotifyingHost(param->convertTo0to1(freq));
    }
    else if (draggingCrossIndex == 1) {
        freq = juce::jlimit(cross1 + 10.0f, 20000.0f, freq);
        cross2 = freq;
        if (auto* param = apvts.getParameter("CROSS_2"))
            param->setValueNotifyingHost(param->convertTo0to1(freq));
    }

    currentDragFreq = freq;
    repaint();
}

void SpectrumAnalyzer::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    draggingCrossIndex = -1;
    isDragging = false;
    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 背景
    g.fillAll(juce::Colour::fromString("FF1A1A1A"));

    // --- グリッド線とラベル描画 ---
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float f : freqs) {
        float x = getXFromFreq(f, bounds.getWidth());
        g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());

        juce::String label = (f >= 1000.0f) ? juce::String(f / 1000.0f, 0) + "k" : juce::String(f, 0);
        g.drawText(label, static_cast<int>(x) + 2, static_cast<int>(bounds.getHeight()) - 15, 30, 15, juce::Justification::bottomLeft);
    }

    // --- クロスオーバー境界線の描画 ---
    float x1 = getXFromFreq(cross1, bounds.getWidth());
    float x2 = getXFromFreq(cross2, bounds.getWidth());

    // ドラッグ中は線を明るくする
    g.setColour(draggingCrossIndex == 0 ? juce::Colours::white : juce::Colours::white.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(x1), 0.0f, bounds.getHeight());

    g.setColour(draggingCrossIndex == 1 ? juce::Colours::white : juce::Colours::white.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(x2), 0.0f, bounds.getHeight());

    // --- スペクトラム波形の描画 ---
    juce::Path spectrumPath;
    bool pathStarted = false;

    // ⚡ 修正: 描画エリアの天井を +12dB に拡張し、0dBで張り付かないようにする
    const float minDB = -80.0f;
    const float maxDB = 12.0f; // 0.0f から 12.0f に拡張

    for (size_t i = 0; i < 512; ++i)
    {
        float mag = currentFrame.magnitudeSpectrum[i];
        if (mag > 0.0f)
        {
            float db = juce::Decibels::gainToDecibels(mag, minDB);

            float freq = (static_cast<float>(i) / 512.0f) * 22050.0f;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > 20000.0f) freq = 20000.0f;

            float x = getXFromFreq(freq, bounds.getWidth());
            // -80dB ~ +12dB の範囲を、画面の高さにマッピング
            float y = juce::jmap(db, minDB, maxDB, bounds.getHeight(), 0.0f);

            if (!pathStarted) {
                spectrumPath.startNewSubPath(x, bounds.getHeight());
                spectrumPath.lineTo(x, y);
                pathStarted = true;
            }
            else {
                spectrumPath.lineTo(x, y);
            }
        }
    }

    if (pathStarted) {
        spectrumPath.lineTo(bounds.getWidth(), bounds.getHeight());
        spectrumPath.closeSubPath();

        // メイン波形の塗りつぶし（半透明の紫）
        juce::Colour fillColour = juce::Colour::fromString("FF764DFF").withAlpha(0.5f);
        g.setColour(fillColour);
        g.fillPath(spectrumPath);

        // 輪郭線（白）
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));
    }

    // ⚡ --- Tame (Resonance Suppression) の氷柱ノッチ描画 ---
    g.setColour(juce::Colours::white.withAlpha(0.6f)); // 半透明の白
    for (size_t i = 0; i < 512; ++i)
    {
        float tameGain = currentFrame.tameSpectrum[i]; // 0.0 ~ 1.0 (1.0=削られていない)

        // 少しでも削られていれば（0.99以下なら）描画
        if (tameGain < 0.99f) {
            float freq = (static_cast<float>(i) / 512.0f) * 22050.0f;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > 20000.0f) freq = 20000.0f;

            float x = getXFromFreq(freq, bounds.getWidth());

            // 元の振幅（波形の高さ）
            float mag = currentFrame.magnitudeSpectrum[i];
            float dbTop = juce::Decibels::gainToDecibels(mag, minDB);

            // 削られた後の振幅（氷柱の下端）
            float dbBot = juce::Decibels::gainToDecibels(mag * tameGain, minDB);

            float yTop = juce::jmap(dbTop, minDB, maxDB, bounds.getHeight(), 0.0f);
            float yBot = juce::jmap(dbBot, minDB, maxDB, bounds.getHeight(), 0.0f);

            // 本来の波形の高さから下に向かって、削られた分だけ縦線を引く
            g.drawLine(x, yTop, x, yBot, 1.5f);
        }
    }
}

void SpectrumAnalyzer::resized()
{
    // 必要に応じてリサイズ処理
}