#include "ui/Pages.h"
#include "PluginProcessor.h"

namespace synthia
{
// ============================================================================ StepSeqEditor
StepSeqEditor::StepSeqEditor (SynthiaProcessor& p) : proc (p)
{
    for (int i = 0; i < 32; ++i)
    {
        values[i] = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (paramId (pid::seqV1 + i)));
        glides[i] = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (paramId (pid::seqG1 + i)));
    }
    steps = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (paramId (pid::seqSteps)));
    setTooltip ("Step sequencer: drag to draw step values (bipolar). Click the small boxes below a step to glide into it. "
                "Steps beyond 'Seq Steps' are greyed out. Route 'Step Seq' in the matrix below.");
}

juce::Rectangle<float> StepSeqEditor::barsArea() const { return getLocalBounds().toFloat().reduced (8, 6).withTrimmedBottom (20); }
juce::Rectangle<float> StepSeqEditor::glideArea() const
{
    auto b = barsArea();
    return { b.getX(), b.getBottom() + 4, b.getWidth(), 14 };
}

int StepSeqEditor::stepAt (float x) const
{
    auto b = barsArea();
    return juce::jlimit (0, 31, (int) ((x - b.getX()) / b.getWidth() * 32.0f));
}

void StepSeqEditor::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (theme::panelEdge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    auto b = barsArea();
    const int n = juce::roundToInt (steps->convertFrom0to1 (steps->getValue()));
    const float w = b.getWidth() / 32.0f;
    const float mid = b.getCentreY();
    g.setColour (theme::panelEdge);
    g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
    for (int i = 0; i < 32; ++i)
    {
        const float v = values[i]->convertFrom0to1 (values[i]->getValue());
        auto cell = juce::Rectangle<float> (b.getX() + i * w, b.getY(), w, b.getHeight()).reduced (1.5f, 0);
        const bool active = i < n;
        g.setColour (theme::knobBg.withAlpha (active ? 1.0f : 0.4f));
        g.fillRect (cell);
        const float y = mid - v * b.getHeight() * 0.5f;
        auto bar = juce::Rectangle<float>::leftTopRightBottom (cell.getX(), juce::jmin (y, mid), cell.getRight(), juce::jmax (y, mid));
        auto col = theme::modc.withAlpha (active ? 0.9f : 0.25f);
        if (i == playing && active) col = juce::Colours::white;
        g.setColour (col);
        g.fillRect (bar.withHeight (juce::jmax (2.0f, bar.getHeight())));
        // glide box
        auto ga = glideArea();
        auto gb = juce::Rectangle<float> (ga.getX() + i * w, ga.getY(), w, ga.getHeight()).reduced (3.0f, 1.0f);
        const bool gl = glides[i]->getValue() > 0.5f;
        g.setColour (gl ? theme::modc.withAlpha (active ? 1.0f : 0.3f) : theme::knobBg.brighter (0.1f));
        g.fillRoundedRectangle (gb, 2.0f);
        if ((i % 4) == 0)
        {
            g.setColour (theme::textDim);
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (juce::String (i + 1), (int) cell.getX() + 2, (int) b.getY() + 1, 20, 10, juce::Justification::left);
        }
    }
}

void StepSeqEditor::setStepFromMouse (const juce::MouseEvent& e)
{
    auto b = barsArea();
    const int i = stepAt ((float) e.x);
    const float v = juce::jlimit (-1.0f, 1.0f, (b.getCentreY() - (float) e.y) / (b.getHeight() * 0.5f));
    if (std::find (gestureSteps.begin(), gestureSteps.end(), i) == gestureSteps.end())
    {
        values[i]->beginChangeGesture();
        gestureSteps.push_back (i);
    }
    // fill skipped steps when dragging fast
    const int from = lastDragStep < 0 ? i : lastDragStep;
    for (int k = juce::jmin (from, i); k <= juce::jmax (from, i); ++k)
    {
        if (std::find (gestureSteps.begin(), gestureSteps.end(), k) == gestureSteps.end())
        {
            values[k]->beginChangeGesture();
            gestureSteps.push_back (k);
        }
        values[k]->setValueNotifyingHost (values[k]->convertTo0to1 (v));
    }
    lastDragStep = i;
    repaint();
}

void StepSeqEditor::mouseDown (const juce::MouseEvent& e)
{
    if (glideArea().expanded (0, 2).contains (e.position))
    {
        const int i = stepAt ((float) e.x);
        auto* gp = glides[i];
        gp->beginChangeGesture();
        gp->setValueNotifyingHost (gp->getValue() > 0.5f ? 0.0f : 1.0f);
        gp->endChangeGesture();
        repaint();
        return;
    }
    draggingBars = true;
    lastDragStep = -1;
    if (e.mods.isAltDown() || e.getNumberOfClicks() > 1)
    {
        const int i = stepAt ((float) e.x);
        values[i]->beginChangeGesture();
        values[i]->setValueNotifyingHost (values[i]->getDefaultValue());
        values[i]->endChangeGesture();
        draggingBars = false;
        repaint();
        return;
    }
    setStepFromMouse (e);
}

void StepSeqEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBars)
        setStepFromMouse (e);
}

void StepSeqEditor::mouseUp (const juce::MouseEvent&)
{
    for (int i : gestureSteps)
        values[i]->endChangeGesture();
    gestureSteps.clear();
    draggingBars = false;
    lastDragStep = -1;
}

// ============================================================================ LayerStrip
LayerStrip::LayerStrip (SynthiaProcessor& p, int b, const juce::String& n, juce::Colour a)
    : name (n), accent (a),
      on (p, b, a, "On"), mute (p, b + 3, juce::Colours::orangered, "M"), solo (p, b + 4, juce::Colours::gold, "S"),
      toFilter (p, b + 5, theme::filter, juce::CharPointer_UTF8 ("\xe2\x86\x92 Filter")),
      level (p, b + 1, a, "Level"), pan (p, b + 2, a, "Pan")
{
    for (juce::Component* c : { (juce::Component*) &on, (juce::Component*) &mute, (juce::Component*) &solo,
                                (juce::Component*) &toFilter, (juce::Component*) &level, (juce::Component*) &pan })
        addAndMakeVisible (c);
}

void LayerStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX() + 8, r.getY() + 7, 3.0f, 11.0f, 1.5f);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (name, 16, 2, getWidth() - 20, 20, juce::Justification::centredLeft);
}

void LayerStrip::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop (20);
    auto row = r.removeFromTop (24);
    on.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (4);
    mute.setBounds (row.removeFromLeft (26));
    row.removeFromLeft (4);
    solo.setBounds (row.removeFromLeft (26));
    row.removeFromLeft (4);
    toFilter.setBounds (row);
    r.removeFromTop (6);
    auto k = r.removeFromTop (72);
    level.setBounds (k.removeFromLeft (k.getWidth() / 2).reduced (4, 0));
    pan.setBounds (k.reduced (4, 0));
}

// ============================================================================ LayersPage
namespace
{
void fillVoicer (SectionPanel& s)
{
    s.caption ("Analysis");
    s.add (pid::vcBands); s.add (pid::vcLow); s.add (pid::vcHigh); s.add (pid::vcSharp, "Sharp");
    s.add (pid::vcFocus); s.add (pid::vcTilt); s.add (pid::vcSmooth);
    s.caption ("Stereo"); s.add (pid::vcSpread, "Spread");
    s.newRow();
    s.caption ("Voices");
    s.add (pid::vcWave); s.add (pid::vcPw, "PW"); s.add (pid::vcMain); s.add (pid::vcSub); s.add (pid::vcOct, "Oct Up");
    s.add (pid::vcInt, "Interval"); s.add (pid::vcIntRatio, "Ratio"); s.add (pid::vcDetune);
    s.newRow();
    s.caption ("Colour");
    s.add (pid::vcDrive); s.add (pid::vcFold); s.add (pid::vcRing); s.add (pid::vcBits, "Bits"); s.add (pid::vcRate, "Rate");
}

void fillSpectral (SectionPanel& s)
{
    s.caption ("STFT"); s.add (pid::spSize); s.add (pid::spMode);
    s.caption ("Freeze"); s.add (pid::spFreeze, "Freeze"); s.add (pid::spFrzBlend, "Blend"); s.add (pid::spDrift, "Drift");
    s.caption ("Shape"); s.add (pid::spBlur); s.add (pid::spPartials); s.add (pid::spShift); s.add (pid::spStretch);
}

void fillGranular (SectionPanel& s)
{
    s.caption ("Grains"); s.add (pid::grSize, "Size"); s.add (pid::grDensity); s.add (pid::grPos); s.add (pid::grSpray); s.add (pid::grWindow);
    s.caption ("Pitch"); s.add (pid::grPitch); s.add (pid::grQuant, "Quantise"); s.add (pid::grPJit, "Jitter");
    s.caption ("Space"); s.add (pid::grPanJit, "Pan Jit"); s.add (pid::grRev); s.add (pid::grFb); s.add (pid::grFreeze, "Freeze");
    s.newRow();
    s.caption ("Stutter (host synced)"); s.add (pid::grStutter, "Stutter"); s.add (pid::grStutDiv, "Slice"); s.add (pid::grStutHold, "Retrigger");
}
} // namespace

LayersPage::LayersPage (SynthiaProcessor& p)
    : proc (p), inputPanel (p, "Input", theme::input), master (p, "Output", theme::dry)
{
    inputPanel.add (pid::inGain, "Gain");
    inputPanel.add (pid::hpf);
    inputPanel.add (pid::gateOn);
    inputPanel.add (pid::gateThr, "Gate Thr");
    inputPanel.add (pid::gateRel, "Gate Rel");
    inputPanel.add (pid::envAtk);
    inputPanel.add (pid::envRel);
    inputPanel.add (pid::pickSens, "Pick Sens");
    inputPanel.add (pid::pickHold, "Retrigger");
    inputPanel.add (pid::pickDecay, "Pick Decay");
    addAndMakeVisible (inputPanel);

    const char* names[] = { "A  Voicer", "B  Spectral", "C  Resonator / Vocoder", "D  Granular", "E  Dry" };
    const juce::Colour cols[] = { theme::voicer, theme::spectral, theme::reso, theme::granular, theme::dry };
    const Section secs[] = { Section::voicer, Section::spectral, Section::reso, Section::gran, Section::dry };
    for (int i = 0; i < 5; ++i)
    {
        layerPanels[i] = std::make_unique<SectionPanel> (p, names[i], cols[i], secs[i], true);
        layerTabs[i].setButtonText (juce::String (names[i]).upToFirstOccurrenceOf ("/", false, false).trim());
        layerTabs[i].setClickingTogglesState (false);
        layerTabs[i].setColour (juce::TextButton::buttonOnColourId, cols[i]);
        layerTabs[i].onClick = [this, i] { selectLayer (i); };
        addAndMakeVisible (layerTabs[i]);
        addChildComponent (*layerPanels[i]);
    }
    fillVoicer (*layerPanels[0]);
    fillSpectral (*layerPanels[1]);
    {
        auto& s = *layerPanels[2];
        s.caption ("Mode"); s.add (pid::rsMode); s.add (pid::rsSlot); s.add (pid::rsExcite);
        s.caption ("Resonator");
        resoGroup = { s.add (pid::rsMat), s.add (pid::rsDecay), s.add (pid::rsBright), s.add (pid::rsInharm, "Inharm") };
        s.newRow();
        s.caption ("Vocoder");
        vocGroup = { s.add (pid::vcoBands, "Bands"), s.add (pid::vcoWave, "Carrier"), s.add (pid::vcoDetune, "Detune"),
                     s.add (pid::vcoNoise, "Noise"), s.add (pid::vcoFormant, "Formant"), s.add (pid::vcoRelease, "Release") };
        s.caption ("Formant");
        formGroup = { s.add (pid::fmVowel), s.add (pid::fmReso, "Reso"), s.add (pid::fmShift) };
        for (int c = 0; c < 4; ++c)
        {
            if ((c & 1) == 0) s.newRow();
            s.caption ("Chord " + juce::String (c + 1));
            const int b = pid::c1Root + c * 4;
            s.add (b, "Root"); s.add (b + 1, "Type"); s.add (b + 2, "Octave"); s.add (b + 3, "Spread");
        }
    }
    fillGranular (*layerPanels[3]);
    layerPanels[4]->add (pid::dryTilt, "Tilt");

    for (auto* sp : panels())
        sp->onRandomize = nullptr; // set by the editor

    const int bases[] = { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn, pid::dryOn };
    const char* shortNames[] = { "A  Voicer", "B  Spectral", "C  Resonator", "D  Granular", "E  Dry" };
    for (int i = 0; i < 5; ++i)
    {
        auto* st = strips.add (new LayerStrip (p, bases[i], shortNames[i], cols[i]));
        addAndMakeVisible (st);
    }
    master.add (pid::outWidth, "Width");
    master.add (pid::outGain, "Out Gain");
    master.add (pid::mix, "Mix");
    addAndMakeVisible (master);
    selectLayer (0);
}

std::vector<SectionPanel*> LayersPage::panels()
{
    std::vector<SectionPanel*> v { &inputPanel, &master };
    for (auto& l : layerPanels) v.push_back (l.get());
    return v;
}

std::vector<ParamKnob*> LayersPage::allKnobs()
{
    std::vector<ParamKnob*> v;
    for (auto* sp : panels())
        v.insert (v.end(), sp->knobs.begin(), sp->knobs.end());
    for (auto* st : strips)
        for (auto* k : st->knobs()) v.push_back (k);
    return v;
}

void LayersPage::selectLayer (int i)
{
    selected = i;
    for (int k = 0; k < 5; ++k)
    {
        layerPanels[k]->setVisible (k == i);
        layerTabs[k].setToggleState (k == i, juce::dontSendNotification);
    }
}

void LayersPage::updateModeDimming()
{
    const int mode = juce::roundToInt (proc.getParam (pid::rsMode));
    for (auto* c : resoGroup) c->setAlpha (mode == 0 ? 1.0f : 0.35f);
    for (auto* c : vocGroup) c->setAlpha (mode == 1 ? 1.0f : 0.35f);
    for (auto* c : formGroup) c->setAlpha (mode == 2 ? 1.0f : 0.35f);
}

void LayersPage::paint (juce::Graphics& g)
{
    // pick LED under the input panel
    auto r = pickLed.toFloat();
    g.setColour (theme::knobBg);
    g.fillEllipse (r);
    g.setColour (theme::input.interpolatedWith (juce::Colours::white, 0.5f).withAlpha (juce::jlimit (0.0f, 1.0f, pickFlash)));
    g.fillEllipse (r.reduced (1.5f));
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Pick", pickLed.getRight() + 6, pickLed.getY() - 2, 60, pickLed.getHeight() + 4, juce::Justification::centredLeft);
}

void LayersPage::resized()
{
    const int stripH = 136;
    const int stripY = getHeight() - stripH - 8;
    inputPanel.setBounds (8, 8, 204, 0);
    inputPanel.setBounds (8, 8, 204, juce::jmin (stripY - 16, inputPanel.preferredHeight()));
    pickLed = { 18, inputPanel.getBottom() + 12, 12, 12 };

    const int x0 = 220, w = getWidth() - x0 - 8;
    const int tabW = w / 5;
    for (int i = 0; i < 5; ++i)
        layerTabs[i].setBounds (x0 + i * tabW, 8, tabW - 6, 26);
    for (auto& lp : layerPanels)
        lp->setBounds (x0, 40, w, stripY - 48);

    const int masterW = 212;
    const int sw = (getWidth() - 16 - masterW - 8 * 5) / 5;
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (8 + i * (sw + 8), stripY, sw, stripH);
    master.setBounds (getWidth() - 8 - masterW, stripY, masterW, stripH);
}

// ============================================================================ FxPage
namespace
{
class ChopperSteps : public juce::Component
{
public:
    explicit ChopperSteps (SynthiaProcessor& p)
    {
        for (int i = 0; i < 16; ++i)
        {
            auto* t = toggles.add (new ParamToggle (p, pid::chop1 + i, theme::filter, juce::String (i + 1)));
            addAndMakeVisible (t);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().withSizeKeepingCentre (getWidth(), 28);
        const float w = (float) r.getWidth() / 16.0f;
        for (int i = 0; i < 16; ++i)
            toggles[i]->setBounds (juce::Rectangle<int> ((int) (r.getX() + i * w), r.getY(), (int) w, r.getHeight()).reduced (i % 4 == 3 ? 4 : 1, 0)
                                       .withTrimmedRight (i % 4 == 3 ? -3 : 0));
    }

private:
    juce::OwnedArray<ParamToggle> toggles;
};
} // namespace

FxPage::FxPage (SynthiaProcessor& p)
    : filter (p, "Main Filter", theme::filter, Section::filter, true),
      amp (p, "Amp Shaper", theme::filter, Section::amp, true),
      chopper (p, "Chopper (16 steps, host synced)", theme::filter),
      delay (p, "Delay", theme::fx, Section::delay, true),
      reverb (p, "Reverb", theme::fx, Section::reverb, true)
{
    filter.add (pid::fltOn, "On"); filter.add (pid::fltType, "Type"); filter.add (pid::fltCut); filter.add (pid::fltRes);
    filter.add (pid::fltDrive, "Drive"); filter.add (pid::fltEnv, "Env Amt"); filter.add (pid::fltPick, "Pick Amt");
    amp.caption ("Pick ADSR"); amp.add (pid::adsrAmt, "Amount"); amp.add (pid::adsrA); amp.add (pid::adsrD); amp.add (pid::adsrS); amp.add (pid::adsrR);
    amp.caption ("Swell"); amp.add (pid::swellOn, "Swell"); amp.add (pid::swellTime, "Time");
    chopper.add (pid::chopOn, "On"); chopper.add (pid::chopDiv, "Rate"); chopper.add (pid::chopDepth, "Depth"); chopper.add (pid::chopSmooth, "Smooth");
    chopper.caption ("Steps");
    chopper.addCustom (new ChopperSteps (p), 12);
    delay.add (pid::dlyOn, "On"); delay.add (pid::dlySync, "Sync"); delay.add (pid::dlyTime, "Time"); delay.add (pid::dlyDiv, "Note");
    delay.add (pid::dlyFb, "Feedback"); delay.add (pid::dlyMix, "Mix"); delay.add (pid::dlyPing, "Ping");
    delay.caption ("Feedback tone"); delay.add (pid::dlyLow, "Low Cut"); delay.add (pid::dlyHigh, "High Cut");
    delay.caption ("Tape"); delay.add (pid::dlyModRate, "Rate"); delay.add (pid::dlyModDepth, "Depth"); delay.add (pid::dlyWidth, "Width");
    reverb.add (pid::rvOn, "On"); reverb.add (pid::rvMode, "Mode"); reverb.add (pid::rvSize); reverb.add (pid::rvDecay); reverb.add (pid::rvInf, "Infinite");
    reverb.caption ("Pre-delay"); reverb.add (pid::rvPreSync, "Sync"); reverb.add (pid::rvPre, "Time"); reverb.add (pid::rvPreDiv, "Note");
    reverb.caption ("Tank"); reverb.add (pid::rvDiff); reverb.add (pid::rvDampLo, "Low Damp"); reverb.add (pid::rvDampHi, "High Damp");
    reverb.add (pid::rvModDepth, "Mod Depth"); reverb.add (pid::rvModRate, "Mod Rate");
    reverb.newRow();
    reverb.caption ("Shimmer / Bloom"); reverb.add (pid::rvShim); reverb.add (pid::rvShimInt, "Interval"); reverb.add (pid::rvBloom, "Bloom");
    reverb.caption ("Output"); reverb.add (pid::rvDuck); reverb.add (pid::rvWidth, "Width"); reverb.add (pid::rvMix, "Mix");
    for (auto* c : panels())
        addAndMakeVisible (c);
}

std::vector<ParamKnob*> FxPage::allKnobs()
{
    std::vector<ParamKnob*> v;
    for (auto* sp : panels())
        v.insert (v.end(), sp->knobs.begin(), sp->knobs.end());
    return v;
}

void FxPage::resized()
{
    const int w = getWidth() - 16;
    const int rowH = SectionPanel::kTitleH + SectionPanel::kCellH + SectionPanel::kCaptionH + 6;
    int y = 8;
    filter.setBounds (8, y, 8 * 64 + 12, rowH);
    amp.setBounds (filter.getRight() + 8, y, w - filter.getWidth() - 8, rowH);
    y += rowH + 8;
    chopper.setBounds (8, y, w, rowH);
    y += rowH + 8;
    delay.setBounds (8, y, w, rowH);
    y += rowH + 8;
    reverb.setBounds (8, y, w, 0);
    reverb.setBounds (8, y, w, reverb.preferredHeight());
}

// ============================================================================ MatrixPanel
namespace
{
class MatrixRow : public juce::Component
{
public:
    MatrixRow (SynthiaProcessor& p, int slot)
        : n (slot), src (p, pid::m1Src + slot * 4), dst (p, pid::m1Src + slot * 4 + 1),
          amt (p, pid::m1Src + slot * 4 + 2, theme::modc), via (p, pid::m1Src + slot * 4 + 3)
    {
        for (juce::Component* c : { (juce::Component*) &src, (juce::Component*) &dst, (juce::Component*) &amt, (juce::Component*) &via })
            addAndMakeVisible (c);
        src.box.setTextWhenNothingSelected ("Source");
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (n + 1), 0, 0, 22, getHeight(), juce::Justification::centred);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        r.removeFromLeft (24);
        src.setBounds (r.removeFromLeft (118));
        r.removeFromLeft (6);
        dst.setBounds (r.removeFromLeft (170));
        r.removeFromLeft (6);
        via.setBounds (r.removeFromRight (100));
        r.removeFromRight (6);
        amt.setBounds (r);
    }

private:
    int n;
    ParamCombo src;
    DestButton dst;
    ParamHSlider amt;
    ParamCombo via;
};
} // namespace

MatrixPanel::MatrixPanel (SynthiaProcessor& p)
{
    for (int i = 0; i < 16; ++i)
        addAndMakeVisible (rows.add (new MatrixRow (p, i)));
}

void MatrixPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (theme::modc);
    g.fillRoundedRectangle (r.getX() + 8, r.getY() + 7, 3.0f, 11.0f, 1.5f);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.drawText ("Modulation Matrix", 16, 2, 300, 20, juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (11.0f));
    const int colW = (getWidth() - 24) / 2;
    for (int c = 0; c < 2; ++c)
    {
        const int x = 8 + c * (colW + 8);
        g.drawText ("Source", x + 24, 24, 118, 14, juce::Justification::centredLeft);
        g.drawText ("Destination", x + 148, 24, 170, 14, juce::Justification::centredLeft);
        g.drawText ("Amount", x + 324, 24, 120, 14, juce::Justification::centredLeft);
        g.drawText ("Via (scaler)", x + colW - 100, 24, 100, 14, juce::Justification::centredLeft);
    }
}

void MatrixPanel::resized()
{
    const int colW = (getWidth() - 24) / 2;
    const int rowH = juce::jmin (30, (getHeight() - 44) / 8);
    for (int i = 0; i < 16; ++i)
    {
        const int c = i / 8, r = i % 8;
        rows[i]->setBounds (8 + c * (colW + 8), 40 + r * rowH, colW, rowH);
    }
}

// ============================================================================ ModPage
ModPage::ModPage (SynthiaProcessor& p)
    : lfo1 (p, "LFO 1", theme::modc, Section::count), lfo2 (p, "LFO 2", theme::modc), random (p, "Random", theme::modc),
      macros (p, "Macros", theme::modc), seqPanel (p, "Step Sequencer", theme::modc), seqEditor (p), matrix (p)
{
    for (int n = 0; n < 2; ++n)
    {
        auto& s = n == 0 ? lfo1 : lfo2;
        const int b = n == 0 ? pid::lfo1Shape : pid::lfo2Shape;
        s.add (b, "Shape"); s.add (b + 1, "Sync"); s.add (b + 2, "Rate"); s.add (b + 3, "Note"); s.add (b + 4, "Phase"); s.add (b + 5, "Retrig");
    }
    random.add (pid::rndShape, "Shape"); random.add (pid::rndSync, "Sync"); random.add (pid::rndRate, "Rate"); random.add (pid::rndDiv, "Note");
    for (int m = 0; m < 4; ++m) macros.add (pid::macro1 + m, "Macro " + juce::String (m + 1));
    seqPanel.add (pid::seqSteps, "Steps"); seqPanel.add (pid::seqDiv, "Rate"); seqPanel.add (pid::seqSync, "Sync");
    for (auto* c : panels())
        addAndMakeVisible (c);
    addAndMakeVisible (seqEditor);
    addAndMakeVisible (matrix);
}

std::vector<ParamKnob*> ModPage::allKnobs()
{
    std::vector<ParamKnob*> v;
    for (auto* sp : panels())
        v.insert (v.end(), sp->knobs.begin(), sp->knobs.end());
    return v;
}

void ModPage::setSourceValues (const float*) {}

void ModPage::resized()
{
    const int rowH = SectionPanel::kTitleH + SectionPanel::kCellH + SectionPanel::kCaptionH + 6;
    const int w = getWidth() - 16;
    int y = 8;
    lfo1.setBounds (8, y, w / 2 - 4, rowH);
    lfo2.setBounds (8 + w / 2 + 4, y, w / 2 - 4, rowH);
    y += rowH + 8;
    random.setBounds (8, y, 6 * 64 + 12, rowH);
    macros.setBounds (random.getRight() + 8, y, 4 * 64 + 12, rowH);
    seqPanel.setBounds (macros.getRight() + 8, y, getWidth() - 8 - (macros.getRight() + 8), rowH);
    y += rowH + 8;
    seqEditor.setBounds (8, y, w, 132);
    y += 140;
    matrix.setBounds (8, y, w, getHeight() - y - 8);
}
} // namespace synthia
