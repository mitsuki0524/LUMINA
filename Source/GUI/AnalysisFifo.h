#pragma once
#include <JuceHeader.h>
#include <array>

// GUIスレッドへ送るための1フレーム分の分析データ
struct AnalysisFrame {
    std::array<float, 512> magnitudeSpectrum{}; // 縮小されたスペクトル表示用
    std::array<float, 24>  barkGainReduction{}; // 各Bark帯域のGR
    std::array<float, 24>  barkPower{}; // 各Bark帯域のパワー
    float rmsLevel = 0.0f;
    float crestFactor = 0.0f;
    float phaseCorrelation = 0.0f;
    bool  isOnset = false;
};

// オーディオスレッドをブロックしないLock-freeなデータ転送クラス
class AnalysisFifo
{
public:
    static constexpr int SIZE = 32;

    void push(const AnalysisFrame& f)
    {
        int s1, n1, s2, n2;
        fifo_.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 > 0) buf_[s1] = f;
        fifo_.finishedWrite(n1 + n2);
    }

    bool pop(AnalysisFrame& out)
    {
        int s1, n1, s2, n2;
        fifo_.prepareToRead(1, s1, n1, s2, n2);
        if (n1 <= 0) return false;
        out = buf_[s1];
        fifo_.finishedRead(n1 + n2);
        return true;
    }

private:
    juce::AbstractFifo fifo_{ SIZE };
    std::array<AnalysisFrame, SIZE> buf_;
};