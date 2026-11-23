#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Pages.h"
#include "ui/TopBar.h"

namespace synthia
{
/** Everything at the fixed design size (theme::kDesignW x kDesignH); the editor scales it. */
class MainView : public juce::Component
{
public:
    explicit MainView (SynthiaProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int i);
    int currentPage() const noexcept { return page; }

    TopBar topBar;
    LayersPage layers;
    FxPage fx;
    ModPage mod;
    juce::TextButton tabs[3];
    std::vector<ParamKnob*> knobsOnPage[3];

private:
    int page = 0;
};

class SynthiaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SynthiaEditor (SynthiaProcessor& p);
    ~SynthiaEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override;

    MainView& getView() { return view; }
    void setScale (float s);

private:
    void timerCallback() override;
    void randomize (Section s);

    SynthiaProcessor& proc;
    SynthiaLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 700 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::Random rng;
};
} // namespace synthia
