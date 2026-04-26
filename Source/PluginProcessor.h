#ifndef LUMINA_PLUGINPROCESSOR_H
#define LUMINA_PLUGINPROCESSOR_H

// JuceHeader.h を直接モジュールと標準ライブラリに置き換え
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <memory>

#include "DSP/SpectralEngine.h"
#include "DSP/SpectralEngine.h"
#include "DSP/HPSSEngine.h"
#include "DSP/MaskingModel.h"
#include "DSP/OnsetDetector.h"
#include "DSP/MSEncoder.h"
#include "GUI/AnalysisFifo.h"

class LUMINAEditor;

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

private:
    MSEncoder msEncoder;

    std::array<SpectralEngine, 2> spectralEngines;
    std::array<HPSSEngine, 2>     hpssEngines;
    std::array<MaskingModel, 2>   maskingModels;
    std::array<OnsetDetector, 2>  onsetDetectors;

    std::array<std::vector<float>, 2> powerWorkspaces;
    std::array<std::vector<float>, 2> tonalMaskWorkspaces;
    std::array<std::vector<float>, 2> binGainsWorkspaces;

    std::vector<int> binToBarkMap;
    double currentSampleRate = 44100.0;

    struct BandParams {
        float threshold;
        float depth;
        float tonalShift;
        float transShift;
        bool isSolo;
        bool isDelta;
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAProcessor)
};

#endif