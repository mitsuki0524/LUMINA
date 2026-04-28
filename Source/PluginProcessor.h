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

class StereoWidthEngine
{
public:
    StereoWidthEngine() {}

    void prepare(double sampleRate, int samplesPerBlock)
    {
        juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 1 };

        lp1.prepare(spec); hp1.prepare(spec);
        lp2.prepare(spec); hp2.prepare(spec);
        ap1_lo.prepare(spec); ap1_hi.prepare(spec); ap2_lo.prepare(spec);

        lp1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        hp1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        lp2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        hp2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        ap1_lo.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
        ap1_hi.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
        ap2_lo.setType(juce::dsp::LinkwitzRileyFilterType::allpass);

        lp1.setCutoffFrequency(200.0f); hp1.setCutoffFrequency(200.0f);
        ap1_lo.setCutoffFrequency(200.0f); ap1_hi.setCutoffFrequency(200.0f);
        lp2.setCutoffFrequency(5000.0f); hp2.setCutoffFrequency(5000.0f);
        ap2_lo.setCutoffFrequency(5000.0f);

        const float coeffTable[4] = { 0.6151f, -0.3693f, 0.7823f, -0.4999f };
        for (int i = 0; i < 4; ++i) {
            stages[i].coeff = coeffTable[i] * 0.45f;
            stages[i].state = 0.0f;
        }
    }

    void setCutoffs(float lowFreq, float highFreq)
    {
        lp1.setCutoffFrequency(lowFreq); hp1.setCutoffFrequency(lowFreq);
        ap1_lo.setCutoffFrequency(lowFreq); ap1_hi.setCutoffFrequency(lowFreq);
        lp2.setCutoffFrequency(highFreq); hp2.setCutoffFrequency(highFreq);
        ap2_lo.setCutoffFrequency(highFreq);
    }

    void processStereoWidth(juce::AudioBuffer<float>& buffer, float wLow, float wMid, float wHigh)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float M = (L[i] + R[i]) * 0.5f;
            float S = (L[i] - R[i]) * 0.5f;

            float low_prelim = lp1.processSample(0, S);
            float rest = hp1.processSample(0, S);
            float mid_prelim = lp2.processSample(0, rest);
            float high_out = hp2.processSample(0, rest);

            float s_low = ap2_lo.processSample(0, ap1_lo.processSample(0, low_prelim));
            float s_mid = ap1_hi.processSample(0, mid_prelim);
            float s_high = high_out;

            s_low *= wLow;
            s_mid *= wMid;

            float s_hi_decorr = s_high;
            for (int k = 0; k < 4; ++k) {
                float y = stages[k].coeff * s_hi_decorr + stages[k].state;
                stages[k].state = s_hi_decorr - stages[k].coeff * y;
                s_hi_decorr = y;
            }

            float amount = juce::jlimit(0.0f, 1.0f, wHigh - 1.0f);
            float s_high_L = s_high;
            float s_high_R = s_high * (1.0f - amount) + s_hi_decorr * amount;
            if (wHigh < 1.0f) {
                s_high_L *= wHigh;
                s_high_R *= wHigh;
            }

            float final_S_L = s_low + s_mid + s_high_L;
            float final_S_R = s_low + s_mid + s_high_R;

            L[i] = M + final_S_L;
            R[i] = M - final_S_R;
        }
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2, ap1_lo, ap1_hi, ap2_lo;
    struct AllPassStage { float coeff; float state; };
    std::array<AllPassStage, 4> stages;
};

class LUMINAProcessor : public juce::AudioProcessor
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

    std::array<std::vector<float>, 2> powerWorkspaces;
    std::array<std::vector<float>, 2> tonalMaskWorkspaces;
    std::array<std::vector<float>, 2> binGainsWorkspaces;
    std::array<std::vector<float>, 2> tameGainsWorkspaces;

    juce::AudioBuffer<float> inputCopyBuffer;
    juce::AudioBuffer<float> dryDelayBuffer;
    int delayWritePosition = 0;

    std::array<std::array<float, 24>, 2> dynEnvelopes{};

    std::vector<int> binToBarkMap;

    double mSampleRate = 44100.0;
    int mFftSize = 4096;

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