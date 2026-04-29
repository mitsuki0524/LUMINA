// ==========================================
// Source/GUI/SpectrumAnalyzer.cpp
// ==========================================
#include "SpectrumAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>

SpectrumAnalyzer::SpectrumAnalyzer(juce::AudioProcessorValueTreeState& vts)
    : apvts(vts), cross1(250.0f), cross2(4000.0f)
{
    setOpaque(true);
}

SpectrumAnalyzer::~SpectrumAnalyzer() {}

void SpectrumAnalyzer::updateFrame(const AnalysisFrame& frame)
{
    // ⚡ 波形描画のスピード調整 (時間的フォールオフ)。値が小さいほど滑らかでゆっくりに。
    float alpha = 0.10f;

    // ⚡ 内部サンプリングレートの更新
    currentFrame.internalSampleRate = frame.internalSampleRate;

    for (size_t i = 0; i < 512; ++i) {
        currentFrame.unprocessedSpectrum[i] = currentFrame.unprocessedSpectrum[i] * (1.0f - alpha) + frame.unprocessedSpectrum[i] * alpha;
        currentFrame.magnitudeSpectrum[i] = currentFrame.magnitudeSpectrum[i] * (1.0f - alpha) + frame.magnitudeSpectrum[i] * alpha;
        currentFrame.tameSpectrum[i] = currentFrame.tameSpectrum[i] * (1.0f - alpha) + frame.tameSpectrum[i] * alpha;
    }

    // ⚡ マスキング閾値の平滑化更新を追加
    for (size_t i = 0; i < 24; ++i) {
        currentMaskingThreshold[i] = currentMaskingThreshold[i] * (1.0f - alpha) + frame.barkMaskingThreshold[i] * alpha;
    }
}

void SpectrumAnalyzer::setCrossovers(float c1, float c2)
{
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
    float tolerance = 10.0f;

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
    g.fillAll(juce::Colour::fromString("FF1A1A1A"));

    // ⚡ 定数設定：マスタリングに最適な表示レンジ (-72dB 〜 0dB)
    const float minDB = -72.0f;
    const float maxDB = 0.0f;
    const float nyquist = static_cast<float>(currentFrame.internalSampleRate) * 0.5f;

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float f : freqs) {
        float x = getXFromFreq(f, bounds.getWidth());
        g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
        juce::String label = (f >= 1000.0f) ? juce::String(f / 1000.0f, 0) + "k" : juce::String(f, 0);
        g.drawText(label, static_cast<int>(x) + 2, static_cast<int>(bounds.getHeight()) - 15, 30, 15, juce::Justification::bottomLeft);
    }

    float x1 = getXFromFreq(cross1, bounds.getWidth());
    float x2 = getXFromFreq(cross2, bounds.getWidth());
    g.setColour(draggingCrossIndex == 0 ? juce::Colours::white : juce::Colours::white.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(x1), 0.0f, bounds.getHeight());
    g.setColour(draggingCrossIndex == 1 ? juce::Colours::white : juce::Colours::white.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(x2), 0.0f, bounds.getHeight());

    if (isDragging) {
        g.setColour(juce::Colours::white);
        float drawX = (draggingCrossIndex == 0) ? x1 : x2;
        juce::String freqStr = juce::String(currentDragFreq, 0) + " Hz";
        g.drawText(freqStr, static_cast<int>(drawX) + 5, 10, 80, 20, juce::Justification::topLeft);
    }

    juce::Colour colourLow = juce::Colour::fromString("FFFF8C00");
    juce::Colour colourMid = juce::Colour::fromString("FF764DFF");
    juce::Colour colourHigh = juce::Colour::fromString("FF00E5FF");

    juce::Colour tameLow = colourLow.withMultipliedBrightness(1.5f).withMultipliedSaturation(0.6f);
    juce::Colour tameMid = colourMid.withMultipliedBrightness(1.5f).withMultipliedSaturation(0.6f);
    juce::Colour tameHigh = colourHigh.withMultipliedBrightness(1.5f).withMultipliedSaturation(0.6f);

    float normX1 = juce::jlimit(0.0f, 1.0f, x1 / bounds.getWidth());
    float normX2 = juce::jlimit(0.0f, 1.0f, x2 / bounds.getWidth());

    juce::ColourGradient bandGradient;
    bandGradient.point1 = { 0.0f, 0.0f };
    bandGradient.point2 = { bounds.getWidth(), 0.0f };
    bandGradient.addColour(0.0f, colourLow);
    if (normX1 > 0.0f) { bandGradient.addColour(normX1, colourLow); bandGradient.addColour(normX1 + 0.001f, colourMid); }
    if (normX2 > 0.0f) { bandGradient.addColour(normX2, colourMid); bandGradient.addColour(normX2 + 0.001f, colourHigh); }
    bandGradient.addColour(1.0f, colourHigh);

    juce::ColourGradient tameGradient;
    tameGradient.point1 = { 0.0f, 0.0f };
    tameGradient.point2 = { bounds.getWidth(), 0.0f };
    tameGradient.addColour(0.0f, tameLow);
    if (normX1 > 0.0f) { tameGradient.addColour(normX1, tameLow); tameGradient.addColour(normX1 + 0.001f, tameMid); }
    if (normX2 > 0.0f) { tameGradient.addColour(normX2, tameMid); tameGradient.addColour(normX2 + 0.001f, tameHigh); }
    tameGradient.addColour(1.0f, tameHigh);

    std::array<float, 512> smoothedPre = currentFrame.unprocessedSpectrum;
    std::array<float, 512> smoothedPost = currentFrame.magnitudeSpectrum;
    std::array<float, 512> smoothedTame = currentFrame.tameSpectrum;

    for (int pass = 0; pass < 3; ++pass) {
        std::array<float, 512> tempPre = smoothedPre;
        std::array<float, 512> tempPost = smoothedPost;
        std::array<float, 512> tempTame = smoothedTame;
        for (int i = 1; i < 511; ++i) {
            smoothedPre[i] = (tempPre[i - 1] + tempPre[i] * 2.0f + tempPre[i + 1]) / 4.0f;
            smoothedPost[i] = (tempPost[i - 1] + tempPost[i] * 2.0f + tempPost[i + 1]) / 4.0f;
            smoothedTame[i] = (tempTame[i - 1] + tempTame[i] * 2.0f + tempTame[i + 1]) / 4.0f;
        }
    }

    // ⚡ スペクトラム・チルト設定 (ピンクノイズを水平に表示させるための補正)
    const float tiltPerOctave = 3.0f;  // +3dB / Octave
    const float tiltRefFreq = 1000.0f; // 1kHzを基準(0dB)とする

    // ⚡ Y座標計算の共通ラムダ式 (一括してチルト補正とY座標変換を行う)
    auto calcY = [&](float linearMag, float freq) -> float {
        float db = juce::Decibels::gainToDecibels(linearMag, minDB);
        float tiltDB = tiltPerOctave * std::log2(std::max(20.0f, freq) / tiltRefFreq);
        db = juce::jlimit(minDB, maxDB, db + tiltDB);
        return juce::jmap(db, minDB, maxDB, bounds.getHeight(), 0.0f);
        };

    // ==========================================
    // ⚡ 1. 心理音響マスキング閾値の描画 (ゴースト曲線)
    // ==========================================
    const float barkCenters[24] = { 50, 150, 250, 350, 450, 570, 700, 840, 1000, 1170, 1370, 1600, 1850, 2150, 2500, 2900, 3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500 };

    juce::Path maskPath;
    bool maskStarted = false;
    for (int b = 0; b < 24; ++b) {
        float mag = currentMaskingThreshold[b];
        if (mag > 0.0f) {
            float freq = barkCenters[b];
            float x = getXFromFreq(freq, bounds.getWidth());
            float y = calcY(mag, freq);

            if (!maskStarted) { maskPath.startNewSubPath(x, y); maskStarted = true; }
            else { maskPath.lineTo(x, y); }
        }
    }
    if (maskStarted) {
        g.setColour(juce::Colour::fromString("80FFD700")); // 少し発光するGold (50%不透明度)
        g.strokePath(maskPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));
    }

    // ==========================================
    // ⚡ 2. 未処理スペクトラムの描画 (Pre)
    // ==========================================
    juce::Path prePath;
    bool preStarted = false;
    for (size_t i = 0; i < 512; ++i) {
        float mag = smoothedPre[i];
        if (mag > 0.0f) {
            float freq = juce::jlimit(20.0f, 20000.0f, (static_cast<float>(i) / 512.0f) * 22050.0f);
            float x = getXFromFreq(freq, bounds.getWidth());
            float y = calcY(mag, freq);

            if (!preStarted) { prePath.startNewSubPath(x, bounds.getHeight()); prePath.lineTo(x, y); preStarted = true; }
            else { prePath.lineTo(x, y); }
        }
    }
    if (preStarted) {
        prePath.lineTo(bounds.getWidth(), bounds.getHeight()); prePath.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.fillPath(prePath);
    }

    // ==========================================
    // ⚡ 3. 抑制された量 (Tame) の描画
    // ==========================================
    juce::Path tamePath;
    bool hasTame = false;
    for (size_t i = 0; i < 512; ++i) {
        if (smoothedTame[i] < 0.99f) hasTame = true;
        float freq = juce::jlimit(20.0f, 20000.0f, (static_cast<float>(i) / 512.0f) * 22050.0f);
        float x = getXFromFreq(freq, bounds.getWidth());
        float yTop = calcY(smoothedPre[i], freq);

        if (i == 0) tamePath.startNewSubPath(x, yTop);
        else tamePath.lineTo(x, yTop);
    }
    if (hasTame) {
        for (int i = 511; i >= 0; --i) {
            float freq = juce::jlimit(20.0f, 20000.0f, (static_cast<float>(i) / 512.0f) * 22050.0f);
            float magBot = smoothedPre[i] * smoothedTame[i];
            float x = getXFromFreq(freq, bounds.getWidth());
            float yBot = calcY(magBot, freq);
            tamePath.lineTo(x, yBot);
        }
        tamePath.closeSubPath();
        g.setGradientFill(tameGradient);
        g.setOpacity(0.85f);
        g.fillPath(tamePath);
    }

    // ==========================================
    // ⚡ 4. 処理後スペクトラムの描画 (Post)
    // ==========================================
    juce::Path postPath;
    bool postStarted = false;
    for (size_t i = 0; i < 512; ++i) {
        float mag = smoothedPost[i];
        if (mag > 0.0f) {
            float freq = juce::jlimit(20.0f, 20000.0f, (static_cast<float>(i) / 512.0f) * 22050.0f);
            float x = getXFromFreq(freq, bounds.getWidth());
            float y = calcY(mag, freq);

            if (!postStarted) { postPath.startNewSubPath(x, bounds.getHeight()); postPath.lineTo(x, y); postStarted = true; }
            else { postPath.lineTo(x, y); }
        }
    }
    if (postStarted) {
        postPath.lineTo(bounds.getWidth(), bounds.getHeight()); postPath.closeSubPath();
        g.setGradientFill(bandGradient);
        g.setOpacity(0.5f);
        g.fillPath(postPath);
        g.setOpacity(1.0f);
        g.strokePath(postPath, juce::PathStrokeType(1.5f));
    }
}

void SpectrumAnalyzer::resized() {}