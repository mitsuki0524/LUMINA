#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include <memory>
#include <cmath>

class SpectralEngine
{
public:
    SpectralEngine() = default;
    ~SpectralEngine() = default;

    void prepare(double sampleRate, int fftSize = 4096, float overlapRatio = 0.75f);

    using ProcessingCallback = std::function<void(float* magnitudes, int numBins)>;
    void setProcessingCallback(ProcessingCallback callback);

    void process(const float* input, float* output, int numSamples);

    // ⚡修正: インライン定義を削除し、宣言のみに変更（二重定義エラー C2084 の解消）
    int getLatencySamples() const;
    int getFftSize() const { return currentFftSize; }
    int getNumBins() const { return currentFftSize / 2 + 1; }

private:
    void processWOLA();

    int currentFftSize = 4096;
    int hopSize = 1024;
    double currentSampleRate = 44100.0;

    float gainCorrection = 1.0f;

    std::unique_ptr<juce::dsp::FFT> fft;

    std::vector<float> inputFifo;
    std::vector<float> outputFifo;

    std::vector<float> inputHop;
    std::vector<float> outputHop;
    int fifoIndex = 0;

    std::vector<float> window;

    std::vector<float> timeData;

    std::vector<float> magnitudes;
    std::vector<float> phases;

    ProcessingCallback onProcessSpectrum = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralEngine)
};