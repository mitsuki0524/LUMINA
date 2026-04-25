#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LUMINAProcessor::LUMINAProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

LUMINAProcessor::~LUMINAProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LUMINAProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Warning C4996 回避のため、ParameterID を明示的に使用
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "THRESHOLD", 1 }, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "DEPTH", 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.5f), 10.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "RELEASE", 1 }, "Release",
        juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.5f), 50.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "LINEAR_PHASE", 1 }, "Linear Phase", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "MS_MODE", 1 }, "M/S Mode", false));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String LUMINAProcessor::getName() const { return JucePlugin_Name; }
bool LUMINAProcessor::acceptsMidi() const { return false; }
bool LUMINAProcessor::producesMidi() const { return false; }
bool LUMINAProcessor::isMidiEffect() const { return false; }
double LUMINAProcessor::getTailLengthSeconds() const { return 0.0; }
int LUMINAProcessor::getNumPrograms() { return 1; }
int LUMINAProcessor::getCurrentProgram() { return 0; }
void LUMINAProcessor::setCurrentProgram(int index) {}
const juce::String LUMINAProcessor::getProgramName(int index) { return {}; }
void LUMINAProcessor::changeProgramName(int index, const juce::String& newName) {}

//==============================================================================
void LUMINAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    stft.prepare(sampleRate);

    const int maxBins = 4096 / 2 + 1;
    hpss.prepare(maxBins, 17, 31);
    maskingModel.prepare(sampleRate, 4096);
    onsetDetector.prepare(sampleRate, maxBins);

    tonalMaskWorkspace.assign(static_cast<size_t>(maxBins), 1.0f);

    gainSmooth.reset(sampleRate, 0.023);
    gainSmooth.setCurrentAndTargetValue(1.0f);

    setLatencySamples(stft.getLatencySamples());
}

void LUMINAProcessor::releaseResources()
{
}

bool LUMINAProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

//==============================================================================
void LUMINAProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // RMS ゲート
    float rms = buffer.getRMSLevel(0, 0, numSamples);
    if (rms < 1e-6f)
    {
        gainSmooth.setTargetValue(1.0f);
        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) data[i] *= gainSmooth.getNextValue();
        }
        return;
    }

    const bool isMsMode = *apvts.getRawParameterValue("MS_MODE") > 0.5f;
    const float threshold = *apvts.getRawParameterValue("THRESHOLD");
    const float depth = *apvts.getRawParameterValue("DEPTH");

    if (isMsMode) msEncoder.encodeMidSide(buffer);

    const float* analysisPointer = buffer.getReadPointer(0);
    bool newFrameReady = stft.process(analysisPointer, numSamples);

    if (newFrameReady)
    {
        const int numBins = stft.bands[0].fftSize / 2 + 1;
        const float* currentPower = stft.bands[0].power.data();

        hpss.process(currentPower, tonalMaskWorkspace.data(), numBins);
        bool isOnset = onsetDetector.detectOnset(currentPower, numBins);
        auto barkPower = maskingModel.calcBarkPower(currentPower, numBins);
        auto maskThresh = maskingModel.calcMaskingThreshold(barkPower);

        float targetGain = isOnset ? 1.0f : maskingModel.computeGainReduction(barkPower, maskThresh, threshold, depth);
        gainSmooth.setTargetValue(targetGain);

        AnalysisFrame frame;
        frame.isOnset = isOnset;
        frame.barkPower = barkPower;

        for (int i = 0; i < 24; ++i)
        {
            float excess = barkPower[i] - maskThresh[i] + threshold;
            if (excess > 0.0f && !isOnset)
                frame.barkGainReduction[i] = juce::Decibels::decibelsToGain(-(excess * depth));
            else
                frame.barkGainReduction[i] = 1.0f;
        }

        const int binsPerUI = numBins / 512;
        for (int i = 0; i < 512; ++i) {
            float sumP = 0.0f;
            int startIdx = i * binsPerUI;
            for (int j = 0; j < binsPerUI; ++j) {
                int idx = startIdx + j;
                if (idx < numBins) sumP += currentPower[idx];
            }
            frame.magnitudeSpectrum[static_cast<size_t>(i)] = std::sqrt(sumP / static_cast<float>(binsPerUI) + 1e-12f);
        }

        analysisFifo.push(frame);
    }

    for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
        float* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            channelData[i] *= gainSmooth.getNextValue();
        }
    }

    if (isMsMode) msEncoder.decodeMidSide(buffer);
}

//==============================================================================
bool LUMINAProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* LUMINAProcessor::createEditor() { return new LUMINAEditor(*this); }

void LUMINAProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LUMINAProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LUMINAProcessor(); }