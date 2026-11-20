#include "ui/LookAndFeel.h"

namespace synthia
{
SynthiaLookAndFeel::SynthiaLookAndFeel()
{
    using namespace theme;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::PopupMenu::backgroundColourId, panel.brighter (0.05f));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, voicer.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::ComboBox::backgroundColourId, knobBg);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::TextButton::buttonColourId, knobBg);
    setColour (juce::TextButton::buttonOnColourId, voicer);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0252833));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, panelEdge);
    setColour (juce::TextEditor::backgroundColourId, knobBg);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, panelEdge);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, panelEdge);
}

void SynthiaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                           juce::Slider& s)
{
    using namespace theme;
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float size = juce::jmin (b.getWidth(), b.getHeight());
    b = b.withSizeKeepingCentre (size, size);
    const auto c = b.getCentre();
    const float r = size * 0.5f;
    const float lw = juce::jmax (2.0f, r * 0.14f);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r - lw, r - lw, 0.0f, start, end, true);
    g.setColour (knobBg.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (from the centre for bipolar parameters)
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    const float to = start + pos * (end - start);
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - lw, r - lw, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
    const bool enabled = s.isEnabled();
    g.setColour (enabled ? accent : accent.withSaturation (0.0f).withAlpha (0.5f));
    g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // modulation ring (live modulated value)
    const double modNorm = s.getProperties().getWithDefault ("modNorm", -1.0);
    if (modNorm >= 0.0)
    {
        const float mto = start + (float) modNorm * (end - start);
        juce::Path m;
        const float mr = r - lw * 2.3f;
        m.addCentredArc (c.x, c.y, mr, mr, 0.0f, juce::jmin (to, mto), juce::jmax (to, mto), true);
        g.setColour (mod);
        g.strokePath (m, juce::PathStrokeType (lw * 0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> mp (c.x + mr * std::sin (mto), c.y - mr * std::cos (mto));
        g.fillEllipse (juce::Rectangle<float> (lw * 1.2f, lw * 1.2f).withCentre (mp));
    }

    // body + pointer
    const float br = r - lw * 2.6f;
    g.setColour (knobBg);
    g.fillEllipse (juce::Rectangle<float> (br * 2, br * 2).withCentre (c));
    g.setColour (panelEdge.brighter (0.2f));
    g.drawEllipse (juce::Rectangle<float> (br * 2, br * 2).withCentre (c), 1.0f);
    const juce::Point<float> p1 (c.x + br * 0.25f * std::sin (to), c.y - br * 0.25f * std::cos (to));
    const juce::Point<float> p2 (c.x + br * 0.9f * std::sin (to), c.y - br * 0.9f * std::cos (to));
    g.setColour (enabled ? text : textDim);
    g.drawLine ({ p1, p2 }, juce::jmax (1.5f, lw * 0.6f));
}

void SynthiaLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace theme;
    const auto accent = s.findColour (juce::Slider::trackColourId);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        auto track = b.withSizeKeepingCentre (b.getWidth(), 6.0f);
        g.setColour (knobBg.brighter (0.15f));
        g.fillRoundedRectangle (track, 3.0f);
        const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
        const float cx = bipolar ? track.getCentreX() : track.getX();
        g.setColour (accent);
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, pos), track.getY(), juce::jmax (cx, pos), track.getBottom()), 3.0f);
        g.setColour (text);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pos, track.getCentreY() }));
    }
    else
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
}

void SynthiaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace theme;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    juce::Colour fill = on ? accent : knobBg;
    if (over) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.1f);
    if (! b.isEnabled()) fill = fill.withAlpha (0.4f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent.brighter (0.2f) : panelEdge.brighter (0.1f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void SynthiaLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

void SynthiaLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    using namespace theme;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cb.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (panelEdge.brighter (0.1f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (textDim);
    g.fillPath (arrow);
}

void SynthiaLookAndFeel::positionComboBoxText (juce::ComboBox& cb, juce::Label& l)
{
    l.setBounds (4, 0, cb.getWidth() - 20, cb.getHeight());
    l.setFont (getComboBoxFont (cb));
}

void SynthiaLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.0f);
    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.0f, 1.0f);
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.append (text, juce::FontOptions (13.0f), findColour (juce::TooltipWindow::textColourId));
    juce::TextLayout tl;
    tl.createLayout (s, (float) w - 16.0f);
    tl.draw (g, { 8.0f, 6.0f, (float) w - 16.0f, (float) h - 12.0f });
}

juce::Rectangle<int> SynthiaLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.setWordWrap (juce::AttributedString::byWord);
    s.append (text, juce::FontOptions (13.0f));
    juce::TextLayout tl;
    tl.createLayout (s, 300.0f);
    const int w = (int) std::ceil (tl.getWidth()) + 18, h = (int) std::ceil (tl.getHeight()) + 14;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 14,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}
} // namespace synthia
