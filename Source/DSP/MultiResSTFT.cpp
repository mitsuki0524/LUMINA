#include "MultiResSTFT.h"

//==============================================================================
MultiResSTFT::MultiResSTFT()
{
    // 各帯域の定義を初期化 (fftSize, hopSize, freqLow, freqHigh)
    bands.push_back({ 4096, 1024,    0.0f,   300.0f, {}, {}, {}, {}, 0 });
    bands.push_back({ 1024,  256,  300.0f,  4000.0f, {}, {}, {}, {}, 0 });
    bands.push_back({ 256,   64, 4000.0f, 22050.0f, {}, {}, {}, {}, 0 });
}

//==============================================================================
void MultiResSTFT::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;

    // 【DSP Safety】再生開始時に全バッファを最大サイズで事前確保します。
    // processBlock 内でのメモリ確保を完全に防ぐための必須処置です。
    for (auto& band : bands) {
        int numBins = band.fftSize / 2 + 1;
        band.spectrum.resize(numBins);
        band.power.resize(numBins, 0.0f);

        band.inputFifo.resize(band.fftSize, 0.0f);

        // juce::dsp::FFT は実数入力の場合、2倍のサイズの作業バッファを要求します
        band.timeDomainData.resize(band.fftSize * 2, 0.0f);
        band.fifoIndex = 0;
    }

    // 統合パワーバッファは、最も周波数解像度が高いLowバンドのビン数に合わせて確保
    mergedPower.resize(bands[0].fftSize / 2 + 1, 0.0f);

    // 窓関数の事前生成 (Hann窓)
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

//==============================================================================
void MultiResSTFT::process(const float* input, int numSamples)
{
    // 各帯域ごとに独立してサンプルの蓄積とFFT解析を行います
    processOneBand(input, numSamples, bands[0], fftLow, hannLow);
    processOneBand(input, numSamples, bands[1], fftMid, hannMid);
    processOneBand(input, numSamples, bands[2], fftHigh, hannHigh);

    // ※後続のフェーズにて、ここで計算された各band.powerを
    // mergedPowerの対応する周波数インデックスに統合する処理を追加します。
}

void MultiResSTFT::processOneBand(const float* input, int numSamples,
    STFTBand& band, juce::dsp::FFT& fft,
    const std::vector<float>& window)
{
    // 【DSP Safety】イテレータを避け、生ポインタと添字による高速なループ処理
    for (int i = 0; i < numSamples; ++i) {
        // FIFOバッファに1サンプルずつ蓄積
        band.inputFifo[band.fifoIndex] = input[i];
        band.fifoIndex++;

        // 指定したFFTサイズ分データが溜まったらFFTを実行
        if (band.fifoIndex >= band.fftSize) {

            // 1. 窓関数の適用とゼロパディング
            for (int j = 0; j < band.fftSize; ++j) {
                band.timeDomainData[j] = band.inputFifo[j] * window[j];
                band.timeDomainData[j + band.fftSize] = 0.0f;
            }

            // 2. FFT実行 (インプレース処理・振幅スペクトルのみ計算)
            fft.performFrequencyOnlyForwardTransform(band.timeDomainData.data());

            // 3. パワースペクトルの算出
            int numBins = band.fftSize / 2 + 1;
            for (int j = 0; j < numBins; ++j) {
                float magnitude = band.timeDomainData[j];
                band.power[j] = magnitude * magnitude;
            }

            // 4. Overlap-Addの入力側処理: ホップサイズ分だけデータを前にシフト
            int shift = band.fftSize - band.hopSize;
            for (int j = 0; j < shift; ++j) {
                band.inputFifo[j] = band.inputFifo[j + band.hopSize];
            }
            band.fifoIndex = shift; // 書き込み位置をシフト位置に戻す
        }
    }
}

//==============================================================================
std::vector<float> MultiResSTFT::getMergedPowerSpectrum() const
{
    return mergedPower;
}

int MultiResSTFT::getLatencySamples() const
{
    // 最大FFTサイズ(4096)がプラグイン全体の基礎レイテンシーとなります
    return bands.empty() ? 0 : bands[0].fftSize;
}