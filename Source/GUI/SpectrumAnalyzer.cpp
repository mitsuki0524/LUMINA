#include "SpectrumAnalyzer.h"
#include <cmath> // ⚡ std::log10 用の必須ヘッダーを追加

SpectrumAnalyzer::SpectrumAnalyzer() {}
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
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // スペクトラム背景（少し明るめ）
    g.fillAll(juce::Colour(0xff2A2A2A));

    // --- Hzグリッドの描画 ---
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.setFont(11.0f);

    const float freqs[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    const juce::String labels[] = { "20", "", "100", "", "", "1k", "", "", "10k", "20k" };

    for (int i = 0; i < 10; ++i) {
        float normX = std::log10(freqs[i] / 20.0f) / std::log10(20000.0f / 20.0f);
        float x = bounds.getWidth() * normX;
        g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
        if (labels[i].isNotEmpty()) {
            g.drawText(labels[i], static_cast<int>(x) + 4, static_cast<int>(bounds.getHeight()) - 16, 30, 16, juce::Justification::left);
        }
    }

    // --- クロスオーバー境界線の描画 ---
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    float xC1 = bounds.getWidth() * (std::log10(cross1 / 20.0f) / std::log10(20000.0f / 20.0f));
    float xC2 = bounds.getWidth() * (std::log10(cross2 / 20.0f) / std::log10(20000.0f / 20.0f));
    float dashPattern[] = { 4.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(xC1, 0, xC1, bounds.getHeight()), dashPattern, 2);
    g.drawDashedLine(juce::Line<float>(xC2, 0, xC2, bounds.getHeight()), dashPattern, 2);

    // --- 3バンド・マルチカラー波形の描画（隙間ゼロ・スナップ修正版） ---
    if (currentFrame.magnitudeSpectrum.size() == 512)
    {
        juce::Path pathLow, pathMid, pathHigh;
        bool firstLow = true, firstMid = true, firstHigh = true;

        // ⚡ 境界線の隙間を埋めるための座標保持変数
        float prevX = 0.0f;
        float prevY = 0.0f;
        float prevFreq = 0.0f;

        for (size_t i = 0; i < 512; ++i)
        {
            float freq = (static_cast<float>(i) / 512.0f) * 22050.0f;
            if (freq < 20.0f) freq = 20.0f;
            if (freq > 20000.0f) freq = 20000.0f;

            float normX = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f);
            float x = bounds.getWidth() * normX;

            float mag = currentFrame.magnitudeSpectrum[i];
            float db = juce::Decibels::gainToDecibels(mag, -80.0f);
            float y = juce::jmap(db, -80.0f, 0.0f, bounds.getHeight(), 0.0f);

            if (freq <= cross1) {
                if (firstLow) { pathLow.startNewSubPath(x, y); firstLow = false; }
                else { pathLow.lineTo(x, y); }
            }
            else if (freq <= cross2) {
                // ⚡ Low から Mid に切り替わる瞬間（両方のPathに線を引いて接着する）
                if (prevFreq <= cross1 && !firstLow) {
                    pathLow.lineTo(x, y); // Lowの線をここまで伸ばす
                    pathMid.startNewSubPath(prevX, prevY); // Midの線を一つ前の点から開始する
                    pathMid.lineTo(x, y);
                    firstMid = false;
                }
                else {
                    if (firstMid) { pathMid.startNewSubPath(x, y); firstMid = false; }
                    else { pathMid.lineTo(x, y); }
                }
            }
            else {
                // ⚡ Mid から High に切り替わる瞬間（両方のPathに線を引いて接着する）
                if (prevFreq <= cross2 && !firstMid) {
                    pathMid.lineTo(x, y); // Midの線をここまで伸ばす
                    pathHigh.startNewSubPath(prevX, prevY); // Highの線を一つ前の点から開始する
                    pathHigh.lineTo(x, y);
                    firstHigh = false;
                }
                else {
                    if (firstHigh) { pathHigh.startNewSubPath(x, y); firstHigh = false; }
                    else { pathHigh.lineTo(x, y); }
                }
            }

            prevX = x;
            prevY = y;
            prevFreq = freq;
        }

        g.setColour(juce::Colour::fromString("FF00E5FF")); // Low (Bright Cyan)
        g.strokePath(pathLow, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colour::fromString("FFFF764D")); // Mid (Ableton Orange)
        g.strokePath(pathMid, juce::PathStrokeType(2.0f));

        g.setColour(juce::Colour::fromString("FFFF00FF")); // High (Bright Magenta)
        g.strokePath(pathHigh, juce::PathStrokeType(2.0f));
    }
}

void SpectrumAnalyzer::resized() {}