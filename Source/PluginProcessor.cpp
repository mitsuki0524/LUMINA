#include "PluginProcessor.h"
#include "PluginEditor.h"

LUMINAProcessor::LUMINAProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

LUMINAProcessor::~LUMINAProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout LUMINAProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "THRESHOLD", 1 }, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "DEPTH", 1 }, "Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes()));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ATTACK", 1 }, "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.5f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "RELEASE", 1 }, "Release",
        juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f, 0.5f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "LINEAR_PHASE", 1 }, "Linear Phase", false,
        juce::AudioParameterBoolAttributes()));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "MS_MODE", 1 }, "M/S Mode", false,
        juce::AudioParameterBoolAttributes()));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "LISTEN_MODE", 1 }, "Listen Mode",
        juce::StringArray{ "Normal", "Delta" }, 0,
        juce::AudioParameterChoiceAttributes()));

    return { params.begin(), params.end() };
}

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

void LUMINAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int fftSize = 4096;
    const int numBins = fftSize / 2 + 1;
    const float overlapRatio = 0.75f;

    binToBarkMap.assign(static_cast<size_t>(numBins), 0);
    const float binFreq = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    for (int i = 0; i < numBins; ++i)
    {
        float hz = static_cast<float>(i) * binFreq;
        float bark = 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
        int barkIdx = static_cast<int>(std::floor(bark));
        binToBarkMap[i] = (barkIdx < 0) ? 0 : (barkIdx >= 24 ? 23 : barkIdx);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        spectralEngines[ch].prepare(sampleRate, fftSize, overlapRatio);
        hpssEngines[ch].prepare(numBins, 17, 31);
        maskingModels[ch].prepare(sampleRate, fftSize);
        onsetDetectors[ch].prepare(sampleRate, numBins);

        powerWorkspaces[ch].assign(static_cast<size_t>(numBins), 0.0f);
        tonalMaskWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);
        binGainsWorkspaces[ch].assign(static_cast<size_t>(numBins), 1.0f);

        // キャプチャに fftSize を追加
        spectralEngines[ch].setProcessingCallback([this, ch, fftSize](float* magnitudes, int numBins)
            {
                auto& hpss = hpssEngines[ch];
                auto& onset = onsetDetectors[ch];
                auto& masking = maskingModels[ch];
                float* power = powerWorkspaces[ch].data();
                float* tonalMask = tonalMaskWorkspaces[ch].data();
                float* binGains = binGainsWorkspaces[ch].data();

                // ⚡ 修正: 振幅を正規化してからパワーを計算（マスキングのdB計算を正常化するため）
                float normFactor = 1.0f / static_cast<float>(fftSize);
                for (int i = 0; i < numBins; ++i) {
                    float normMag = magnitudes[i] * normFactor;
                    power[i] = normMag * normMag;
                }

                hpss.process(power, tonalMask, numBins);
                bool isOnset = onset.detectOnset(power, numBins);

                auto barkPower = masking.calcBarkPower(power, numBins);
                auto maskThresh = masking.calcMaskingThreshold(barkPower);
                auto tonalRatio = masking.calcBarkTonalRatio(tonalMask, numBins);

                const bool isMsMode = *apvts.getRawParameterValue("MS_MODE") > 0.5f;
                const float threshold = *apvts.getRawParameterValue("THRESHOLD");
                float depth = *apvts.getRawParameterValue("DEPTH");
                const int listenMode = static_cast<int>(*apvts.getRawParameterValue("LISTEN_MODE"));

                if (isMsMode && ch == 1) {
                    depth = 0.0f;
                }

                std::array<float, 24> barkGain;
                barkGain.fill(1.0f);

                for (int i = 0; i < 24; ++i)
                {
                    // ⚡ 修正: マイナス値のThresholdを「引く」ことで、マスキング曲線を下にシフトさせてピークを露出させる
                    float excess = barkPower[i] - maskThresh[i] - threshold;

                    if (excess > 0.0f && !isOnset)
                    {
                        float bandReductionDB = excess * depth * tonalRatio[i];
                        barkGain[i] = juce::Decibels::decibelsToGain(-bandReductionDB);
                    }
                }

                for (int i = 0; i < numBins; ++i)
                {
                    float originalMag = magnitudes[i];
                    float gain = barkGain[static_cast<size_t>(binToBarkMap[i])];

                    if (listenMode == 1) // Delta Mode
                    {
                        // 削り取られた成分（抑制量）のみを出力する
                        magnitudes[i] = originalMag * (1.0f - gain);
                    }
                    else // Normal Mode
                    {
                        magnitudes[i] = originalMag * gain;
                    }
                    binGains[i] = gain;
                }

                if (ch == 0)
                {
                    AnalysisFrame frame;
                    frame.isOnset = isOnset;
                    frame.barkPower = barkPower;
                    frame.barkGainReduction = barkGain;

                    const int binsPerUI = numBins / 512;
                    for (int i = 0; i < 512; ++i) {
                        float sumP = 0.0f;
                        int startIdx = i * binsPerUI;
                        for (int j = 0; j < binsPerUI; ++j) {
                            int idx = startIdx + j;
                            if (idx < numBins) {
                                // UIには正規化前の表示用パワーを送る
                                float rawMag = (listenMode == 1) ? (magnitudes[idx] / (1.0f - binGains[idx] + 1e-6f)) : (magnitudes[idx] / (binGains[idx] + 1e-6f));
                                sumP += rawMag * rawMag;
                            }
                        }
                        frame.magnitudeSpectrum[static_cast<size_t>(i)] = std::sqrt(sumP / static_cast<float>(binsPerUI) + 1e-12f);
                    }
                    analysisFifo.push(frame);
                }
            });
    }

    setLatencySamples(spectralEngines[0].getLatencySamples());
}

void LUMINAProcessor::releaseResources() {}

bool LUMINAProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void LUMINAProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    const bool isMsMode = *apvts.getRawParameterValue("MS_MODE") > 0.5f;

    if (isMsMode) msEncoder.encodeMidSide(buffer);

    if (buffer.getNumChannels() > 0) {
        float* chData0 = buffer.getWritePointer(0);
        spectralEngines[0].process(chData0, chData0, numSamples);
    }

    if (buffer.getNumChannels() > 1) {
        float* chData1 = buffer.getWritePointer(1);
        spectralEngines[1].process(chData1, chData1, numSamples);
    }

    if (isMsMode) msEncoder.decodeMidSide(buffer);
}

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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LUMINAProcessor(); }