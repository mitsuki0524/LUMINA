#include "GRMeter.h"

GRMeter::GRMeter()
{
    visualGains.fill(0.0f);
}

void GRMeter::updateFrame(const AnalysisFrame& frame)
{
    for (int i = 0; i < 24; ++i)
    {
        // 実際のGR量（dBなど）を 0.0 ~ 1.0 の描画用に変換
        // 1.0 (加工なし) なら 0.0 (バーの長さ0)、0.5 なら 0.5 のように変換
        float targetGR = 1.0f - frame.barkGainReduction[i];

        // 立ち上がりは速く、戻りは少しゆっくりにして視認性を高める
        if (targetGR > visualGains[i])
            visualGains[i] = targetGR;
        else
            visualGains[i] = visualGains[i] * 0.85f + targetGR * 0.15f;
    }
}

void GRMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();

    // 24本のバーを描画
    float barSpacing = 1.0f;
    float barWidth = (width / 24.0f) - barSpacing;

    // ゲインリダクションを示す色（警告色としてのオレンジ/赤）
    auto grColour = juce::Colours::orange.withAlpha(0.6f);

    for (int i = 0; i < 24; ++i)
    {
        float grAmount = visualGains[i];
        if (grAmount > 0.001f)
        {
            float x = i * (barWidth + barSpacing);
            // 上から下に伸びるバーを描画
            float barHeight = height * grAmount;

            g.setColour(grColour);
            g.fillRect(x, 0.0f, barWidth, barHeight);

            // 先端を少し明るくして強調
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawHorizontalLine((int)barHeight, x, x + barWidth);
        }
    }
}

void GRMeter::resized()
{
}