// ==========================================
// Source/GUI/SpectrumAnalyzer.cpp
// ==========================================
#include "SpectrumAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

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

    // --- データ再マッピング ---
    // ⚡ 動的メモリ確保(std::vector)を排除し、固定長バッファ(std::array)を使用
    constexpr int MAX_PIXELS = 4096;
    int numPixels = std::min(static_cast<int>(bounds.getWidth()), MAX_PIXELS);
    if (numPixels <= 0) return;

    std::array<float, MAX_PIXELS> preData = { 0 };
    std::array<float, MAX_PIXELS> postData = { 0 };
    std::array<float, MAX_PIXELS> tameData = { 1.0f };
    const float nyquist = 22050.0f;

    // ⚡ Solo時のGUI側での強制ゼロ化（壁）を完全撤去
    // これにより、DSPから送られてきたリアルなクロスフェード波形をそのまま描画します。
    for (int x = 0; x < numPixels; ++x) {
        float freq = getFreqFromX(static_cast<float>(x), static_cast<float>(numPixels));
        float linearIdx = (freq / nyquist) * 511.0f;
        int idx = static_cast<int>(linearIdx);
        float frac = linearIdx - static_cast<float>(idx);

        if (idx >= 0 && idx < 511) {
            preData[x] = currentFrame.unprocessedSpectrum[idx] * (1.0f - frac) + currentFrame.unprocessedSpectrum[idx + 1] * frac;
            tameData[x] = currentFrame.tameSpectrum[idx] * (1.0f - frac) + currentFrame.tameSpectrum[idx + 1] * frac;
            postData[x] = currentFrame.magnitudeSpectrum[idx] * (1.0f - frac) + currentFrame.magnitudeSpectrum[idx + 1] * frac;
        }
        else {
            preData[x] = 0.0f; postData[x] = 0.0f; tameData[x] = 1.0f;
        }
    }

    // --- 均等な平滑化 ---
    std::array<float, MAX_PIXELS> smoothedPre = preData;
    std::array<float, MAX_PIXELS> smoothedPost = postData;
    for (int pass = 0; pass < 2; ++pass) {
        std::array<float, MAX_PIXELS> tempPre = smoothedPre;
        std::array<float, MAX_PIXELS> tempPost = smoothedPost;
        for (int i = 1; i < numPixels - 1; ++i) {
            smoothedPre[i] = (tempPre[i - 1] + tempPre[i] * 2.0f + tempPre[i + 1]) / 4.0f;
            smoothedPost[i] = (tempPost[i - 1] + tempPost[i] * 2.0f + tempPost[i + 1]) / 4.0f;
        }
    }

    // --- 連続的なY座標計算 ---
    auto calcY = [&](float mag, float x) -> float {
        float freq = getFreqFromX(x, bounds.getWidth());
        float tiltDB = 3.0f * std::log2(std::max(20.0f, freq) / 1000.0f);
        // 無音時(-120dB)でも不連続なジャンプを起こさせない
        float db = juce::Decibels::gainToDecibels(mag, -120.0f) + tiltDB;
        db = juce::jlimit(minDB, maxDB, db);
        return juce::jmap(db, minDB, maxDB, bounds.getHeight(), 0.0f);
        };

    // --- パスの生成 ---
    juce::Path prePath, tamePath, postPath, postOutline;

    for (int x = 0; x < numPixels; ++x) {
        float yPre = calcY(smoothedPre[x], static_cast<float>(x));
        float yPost = calcY(smoothedPost[x], static_cast<float>(x));

        if (x == 0) {
            prePath.startNewSubPath(0.0f, yPre);
            postOutline.startNewSubPath(0.0f, yPost);
        }
        else {
            prePath.lineTo(static_cast<float>(x), yPre);
            postOutline.lineTo(static_cast<float>(x), yPost);
        }
    }

    // Tame用ポリゴン (上からPre、下からTame適用後のPostで閉じる)
    tamePath.startNewSubPath(0.0f, bounds.getHeight());
    for (int x = 0; x < numPixels; ++x) tamePath.lineTo(static_cast<float>(x), calcY(smoothedPre[x], static_cast<float>(x)));
    for (int x = numPixels - 1; x >= 0; --x) tamePath.lineTo(static_cast<float>(x), calcY(smoothedPre[x] * tameData[x], static_cast<float>(x)));
    tamePath.closeSubPath();

    // Post用ポリゴン (底面と波形で閉じる)
    postPath.startNewSubPath(0.0f, bounds.getHeight());
    for (int x = 0; x < numPixels; ++x) postPath.lineTo(static_cast<float>(x), calcY(smoothedPost[x], static_cast<float>(x)));
    postPath.lineTo(static_cast<float>(numPixels - 1), bounds.getHeight());
    postPath.closeSubPath();

    // --- DSP同期の水平グラデーション構築 ---
    juce::Colour cLow = juce::Colour::fromString("FFFF8C00");
    juce::Colour cMid = juce::Colour::fromString("FF764DFF");
    juce::Colour cHigh = juce::Colour::fromString("FF00E5FF");
    const float transRatio = 1.414f; // DSPのクロスフェード比率と完全一致させる

    auto makeHorizontalGradient = [&](float alpha) -> juce::ColourGradient {
        juce::ColourGradient grad;
        grad.point1 = { 0.0f, 0.0f };
        grad.point2 = { bounds.getWidth(), 0.0f };

        // Crossover 1 (Low -> Mid) の遷移帯域を計算
        float nx1_l = juce::jlimit(0.0f, 1.0f, getXFromFreq(cross1 / transRatio, bounds.getWidth()) / bounds.getWidth());
        float nx1_u = juce::jlimit(0.0f, 1.0f, getXFromFreq(cross1 * transRatio, bounds.getWidth()) / bounds.getWidth());
        // Crossover 2 (Mid -> High) の遷移帯域を計算
        float nx2_l = juce::jlimit(0.0f, 1.0f, getXFromFreq(cross2 / transRatio, bounds.getWidth()) / bounds.getWidth());
        float nx2_u = juce::jlimit(0.0f, 1.0f, getXFromFreq(cross2 * transRatio, bounds.getWidth()) / bounds.getWidth());

        // JUCEのColourGradientのポイント追加は昇順である必要があるための安全処理
        nx1_u = std::max(nx1_l + 0.001f, nx1_u);
        nx2_l = std::max(nx1_u + 0.001f, nx2_l);
        nx2_u = std::max(nx2_l + 0.001f, nx2_u);

        grad.addColour(0.0f, cLow.withAlpha(alpha));
        if (nx1_l > 0.0f) grad.addColour(nx1_l, cLow.withAlpha(alpha));
        if (nx1_u < 1.0f) grad.addColour(nx1_u, cMid.withAlpha(alpha));
        if (nx2_l < 1.0f) grad.addColour(nx2_l, cMid.withAlpha(alpha));
        if (nx2_u < 1.0f) grad.addColour(nx2_u, cHigh.withAlpha(alpha));
        grad.addColour(1.0f, cHigh.withAlpha(alpha));

        return grad;
        };

    // ==========================================
    // 描画実行
    // ==========================================

    // 1. Pre (未処理) 波形の輪郭線
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.strokePath(prePath, juce::PathStrokeType(1.0f));

    // 2. Tame (抑制部分) の塗りつぶし
    g.setGradientFill(makeHorizontalGradient(0.2f));
    g.fillPath(tamePath);

    // 3. Post (処理後) の塗りつぶし + 縦グラデーション(Clip処理による合成)
    g.saveState();
    g.reduceClipRegion(postPath);

    // 3-a: 水平方向のDSP同期カラーブレンド
    g.setGradientFill(makeHorizontalGradient(0.7f));
    g.fillAll();

    // 3-b: 縦方向の発光・透明感フェード (背景色へ溶け込む)
    juce::ColourGradient vFade(juce::Colours::transparentBlack, 0.0f, 0.0f, bgColor, 0.0f, bounds.getHeight(), false);
    g.setGradientFill(vFade);
    g.fillAll();
    g.restoreState();

    // 4. 最前面の Post 波形輪郭線 (発光)
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.strokePath(postOutline, juce::PathStrokeType(1.5f));
}

void SpectrumAnalyzer::resized() {}