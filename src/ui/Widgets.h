#pragma once
// Parameter-bound controls and the section panel that lays them out.

#include <juce_audio_processors/juce_audio_processors.h>
#include "params/Params.h"
#include "ui/Theme.h"

namespace synthia
{
class SynthiaProcessor;

juce::String tooltipFor (int index);

/** Rotary knob with name label (shows the value while hovered/dragged) and modulation ring. */
class ParamKnob : public juce::Component
{
public:
    ParamKnob (SynthiaProcessor& p, int index, juce::Colour accent, const juce::String& label = {});
    ~ParamKnob() override;
    void resized() override;
    /** Effective normalised value from the engine (-1 = not modulated). Repaints only on change. */
    void setModNorm (float n);
    int getIndex() const noexcept { return index; }

    struct KnobSlider : juce::Slider
    {
        std::function<void (bool)> onHover;
        void mouseEnter (const juce::MouseEvent& e) override { Slider::mouseEnter (e); if (onHover) onHover (true); }
        void mouseExit (const juce::MouseEvent& e) override { Slider::mouseExit (e); if (onHover) onHover (false); }
    };
    KnobSlider slider;

private:
    void updateLabel();
    juce::Label label;
    juce::String name;
    int index;
    bool hovering = false;
    float lastMod = -2.0f;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

/** Toggle button bound to a bool parameter. */
class ParamToggle : public juce::Component
{
public:
    ParamToggle (SynthiaProcessor& p, int index, juce::Colour accent, const juce::String& text = {});
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::TextButton button;

private:
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

/** Combo box bound to a choice parameter, with an optional caption above. */
class ParamCombo : public juce::Component
{
public:
    ParamCombo (SynthiaProcessor& p, int index, const juce::String& caption = {});
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
};

/** Matrix destination picker: a button that opens a menu grouped by section. */
class DestButton : public juce::Component
{
public:
    DestButton (SynthiaProcessor& p, int index);
    void resized() override { button.setBounds (getLocalBounds()); }

private:
    void showMenu();
    void update (float v);
    juce::TextButton button;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

/** Horizontal slider bound to a parameter (used in the matrix). */
class ParamHSlider : public juce::Component
{
public:
    ParamHSlider (SynthiaProcessor& p, int index, juce::Colour accent);
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider slider;

private:
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
};

/** Peak meter with decay (reads a value pushed by the editor timer). */
class Meter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void push (float peak);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, hold = 0.0f;
    int holdCounter = 0;
};

/** Titled panel with a grid of parameter controls (cell grid, optional group captions). */
class SectionPanel : public juce::Component
{
public:
    static constexpr int kCellW = 64, kCellH = 70, kCaptionH = 14, kTitleH = 24;

    SectionPanel (SynthiaProcessor& p, const juce::String& title, juce::Colour accent, Section randomSection = Section::count, bool dice = false);

    /** Adds the default control for parameter `index` (knob / toggle / combo). */
    juce::Component* add (int index, const juce::String& label = {}, int span = -1);
    /** Adds an arbitrary component spanning `span` cells. */
    void addCustom (juce::Component* c, int span, int rows = 1);
    void caption (const juce::String& text) { pendingCaption = text; }
    void newRow() { pendingNewRow = true; }

    int rowsUsed() const;
    int preferredHeight() const { return kTitleH + rowsUsed() * (kCellH + kCaptionH) + 6; }

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (Section)> onRandomize;
    std::vector<ParamKnob*> knobs;

    juce::Colour accent;

private:
    struct Item
    {
        juce::Component* comp;
        int span, rows;
        bool newRow;
        juce::String caption;
        juce::Rectangle<int> cellArea;
    };
    SynthiaProcessor& proc;
    juce::String title;
    Section section;
    juce::OwnedArray<juce::Component> owned;
    std::vector<Item> items;
    juce::String pendingCaption;
    bool pendingNewRow = false;
    juce::TextButton diceButton { "Rnd" };
};
} // namespace synthia
