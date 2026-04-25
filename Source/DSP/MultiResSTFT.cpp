#include "MultiResSTFT.h"

MultiResSTFT::MultiResSTFT()
{
    bands.push_back({ 4096, 1024,    0.0f,   300.0f, {}, {}, {}, {}, 0 });
    bands.push_back({ 1024,  256,  300.0f,  4000.0f, {}, {}, {}, {}, 0 });
    bands.push_back({ 256,   64, 4000.0f, 22050.0f, {}, {}, {}, {}, 0 });
}

void MultiResSTFT::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;

    for (auto& band : bands) {
        int numBins = band.fftSize / 2 + 1;
        band.spectrum.resize(numBins);
        band.power.resize(numBins, 0.0f);
        band.inputFifo.resize(band.fftSize, 0.0f);
        band.timeDomainData.resize(band.fftSize * 2, 0.0f);
        band.fifoIndex = 0;
    }

    mergedPower.resize(bands[0].fftSize / 2 + 1, 0.0f);

    auto makeHann = [](int size) {
        std::vector<float> win(size);
        for (int i = 0; i < size; ++i) {
            win[i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * i / (size - 1)));
        }
        return win;
        };

    hannLow = makeHann(bands[0].fftSize);
    hannMid = makeHann(bands[1].fftSize);
    hannHigh = makeHann(bands[2].fftSize);
}

bool MultiResSTFT::process(const float* input, int numSamples)
{
    // 最も解像度が高い（更新頻度が低い）Band 0 を基準にフレームの更新を判定
    bool frameReady = processOneBand(input, numSamples, bands[0], fftLow, hannLow);

    processOneBand(input, numSamples, bands[1], fftMid, hannMid);
    processOneBand(input, numSamples, bands[2], fftHigh, hannHigh);

    return frameReady;
}

bool MultiResSTFT::processOneBand(const float* input, int numSamples,
    STFTBand& band, juce::dsp::FFT& fft,
    const std::vector<float>& window)
{
    bool performedFFT = false;

    for (int i = 0; i < numSamples; ++i) {
        band.inputFifo[band.fifoIndex] = input[i];
        band.fifoIndex++;

        if (band.fifoIndex >= band.fftSize) {

            for (int j = 0; j < band.fftSize; ++j) {
                band.timeDomainData[j] = band.inputFifo[j] * window[j];
                band.timeDomainData[j + band.fftSize] = 0.0f;
            }

            fft.performFrequencyOnlyForwardTransform(band.timeDomainData.data());

            int numBins = band.fftSize / 2 + 1;
            for (int j = 0; j < numBins; ++j) {
                float magnitude = band.timeDomainData[j];
                band.power[j] = magnitude * magnitude;
            }

            int shift = band.fftSize - band.hopSize;
            for (int j = 0; j < shift; ++j) {
                band.inputFifo[j] = band.inputFifo[j + band.hopSize];
            }
            band.fifoIndex = shift;

            performedFFT = true; // FFTが実行されたフラグを立てる
        }
    }

    return performedFFT;
}

std::vector<float> MultiResSTFT::getMergedPowerSpectrum() const
{
    return mergedPower;
}

int MultiResSTFT::getLatencySamples() const
{
    return bands.empty() ? 0 : bands[0].fftSize;
}