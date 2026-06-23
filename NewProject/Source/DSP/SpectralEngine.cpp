#include "SpectralEngine.h"

void SpectralEngine::prepare(double sampleRate, int fftSize, float overlapRatio)
{
    currentSampleRate = sampleRate;
    currentFftSize = fftSize;

    int order = static_cast<int>(std::log2(fftSize));
    fft = std::make_unique<juce::dsp::FFT>(order);

    hopSize = static_cast<int>(std::round(fftSize * (1.0f - overlapRatio)));

    // ⚡ SIMD用にゼロクリア付きでヒープ確保 (アライメント保証)
    inputFifo.allocate(static_cast<size_t>(fftSize), true);
    outputFifo.allocate(static_cast<size_t>(fftSize), true);
    inputHop.allocate(static_cast<size_t>(hopSize), true);
    outputHop.allocate(static_cast<size_t>(hopSize), true);

    timeData.allocate(static_cast<size_t>(fftSize * 2), true);

    int numBins = fftSize / 2 + 1;
    magnitudes.allocate(static_cast<size_t>(numBins), true);
    phases.allocate(static_cast<size_t>(numBins), true);

    // ⚡ Kaiser-Bessel窓の生成 (β = 8.0: マスタリンググレードの -69dB サイドローブ減衰)
    window.allocate(static_cast<size_t>(fftSize), true);
    generateKaiserWindow(fftSize, 8.0f);

    fifoIndex = 0;
}

// ⚡ 第1種0次の変形ベッセル関数 I0(x) の厳密な近似計算 (30次まで拡張して精度向上)
float SpectralEngine::besselI0(float x)
{
    float sum = 1.0f;
    float term = 1.0f;
    const float xHalf = x * 0.5f;
    for (int k = 1; k <= 30; ++k)
    {
        const float kFloat = static_cast<float>(k);
        term *= (xHalf / kFloat) * (xHalf / kFloat);
        sum += term;
        if (term < 1e-12f * sum) break;
    }
    return sum;
}

void SpectralEngine::generateKaiserWindow(int size, float beta)
{
    const float denominator = besselI0(beta);
    const float N = static_cast<float>(size);

    // 1. 標準的な Kaiser 窓の生成
    for (int n = 0; n < size; ++n) {
        const float x = (2.0f * static_cast<float>(n) / N) - 1.0f;
        const float arg = beta * std::sqrt(std::max(0.0f, 1.0f - x * x));
        window[n] = besselI0(arg) / denominator;
    }

    // 2. ⚡ 完璧なヌル（-100dB以下）を実現するための「COLA正規化」 ⚡
    // 窓をホップサイズごとに重ね合わせた時の合計値を、位相（0～hopSize-1）ごとに計算します
    std::vector<double> overlapSum(static_cast<size_t>(hopSize), 0.0);
    for (int n = 0; n < size; ++n) {
        double w = static_cast<double>(window[n]);
        overlapSum[static_cast<size_t>(n % hopSize)] += (w * w); // WOLAなので自乗和
    }

    // 3. 各サンプルを、その位置の合計値の平方根で割ります
    // これにより、オーバーラップ加算後の振幅が「時間軸上のどこでも完全に1.0」になります
    for (int n = 0; n < size; ++n) {
        double s = overlapSum[static_cast<size_t>(n % hopSize)];
        if (s > 1e-15) {
            window[n] /= static_cast<float>(std::sqrt(s));
        }
    }

    // 窓自体が正規化されたため、一律のゲイン補正は不要（1.0）
    gainCorrection = 1.0f;
}

void SpectralEngine::setProcessingCallback(ProcessingCallback callback)
{
    onProcessSpectrum = callback;
}

void SpectralEngine::process(const float* input, float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float currentInput = input[i];
        if (std::isnan(currentInput) || std::isinf(currentInput)) currentInput = 0.0f;

        output[i] = outputHop[fifoIndex];
        inputHop[fifoIndex] = currentInput;

        fifoIndex++;

        if (fifoIndex >= hopSize)
        {
            // FIFOシフト (C標準関数を用いて高速にメモリコピー)
            std::memmove(inputFifo.getData(), inputFifo.getData() + hopSize, static_cast<size_t>(currentFftSize - hopSize) * sizeof(float));
            std::memcpy(inputFifo.getData() + currentFftSize - hopSize, inputHop.getData(), static_cast<size_t>(hopSize) * sizeof(float));

            // WOLA実行
            processWOLA();

            for (int j = 0; j < hopSize; ++j) {
                outputHop[j] = outputFifo[j];
                outputFifo[j] = 0.0f;
            }

            // 出力FIFOのシフト
            std::memmove(outputFifo.getData(), outputFifo.getData() + hopSize, static_cast<size_t>(currentFftSize - hopSize) * sizeof(float));
            std::memset(outputFifo.getData() + currentFftSize - hopSize, 0, static_cast<size_t>(hopSize) * sizeof(float));

            fifoIndex = 0;
        }
    }
}

void SpectralEngine::processWOLA()
{
    if (fft == nullptr) return;

    float* tData = timeData.getData();
    const float* inFifo = inputFifo.getData();
    const float* win = window.getData();
    int N = currentFftSize;
    int numBins = N / 2 + 1;

    // SIMD最適化: 分析窓関数の乗算とゼロパディング
    juce::FloatVectorOperations::multiply(tData, inFifo, win, N);
    juce::FloatVectorOperations::clear(tData + N, N);

    fft->performRealOnlyForwardTransform(tData);

    float* mags = magnitudes.getData();
    float* phs = phases.getData();

    mags[0] = std::abs(tData[0]);
    phs[0] = (tData[0] < 0.0f) ? juce::MathConstants<float>::pi : 0.0f;

    // 複素スペクトルから振幅と位相を分離
    for (int k = 1; k < numBins - 1; ++k) {
        float re = tData[2 * k];
        float im = tData[2 * k + 1];
        mags[k] = std::sqrt(re * re + im * im);
        phs[k] = std::atan2(im, re);
    }

    mags[numBins - 1] = std::abs(tData[1]);
    phs[numBins - 1] = (tData[1] < 0.0f) ? juce::MathConstants<float>::pi : 0.0f;

    // 外部のダイナミクス処理コールバックを実行
    if (onProcessSpectrum != nullptr) {
        onProcessSpectrum(mags, numBins);
    }

    // 振幅と位相から複素スペクトルへ再合成
    float mDC = mags[0];
    if (std::isnan(mDC) || std::isinf(mDC)) mDC = 0.0f;
    tData[0] = mDC * std::cos(phs[0]);

    float mNyq = mags[numBins - 1];
    if (std::isnan(mNyq) || std::isinf(mNyq)) mNyq = 0.0f;
    tData[1] = mNyq * std::cos(phs[numBins - 1]);

    for (int k = 1; k < numBins - 1; ++k) {
        float m = mags[k];
        if (std::isnan(m) || std::isinf(m)) m = 0.0f;
        float p = phs[k];
        tData[2 * k] = m * std::cos(p);
        tData[2 * k + 1] = m * std::sin(p);
    }

    fft->performRealOnlyInverseTransform(tData);

    float* outFifo = outputFifo.getData();
    float correction = gainCorrection;

    // 最適化: 合成窓関数の適用、ゲイン補正、およびオーバーラップ加算を1つのループで実行
    for (int i = 0; i < N; ++i) {
        float outSample = tData[i];
        if (std::isnan(outSample) || std::isinf(outSample)) outSample = 0.0f;
        outFifo[i] += outSample * win[i] * correction; // 合成窓 w_s(n) の適用
    }
}