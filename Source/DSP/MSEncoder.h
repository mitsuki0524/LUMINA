// ==========================================
// Source/DSP/MSEncoder.h
// ==========================================
#pragma once
#include <JuceHeader.h>

class MSEncoder
{
public:
    MSEncoder() = default;
    ~MSEncoder() = default;

    // L/R を Mid/Side に変換（インプレース処理）
    void encodeMidSide(juce::AudioBuffer<float>& buffer);
    // Mid/Side を L/R に戻す（インプレース処理）
    void decodeMidSide(juce::AudioBuffer<float>& buffer);
};