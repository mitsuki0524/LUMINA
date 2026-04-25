#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    // GUI表示用に縮小された512ビンの周波数を事前計算
    // 例として 0〜22050Hz を 512分割したと仮定してマッピング
    binFrequencies.resize(512);
    for (int i = 0; i < 512; ++i)
    {
        float freq = (i / 511.0f) * 22050.0f;
        // 対数計算の安全のため、最低周波数を20Hzに制限
        if (freq < 20.0f) freq = 20.0f;
        binFrequencies[i] = freq;
    }
}

void SpectrumAnalyzer::updateFrame(const AnalysisFrame& newFrame)
{
    currentFrame = newFrame;
    // 注意: ここで repaint() は呼びません。
    // Timerループで一括して repaint() を呼ぶことで、UIの過負荷を防ぎます。
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float width = bounds.getWidth();
    float height = bounds.getHeight();

    // 1. 背景とグリッド線の描画
    g.setColour(juce::Colour(0xff2A2A2A));
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colours::grey.withAlpha(0.2f));
    // 簡易的なHzグリッド（100Hz, 1kHz, 10kHz）
    float x100 = mapToLogX(100.0f, width);
    float x1k = mapToLogX(1000.0f, width);
    float x10k = mapToLogX(10000.0f, width);
    g.drawVerticalLine((int)x100, 0.0f, height);
    g.drawVerticalLine((int)x1k, 0.0f, height);
    g.drawVerticalLine((int)x10k, 0.0f, height);

    // 2. Bark帯域パワー（マスキングモデルの認識状態）の描画
    // 背景のブロック状のグラフとして、耳が捉えているエネルギーを可視化
    g.setColour(juce::Colour(0xff4A90E2).withAlpha(0.3f)); // 薄いブルー
    for (int i = 0; i < 24; ++i)
    {
        float barkDB = currentFrame.barkPower[i];
        if (barkDB > -96.0f)
        {
            float freqLeft = getBarkCenterFreq(std::max(0, i - 1));
            float freqRight = getBarkCenterFreq(i);
            float xL = mapToLogX(freqLeft, width);
            float xR = mapToLogX(freqRight, width);

            // dBをLinearに簡易変換してY軸計算
            float mag = juce::Decibels::decibelsToGain(barkDB);
            float y = mapToLogY(mag, height);

            g.fillRect(xL, y, std::max(1.0f, xR - xL), height - y);
        }
    }

    // 3. 高解像度スペクトルパスの構築と描画
    juce::Path spectrumPath;
    bool firstPoint = true;

    for (int i = 0; i < 512; ++i)
    {
        float x = mapToLogX(binFrequencies[i], width);
        float y = mapToLogY(currentFrame.magnitudeSpectrum[i], height);

        if (firstPoint) {
            spectrumPath.startNewSubPath(x, y);
            firstPoint = false;
        }
        else {
            spectrumPath.lineTo(x, y);
        }
    }

    // スペクトル線の描画（鮮やかな緑）
    g.setColour(juce::Colour(0xff1D9E75));
    g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));

    // スペクトル下部のグラデーション塗りつぶし
    spectrumPath.lineTo(width, height);
    spectrumPath.lineTo(0.0f, height);
    spectrumPath.closeSubPath();

    juce::ColourGradient gradient(juce::Colour(0xff1D9E75).withAlpha(0.4f), 0, 0,
        juce::Colours::transparentBlack, 0, height, false);
    g.setGradientFill(gradient);
    g.fillPath(spectrumPath);
}

void SpectrumAnalyzer::resized()
{
    // 現在は内部コンポーネントを持たないため空
}

float SpectrumAnalyzer::mapToLogX(float freqHz, float width) const
{
    const float minFreq = 20.0f;
    const float maxFreq = 22050.0f;

    float f = juce::jlimit(minFreq, maxFreq, freqHz);
    float minLog = std::log10(minFreq);
    float maxLog = std::log10(maxFreq);
    float currentLog = std::log10(f);

    return width * ((currentLog - minLog) / (maxLog - minLog));
}

float SpectrumAnalyzer::mapToLogY(float magnitude, float height) const
{
    const float minDB = -96.0f;
    const float maxDB = 0.0f;

    // 振幅をデシベルに変換（下限 -96dB）
    float db = juce::Decibels::gainToDecibels(magnitude, minDB);
    db = juce::jlimit(minDB, maxDB, db);

    // Y軸は上が0、下がheightになるため反転させる
    return height * (1.0f - ((db - minDB) / (maxDB - minDB)));
}

float SpectrumAnalyzer::getBarkCenterFreq(int barkIndex) const
{
    // Bark 0~23 に対する概算の代表周波数テーブル
    static const float barkFreqs[24] = {
        50, 150, 250, 350, 450, 570, 700, 840, 1000, 1170, 1370, 1600,
        1850, 2150, 2500, 2900, 3400, 4000, 4800, 5800, 7000, 8500, 10500, 13500
    };
    if (barkIndex < 0) return 20.0f;
    if (barkIndex > 23) return 20000.0f;
    return barkFreqs[barkIndex];
}