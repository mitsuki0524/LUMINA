#include "MultiResSTFT.h"

MultiResSTFT::MultiResSTFT()
{
    // Band 0: Low (0 - 300Hz)
    bands.push_back({ 4096, 1024,    0.0f,   300.0f, {}, {}, {}, {}, 0 });
    // Band 1: Mid (300 - 4000Hz)
    bands.push_back({ 1024,  256,  300.0f,  4000.0f, {}, {}, {}, {}, 0 });
    // Band 2: High (4000 - Nyquist)
    bands.push_back({ 256,   64, 4000.0f, 22050.0f, {}, {}, {}, {}, 0 });
}

void MultiResSTFT::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;

    for (auto& band : bands) {
        int numBins = band.fftSize / 2 + 1;
        band.spectrum.resize(static_cast<size_t>(numBins));
        band.power.resize(static_cast<size_t>(numBins), 0.0f);
        band.inputFifo.resize(static_cast<size_t>(band.fftSize), 0.0f);
        band.timeDomainData.resize(static_cast<size_t>(band.fftSize * 2), 0.0f);
        band.fifoIndex = 0;
    }

    int maxBins = bands[0].fftSize / 2 + 1;
    mergedPower.assign(static_cast<size_t>(maxBins), 0.0f);

    auto makeHann = [](int size) {
        std::vector<float> win(static_cast<size_t>(size));
        for (int i = 0; i < size; ++i) {
            win[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * i / (size - 1)));
        }
        return win;
        };

    hannLow = makeHann(bands[0].fftSize);
    hannMid = makeHann(bands[1].fftSize);
    hannHigh = makeHann(bands[2].fftSize);

    // --- 統合アルゴリズムのための事前計算（愚直なセットアップ） ---
    binRatioMid = bands[0].fftSize / bands[1].fftSize;   // 例: 4096 / 1024 = 4
    binRatioHigh = bands[0].fftSize / bands[2].fftSize;  // 例: 4096 / 256 = 16

    // 低解像度のビンを高解像度に複製する際、帯域の合計エネルギー（パワー密度）を保つための係数
    scaleMid = 1.0f / static_cast<float>(binRatioMid);
    scaleHigh = 1.0f / static_cast<float>(binRatioHigh);

    // Hzから対応する最高解像度（Band0）のビンインデックスを算出
    float hzPerBin = static_cast<float>(sampleRate) / static_cast<float>(bands[0].fftSize);
    crossoverBinMid = static_cast<int>(300.0f / hzPerBin);
    crossoverBinHigh = static_cast<int>(4000.0f / hzPerBin);

    // 安全装置: インデックスが配列外を指さないようにクランプ
    if (crossoverBinMid < 0) crossoverBinMid = 0;
    if (crossoverBinHigh > maxBins) crossoverBinHigh = maxBins;
}

bool MultiResSTFT::process(const float* input, int numSamples)
{
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

    // --- 生ポインタの取得（DSPセーフ） ---
    float* fifoPtr = band.inputFifo.data();
    float* timeDataPtr = band.timeDomainData.data();
    const float* winPtr = window.data();

    for (int i = 0; i < numSamples; ++i) {
        fifoPtr[band.fifoIndex] = input[i];
        band.fifoIndex++;

        if (band.fifoIndex >= band.fftSize) {

            // 窓掛けとゼロ埋め
            for (int j = 0; j < band.fftSize; ++j) {
                timeDataPtr[j] = fifoPtr[j] * winPtr[j];
                timeDataPtr[j + band.fftSize] = 0.0f;
            }

            fft.performFrequencyOnlyForwardTransform(timeDataPtr);

            int numBins = band.fftSize / 2 + 1;
            float* powerPtr = band.power.data();

            for (int j = 0; j < numBins; ++j) {
                float magnitude = timeDataPtr[j];
                powerPtr[j] = magnitude * magnitude;
            }

            // ホップサイズ分シフト
            int shift = band.fftSize - band.hopSize;
            for (int j = 0; j < shift; ++j) {
                fifoPtr[j] = fifoPtr[j + band.hopSize];
            }
            band.fifoIndex = shift;

            performedFFT = true;
        }
    }

    return performedFFT;
}

void MultiResSTFT::updateMergedPower()
{
    const float* power0 = bands[0].power.data();
    const float* power1 = bands[1].power.data();
    const float* power2 = bands[2].power.data();
    float* dest = mergedPower.data();
    int numBins = static_cast<int>(mergedPower.size());

    // 愚直なループ展開: 浮動小数点の掛け算・割り算をインデックス計算から完全排除
    for (int i = 0; i < numBins; ++i)
    {
        if (i < crossoverBinMid)
        {
            // Low Band: そのままコピー
            dest[i] = power0[i];
        }
        else if (i < crossoverBinHigh)
        {
            // Mid Band: インデックスを間引きし、エネルギーをスケーリング
            dest[i] = power1[i / binRatioMid] * scaleMid;
        }
        else
        {
            // High Band: さらにインデックスを間引き、エネルギーをスケーリング
            dest[i] = power2[i / binRatioHigh] * scaleHigh;
        }
    }
}

const float* MultiResSTFT::getMergedPowerPointer() const
{
    return mergedPower.data();
}

int MultiResSTFT::getLatencySamples() const
{
    return bands.empty() ? 0 : bands[0].fftSize;
}