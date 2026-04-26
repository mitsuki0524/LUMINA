// ==========================================
// Source/DSP/AnalyzerCore.cpp
// ==========================================
#include "AnalyzerCore.h"
#include <cmath>

AnalyzerCore::AnalyzerCore() {}

void AnalyzerCore::prepare(double sampleRate, int fftSize, int hopSize)
{
    currentSampleRate = sampleRate;
    currentFftSize = fftSize;
    currentHopSize = hopSize;
    numBins = fftSize / 2 + 1;

    accumulatedSpectrum.assign(static_cast<size_t>(numBins), 0.0f);
    smoothedSpectrum.assign(static_cast<size_t>(numBins), 0.0f);

    autoBandState.store(State::Idle);
    analysisProgress.store(0.0f);
    analyzedFrames = 0;
    targetFrames = static_cast<int>((sampleRate * 3.0) / hopSize);

    inputRmsSq = 0.0f;
    outputRmsSq = 0.0f;
    currentMatchingGain.store(1.0f);
}

void AnalyzerCore::startAutoBand(float timeInSeconds)
{
    // ⚡ ターゲットの学習フレーム数を秒数から動的に計算
    targetFrames = static_cast<int>((currentSampleRate * timeInSeconds) / currentHopSize);

    std::fill(accumulatedSpectrum.begin(), accumulatedSpectrum.end(), 0.0f);
    analyzedFrames = 0;
    analysisProgress.store(0.0f);
    autoBandState.store(State::WaitingForSignal);
}

void AnalyzerCore::processSpectrumFrame(const float* currentPower, int numBinsToProcess)
{
    State currentState = autoBandState.load();
    if (currentState == State::Idle || currentState == State::Done) return;

    float totalEnergy = 0.0f;
    for (int i = 0; i < numBinsToProcess; ++i) {
        totalEnergy += currentPower[i];
    }

    const float silenceThreshold = 1e-6f;

    if (currentState == State::WaitingForSignal) {
        if (totalEnergy > silenceThreshold) {
            autoBandState.store(State::Analyzing);
        }
        else {
            return;
        }
    }

    if (autoBandState.load() == State::Analyzing) {
        for (int i = 0; i < numBinsToProcess; ++i) {
            accumulatedSpectrum[static_cast<size_t>(i)] += currentPower[i];
        }

        analyzedFrames++;
        analysisProgress.store(static_cast<float>(analyzedFrames) / targetFrames);

        if (analyzedFrames >= targetFrames) {
            calculateOptimalCrossovers();
            autoBandState.store(State::Done);
        }
    }
}

void AnalyzerCore::calculateOptimalCrossovers()
{
    const int smoothWindow = 5;
    for (int i = 0; i < numBins; ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int j = -smoothWindow; j <= smoothWindow; ++j) {
            int idx = i + j;
            if (idx >= 0 && idx < numBins) {
                sum += accumulatedSpectrum[static_cast<size_t>(idx)];
                count++;
            }
        }
        smoothedSpectrum[static_cast<size_t>(i)] = sum / static_cast<float>(count);
    }

    float binFreq = static_cast<float>(currentSampleRate) / static_cast<float>(currentFftSize);

    auto findValley = [&](float minHz, float maxHz) -> float {
        int startBin = static_cast<int>(minHz / binFreq);
        int endBin = static_cast<int>(maxHz / binFreq);

        startBin = juce::jlimit(0, numBins - 1, startBin);
        endBin = juce::jlimit(0, numBins - 1, endBin);

        float minPower = 1e9f;
        int minBin = startBin;

        for (int i = startBin; i <= endBin; ++i) {
            if (smoothedSpectrum[static_cast<size_t>(i)] < minPower) {
                minPower = smoothedSpectrum[static_cast<size_t>(i)];
                minBin = i;
            }
        }
        return static_cast<float>(minBin) * binFreq;
        };

    float c1 = findValley(80.0f, 800.0f);
    float c2 = findValley(1500.0f, 8000.0f);

    proposedCross1.store(c1);
    proposedCross2.store(c2);
}

void AnalyzerCore::setAutoLevelActive(bool active)
{
    isAutoLevelActive.store(active);
    if (!active) currentMatchingGain.store(1.0f);
}

void AnalyzerCore::processAudioBlock(const float* const* inputChannels, float* const* outputChannels, int numChannels, int numSamples)
{
    if (!isAutoLevelActive.load()) return;

    float blockInputRmsSq = 0.0f;
    float blockOutputRmsSq = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch) {
        for (int i = 0; i < numSamples; ++i) {
            float inSample = inputChannels[ch][i];
            float outSample = outputChannels[ch][i];
            blockInputRmsSq += inSample * inSample;
            blockOutputRmsSq += outSample * outSample;
        }
    }
    blockInputRmsSq /= static_cast<float>(numChannels * numSamples);
    blockOutputRmsSq /= static_cast<float>(numChannels * numSamples);

    float blockTime = static_cast<float>(numSamples) / static_cast<float>(currentSampleRate);
    float alpha = 1.0f - std::exp(-blockTime / 0.3f);

    inputRmsSq = (1.0f - alpha) * inputRmsSq + alpha * blockInputRmsSq;
    outputRmsSq = (1.0f - alpha) * outputRmsSq + alpha * blockOutputRmsSq;

    float targetGain = 1.0f;
    if (outputRmsSq > 1e-10f && inputRmsSq > 1e-10f) {
        targetGain = std::sqrt(inputRmsSq / outputRmsSq);
    }

    targetGain = juce::jlimit(0.25f, 3.98f, targetGain);
    float currentGain = currentMatchingGain.load();

    for (int i = 0; i < numSamples; ++i) {
        currentGain = currentGain * 0.999f + targetGain * 0.001f;
        for (int ch = 0; ch < numChannels; ++ch) {
            outputChannels[ch][i] *= currentGain;
        }
    }
    currentMatchingGain.store(currentGain);
}

AnalyzerCore::State AnalyzerCore::getAutoBandState() const { return autoBandState.load(); }
float AnalyzerCore::getAutoBandProgress() const { return analysisProgress.load(); }
float AnalyzerCore::getProposedCross1() const { return proposedCross1.load(); }
float AnalyzerCore::getProposedCross2() const { return proposedCross2.load(); }
float AnalyzerCore::getMatchingGain() const { return currentMatchingGain.load(); }