#pragma once
#include "ui/Widgets.h"

namespace synthia
{
class SynthiaProcessor;

/** Preset browser, A/B, randomize + chaos, meters, quality, mix, align, bypass, UI scale. */
class TopBar : public juce::Component
{
public:
    explicit TopBar (SynthiaProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshPresetName();
    void pushMeters (float in, float out) { inMeter.push (in); outMeter.push (out); }

    std::function<void()> onRandomizeAll;
    std::function<void (float)> onScale;
    ParamKnob mixKnob;

private:
    void showPresetMenu();
    void askName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done);

    SynthiaProcessor& proc;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn, saveBtn { "Save" };
    juce::TextButton aBtn { "A" }, bBtn { "B" }, copyBtn;
    juce::TextButton rndBtn { "Randomize" };
    juce::Slider chaos;
    Meter inMeter, outMeter;
    ParamCombo quality;
    ParamToggle align, bypass;
    juce::ComboBox scaleBox;
    std::unique_ptr<juce::AlertWindow> dialog;
};
} // namespace synthia
