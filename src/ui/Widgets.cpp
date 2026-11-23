#include "ui/Widgets.h"
#include "PluginProcessor.h"

namespace synthia
{
juce::String tooltipFor (int index)
{
    const auto& s = spec (index);
    juce::String t;
    t << s.name << ": " << s.tooltip;
    if (s.kind == Kind::F || s.kind == Kind::I)
    {
        t << "\nRange " << formatValue (index, s.min) << " .. " << formatValue (index, s.max) << ", default " << formatValue (index, s.def) << ".";
        t << "\nDouble-click: reset. Shift-drag: fine.";
        if (s.modulatable)
            t << " Modulatable (see Modulation page).";
    }
    return t;
}

// ---------------------------------------------------------------------------
ParamKnob::ParamKnob (SynthiaProcessor& p, int idx, juce::Colour accent, const juce::String& lbl) : index (idx)
{
    const auto& s = spec (idx);
    name = lbl.isNotEmpty() ? lbl : juce::String (s.name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.getProperties().set ("bipolar", s.min < 0.0f && s.max > 0.0f);
    slider.setTooltip (tooltipFor (idx));
    slider.setVelocityModeParameters (1.0, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    auto* prm = p.apvts.getParameter (paramId (idx));
    attachment = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (prm), slider, nullptr);
    slider.setDoubleClickReturnValue (true, s.def);
    slider.onHover = [this] (bool h) { hovering = h; updateLabel(); };
    slider.onValueChange = [this] { if (hovering || slider.isMouseButtonDown()) updateLabel(); };
    slider.onDragEnd = [this] { updateLabel(); };
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, theme::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    updateLabel();
}

ParamKnob::~ParamKnob() = default;

void ParamKnob::updateLabel()
{
    if (hovering || slider.isMouseButtonDown())
    {
        label.setText (formatValue (index, (float) slider.getValue(), 14), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, theme::text);
    }
    else
    {
        label.setText (name, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, theme::textDim);
    }
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (15).expanded (6, 0));
    slider.setBounds (r);
}

void ParamKnob::setModNorm (float n)
{
    if (std::abs (n - lastMod) < 0.002f)
        return;
    lastMod = n;
    slider.getProperties().set ("modNorm", (double) n);
    slider.repaint();
}

// ---------------------------------------------------------------------------
ParamToggle::ParamToggle (SynthiaProcessor& p, int idx, juce::Colour accent, const juce::String& text)
{
    button.setButtonText (text.isNotEmpty() ? text : juce::String (spec (idx).name));
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    button.setTooltip (tooltipFor (idx));
    attachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (paramId (idx)), button, nullptr);
    addAndMakeVisible (button);
}

// ---------------------------------------------------------------------------
ParamCombo::ParamCombo (SynthiaProcessor& p, int idx, const juce::String& caption)
{
    box.addItemList (choicesFor (spec (idx).choices), 1);
    box.setTooltip (tooltipFor (idx));
    attachment = std::make_unique<juce::ComboBoxParameterAttachment> (*p.apvts.getParameter (paramId (idx)), box, nullptr);
    label.setText (caption.isNotEmpty() ? caption : juce::String (spec (idx).name), juce::dontSendNotification);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, theme::textDim);
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (box);
    addAndMakeVisible (label);
}

void ParamCombo::resized()
{
    auto r = getLocalBounds();
    if (r.getHeight() > 36)
    {
        auto top = r.removeFromTop (r.getHeight() / 2 - 4);
        label.setBounds (top.removeFromBottom (15));
        box.setBounds (r.removeFromTop (24));
        label.setVisible (true);
    }
    else
    {
        label.setVisible (false);
        box.setBounds (r);
    }
}

// ---------------------------------------------------------------------------
DestButton::DestButton (SynthiaProcessor& p, int idx)
{
    param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (paramId (idx)));
    button.setTooltip (tooltipFor (idx));
    button.onClick = [this] { showMenu(); };
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { update (v); }, nullptr);
    attachment->sendInitialUpdate();
    addAndMakeVisible (button);
}

void DestButton::update (float v)
{
    const int i = juce::roundToInt (v);
    button.setButtonText (choicesFor (ChoiceList::modDest)[i]);
    button.setToggleState (i > 0, juce::dontSendNotification);
}

void DestButton::showMenu()
{
    static const char* sectionNames[] = { "Global", "Input", "Mixer", "A Voicer", "B Spectral", "C Resonator", "D Granular",
                                          "E Dry", "Filter", "Amp Shaper", "Delay", "Reverb", "Modulation", "Matrix" };
    juce::PopupMenu root;
    root.addItem (1, "Off", true, juce::roundToInt (param->convertFrom0to1 (param->getValue())) == 0);
    const auto& dests = modDestinations();
    const int current = juce::roundToInt (param->convertFrom0to1 (param->getValue()));
    for (int sec = 0; sec < (int) Section::count; ++sec)
    {
        juce::PopupMenu sub;
        for (int d = 0; d < (int) dests.size(); ++d)
            if ((int) spec (dests[(size_t) d]).section == sec)
                sub.addItem (d + 2, spec (dests[(size_t) d]).name, true, current == d + 1);
        if (sub.getNumItems() > 0)
            root.addSubMenu (sectionNames[sec], sub);
    }
    root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                        [this] (int r)
                        {
                            if (r <= 0) return;
                            attachment->setValueAsCompleteGesture ((float) (r - 1));
                        });
}

// ---------------------------------------------------------------------------
ParamHSlider::ParamHSlider (SynthiaProcessor& p, int idx, juce::Colour accent)
{
    const auto& s = spec (idx);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.getProperties().set ("bipolar", s.min < 0.0f && s.max > 0.0f);
    slider.setTooltip (tooltipFor (idx));
    attachment = std::make_unique<juce::SliderParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (paramId (idx))), slider, nullptr);
    slider.setDoubleClickReturnValue (true, s.def);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
}

// ---------------------------------------------------------------------------
void Meter::push (float peak)
{
    level = juce::jmax (peak, level * 0.82f);
    if (peak >= hold) { hold = peak; holdCounter = 30; }
    else if (--holdCounter < 0) hold *= 0.9f;
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::knobBg);
    g.fillRoundedRectangle (r, 2.0f);
    auto toY = [&] (float v)
    {
        const float db = juce::Decibels::gainToDecibels (v, -60.0f);
        return r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    };
    const float y = toY (level);
    const auto col = level > 0.99f ? juce::Colours::red : (level > 0.5f ? juce::Colours::orange : theme::granular);
    g.setColour (col);
    g.fillRoundedRectangle (r.withTop (y), 2.0f);
    g.setColour (theme::text.withAlpha (0.7f));
    g.fillRect (r.getX(), toY (hold), r.getWidth(), 1.5f);
}

// ---------------------------------------------------------------------------
SectionPanel::SectionPanel (SynthiaProcessor& p, const juce::String& t, juce::Colour a, Section sec, bool dice)
    : accent (a), proc (p), title (t), section (sec)
{
    if (dice)
    {
        diceButton.setTooltip ("Randomize this section (amount = Chaos knob in the top bar).");
        diceButton.onClick = [this] { if (onRandomize) onRandomize (section); };
        addAndMakeVisible (diceButton);
    }
}

juce::Component* SectionPanel::add (int index, const juce::String& label, int span)
{
    const auto& s = spec (index);
    juce::Component* c = nullptr;
    int defSpan = 1;
    switch (s.kind)
    {
        case Kind::F:
        case Kind::I:
        {
            auto* k = new ParamKnob (proc, index, accent, label);
            knobs.push_back (k);
            c = k;
            break;
        }
        case Kind::B:
            c = new ParamToggle (proc, index, accent, label);
            break;
        case Kind::C:
            c = new ParamCombo (proc, index, label);
            defSpan = 2;
            break;
    }
    owned.add (c);
    addAndMakeVisible (c);
    items.push_back ({ c, span > 0 ? span : defSpan, 1, pendingNewRow, pendingCaption, {} });
    pendingNewRow = false;
    pendingCaption = {};
    return c;
}

void SectionPanel::addCustom (juce::Component* c, int span, int rows)
{
    addAndMakeVisible (c);
    items.push_back ({ c, span, rows, pendingNewRow, pendingCaption, {} });
    pendingNewRow = false;
    pendingCaption = {};
}

int SectionPanel::rowsUsed() const
{
    const int cols = juce::jmax (1, (getWidth() - 12) / kCellW);
    int row = 0, col = 0, maxRow = 1;
    for (auto& it : items)
    {
        if ((it.newRow && col > 0) || col + it.span > cols)
        {
            ++row;
            col = 0;
        }
        col += it.span;
        maxRow = juce::jmax (maxRow, row + it.rows);
    }
    return maxRow;
}

void SectionPanel::resized()
{
    const int cols = juce::jmax (1, (getWidth() - 12) / kCellW);
    const int x0 = 6, y0 = kTitleH;
    int row = 0, col = 0;
    std::array<int, 16> rowRight {};
    for (auto& it : items)
    {
        if ((it.newRow && col > 0) || col + it.span > cols)
        {
            ++row;
            col = 0;
        }
        const int x = x0 + col * kCellW;
        const int y = y0 + row * (kCellH + kCaptionH) + kCaptionH;
        it.cellArea = { x, y - kCaptionH, it.span * kCellW, kCaptionH };
        auto b = juce::Rectangle<int> (x, y, it.span * kCellW, it.rows * (kCellH + kCaptionH) - kCaptionH).reduced (3, 2);
        if (dynamic_cast<ParamToggle*> (it.comp) != nullptr)
            b = b.withSizeKeepingCentre (b.getWidth(), 24);
        else if (dynamic_cast<ParamCombo*> (it.comp) != nullptr)
            b = b.withSizeKeepingCentre (b.getWidth(), 48);
        it.comp->setBounds (b);
        col += it.span;
        rowRight[(size_t) juce::jmin (15, row)] = juce::jmax (rowRight[(size_t) juce::jmin (15, row)], x + it.span * kCellW);
    }
    const int rowH = kCellH + kCaptionH;
    if (footer != nullptr)
    {
        const int fy = y0 + rowsUsed() * rowH + 6;
        footer->setBounds (6, fy, getWidth() - 12, juce::jmax (10, getHeight() - fy - 6));
    }
    for (auto& [c, r] : sides)
    {
        const int sx = rowRight[(size_t) juce::jlimit (0, 15, r)] + 10;
        c->setBounds (sx, y0 + r * rowH + 6, juce::jmax (10, getWidth() - 6 - sx), rowH - 8);
    }
    diceButton.setBounds (getWidth() - 42, 3, 36, 18);
}

void SectionPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX() + 8, r.getY() + 7, 3.0f, 11.0f, 1.5f);
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.setColour (theme::text);
    g.drawText (title, 16, 2, getWidth() - 60, kTitleH - 4, juce::Justification::centredLeft);
    // group captions
    g.setFont (juce::FontOptions (10.5f));
    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& it = items[i];
        if (it.caption.isEmpty())
            continue;
        // extend the caption line over the following items of the same row until the next caption
        int right = it.cellArea.getRight();
        for (size_t j = i + 1; j < items.size(); ++j)
        {
            if (items[j].caption.isNotEmpty() || items[j].cellArea.getY() != it.cellArea.getY())
                break;
            right = items[j].cellArea.getRight();
        }
        auto cap = juce::Rectangle<int>::leftTopRightBottom (it.cellArea.getX() + 3, it.cellArea.getY(), right - 3, it.cellArea.getBottom());
        g.setColour (accent.withAlpha (0.8f));
        g.drawText (it.caption.toUpperCase(), cap, juce::Justification::centredLeft);
        const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), it.caption.toUpperCase()) + 6;
        g.setColour (theme::panelEdge.brighter (0.15f));
        g.fillRect (cap.getX() + tw, cap.getCentreY(), juce::jmax (0, cap.getWidth() - tw), 1);
    }
}
} // namespace synthia
