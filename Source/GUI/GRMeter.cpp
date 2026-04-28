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
    const float maxGrHeight = 60.0f;

    for (int i = 0; i < 24; ++i) {
        float grDbM = juce::Decibels::gainToDecibels(displayedGR_M[i]);
        if (grDbM > 0.0f) grDbM = 0.0f;
        float hM = juce::jlimit(0.0f, maxGrHeight, juce::jmap(grDbM, -18.0f, 0.0f, maxGrHeight, 0.0f));

        float grDbS = juce::Decibels::gainToDecibels(displayedGR_S[i]);
        if (grDbS > 0.0f) grDbS = 0.0f;
        float hS = juce::jlimit(0.0f, maxGrHeight, juce::jmap(grDbS, -18.0f, 0.0f, maxGrHeight, 0.0f));

        float x = i * blockWidth;
        float halfW = blockWidth * 0.5f;

        // ⚡ 左半分: Mid (オレンジ)
        if (hM > 1.0f) {
            juce::Rectangle<float> barM(x + 1.0f, 0.0f, halfW - 1.0f, hM);
            g.setColour(juce::Colour::fromString("B3FF8C00"));
            g.fillRect(barM);
            g.setColour(juce::Colour::fromString("FFFF8C00"));
            g.fillRect(barM.getX(), barM.getBottom() - 2.0f, barM.getWidth(), 2.0f);
        }

        // ⚡ 右半分: Side (シアン)
        if (hS > 1.0f) {
            juce::Rectangle<float> barS(x + halfW, 0.0f, halfW - 1.0f, hS);
            g.setColour(juce::Colour::fromString("B300E5FF"));
            g.fillRect(barS);
            g.setColour(juce::Colour::fromString("FF00E5FF"));
            g.fillRect(barS.getX(), barS.getBottom() - 2.0f, barS.getWidth(), 2.0f);
        }
    }
}