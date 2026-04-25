#ifndef LUMINA_GRMeter_H
#define LUMINA_GRMeter_H

#include <JuceHeader.h>
#include "AnalysisFifo.h"

/**
 * @class GRMeter
 * @brief 24帯域のBarkスケールに基づいたゲインリダクションを表示するメーター。
 * スペクトラムアナライザの上に重ねるか、独立して配置することで、
 * どの帯域が抑制されているかを視覚化します。
 */
class GRMeter : public juce::Component
{
public:
    GRMeter();
    ~GRMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /**
     * @brief 最新の解析フレームを渡し、描画データを更新
     */
    void updateFrame(const AnalysisFrame& frame);

private:
    AnalysisFrame currentFrame;

    // 表示を滑らかにするためのフォールオフ（減衰）係数
    std::array<float, 24> visualGains;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMeter)
};

#endif // LUMINA_GRMeter_H