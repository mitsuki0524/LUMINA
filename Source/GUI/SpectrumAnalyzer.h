#ifndef LUMINA_SPECTRUMANALYZER_H
#define LUMINA_SPECTRUMANALYZER_H

// JuceHeader.h を直接モジュールに置き換え
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "AnalysisFifo.h"

class SpectrumAnalyzer : public juce::Component
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override;

    void updateFrame(const AnalysisFrame& frame);
    void setCrossovers(float c1, float c2);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AnalysisFrame currentFrame;
    float cross1 = 250.0f;
    float cross2 = 4000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

#endif // LUMINA_SPECTRUMANALYZER_H