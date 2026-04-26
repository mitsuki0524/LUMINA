#pragma once

// JuceHeader.h を直接モジュールと標準ライブラリに置き換え
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

class AbletonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AbletonLookAndFeel()
    {
        // 全体的に少し明るめのダークテーマへ調整
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour::fromString("FF282828"));
        setColour(juce::Slider::thumbColourId, juce::Colour::fromString("FFFFA07A")); // Light Salmon/Orange
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromString("FFFFA07A"));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromString("FF555555"));
        setColour(juce::Label::textColourId, juce::Colour::fromString("FFEEEEEE"));
        setColour(juce::GroupComponent::outlineColourId, juce::Colour::fromString("FF555555"));
        setColour(juce::GroupComponent::textColourId, juce::Colour::fromString("FFCCCCCC"));
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromString("FF333333"));
        setColour(juce::TextButton::textColourOffId, juce::Colour::fromString("FFDDDDDD"));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto trackWidth = 5.0f; // 少し太くして視認性アップ

        // Background Track
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, radius - trackWidth, radius - trackWidth,
            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(findColour(juce::Slider::rotarySliderOutlineColourId));
        g.strokePath(backgroundArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (slider.isEnabled())
        {
            auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, radius - trackWidth, radius - trackWidth,
                0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(valueArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
        bool isOn = button.getToggleState();

        // トグルボタンのモダンフラット描画
        g.setColour(isOn ? findColour(juce::Slider::thumbColourId) : juce::Colour::fromString("FF444444"));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(isOn ? juce::Colours::black : juce::Colour::fromString("FFCCCCCC"));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, true);

        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
};