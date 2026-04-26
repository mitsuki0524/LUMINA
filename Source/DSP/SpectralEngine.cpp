#include "SpectralEngine.h"

void SpectralEngine::prepare(double sampleRate, int fftSize, float overlapRatio)
{
    currentSampleRate = sampleRate;
    currentFftSize = fftSize;

    int order = static_cast<int>(std::log2(fftSize));
    fft = std::make_unique<juce::dsp::FFT>(order);

    hopSize = static_cast<int>(std::round(fftSize * (1.0f - overlapRatio)));

    inputFifo.assign(static_cast<size_t>(fftSize), 0.0f);
    outputFifo.assign(static_cast<size_t>(fftSize), 0.0f);

    inputHop.assign(static_cast<size_t>(hopSize), 0.0f);
    outputHop.assign(static_cast<size_t>(hopSize), 0.0f);
    fifoIndex = 0;

    timeData.assign(static_cast<size_t>(fftSize * 2), 0.0f);

    int numBins = fftSize / 2 + 1;
    magnitudes.assign(static_cast<size_t>(numBins), 0.0f);
    phases.assign(static_cast<size_t>(numBins), 0.0f);

    window.assign(static_cast<size_t>(fftSize), 0.0f);
    double windowPowerSum = 0.0;
    for (int i = 0; i < fftSize; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * i / static_cast<float>(fftSize)));
        windowPowerSum += static_cast<double>(window[i]) * window[i];
    }

    gainCorrection = static_cast<float>(static_cast<double>(hopSize) / windowPowerSum);
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

        output[i] = outputHop[static_cast<size_t>(fifoIndex)];
        inputHop[static_cast<size_t>(fifoIndex)] = currentInput;

        fifoIndex++;

        if (fifoIndex >= hopSize)
        {
            std::memmove(inputFifo.data(), inputFifo.data() + hopSize, static_cast<size_t>(currentFftSize - hopSize) * sizeof(float));
            std::memcpy(inputFifo.data() + currentFftSize - hopSize, inputHop.data(), static_cast<size_t>(hopSize) * sizeof(float));

            processWOLA();

            for (int j = 0; j < hopSize; ++j) {
                outputHop[j] = outputFifo[j];
                outputFifo[j] = 0.0f;
            }

            std::copy(outputFifo.begin() + hopSize, outputFifo.end(), outputFifo.begin());
            std::fill(outputFifo.end() - hopSize, outputFifo.end(), 0.0f);

            fifoIndex = 0;
        }
    }
}

void SpectralEngine::processWOLA()
{
    if (fft == nullptr) return;

    float* tData = timeData.data();
    const float* inFifo = inputFifo.data();
    const float* win = window.data();
    int N = currentFftSize;
    int numBins = N / 2 + 1;

    for (int i = 0; i < N; ++i) {
        tData[i] = inFifo[i] * win[i];
    }
    for (int i = N; i < 2 * N; ++i) {
        tData[i] = 0.0f;
    }

    fft->performRealOnlyForwardTransform(tData);

    float* mags = magnitudes.data();
    float* phs = phases.data();

    // JUCEの特殊なパック形式（[0]にDC、[1]にNyquist）を正確に解凍
    mags[0] = std::abs(tData[0]);
    phs[0] = (tData[0] < 0.0f) ? juce::MathConstants<float>::pi : 0.0f;

    for (int k = 1; k < numBins - 1; ++k) {
        float re = tData[2 * k];
        float im = tData[2 * k + 1];
        mags[k] = std::sqrt(re * re + im * im);
        phs[k] = std::atan2(im, re);
    }

    mags[numBins - 1] = std::abs(tData[1]);
    phs[numBins - 1] = (tData[1] < 0.0f) ? juce::MathConstants<float>::pi : 0.0f;

    if (onProcessSpectrum != nullptr) {
        onProcessSpectrum(mags, numBins);
    }

    // 極座標からJUCEパック形式へ正確に再合成
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

    float* outFifo = outputFifo.data();
    for (int i = 0; i < N; ++i) {
        float outSample = tData[i];
        if (std::isnan(outSample) || std::isinf(outSample)) outSample = 0.0f;
        outFifo[i] += outSample * win[i] * gainCorrection;
    }
}

int SpectralEngine::getLatencySamples() const
{
    return currentFftSize;
}