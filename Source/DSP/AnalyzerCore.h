// ==========================================
// Source/DSP/AnalyzerCore.h
// ==========================================
#ifndef LUMINA_ANALYZERCORE_H
#define LUMINA_ANALYZERCORE_H

#include <JuceHeader.h>
#include <vector>
#include <atomic>

class AnalyzerCore
{
public:
    enum class State {
        Idle,
        WaitingForSignal,
        Analyzing,
        Done
    };

    AnalyzerCore();
    ~AnalyzerCore() = default;

    void prepare(double sampleRate, int fftSize, int hopSize);

    // ⚡ 再トリガーと学習時間の設定に対応
    void startAutoBand(float timeInSeconds = 3.0f);
    void resetToIdle(); // ⚡ 追加: ステートを初期化してループを防ぐ
    void processSpectrumFrame(const float* currentPower, int numBins);

    void setAutoLevelActive(bool active);
    void processAudioBlock(const float* const* inputChannels, float* const* outputChannels, int numChannels, int numSamples);

    State getAutoBandState() const;
    float getAutoBandProgress() const;
    float getProposedCross1() const;
    float getProposedCross2() const;
    float getMatchingGain() const;

private:
    void calculateOptimalCrossovers();

    double currentSampleRate = 44100.0;
    int currentFftSize = 4096;
    int currentHopSize = 1024;
    int numBins = 2049;

    std::atomic<State> autoBandState{ State::Idle };
    std::atomic<float> analysisProgress{ 0.0f };
    std::atomic<float> proposedCross1{ 250.0f };
    std::atomic<float> proposedCross2{ 4000.0f };

    int analyzedFrames = 0;
    int targetFrames = 0;

    std::vector<float> accumulatedSpectrum;
    std::vector<float> smoothedSpectrum;

    std::atomic<bool> isAutoLevelActive{ false };
    std::atomic<float> currentMatchingGain{ 1.0f };

    float inputRmsSq = 0.0f;
    float outputRmsSq = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerCore)
};

#endif // LUMINA_ANALYZERCORE_H