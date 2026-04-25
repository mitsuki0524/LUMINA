#pragma once

#include <JuceHeader.h>
#include <vector>
#include <complex>

struct STFTBand {
    int fftSize;       // FFTサイズ (4096/1024/256)
    int hopSize;       // ホップサイズ (fftSize/4 推奨)
    float freqLow;     // 担当下限周波数 (Hz)
    float freqHigh;    // 担当上限周波数 (Hz)

    // 分析結果
    std::vector<std::complex<float>> spectrum;
    std::vector<float> power;

    // --- リアルタイム処理用の内部バッファ (動的アロケート回避用) ---
    std::vector<float> inputFifo;       // 入力サンプルの蓄積用
    std::vector<float> timeDomainData;  // FFT作業用バッファ
    int fifoIndex = 0;                  // 現在の書き込み位置
};

class MultiResSTFT
{
public:
    MultiResSTFT();
    ~MultiResSTFT() = default;

    void prepare(double sampleRate);
    void process(const float* input, int numSamples);

    std::vector<float> getMergedPowerSpectrum() const;
    int getLatencySamples() const;

    // 3バンド構成: 低(〜300Hz) / 中(300〜4kHz) / 高(4kHz〜)
    std::vector<STFTBand> bands;

private:
    void processOneBand(const float* input, int numSamples,
        STFTBand& band, juce::dsp::FFT& fft,
        const std::vector<float>& window);

    // 帯域ごとのFFTオブジェクト (コンストラクタで2^orderを指定)
    juce::dsp::FFT fftLow{ 12 }; // 2^12 = 4096
    juce::dsp::FFT fftMid{ 10 }; // 2^10 = 1024
    juce::dsp::FFT fftHigh{ 8 }; // 2^8  =  256

    // 事前計算済みの窓関数バッファ
    std::vector<float> hannLow;
    std::vector<float> hannMid;
    std::vector<float> hannHigh;

    // マスキング閾値計算などに渡すための全帯域合成バッファ
    std::vector<float> mergedPower;

    double currentSampleRate = 44100.0;
};