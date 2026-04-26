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
    float threshold;
    float depth;
    float tonalShift;
    float transShift;
    bool isBypass; // ⚡
    bool isSolo;
    bool isDelta;
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
    std::array<SpectralEngine, 2> spectralEngines;
    std::array<HPSSEngine, 2>     hpssEngines;
    std::array<MaskingModel, 2>   maskingModels;
    std::array<OnsetDetector, 2>  onsetDetectors;

    std::array<std::vector<float>, 2> powerWorkspaces;
    std::array<std::vector<float>, 2> tonalMaskWorkspaces;
    std::array<std::vector<float>, 2> binGainsWorkspaces;

    juce::AudioBuffer<float> inputCopyBuffer;
    std::vector<int> binToBarkMap;

    double mSampleRate = 44100.0;

    struct ParamCache {
        std::atomic<float>* cross1 = nullptr;
        std::atomic<float>* cross2 = nullptr;
        std::atomic<float>* msMode = nullptr;
        std::atomic<float>* autoLevel = nullptr;
        std::atomic<float>* autoBandTrigger = nullptr;

        // ⚡ 新規マスターコントロール
        std::atomic<float>* masterInGain = nullptr;
        std::atomic<float>* masterOutGain = nullptr;
        std::atomic<float>* masterDryWet = nullptr;
        std::atomic<float>* autoBandTime = nullptr;

        struct Band {
            std::atomic<float>* threshM = nullptr;
            std::atomic<float>* depthM = nullptr;
            std::atomic<float>* tonalM = nullptr;
            std::atomic<float>* transM = nullptr;
            std::atomic<float>* threshS = nullptr;
            std::atomic<float>* depthS = nullptr;
            std::atomic<float>* tonalS = nullptr;
            std::atomic<float>* transS = nullptr;
            std::atomic<float>* bypass = nullptr; // ⚡
            std::atomic<float>* solo = nullptr;
            std::atomic<float>* delta = nullptr;
            std::atomic<float>* link = nullptr;
        } bands[3];
    } cache;

    void updateParamCache();
    void handleAutoBandResult();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAProcessor)
};

#endif