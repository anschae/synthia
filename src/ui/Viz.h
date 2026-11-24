#pragma once
// Live displays. Each reads dsp::VizData (written by the audio thread) and/or
// parameter values, and repaints when the editor timer calls tick() -- only
// for displays that are currently showing.

#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/VizData.h"
#include "params/Params.h"
#include "ui/Theme.h"

namespace synthia
{
class SynthiaProcessor;

/** Dark display box with a caption and an optional legend. */
class VizBox : public juce::Component
{
public:
    VizBox (SynthiaProcessor& p, const juce::String& caption, juce::Colour accent);
    void paint (juce::Graphics&) final;
    virtual void tick() { if (isShowing()) repaint(); }

    struct LegendItem { juce::Colour colour; juce::String text; int kind; }; // kind 0 box, 1 line, 2 dashed
    void setLegend (std::vector<LegendItem> items) { legend = std::move (items); }

protected:
    /** Draw inside `area` (below the caption line). */
    virtual void paintContent (juce::Graphics& g, juce::Rectangle<float> area) = 0;
    float param (int index) const;
    /** Effective (modulated) value if the parameter is modulated, else the parameter value. */
    float effective (int index) const;
    bool layerOn (int onParam) const;
    void drawNote (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text) const;

    SynthiaProcessor& proc;
    dsp::VizData& viz;
    juce::String caption;
    juce::Colour accent;
    std::vector<LegendItem> legend;
};

class PickTimeline : public VizBox
{
public:
    explicit PickTimeline (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class VoicerBands : public VizBox
{
public:
    explicit VoicerBands (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

/** The Voicer's waveform (morph + PW) and the mix of all its voices, 2 cycles. */
class WaveView : public VizBox
{
public:
    explicit WaveView (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class GranularView : public VizBox
{
public:
    explicit GranularView (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class FilterCurve : public VizBox
{
public:
    explicit FilterCurve (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    float responseDb (int type, float hz, float cutoff, float res) const;
};

class AdsrView : public VizBox
{
public:
    explicit AdsrView (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class SpectrumView : public VizBox
{
public:
    explicit SpectrumView (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class ResoVoices : public VizBox
{
public:
    explicit ResoVoices (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class VowelMap : public VizBox
{
public:
    explicit VowelMap (SynthiaProcessor& p);
    void tick() override;

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

/** Mini keyboard with the notes of one chord slot. */
class ChordKeyboard : public juce::Component
{
public:
    ChordKeyboard (SynthiaProcessor& p, int slot);
    void paint (juce::Graphics&) override;
    void tick() { if (isShowing()) repaint(); }

private:
    SynthiaProcessor& proc;
    int slot;
};

class LfoPreview : public VizBox
{
public:
    LfoPreview (SynthiaProcessor& p, int lfoIndex);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;

private:
    int lfo;
};

class DelayView : public VizBox
{
public:
    explicit DelayView (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

class ReverbView : public VizBox
{
public:
    explicit ReverbView (SynthiaProcessor& p);

protected:
    void paintContent (juce::Graphics&, juce::Rectangle<float>) override;
};

// helpers shared by the displays
namespace vizutil
{
inline float freqToX (float hz, float lo, float hi, juce::Rectangle<float> r)
{
    return r.getX() + r.getWidth() * std::log (hz / lo) / std::log (hi / lo);
}
void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r, float lo, float hi, float dbLo, float dbHi, bool labels);
} // namespace vizutil
} // namespace synthia
