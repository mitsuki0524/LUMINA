// ==========================================
// Source/DSP/MSEncoder.cpp
// ==========================================
#include "MSEncoder.h"

void MSEncoder::encodeMidSide(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2) return;

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    int numSamples = buffer.getNumSamples();

    // 生ポインタで高速に M/S 変換: M = (L+R)*0.5, S = (L-R)*0.5
    // （コンパイラの自動AVX2/SIMDベクタライゼーションが最も効果的に働く形態です）
    for (int i = 0; i < numSamples; ++i) {
        float l = left[i];
        float r = right[i];
        left[i] = (l + r) * 0.5f;
        right[i] = (l - r) * 0.5f;
    }
}

void MSEncoder::decodeMidSide(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2) return;

    float* mid = buffer.getWritePointer(0);
    float* side = buffer.getWritePointer(1);
    int numSamples = buffer.getNumSamples();

    // 生ポインタで高速に L/R 復元: L = M+S, R = M-S
    for (int i = 0; i < numSamples; ++i) {
        float m = mid[i];
        float s = side[i];
        mid[i] = m + s;
        side[i] = m - s;
    }
}