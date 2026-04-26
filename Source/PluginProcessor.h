#ifndef LUMINA_PLUGINPROCESSOR_H
#define LUMINA_PLUGINPROCESSOR_H

#include <JuceHeader.h>
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

    // 解析モジュールへ渡すためのワークスペース群
    std::array<std::vector<float>, 2> powerWorkspaces; // ⚡追加: パワースペクトル用
    std::array<std::vector<float>, 2> tonalMaskWorkspaces;
    std::array<std::vector<float>, 2> binGainsWorkspaces;

    std::vector<int> binToBarkMap;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAProcessor)
};

#endif // LUMINA_PLUGINPROCESSOR_H