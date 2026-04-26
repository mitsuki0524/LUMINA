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

    // --- 3バンド・マルチカラー波形の描画（塗りつぶし追加版） ---
    if (currentFrame.magnitudeSpectrum.size() == 512)
    {
        juce::Path pathLow, pathMid, pathHigh;
        bool firstLow = true, firstMid = true, firstHigh = true;

        float prevX = 0.0f;
        float prevY = 0.0f;
        float prevFreq = 0.0f;

        // 塗りつぶし用に開始と終了のX座標を記録
        float startXLow = 0.0f, endXLow = 0.0f;
        float startXMid = 0.0f, endXMid = 0.0f;
        float startXHigh = 0.0f, endXHigh = 0.0f;

        for (size_t i = 0; i < 512; ++i)
        {
            float freq = (static_cast<float>(i) / 512.0f) * 22050.0f;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > 20000.0f) freq = 20000.0f;

            float x = getXFromFreq(freq, bounds.getWidth());

            float mag = currentFrame.magnitudeSpectrum[i];
            float db = juce::Decibels::gainToDecibels(mag, -80.0f);
            float y = juce::jmap(db, -80.0f, 0.0f, bounds.getHeight(), 0.0f);

            if (freq <= cross1) {
                if (firstLow) { pathLow.startNewSubPath(x, y); startXLow = x; firstLow = false; }
                else { pathLow.lineTo(x, y); }
                endXLow = x;
            }
            else if (freq <= cross2) {
                if (prevFreq <= cross1 && !firstLow) {
                    pathLow.lineTo(x, y);
                    endXLow = x;
                    pathMid.startNewSubPath(prevX, prevY);
                    startXMid = prevX;
                    pathMid.lineTo(x, y);
                    firstMid = false;
                }
                else {
                    if (firstMid) { pathMid.startNewSubPath(x, y); startXMid = x; firstMid = false; }
                    else { pathMid.lineTo(x, y); }
                }
                endXMid = x;
            }
            else {
                if (prevFreq <= cross2 && !firstMid) {
                    pathMid.lineTo(x, y);
                    endXMid = x;
                    pathHigh.startNewSubPath(prevX, prevY);
                    startXHigh = prevX;
                    pathHigh.lineTo(x, y);
                    firstHigh = false;
                }
                else {
                    if (firstHigh) { pathHigh.startNewSubPath(x, y); startXHigh = x; firstHigh = false; }
                    else { pathHigh.lineTo(x, y); }
                }
                endXHigh = x;
            }

            prevX = x;
            prevY = y;
            prevFreq = freq;
        }

        // --- 塗りつぶし処理 ---
        auto fillPath = [&](juce::Path p, float startX, float endX, juce::Colour color) {
            if (p.isEmpty()) return;
            p.lineTo(endX, bounds.getHeight());
            p.lineTo(startX, bounds.getHeight());
            p.closeSubPath();
            g.setColour(color.withAlpha(0.25f)); // 半透明で塗りつぶし
            g.fillPath(p);
            };

        fillPath(pathLow, startXLow, endXLow, juce::Colour::fromString("FF00E5FF"));
        fillPath(pathMid, startXMid, endXMid, juce::Colour::fromString("FFFF764D"));
        fillPath(pathHigh, startXHigh, endXHigh, juce::Colour::fromString("FFFF00FF"));

        // --- ストローク（線）処理 ---
        g.setColour(juce::Colour::fromString("FF00E5FF")); // Low (Bright Cyan)
        g.strokePath(pathLow, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colour::fromString("FFFF764D")); // Mid (Ableton Orange)
        g.strokePath(pathMid, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colour::fromString("FFFF00FF")); // High (Bright Magenta)
        g.strokePath(pathHigh, juce::PathStrokeType(2.0f));
    }

    // --- ドラッグ中のツールチップ（周波数表示） ---
    if (isDragging && draggingCrossIndex != -1) {
        float drawX = (draggingCrossIndex == 0) ? xC1 : xC2;
        juce::String freqStr = juce::String(currentDragFreq, 0) + " Hz";

        juce::Rectangle<float> tooltipBounds(drawX + 8.0f, 10.0f, 60.0f, 20.0f);
        // 右端にはみ出る場合は左側に表示
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