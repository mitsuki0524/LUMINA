#include "GRMeter.h"
#include <cmath>

GRMeter::GRMeter()
{
    // 初期状態は1.0 (ゲインリダクション 0dB)
    displayedGR.fill(1.0f);
}

GRMeter::~GRMeter() {}

void GRMeter::updateFrame(const AnalysisFrame& frame)
{
    // メーターの滑らかなアニメーション（アタック速め、リリース遅め）
    for (size_t i = 0; i < 24; ++i) {
        float target = frame.barkGainReduction[i];
        if (target < displayedGR[i]) {
            displayedGR[i] = target; // アタック（即座に反応）
        }
        else {
            displayedGR[i] = displayedGR[i] * 0.85f + target * 0.15f; // リリース（滑らかに戻る）
        }
    }
    repaint();
}

void GRMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float barWidth = bounds.getWidth() / 24.0f;

    // アナライザーの上部から最大60ピクセル分下がるように設定
    const float maxGrHeight = 60.0f;

    for (int i = 0; i < 24; ++i) {
        // リニアゲインをdBに変換
        float grDb = juce::Decibels::gainToDecibels(displayedGR[i]);
        if (grDb > 0.0f) grDb = 0.0f; // 安全のためのクランプ

        // -18dB を最大の振り幅として高さをマッピング
        float mappedHeight = juce::jmap(grDb, -18.0f, 0.0f, maxGrHeight, 0.0f);
        mappedHeight = juce::jlimit(0.0f, maxGrHeight, mappedHeight);

        // わずかでも削られていれば描画
        if (mappedHeight > 1.0f) {
            juce::Rectangle<float> bar(i * barWidth + 1.0f, 0.0f, barWidth - 2.0f, mappedHeight);

            // Ableton風のオレンジ色（半透明）
            g.setColour(juce::Colour::fromString("99FF764D"));
            g.fillRect(bar);

            // バーの一番下にソリッドなラインを描画して視認性を高める
            g.setColour(juce::Colour::fromString("FFFF764D"));
            g.fillRect(bar.getX(), bar.getBottom() - 2.0f, bar.getWidth(), 2.0f);
        }
    }
}