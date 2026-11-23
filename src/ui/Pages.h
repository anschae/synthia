#pragma once
// Editor pages: Layers (input, A-E, mixer), Filter & FX, Modulation; plus the top bar.

#include "ui/Viz.h"
#include "ui/Widgets.h"

namespace synthia
{
class SynthiaProcessor;

/** 32-step bipolar bar editor + glide row; draws the playing step. */
class StepSeqEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepSeqEditor (SynthiaProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setPlayingStep (int s) { if (s != playing) { playing = s; repaint(); } }
    void refresh() { repaint(); }

private:
    juce::Rectangle<float> barsArea() const;
    juce::Rectangle<float> glideArea() const;
    int stepAt (float x) const;
    void setStepFromMouse (const juce::MouseEvent& e);
    SynthiaProcessor& proc;
    juce::RangedAudioParameter* values[32] {};
    juce::RangedAudioParameter* glides[32] {};
    juce::RangedAudioParameter* steps = nullptr;
    int playing = -1, lastDragStep = -1;
    bool draggingBars = false;
    std::vector<int> gestureSteps;
};

/** Mixer strip for one layer. */
class LayerStrip : public juce::Component
{
public:
    LayerStrip (SynthiaProcessor& p, int baseParam, const juce::String& name, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::vector<ParamKnob*> knobs() { return { &level, &pan }; }
    std::function<void()> onSelect;

private:
    juce::String name;
    juce::Colour accent;
    ParamToggle on, mute, solo, toFilter;
    ParamKnob level, pan;
};

class LayersPage : public juce::Component
{
public:
    explicit LayersPage (SynthiaProcessor& p);
    void resized() override;
    void paint (juce::Graphics&) override;
    void selectLayer (int i);
    void updateModeDimming();
    std::vector<ParamKnob*> allKnobs();
    std::vector<SectionPanel*> panels();
    void tick();

private:
    SynthiaProcessor& proc;
    SectionPanel inputPanel;
    std::unique_ptr<SectionPanel> layerPanels[5];
    juce::TextButton layerTabs[5];
    juce::OwnedArray<LayerStrip> strips;
    SectionPanel master;
    int selected = 0;
    PickTimeline pickTimeline;
    VoicerBands voicerBands;
    SpectrumView spectrum;
    GranularView granView;
    std::unique_ptr<juce::Component> resoFooter;
    std::vector<ChordKeyboard*> keyboards;
    ResoVoices* resoVoices = nullptr;
    VowelMap* vowelMap = nullptr;
    // groups inside the C panel that are dimmed depending on the mode
    std::vector<juce::Component*> resoGroup, vocGroup, formGroup;
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (SynthiaProcessor& p);
    void resized() override;
    std::vector<ParamKnob*> allKnobs();
    std::vector<SectionPanel*> panels() { return { &filter, &amp, &chopper, &delay, &reverb }; }
    void tick();

private:
    SectionPanel filter, amp, chopper, delay, reverb;
    FilterCurve filterCurve;
    AdsrView adsrView;
    DelayView delayView;
    ReverbView reverbView;
    juce::Component* chopSteps = nullptr;
};

class MatrixPanel : public juce::Component
{
public:
    explicit MatrixPanel (SynthiaProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() { if (isShowing()) for (auto* r : rows) r->repaint(); }

private:
    juce::OwnedArray<juce::Component> rows;
};

class ModPage : public juce::Component
{
public:
    explicit ModPage (SynthiaProcessor& p);
    void resized() override;
    std::vector<ParamKnob*> allKnobs();
    std::vector<SectionPanel*> panels() { return { &lfo1, &lfo2, &random, &macros, &seqPanel }; }
    void setPlayingStep (int s) { seqEditor.setPlayingStep (s); }
    void setSourceValues (const float* v);
    void tick() { lfoView1.tick(); lfoView2.tick(); matrix.tick(); }

private:
    SectionPanel lfo1, lfo2, random, macros, seqPanel;
    LfoPreview lfoView1, lfoView2;
    StepSeqEditor seqEditor;
    MatrixPanel matrix;
};
} // namespace synthia
