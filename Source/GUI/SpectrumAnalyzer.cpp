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
    float alpha = 0.12f; // 滑らかさと追従性のバランス
    currentFrame.internalSampleRate = frame.internalSampleRate;

    for (size_t i = 0; i < 512; ++i) {
        currentFrame.unprocessedSpectrum[i] = currentFrame.unprocessedSpectrum[i] * (1.0f - alpha) + frame.unprocessedSpectrum[i] * alpha;
        currentFrame.magnitudeSpectrum[i] = currentFrame.magnitudeSpectrum[i] * (1.0f - alpha) + frame.magnitudeSpectrum[i] * alpha;
        currentFrame.tameSpectrum[i] = currentFrame.tameSpectrum[i] * (1.0f - alpha) + frame.tameSpectrum[i] * alpha;
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
    float tolerance = 15.0f;

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
    const juce::Colour bgColor = juce::Colour::fromString("FF1A1A1A");
    g.fillAll(bgColor);

    const float minDB = -72.0f;
    const float maxDB = 0.0f;
    const float silenceThreshold = 0.00001f;

    // --- 背景グリッド描画 ---
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    for (float f : freqs) {
        float x = getXFromFreq(f, bounds.getWidth());
        g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
    }

    // --- クロスオーバー線描画 ---
    float x1 = getXFromFreq(cross1, bounds.getWidth());
    float x2 = getXFromFreq(cross2, bounds.getWidth());
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawVerticalLine(static_cast<int>(x1), 0.0f, bounds.getHeight());
    g.drawVerticalLine(static_cast<int>(x2), 0.0f, bounds.getHeight());

    // --- データ再マッピング (ピクセルごとに正確な周波数を抽出) ---
    // 画面の各ピクセルにおける値を格納する一時配列
    const int numPixels = getWidth();
    if (numPixels <= 0) return;

    std::vector<float> preData(numPixels), postData(numPixels), tameData(numPixels);
    const float nyquist = 22050.0f; // DSP側の最大周波数

    for (int x = 0; x < numPixels; ++x) {
        float freq = getFreqFromX((float)x, (float)numPixels);
        float linearIdx = (freq / nyquist) * 511.0f;
        int idx = (int)linearIdx;
        float frac = linearIdx - (float)idx;

        if (idx >= 0 && idx < 511) {
            preData[x] = currentFrame.unprocessedSpectrum[idx] * (1.0f - frac) + currentFrame.unprocessedSpectrum[idx + 1] * frac;
            postData[x] = currentFrame.magnitudeSpectrum[idx] * (1.0f - frac) + currentFrame.magnitudeSpectrum[idx + 1] * frac;
            tameData[x] = currentFrame.tameSpectrum[idx] * (1.0f - frac) + currentFrame.tameSpectrum[idx + 1] * frac;
        }
        else {
            preData[x] = 0.0f; postData[x] = 0.0f; tameData[x] = 1.0f;
        }
    }

    // --- 賢い平滑化 (無音境界でのスロープ発生を防止) ---
    auto smoothedPre = preData;
    auto smoothedPost = postData;
    for (int pass = 0; pass < 2; ++pass) {
        auto tempPre = smoothedPre;
        auto tempPost = smoothedPost;
        for (int i = 1; i < numPixels - 1; ++i) {
            if (tempPre[i] > silenceThreshold && tempPre[i - 1] > silenceThreshold && tempPre[i + 1] > silenceThreshold)
                smoothedPre[i] = (tempPre[i - 1] + tempPre[i] * 2.0f + tempPre[i + 1]) / 4.0f;
            if (tempPost[i] > silenceThreshold && tempPost[i - 1] > silenceThreshold && tempPost[i + 1] > silenceThreshold)
                smoothedPost[i] = (tempPost[i - 1] + tempPost[i] * 2.0f + tempPost[i + 1]) / 4.0f;
        }
    }

    // --- Y座標計算ラムダ (チルト補正込) ---
    auto calcY = [&](float mag, float x) -> float {
        if (mag <= silenceThreshold) return bounds.getHeight();
        float freq = getFreqFromX(x, bounds.getWidth());
        float db = juce::Decibels::gainToDecibels(mag, minDB);
        float tilt = 3.0f * std::log2(std::max(20.0f, freq) / 1000.0f);
        db = juce::jlimit(minDB, maxDB, db + tilt);
        return juce::jmap(db, minDB, maxDB, bounds.getHeight(), 0.0f);
        };

    // --- カラー定義 ---
    juce::Colour cLow = juce::Colour::fromString("FFFF8C00");
    juce::Colour cMid = juce::Colour::fromString("FF764DFF");
    juce::Colour cHigh = juce::Colour::fromString("FF00E5FF");

    // ==========================================
    // 1. Pre (未処理) 波形の輪郭線
    // ==========================================
    juce::Path prePath;
    for (int x = 0; x < numPixels; ++x) {
        float y = calcY(smoothedPre[x], (float)x);
        if (x == 0) prePath.startNewSubPath((float)x, y); else prePath.lineTo((float)x, y);
    }
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));

    // ==========================================
    // 2. 帯域ごとの塗りつぶし (Tame & Post)
    // ==========================================
    auto drawBand = [&](float xStart, float xEnd, juce::Colour col) {
        if (xStart >= xEnd) return;

        // --- 抑制部分 (Tame) の塗りつぶし ---
        juce::Path tPath;
        tPath.startNewSubPath(xStart, bounds.getHeight());
        for (int x = (int)xStart; x <= (int)xEnd; ++x) {
            tPath.lineTo((float)x, calcY(smoothedPre[x], (float)x));
        }
        for (int x = (int)xEnd; x >= (int)xStart; --x) {
            tPath.lineTo((float)x, calcY(smoothedPre[x] * tameData[x], (float)x));
        }
        tPath.closeSubPath();
        g.setColour(col.withAlpha(0.2f));
        g.fillPath(tPath);

        // --- 処理後 (Post) の塗りつぶし + 縦グラデーション ---
        juce::Path pPath;
        pPath.startNewSubPath(xStart, bounds.getHeight());
        for (int x = (int)xStart; x <= (int)xEnd; ++x) {
            pPath.lineTo((float)x, calcY(smoothedPost[x], (float)x));
        }
        pPath.lineTo(xEnd, bounds.getHeight());
        pPath.closeSubPath();

        // 音量が大きいほど明るくなる縦グラデーション
        juce::ColourGradient grad(col.withAlpha(0.7f), 0, 0, col.withAlpha(0.0f), 0, bounds.getHeight(), false);
        g.setGradientFill(grad);
        g.fillPath(pPath);
        };

    drawBand(0.0f, x1, cLow);
    drawBand(x1, x2, cMid);
    drawBand(x2, bounds.getWidth(), cHigh);

    // ==========================================
    // 3. 最前面の Post 波形輪郭線 (発光)
    // ==========================================
    juce::Path postPath;
    for (int x = 0; x < numPixels; ++x) {
        float y = calcY(smoothedPost[x], (float)x);
        if (x == 0) postPath.startNewSubPath((float)x, y); else postPath.lineTo((float)x, y);
    }
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.strokePath(postPath, juce::PathStrokeType(1.5f));
}

void SpectrumAnalyzer::resized() {}