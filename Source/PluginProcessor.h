#pragma once

#include <JuceHeader.h>
#include "DSP/MultiResSTFT.h"
#include "DSP/HPSSEngine.h"
#include "DSP/MaskingModel.h"
#include "DSP/OnsetDetector.h"
#include "DSP/MSEncoder.h"
#include "GUI/AnalysisFifo.h"

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

    // GUI へのデータ送信用 FIFO
    AnalysisFifo analysisFifo;

private:
    // DSPモジュール群のインスタンス化
    MSEncoder     msEncoder;
    MultiResSTFT  stft;
    HPSSEngine    hpss;
    MaskingModel  maskingModel;
    OnsetDetector onsetDetector;

    // ジッパーノイズ防止用スムーザー
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmooth;

    // HPSSからのマスク結果を受け取るバッファ (動的アロケート回避のため事前確保)
    std::vector<float> tonalMaskWorkspace;

    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUMINAProcessor)
};