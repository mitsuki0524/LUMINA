// ==========================================
// Source/GUI/SpectrumAnalyzer.cpp
// ==========================================
#include "SpectrumAnalyzer.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer(juce::AudioProcessorValueTreeState& vts)
    : apvts(vts)
{
    // マウスイベントを受け取るために必要
    setInterceptsMouseClicks(true, false);
}

SpectrumAnalyzer::~SpectrumAnalyzer() {}

void SpectrumAnalyzer::updateFrame(const AnalysisFrame& frame)
{
    currentFrame = frame;
    repaint();
}

void SpectrumAnalyzer::setCrossovers(float c1, float c2)
{
    cross1 = c1;
    cross2 = c2;
    // ドロップ完了後など、外部から値が変更された場合は再描画
    if (!isDragging) repaint();
}

float SpectrumAnalyzer::getFreqFromX(float x, float width) const
{
    float normX = juce::jlimit(0.0f, 1.0f, x / width);
    return 20.0f * std::pow(20000.0f / 20.0f, normX);
}

float SpectrumAnalyzer::getXFromFreq(float freq, float width) const
{
    return width * (std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f));
}

void SpectrumAnalyzer::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat();
    float xC1 = getXFromFreq(cross1, bounds.getWidth());
    float xC2 = getXFromFreq(cross2, bounds.getWidth());

    // クリックしたX座標がどちらの境界線に近いかを判定 (ヒットエリアは±15ピクセル)
    float clickX = e.position.x;
    float dist1 = std::abs(clickX - xC1);
    float dist2 = std::abs(clickX - xC2);

    if (dist1 < 15.0f && dist1 <= dist2) {
        draggingCrossIndex = 0;
        isDragging = true;
        currentDragFreq = cross1;
    }
    else if (dist2 < 15.0f) {
        draggingCrossIndex = 1;
        isDragging = true;
        currentDragFreq = cross2;
    }
}

void SpectrumAnalyzer::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging || draggingCrossIndex == -1) return;

    auto bounds = getLocalBounds().toFloat();
    float newFreq = getFreqFromX(e.position.x, bounds.getWidth());

    // 帯域の逆転を防ぐための制約（10Hzの安全マージン）
    if (draggingCrossIndex == 0) {
        newFreq = juce::jlimit(20.0f, cross2 - 10.0f, newFreq);
        currentDragFreq = newFreq;
        if (auto* param = apvts.getParameter("CROSS_1")) {
            param->setValueNotifyingHost(param->convertTo0to1(newFreq));
        }
    }
    else if (draggingCrossIndex == 1) {
        newFreq = juce::jlimit(cross1 + 10.0f, 20000.0f, newFreq);
        currentDragFreq = newFreq;
        if (auto* param = apvts.getParameter("CROSS_2")) {
            param->setValueNotifyingHost(param->convertTo0to1(newFreq));
        }
    }
    repaint(); // ツールチップと線の描画を更新
}

void SpectrumAnalyzer::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isDragging = false;
    draggingCrossIndex = -1;
    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // スペクトラム背景
    g.fillAll(juce::Colour(0xff2A2A2A));

    // --- Hzグリッドの描画 ---
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.setFont(11.0f);

    const float freqs[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const juce::String labels[] = { "20", "", "100", "", "", "1k", "", "", "10k", "20k" };

    for (int i = 0; i < 10; ++i) {
        float x = getXFromFreq(freqs[i], bounds.getWidth());
        g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
        if (labels[i].isNotEmpty()) {
            g.drawText(labels[i], static_cast<int>(x) + 4, static_cast<int>(bounds.getHeight()) - 16, 30, 16, juce::Justification::left);
        }
    }

    // --- クロスオーバー境界線の描画 ---
    float xC1 = getXFromFreq(cross1, bounds.getWidth());
    float xC2 = getXFromFreq(cross2, bounds.getWidth());

    // ドラッグ中の境界線はハイライトさせる
    g.setColour((draggingCrossIndex == 0) ? juce::Colours::white : juce::Colours::white.withAlpha(0.4f));
    float dashPattern[] = { 4.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(xC1, 0, xC1, bounds.getHeight()), dashPattern, 2);

    g.setColour((draggingCrossIndex == 1) ? juce::Colours::white : juce::Colours::white.withAlpha(0.4f));
    g.drawDashedLine(juce::Line<float>(xC2, 0, xC2, bounds.getHeight()), dashPattern, 2);

    // --- 波形の描画（Solo色反転対応版） ---
    if (currentFrame.magnitudeSpectrum.size() == 512)
    {
        juce::Path fullPath;
        bool firstPoint = true;
        float startX = 0.0f, endX = 0.0f;

        // 1. スペクトラム全体を1つのパスとして構築
        for (size_t i = 0; i < 512; ++i)
        {
            float freq = (static_cast<float>(i) / 512.0f) * 22050.0f;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > 20000.0f) freq = 20000.0f;

            float x = getXFromFreq(freq, bounds.getWidth());
            float mag = currentFrame.magnitudeSpectrum[i];
            float db = juce::Decibels::gainToDecibels(mag, -80.0f);
            float y = juce::jmap(db, -80.0f, 0.0f, bounds.getHeight(), 0.0f);

            if (firstPoint) {
                fullPath.startNewSubPath(x, y);
                startX = x;
                firstPoint = false;
            }
            else {
                fullPath.lineTo(x, y);
            }
            endX = x;
        }

        // 塗りつぶし用に、底辺を閉じたパスを作成
        juce::Path closedPath = fullPath;
        closedPath.lineTo(endX, bounds.getHeight());
        closedPath.lineTo(startX, bounds.getHeight());
        closedPath.closeSubPath();

        // バンドカラーの定義
        auto colorLow = juce::Colour::fromString("FF00E5FF");
        auto colorMid = juce::Colour::fromString("FFFF764D");
        auto colorHigh = juce::Colour::fromString("FFFF00FF");

        // 2. Solo状態に応じた描画の分岐
        if (currentFrame.activeSoloBand == 0) {
            // LowがSolo: 全体をシアンで描画
            g.setColour(colorLow.withAlpha(0.25f)); g.fillPath(closedPath);
            g.setColour(colorLow); g.strokePath(fullPath, juce::PathStrokeType(2.0f));
        }
        else if (currentFrame.activeSoloBand == 1) {
            // MidがSolo: 全体をオレンジで描画
            g.setColour(colorMid.withAlpha(0.25f)); g.fillPath(closedPath);
            g.setColour(colorMid); g.strokePath(fullPath, juce::PathStrokeType(2.0f));
        }
        else if (currentFrame.activeSoloBand == 2) {
            // HighがSolo: 全体をマゼンタで描画
            g.setColour(colorHigh.withAlpha(0.25f)); g.fillPath(closedPath);
            g.setColour(colorHigh); g.strokePath(fullPath, juce::PathStrokeType(2.0f));
        }
        else {
            // 通常時: クリッピング領域（表示制限）を使って3色に分割描画

            // Low Zone (画面左端 から xC1 まで)
            g.saveState();
            g.reduceClipRegion(0, 0, static_cast<int>(xC1), static_cast<int>(bounds.getHeight()));
            g.setColour(colorLow.withAlpha(0.25f)); g.fillPath(closedPath);
            g.setColour(colorLow); g.strokePath(fullPath, juce::PathStrokeType(2.0f));
            g.restoreState();

            // Mid Zone (xC1 から xC2 まで)
            g.saveState();
            g.reduceClipRegion(static_cast<int>(xC1), 0, static_cast<int>(xC2 - xC1), static_cast<int>(bounds.getHeight()));
            g.setColour(colorMid.withAlpha(0.25f)); g.fillPath(closedPath);
            g.setColour(colorMid); g.strokePath(fullPath, juce::PathStrokeType(2.0f));
            g.restoreState();

            // High Zone (xC2 から 画面右端 まで)
            g.saveState();
            g.reduceClipRegion(static_cast<int>(xC2), 0, static_cast<int>(bounds.getWidth() - xC2), static_cast<int>(bounds.getHeight()));
            g.setColour(colorHigh.withAlpha(0.25f)); g.fillPath(closedPath);
            g.setColour(colorHigh); g.strokePath(fullPath, juce::PathStrokeType(2.0f));
            g.restoreState();
        }
    }

    // --- ドラッグ中のツールチップ（周波数表示） ---
    if (isDragging && draggingCrossIndex != -1) {
        float drawX = (draggingCrossIndex == 0) ? xC1 : xC2;
        juce::String freqStr = juce::String(currentDragFreq, 0) + " Hz";

        juce::Rectangle<float> tooltipBounds(drawX + 8.0f, 10.0f, 60.0f, 20.0f);
        if (tooltipBounds.getRight() > bounds.getWidth()) {
            tooltipBounds.setX(drawX - 68.0f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(tooltipBounds, 4.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(freqStr, tooltipBounds, juce::Justification::centred, false);
    }
}

void SpectrumAnalyzer::resized() {}