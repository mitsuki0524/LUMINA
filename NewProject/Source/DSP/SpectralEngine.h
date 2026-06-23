#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <cmath>

class SpectralEngine
{
public:
    SpectralEngine() = default;
    ~SpectralEngine() = default;

    /**
     * @brief エンジンの初期化
     * @param sampleRate サンプルレート (PluginProcessorから動的に渡される)
     * @param fftSize FFTサイズ (44.1k=4096, 96k=8192など可変対応)
     * @param overlapRatio オーバーラップ率 (デフォルト0.75 = 75%)
     */
    void prepare(double sampleRate, int fftSize = 4096, float overlapRatio = 0.75f);

    using ProcessingCallback = std::function<void(float* magnitudes, int numBins)>;
    void setProcessingCallback(ProcessingCallback callback);

    void process(const float* input, float* output, int numSamples);

    int getLatencySamples() const { return currentFftSize; }
    int getFftSize() const { return currentFftSize; }
    int getNumBins() const { return currentFftSize / 2 + 1; }

private:
    void processWOLA();

    // --- Kaiser-Bessel Window 生成用数学関数 ---
    static float besselI0(float x);
    void generateKaiserWindow(int size, float beta);

    int currentFftSize = 4096;
    int hopSize = 1024;
    double currentSampleRate = 44100.0;
    float gainCorrection = 1.0f;

    std::unique_ptr<juce::dsp::FFT> fft;

    // ⚡ AVX2/SIMD アライメントを考慮したメモリ管理
    juce::HeapBlock<float> inputFifo;
    juce::HeapBlock<float> outputFifo;
    juce::HeapBlock<float> inputHop;
    juce::HeapBlock<float> outputHop;
    juce::HeapBlock<float> timeData;
    juce::HeapBlock<float> magnitudes;
    juce::HeapBlock<float> phases;
    juce::HeapBlock<float> window;

    int fifoIndex = 0;
    ProcessingCallback onProcessSpectrum = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralEngine)
};