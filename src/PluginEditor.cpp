#include "PluginEditor.h"
#include "model/Randomizer.h"

namespace synthia
{
MainView::MainView (SynthiaProcessor& p) : topBar (p), layers (p), fx (p), mod (p)
{
    addAndMakeVisible (topBar);
    addChildComponent (layers);
    addChildComponent (fx);
    addChildComponent (mod);
    const char* names[] = { "Layers", "Filter & FX", "Modulation" };
    for (int i = 0; i < 3; ++i)
    {
        tabs[i].setButtonText (names[i]);
        tabs[i].setColour (juce::TextButton::buttonOnColourId, theme::text);
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    knobsOnPage[0] = layers.allKnobs();
    knobsOnPage[1] = fx.allKnobs();
    knobsOnPage[2] = mod.allKnobs();
    knobsOnPage[0].push_back (&topBar.mixKnob);
    showPage (0);
}

void MainView::showPage (int i)
{
    page = i;
    layers.setVisible (i == 0);
    fx.setVisible (i == 1);
    mod.setVisible (i == 2);
    for (int k = 0; k < 3; ++k)
        tabs[k].setToggleState (k == i, juce::dontSendNotification);
}

void MainView::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void MainView::resized()
{
    topBar.setBounds (0, 0, getWidth(), 64);
    for (int i = 0; i < 3; ++i)
        tabs[i].setBounds (8 + i * 130, 68, 124, 24);
    const auto content = juce::Rectangle<int> (0, 96, getWidth(), getHeight() - 96);
    layers.setBounds (content);
    fx.setBounds (content);
    mod.setBounds (content);
}

// ---------------------------------------------------------------------------
SynthiaEditor::SynthiaEditor (SynthiaProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    view.setBounds (0, 0, theme::kDesignW, theme::kDesignH);

    for (auto* sp : view.layers.panels()) sp->onRandomize = [this] (Section s) { randomize (s); };
    for (auto* sp : view.fx.panels()) sp->onRandomize = [this] (Section s) { randomize (s); };
    view.topBar.onRandomizeAll = [this] { randomize (Section::count); };
    view.topBar.onScale = [this] (float s) { setScale (s); };
    proc.presets.onChange = [this] { view.topBar.refreshPresetName(); view.mod.repaint(); };

    constrainer.setFixedAspectRatio ((double) theme::kDesignW / theme::kDesignH);
    constrainer.setSizeLimits ((int) (theme::kDesignW * 0.75), (int) (theme::kDesignH * 0.75), theme::kDesignW * 2, theme::kDesignH * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const float s = juce::jlimit (0.75f, 2.0f, proc.uiScale);
    setSize (juce::roundToInt (theme::kDesignW * s), juce::roundToInt (theme::kDesignH * s));
    lastPickCount = proc.engine.pickCount.load();
    startTimerHz (30);
}

SynthiaEditor::~SynthiaEditor()
{
    proc.presets.onChange = nullptr;
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SynthiaEditor::setScale (float s)
{
    s = juce::jlimit (0.75f, 2.0f, s);
    setSize (juce::roundToInt (theme::kDesignW * s), juce::roundToInt (theme::kDesignH * s));
}

void SynthiaEditor::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void SynthiaEditor::resized()
{
    const float s = (float) getWidth() / (float) theme::kDesignW;
    proc.uiScale = s;
    view.setTransform (juce::AffineTransform::scale (s));
}

void SynthiaEditor::randomize (Section s)
{
    auto get = [this] (int i) { return proc.apvts.getParameter (paramId (i))->getValue(); };
    auto set = [this] (int i, float v)
    {
        auto* prm = proc.apvts.getParameter (paramId (i));
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (v);
        prm->endChangeGesture();
    };
    Randomizer::randomize (s, proc.chaos, rng, get, set);
    view.mod.repaint();
}

void SynthiaEditor::timerCallback()
{
    auto& e = proc.engine;
    view.topBar.pushMeters (e.inPeak.exchange (0.0f), e.outPeak.exchange (0.0f));
    for (auto* k : view.knobsOnPage[view.currentPage()])
        k->setModNorm (e.params.uiModulatedNorm (k->getIndex()));

    const int pc = e.pickCount.load();
    if (pc != lastPickCount) { lastPickCount = pc; pickFlash = 1.0f; }
    else pickFlash *= 0.8f;
    if (view.currentPage() == 0)
    {
        view.layers.setPickFlash (pickFlash);
        view.layers.updateModeDimming();
    }
    if (view.currentPage() == 2)
        view.mod.setPlayingStep (e.mod.uiStep.load());
}
} // namespace synthia
