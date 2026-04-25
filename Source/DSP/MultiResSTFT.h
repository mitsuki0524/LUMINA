#pragma once

#include <JuceHeader.h>
#include <vector>
#include <complex>

struct STFTBand {
    int fftSize;
    int hopSize;
    float freqLow;
    float freqHigh;

    std::vector<std::complex<float>> spectrum;
    std::vector<float> power;

    std::vector<float> inputFifo;
    std::vector<float> timeDomainData;
    int fifoIndex = 0;
};

class MultiResSTFT
{
public:
    MultiResSTFT();
    ~MultiResSTFT() = default;

    void prepare(double sampleRate);

    /**
     * @brief オーディオを処理し、低域(Band 0)の新しいFFTフレームが生成されたらtrueを返す
     */
    bool process(const float* input, int numSamples);

    std::vector<float> getMergedPowerSpectrum() const;
    int getLatencySamples() const;

    std::vector<STFTBand> bands;

private:
    bool processOneBand(const float* input, int numSamples,
        STFTBand& band, juce::dsp::FFT& fft,
        const std::vector<float>& window);

    juce::dsp::FFT fftLow{ 12 };
    juce::dsp::FFT fftMid{ 10 };
    juce::dsp::FFT fftHigh{ 8 };

    std::vector<float> hannLow;
    std::vector<float> hannMid;
    std::vector<float> hannHigh;

    std::vector<float> mergedPower;

    double currentSampleRate = 44100.0;
};