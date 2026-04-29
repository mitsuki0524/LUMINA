// ==========================================
// Source/PluginProcessor.h
// ==========================================
#ifndef LUMINA_PLUGINPROCESSOR_H
#define LUMINA_PLUGINPROCESSOR_H

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <memory>
#include <atomic>

#include "DSP/SpectralEngine.h"
#include "DSP/HPSSEngine.h"
#include "DSP/MaskingModel.h"
#include "DSP/OnsetDetector.h"
#include "DSP/MSEncoder.h"
#include "DSP/AnalyzerCore.h"
#include "GUI/AnalysisFifo.h"

struct BandParams {
    float tameM;
    float tameS;
    float width;
    float threshold;
    float depth;
    float tonalShift;
    float transShift;
    float attack;
    float release;
    bool isBypass;
    bool isSolo;
    bool isDelta;
};

// ⚡ 高域用 All-pass デコリレーター (Schroeder位相分散)
class AllPassDecorrelator
{
public:
    static constexpr int maxStages = 8;
    struct AllPassStage { float coeff = 0.0f; float state = 0.0f; };

    void prepare(double sampleRate) {
        juce::ignoreUnused(sampleRate);
        const float coeffTable[maxStages] = { 0.6151f, -0.3693f, 0.7823f, -0.4999f, 0.8347f, -0.2173f, 0.6879f, -0.5431f };
        for (int i = 0; i < maxStages; ++i) {
            stages[i].coeff = coeffTable[i] * 0.85f; // Intensity
            stages[i].state = 0.0f;
        }
    }

    void reset() {
        for (auto& s : stages) s.state = 0.0f;
    }

    inline float processSample(float input) {
        float x = input;
        for (int i = 0; i < maxStages; ++i) {
            const float a = stages[i].coeff;
            const float y = a * x + stages[i].state;
            stages[i].state = x - a * y;
            x = y;
        }
        return x;
    }

private:
    std::array<AllPassStage, maxStages> stages;
};

// ⚡ 位相補償付き 3バンド Linkwitz-Riley M/S 幅操作エンジン (サブトラクション完全再構成版)
class StereoWidthEngine
{
public:
    StereoWidthEngine() {}

    void prepare(double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 1 };

        m_lp1.prepare(spec); m_lp2.prepare(spec);
        m_ap1.prepare(spec); m_ap2_rest.prepare(spec); m_ap2_low.prepare(spec);

        s_lp1.prepare(spec); s_lp2.prepare(spec);
        s_ap1.prepare(spec); s_ap2_rest.prepare(spec); s_ap2_low.prepare(spec);

        auto setTypes = [](juce::dsp::LinkwitzRileyFilter<float>& lp1, juce::dsp::LinkwitzRileyFilter<float>& lp2,
            juce::dsp::LinkwitzRileyFilter<float>& ap1, juce::dsp::LinkwitzRileyFilter<float>& ap2_rest,
            juce::dsp::LinkwitzRileyFilter<float>& ap2_low)
            {
                lp1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
                lp2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
                ap1.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
                ap2_rest.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
                ap2_low.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
            };

        setTypes(m_lp1, m_lp2, m_ap1, m_ap2_rest, m_ap2_low);
        setTypes(s_lp1, s_lp2, s_ap1, s_ap2_rest, s_ap2_low);

        decorrelator.prepare(sampleRate);
    }

    void setCutoffs(float lowFreq, float highFreq)
    {
        m_lp1.setCutoffFrequency(lowFreq);
        m_ap1.setCutoffFrequency(lowFreq);
        m_lp2.setCutoffFrequency(highFreq);
        m_ap2_rest.setCutoffFrequency(highFreq);
        m_ap2_low.setCutoffFrequency(highFreq);

        s_lp1.setCutoffFrequency(lowFreq);
        s_ap1.setCutoffFrequency(lowFreq);
        s_lp2.setCutoffFrequency(highFreq);
        s_ap2_rest.setCutoffFrequency(highFreq);
        s_ap2_low.setCutoffFrequency(highFreq);
    }

    void processMS(juce::AudioBuffer<float>& msBuffer, float wLow, float wMid, float wHigh)
    {
        float* M = msBuffer.getWritePointer(0);
        float* S = msBuffer.getWritePointer(1);

        for (int i = 0; i < msBuffer.getNumSamples(); ++i)
        {
            // --- Mid の分割と位相補償 ---
            float m_low_pre = m_lp1.processSample(0, M[i]);
            float m_ap1_out = m_ap1.processSample(0, M[i]);
            float m_rest = m_ap1_out - m_low_pre;
            float m_mi = m_lp2.processSample(0, m_rest);
            float m_ap2_out = m_ap2_rest.processSample(0, m_rest);
            float m_hi = m_ap2_out - m_mi;
            float m_lo = m_ap2_low.processSample(0, m_low_pre);

            // --- Side の分割と位相補償 ---
            float s_low_pre = s_lp1.processSample(0, S[i]);
            float s_ap1_out = s_ap1.processSample(0, S[i]);
            float s_rest = s_ap1_out - s_low_pre;
            float s_mi = s_lp2.processSample(0, s_rest);
            float s_ap2_out = s_ap2_rest.processSample(0, s_rest);
            float s_hi = s_ap2_out - s_mi;
            float s_lo = s_ap2_low.processSample(0, s_low_pre);

            // Width コントロール
            s_lo *= wLow;
            s_mi *= wMid;

            float s_hi_decorr = decorrelator.processSample(s_hi);

            if (wHigh > 1.0f)
            {
                float blend = juce::jlimit(0.0f, 1.0f, wHigh - 1.0f);
                s_hi = s_hi * (1.0f - blend) + s_hi_decorr * blend;
            }
            else
            {
                s_hi *= wHigh;
            }

            M[i] = m_lo + m_mi + m_hi;
            S[i] = s_lo + s_mi + s_hi;
        }
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> m_lp1, m_lp2, m_ap1, m_ap2_rest, m_ap2_low;
    juce::dsp::LinkwitzRileyFilter<float> s_lp1, s_lp2, s_ap1, s_ap2_rest, s_ap2_low;
    AllPassDecorrelator decorrelator;
};

// ⚡ リスナーとAsyncUpdaterを継承し、安全なRebuildアーキテクチャを構築
class LUMINAProcessor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener,
    public juce::AsyncUpdater
{
public:
    LUMINAProcessor();
    ~LUMINAProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ⚡ パラメータ変更の検知
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // ⚡ 非同期でのエンジン再構築
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    AnalysisFifo analysisFifo;
    AnalyzerCore analyzerCore;

private:
    MSEncoder msEncoder;
    StereoWidthEngine widthEngine;

    std::array<SpectralEngine, 2> spectralEngines;
    std::array<HPSSEngine, 2>     hpssEngines;
    std::array<MaskingModel, 2>   maskingModels;
    std::array<OnsetDetector, 2>  onsetDetectors;

    // ⚡ DSP Safety: ラムダ式内での動的メモリ確保を排除するためのワークスペース
    std::array<std::vector<float>, 2> powerWorkspaces;
    std::array<std::vector<float>, 2> tonalMaskWorkspaces;
    std::array<std::vector<float>, 2> binGainsWorkspaces;
    std::array<std::vector<float>, 2> tameGainsWorkspaces;

    std::array<std::vector<float>, 2> rawMagsWorkspaces;
    std::array<std::vector<float>, 2> wLowArrWorkspaces;
    std::array<std::vector<float>, 2> wMidArrWorkspaces;
    std::array<std::vector<float>, 2> wHighArrWorkspaces;
    std::array<std::vector<float>, 2> smoothAlphasWorkspaces;
    std::array<std::vector<float>, 2> envWorkspaces;

    juce::AudioBuffer<float> inputCopyBuffer;
    juce::AudioBuffer<float> dryDelayBuffer;
    int delayWritePosition = 0;

    juce::AudioBuffer<float> lookaheadBuffer;
    int lookaheadWritePos = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentOversamplingState = 0;
    float currentLookaheadMs = 0.0f;

    std::array<std::array<float, 24>, 2> dynEnvelopes{};
    std::vector<int> binToBarkMap;

    double mSampleRate = 44100.0;
    int mSamplesPerBlock = 512; // Rebuild用に保持
    int mFftSize = 4096;

    // ⚡ オーディオスレッドとメッセージスレッドの競合を防ぐロック
    juce::SpinLock processLock;

    struct ParamCache {
        std::atomic<float>* proMode = nullptr;
        std::atomic<float>* oversampling = nullptr;
        std::atomic<float>* lookahead = nullptr;
        std::atomic<float>* widthCross1 = nullptr;
        std::atomic<float>* widthCross2 = nullptr;
        std::atomic<float>* autoLevelTimePro = nullptr;

        std::atomic<float>* cross1 = nullptr;
        std::atomic<float>* cross2 = nullptr;
        std::atomic<float>* msMode = nullptr;
        std::atomic<float>* autoLevel = nullptr;
        std::atomic<float>* autoBandTrigger = nullptr;

        std::atomic<float>* masterInGain = nullptr;
        std::atomic<float>* masterOutGain = nullptr;
        std::atomic<float>* masterDryWet = nullptr;
        std::atomic<float>* autoBandTime = nullptr;

        struct Band {
            std::atomic<float>* tame = nullptr;
            std::atomic<float>* tameS = nullptr;
            std::atomic<float>* width = nullptr;
            std::atomic<float>* threshM = nullptr;
            std::atomic<float>* depthM = nullptr;
            std::atomic<float>* tonalM = nullptr;
            std::atomic<float>* transM = nullptr;
            std::atomic<float>* threshS = nullptr;
            std::atomic<float>* depthS = nullptr;
            std::atomic<float>* tonalS = nullptr;
            std::atomic<float>* transS = nullptr;
            std::atomic<float>* bypass = nullptr;
            std::atomic<float>* solo = nullptr;
            std::atomic<float>* delta = nullptr;
            std::atomic<float>* link = nullptr;

            std::atomic<float>* tameSharp = nullptr;
            std::atomic<float>* tameSpeed = nullptr;
            std::atomic<float>* attackM = nullptr;
            std::atomic<float>* attackS = nullptr;
            std::atomic<float>* releaseM = nullptr;
            std::atomic<float>* releaseS = nullptr;
            std::atomic<float>* hpssBlur = nullptr;
            std::atomic<float>* hpssRes = nullptr;
            std::atomic<float>* linkAmt = nullptr;
        } bands[3];
    } cache;

    void updateParamCache();
    void handleAutoBandResult();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAProcessor)
};

#endif // LUMINA_PLUGINPROCESSOR_H