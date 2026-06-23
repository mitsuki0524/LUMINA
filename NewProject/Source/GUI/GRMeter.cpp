// ==========================================
// Source/GUI/GRMeter.cpp
// ==========================================
#include "GRMeter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

GRMeter::GRMeter()
{
    displayedGR_M.fill(1.0f);
    displayedGR_S.fill(1.0f);
}

GRMeter::~GRMeter() {}

void GRMeter::updateFrame(const AnalysisFrame& frame)
{
    float alphaAttack = 0.3f;
    float alphaRelease = 0.1f;

    for (size_t i = 0; i < 24; ++i) {
        float targetM = frame.barkGainReductionM[i];
        if (targetM < displayedGR_M[i]) displayedGR_M[i] = displayedGR_M[i] * (1.0f - alphaAttack) + targetM * alphaAttack;
        else displayedGR_M[i] = displayedGR_M[i] * (1.0f - alphaRelease) + targetM * alphaRelease;

        float targetS = frame.barkGainReductionS[i];
        if (targetS < displayedGR_S[i]) displayedGR_S[i] = displayedGR_S[i] * (1.0f - alphaAttack) + targetS * alphaAttack;
        else displayedGR_S[i] = displayedGR_S[i] * (1.0f - alphaRelease) + targetS * alphaRelease;
    }
    repaint();
}

void GRMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float blockWidth = bounds.getWidth() / 24.0f;

    // ⚡ あなたの元の設定を維持（描画エリアを上部60pxに制限）
    const float maxGrHeight = 60.0f;
    const float maxGrDb = -18.0f;

    // ⚡ 桜色と白色の定義
    const juce::Colour colorM = juce::Colours::white;
    const juce::Colour colorS = juce::Colour::fromString("FFFFB7C5"); // 🌸 桜色
    const float maxOpacity = 0.66f; // ⚡ 最大濃度を2/3に制限

    for (int i = 0; i < 24; ++i) {
        // --- Mid成分の計算 ---
        float grDbM = juce::Decibels::gainToDecibels(displayedGR_M[i]);
        if (grDbM > 0.0f) grDbM = 0.0f;
        float hM = juce::jlimit(0.0f, maxGrHeight, juce::jmap(grDbM, maxGrDb, 0.0f, maxGrHeight, 0.0f));

        // --- Side成分の計算 ---
        float grDbS = juce::Decibels::gainToDecibels(displayedGR_S[i]);
        if (grDbS > 0.0f) grDbS = 0.0f;
        float hS = juce::jlimit(0.0f, maxGrHeight, juce::jmap(grDbS, maxGrDb, 0.0f, maxGrHeight, 0.0f));

        float x = i * blockWidth;
        float halfW = blockWidth * 0.5f;

        // ⚡ 左半分: Mid (白色の縦グラデーション)
        if (hM > 1.0f) {
            juce::Rectangle<float> barM(x + 1.0f, 0.0f, halfW - 1.0f, hM);
            float intensityM = juce::jlimit(0.0f, 1.0f, grDbM / maxGrDb) * maxOpacity;

            // 上から下へ濃くなるグラデーション
            juce::ColourGradient gradM(colorM.withAlpha(0.05f), 0, 0,
                colorM.withAlpha(intensityM), 0, hM, false);
            g.setGradientFill(gradM);
            g.fillRect(barM);

            // 先端のLEDチップ（少し明るい白）
            g.setColour(colorM.withAlpha(std::min(maxOpacity, intensityM + 0.1f)));
            g.fillRect(barM.getX(), barM.getBottom() - 1.5f, barM.getWidth(), 1.5f);
        }

        // ⚡ 右半分: Side (桜色の縦グラデーション)
        if (hS > 1.0f) {
            juce::Rectangle<float> barS(x + halfW, 0.0f, halfW - 1.0f, hS);
            float intensityS = juce::jlimit(0.0f, 1.0f, grDbS / maxGrDb) * maxOpacity;

            // 上から下へ濃くなるグラデーション
            juce::ColourGradient gradS(colorS.withAlpha(0.05f), 0, 0,
                colorS.withAlpha(intensityS), 0, hS, false);
            g.setGradientFill(gradS);
            g.fillRect(barS);

            // 先端のLEDチップ（少し明るい桜色）
            g.setColour(colorS.withAlpha(std::min(maxOpacity, intensityS + 0.1f)));
            g.fillRect(barS.getX(), barS.getBottom() - 1.5f, barS.getWidth(), 1.5f);
        }
    }
}