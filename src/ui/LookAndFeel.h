#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"

namespace synthia
{
/** Dark flat look. Knobs show the modulated value as a yellow ring when the
    slider's "modNorm" property is >= 0 (set by the editor from the engine). */
class SynthiaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthiaLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (12.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::FontOptions (juce::jmin (12.5f, (float) h * 0.6f)); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
};
} // namespace synthia
