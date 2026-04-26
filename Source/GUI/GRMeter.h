#ifndef LUMINA_GRMETER_H
#define LUMINA_GRMETER_H

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "AnalysisFifo.h"

class GRMeter : public juce::Component
{
public:
    GRMeter();
    ~GRMeter() override;

    void updateFrame(const AnalysisFrame& frame);
    void paint(juce::Graphics& g) override;

private:
    std::array<float, 24> displayedGR; // メーターの滑らかなアニメーション用

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMeter)
};

#endif