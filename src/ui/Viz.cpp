#include "ui/Viz.h"
#include "PluginProcessor.h"
#include "dsp/Formant.h"
#include "dsp/Resonator.h"

namespace synthia
{
namespace
{
const juce::Colour boxBg { 0xff14161c };
const juce::Colour boxEdge { 0xff2c2f3a };
const juce::Colour gridCol { 0xff22252e };
const juce::Colour greyBar { 0xff3a3f4d };

float toDb (float lin) { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }

juce::Font smallFont (float size = 10.0f, bool bold = false)
{
    return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
}

void dashedLine (juce::Graphics& g, juce::Line<float> l, float thickness, float dash = 4.0f, float gap = 3.0f)
{
    const float d[] = { dash, gap };
    g.drawDashedLine (l, d, 2, thickness);
}
} // namespace

namespace vizutil
{
void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r, float lo, float hi, float dbLo, float dbHi, bool labels)
{
    g.setFont (smallFont (9.0f));
    const std::pair<float, const char*> marks[] = { { 50.f, "50" }, { 100.f, "100" }, { 200.f, "200" }, { 500.f, "500" }, { 1000.f, "1k" },
                                                    { 2000.f, "2k" }, { 5000.f, "5k" }, { 10000.f, "10k" } };
    for (auto& [hz, txt] : marks)
    {
        if (hz <= lo || hz >= hi) continue;
        const float x = freqToX (hz, lo, hi, r);
        g.setColour (gridCol);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        if (labels)
        {
            g.setColour (theme::textDim);
            g.drawText (txt, juce::Rectangle<float> (x - 20, r.getBottom() + 1, 40, 11), juce::Justification::centred);
        }
    }
    const float step = (dbHi - dbLo) > 60.0f ? 20.0f : 12.0f;
    for (float db = dbHi - step; db > dbLo; db -= step)
    {
        const float y = r.getY() + r.getHeight() * (dbHi - db) / (dbHi - dbLo);
        g.setColour (gridCol);
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        if (labels)
        {
            g.setColour (theme::textDim);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getX() - 30, y - 6, 26, 12), juce::Justification::centredRight);
        }
    }
}
} // namespace vizutil

// ============================================================================ VizBox
VizBox::VizBox (SynthiaProcessor& p, const juce::String& c, juce::Colour a)
    : proc (p), viz (p.engine.viz), caption (c), accent (a)
{
    setInterceptsMouseClicks (false, false);
}

float VizBox::param (int index) const { return proc.getParam (index); }

float VizBox::effective (int index) const
{
    const float n = proc.engine.params.uiModulatedNorm (index);
    return n >= 0.0f ? spec (index).fromNorm (n) : param (index);
}

bool VizBox::layerOn (int onParam) const
{
    return param (onParam) >= 0.5f && param (onParam + 3) < 0.5f && param (onParam + 1) > 0.0001f;
}

void VizBox::drawNote (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text) const
{
    g.setColour (theme::textDim);
    g.setFont (smallFont (12.0f));
    g.drawText (text, area, juce::Justification::centred);
}

void VizBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (boxBg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (boxEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (accent);
    g.setFont (smallFont (10.5f, true));
    g.drawText (caption.toUpperCase(), juce::Rectangle<float> (10, 4, r.getWidth() * 0.6f, 14), juce::Justification::centredLeft);
    // legend, right aligned
    float x = r.getRight() - 10.0f;
    g.setFont (smallFont (10.0f));
    for (auto it = legend.rbegin(); it != legend.rend(); ++it)
    {
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), it->text);
        x -= tw;
        g.setColour (theme::textDim);
        g.drawText (it->text, juce::Rectangle<float> (x, 4, tw + 2, 14), juce::Justification::centredLeft);
        x -= 16.0f;
        g.setColour (it->colour);
        if (it->kind == 0)
            g.fillRoundedRectangle (x, 7, 10, 9, 2.0f);
        else if (it->kind == 1)
            g.fillRect (x, 11.0f, 12.0f, 2.0f);
        else
            dashedLine (g, { x, 12.0f, x + 12.0f, 12.0f }, 1.6f, 3.0f, 2.0f);
        x -= 10.0f;
    }
    juce::Graphics::ScopedSaveState ss (g);
    auto area = r.reduced (8.0f, 4.0f).withTrimmedTop (16.0f);
    g.reduceClipRegion (getLocalBounds().reduced (1));
    paintContent (g, area);
}

// ============================================================================ PickTimeline
PickTimeline::PickTimeline (SynthiaProcessor& p) : VizBox (p, "Picks", theme::input)
{
    setLegend ({ { theme::text, "Pick", 1 }, { theme::mod, "Env", 1 }, { juce::Colour (0xff6b7280), "Gate", 2 } });
}

void PickTimeline::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.withTrimmedBottom (12.0f);
    const int N = dsp::VizData::kTimelineVisible;
    const int w = viz.tlWrite.load();
    const float mid = r.getY() + r.getHeight() * 0.62f;
    auto yUp = [&] (float lin) { return mid - (mid - r.getY() - 6.0f) * juce::jlimit (0.0f, 1.0f, (toDb (lin) + 60.0f) / 60.0f); };
    auto yDown = [&] (float lin) { return mid + (r.getBottom() - mid) * juce::jlimit (0.0f, 1.0f, (toDb (lin) + 60.0f) / 60.0f); };
    const float dx = r.getWidth() / (float) (N - 1);
    juce::Path wave, env;
    std::vector<float> picks;
    for (int i = 0; i < N; ++i)
    {
        const int idx = ((w - N + i) % dsp::VizData::kTimelineCols + dsp::VizData::kTimelineCols) % dsp::VizData::kTimelineCols;
        const float x = r.getX() + i * dx;
        const float pk = viz.tlPeak[(size_t) idx].load();
        if (i == 0) wave.startNewSubPath (x, yUp (pk)); else wave.lineTo (x, yUp (pk));
        const float e = viz.tlEnv[(size_t) idx].load();
        if (i == 0) env.startNewSubPath (x, yUp (e)); else env.lineTo (x, yUp (e));
        if (viz.tlPick[(size_t) idx].load()) picks.push_back (x);
    }
    for (int i = N - 1; i >= 0; --i)
    {
        const int idx = ((w - N + i) % dsp::VizData::kTimelineCols + dsp::VizData::kTimelineCols) % dsp::VizData::kTimelineCols;
        wave.lineTo (r.getX() + i * dx, yDown (viz.tlPeak[(size_t) idx].load()));
    }
    wave.closeSubPath();
    g.setColour (juce::Colour (0xff3b4250));
    g.fillPath (wave);
    const float thr = viz.gateThreshold.load();
    if (thr > 0.0f)
    {
        g.setColour (juce::Colour (0xff6b7280));
        dashedLine (g, { r.getX(), yUp (thr), r.getRight(), yUp (thr) }, 1.0f, 3.0f, 3.0f);
    }
    g.setColour (theme::mod);
    g.strokePath (env, juce::PathStrokeType (1.3f));
    g.setColour (theme::text);
    for (float x : picks)
    {
        g.drawLine (x, r.getY() + 4, x, r.getBottom() - 2, 1.2f);
        juce::Path t;
        t.addTriangle (x - 4, r.getY() + 2, x + 4, r.getY() + 2, x, r.getY() + 8);
        g.fillPath (t);
    }
    g.setColour (theme::textDim);
    g.setFont (smallFont (9.0f));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x88\x92" "3 s"), juce::Rectangle<float> (r.getX(), r.getBottom() + 1, 40, 11), juce::Justification::centredLeft);
    g.drawText ("now", juce::Rectangle<float> (r.getRight() - 40, r.getBottom() + 1, 40, 11), juce::Justification::centredRight);
}

// ============================================================================ VoicerBands
VoicerBands::VoicerBands (SynthiaProcessor& p) : VizBox (p, {}, theme::voicer)
{
    caption = juce::CharPointer_UTF8 ("Bands \xc2\xb7 live");
    setLegend ({ { theme::voicer, "Band level", 0 }, { greyBar, "suppressed by Focus", 0 }, { theme::mod, "Tilt", 2 } });
}

void VoicerBands::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    constexpr float flo = 35.0f, fhi = 8000.0f, dbLo = -60.0f, dbHi = 0.0f;
    auto plot = r.withTrimmedLeft (26.0f).withTrimmedBottom (13.0f).withTrimmedTop (4.0f);
    const int K = viz.vcBands.load();
    const float lo = K > 0 ? viz.vcLo.load() : param (pid::vcLow);
    const float hi = K > 0 ? viz.vcHi.load() : param (pid::vcHigh);
    // outside the band range
    g.setColour (juce::Colour (0xff0d0e12));
    g.fillRect (plot.withRight (vizutil::freqToX (lo, flo, fhi, plot)));
    g.fillRect (plot.withLeft (vizutil::freqToX (hi, flo, fhi, plot)));
    vizutil::drawFreqGrid (g, plot, flo, fhi, dbLo, dbHi, true);
    auto Y = [&] (float db) { return plot.getY() + plot.getHeight() * (dbHi - juce::jlimit (dbLo, dbHi, db)) / (dbHi - dbLo); };

    if (! layerOn (pid::vcOn) || K <= 1)
    {
        drawNote (g, plot, "Voicer is off");
        return;
    }
    const float sp = std::pow (hi / lo, 1.0f / (float) (K - 1));
    const float bw = plot.getWidth() * std::log (sp) / std::log (fhi / flo) * 0.72f;
    for (int k = 0; k < K; ++k)
    {
        const float fc = lo * std::pow (sp, (float) k);
        const float a = viz.vcAmp[(size_t) k].load();
        if (a < 1.0e-4f) continue;
        const float w = viz.vcWeight[(size_t) k].load();
        const float x = vizutil::freqToX (fc, flo, fhi, plot);
        const float top = Y (toDb (a));
        g.setColour (greyBar);
        g.fillRoundedRectangle (x - bw * 0.5f, top, bw, plot.getBottom() - top, 1.5f);
        if (w > 0.02f)
        {
            const float tw = Y (toDb (a * w));
            g.setColour (theme::voicer.withAlpha (0.45f + 0.55f * w));
            g.fillRoundedRectangle (x - bw * 0.5f, tw, bw, plot.getBottom() - tw, 1.5f);
            if (w >= 0.999f)
            {
                g.setColour (juce::Colour (0xffbff3fa));
                g.fillRect (x - bw * 0.5f, top - 3.0f, bw, 2.0f);
            }
        }
    }
    // tilt (+-9 dB across the range around -6 dB)
    const float tilt = effective (pid::vcTilt);
    const float x0 = vizutil::freqToX (lo, flo, fhi, plot), x1 = vizutil::freqToX (hi, flo, fhi, plot);
    g.setColour (theme::mod);
    dashedLine (g, { x0, Y (-6.0f - tilt * 9.0f), x1, Y (-6.0f + tilt * 9.0f) }, 1.4f, 5.0f, 4.0f);
    // range markers
    g.setFont (smallFont (10.0f));
    for (int i = 0; i < 2; ++i)
    {
        const float hz = i == 0 ? lo : hi;
        const float x = vizutil::freqToX (hz, flo, fhi, plot);
        g.setColour (theme::voicer);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.5f);
        const auto txt = (i == 0 ? "Low " : "High ") + formatValue (i == 0 ? pid::vcLow : pid::vcHigh, hz);
        g.drawText (txt, juce::Rectangle<float> (i == 0 ? x + 5 : x - 125, plot.getY() + 2, 120, 12),
                    i == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight);
    }
}

// ============================================================================ WaveView
WaveView::WaveView (SynthiaProcessor& p) : VizBox (p, "Wave", theme::voicer)
{
    setLegend ({ { theme::voicer, "Wave", 1 }, { theme::text, "All voices", 1 } });
}

namespace
{
/** Naive (non band-limited) version of the Voicer's waveform morph, for drawing only. */
float voicerShape (float t, float wave, float pw)
{
    t -= std::floor (t);
    const float sine = std::sin (juce::MathConstants<float>::twoPi * t);
    const float saw = 2.0f * t - 1.0f;
    auto pulse = [t] (float d) { return (t < d ? 1.0f : -1.0f) - (2.0f * d - 1.0f); };
    if (wave < 1.0f) return sine + (saw - sine) * wave;
    if (wave < 2.0f) return saw + (pulse (0.5f) - saw) * (wave - 1.0f);
    return pulse (0.5f + (pw - 0.5f) * juce::jmin (1.0f, wave - 2.0f));
}
} // namespace

void WaveView::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced (2.0f, 3.0f);
    const float wave = effective (pid::vcWave), pw = effective (pid::vcPw);
    const float lm = effective (pid::vcMain), ls = effective (pid::vcSub), lo = effective (pid::vcOct), li = effective (pid::vcInt);
    static constexpr float intervals[] = { -12, -7, -5, 5, 7, 12, 19 };
    const float ratio = std::exp2 (intervals[juce::jlimit (0, 6, juce::roundToInt (param (pid::vcIntRatio)))] / 12.0f);
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.42f;
    g.setColour (juce::Colour (0xff2a2e38));
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    // 2 cycles of the main voice (= one cycle of the sub)
    const float cycles = 2.0f;
    const int n = 360;
    juce::Path pWave, pMix;
    float peak = 1.0e-3f;
    std::vector<float> mix ((size_t) n + 1);
    for (int i = 0; i <= n; ++i)
    {
        const float t = cycles * (float) i / (float) n;
        float m = lm * voicerShape (t, wave, pw);
        if (ls > 0.001f) m += ls * voicerShape (t * 0.5f, wave, pw);
        if (lo > 0.001f) m += lo * voicerShape (t * 2.0f, wave, pw);
        if (li > 0.001f) m += li * voicerShape (t * ratio, wave, pw);
        mix[(size_t) i] = m;
        peak = juce::jmax (peak, std::abs (m));
    }
    for (int i = 0; i <= n; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) n;
        const float yw = mid - amp * voicerShape (cycles * (float) i / (float) n, wave, pw) * 0.85f;
        const float ym = mid - amp * mix[(size_t) i] / peak;
        if (i == 0) { pWave.startNewSubPath (x, yw); pMix.startNewSubPath (x, ym); }
        else { pWave.lineTo (x, yw); pMix.lineTo (x, ym); }
    }
    g.setColour (theme::text.withAlpha (0.55f));
    g.strokePath (pMix, juce::PathStrokeType (1.2f));
    g.setColour (theme::voicer);
    g.strokePath (pWave, juce::PathStrokeType (1.8f));
    // unison badge
    const int U = juce::roundToInt (param (pid::vcUniVoices));
    if (U > 1)
    {
        const juce::String txt = "Unison " + juce::String (U) + " x " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb1"))
                                 + formatValue (pid::vcUniDetune, effective (pid::vcUniDetune)).trimCharactersAtStart ("+");
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), txt) + 14.0f;
        auto badge = juce::Rectangle<float> (r.getRight() - w, r.getBottom() - 15.0f, w, 15.0f);
        g.setColour (theme::voicer);
        g.fillRoundedRectangle (badge, 7.5f);
        g.setColour (juce::Colours::black);
        g.drawText (txt, badge, juce::Justification::centred);
    }
}

// ============================================================================ GranularView
GranularView::GranularView (SynthiaProcessor& p) : VizBox (p, {}, theme::granular)
{
    caption = juce::CharPointer_UTF8 ("Buffer \xc2\xb7 8 s  \xe2\x86\x92  zoom 2 s");
    setLegend ({ { juce::Colour (0xffbbf7d0), "Grain forward", 0 }, { juce::Colour (0xfffdba74), "Grain reverse", 0 }, { juce::Colour (0xfff87171), "Write head", 1 } });
}

void GranularView::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int C = dsp::VizData::kGrCols;
    const float bufSec = juce::jmax (1.0f, viz.grBufferSeconds.load());
    const int wc = viz.grWriteCol.load();
    auto colPeak = [&] (float secondsAgo)
    {
        const int back = (int) (secondsAgo / bufSec * (float) C);
        const int idx = ((wc - back) % C + C) % C;
        return juce::jmin (1.0f, viz.grCol[(size_t) idx].load());
    };
    auto drawWave = [&] (juce::Rectangle<float> a, float spanSec, int points)
    {
        juce::Path p;
        const float mid = a.getCentreY();
        for (int i = 0; i <= points; ++i)
        {
            const float ago = spanSec * (1.0f - (float) i / (float) points);
            const float v = std::sqrt (colPeak (ago)) * a.getHeight() * 0.45f;
            const float x = a.getX() + a.getWidth() * (float) i / (float) points;
            if (i == 0) p.startNewSubPath (x, mid - v); else p.lineTo (x, mid - v);
        }
        for (int i = points; i >= 0; --i)
        {
            const float ago = spanSec * (1.0f - (float) i / (float) points);
            const float v = std::sqrt (colPeak (ago)) * a.getHeight() * 0.45f;
            p.lineTo (a.getX() + a.getWidth() * (float) i / (float) points, mid + v);
        }
        p.closeSubPath();
        g.setColour (juce::Colour (0xff2f4a3a));
        g.fillPath (p);
    };
    const bool frozen = viz.grFrozen.load() != 0;
    if (! layerOn (pid::grOn))
    {
        drawNote (g, r, "Granular is off");
        return;
    }
    // ---- overview: whole buffer
    auto ov = r.removeFromTop (40.0f).withTrimmedTop (4.0f);
    g.setColour (juce::Colour (0xff191c22));
    g.fillRoundedRectangle (ov, 3.0f);
    drawWave (ov, bufSec, 400);
    const float zoomSec = 2.0f;
    auto zoomRect = ov.withLeft (ov.getRight() - ov.getWidth() * zoomSec / bufSec);
    g.setColour (theme::granular.withAlpha (0.08f));
    g.fillRect (zoomRect);
    g.setColour (theme::granular);
    g.drawRect (zoomRect, 1.0f);
    g.setFont (smallFont (9.0f));
    g.setColour (theme::textDim);
    for (int sec = 0; sec <= 8; sec += 2)
    {
        const float x = ov.getRight() - ov.getWidth() * (float) sec / bufSec;
        g.drawText (sec == 0 ? juce::String ("now") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (sec) + " s",
                    juce::Rectangle<float> (x - 22, ov.getBottom() + 1, 44, 11), juce::Justification::centred);
    }
    r.removeFromTop (16.0f);
    // ---- zoom: last 2 s
    auto z = r.withTrimmedBottom (13.0f);
    g.setColour (juce::Colour (0xff191c22));
    g.fillRoundedRectangle (z, 3.0f);
    drawWave (z, zoomSec, 300);
    auto X = [&] (float ago) { return z.getRight() - z.getWidth() * ago / zoomSec; };
    const float mid = z.getCentreY();
    auto Ysemi = [&] (float st) { return mid - juce::jlimit (-24.0f, 24.0f, st) / 24.0f * z.getHeight() * 0.42f; };
    // pitch lanes
    g.setFont (smallFont (9.0f));
    for (int st : { 24, 12, 0, -12, -24 })
    {
        g.setColour (juce::Colour (0xff262a33));
        dashedLine (g, { z.getX(), Ysemi ((float) st), z.getRight(), Ysemi ((float) st) }, 1.0f, 2.0f, 4.0f);
        g.setColour (theme::textDim);
        g.drawText ((st > 0 ? "+" : "") + juce::String (st) + " st", juce::Rectangle<float> (z.getX() + 4, Ysemi ((float) st) - 12, 42, 11),
                    juce::Justification::centredLeft);
    }
    // position + spray region (from the parameters)
    const bool stutter = viz.grStutActive.load() != 0;
    if (! stutter)
    {
        const float pos = effective (pid::grPos) * 0.001f, spray = effective (pid::grSpray) * 0.001f;
        const float xa = X (juce::jmin (zoomSec, pos + spray)), xb = X (juce::jmin (zoomSec, pos));
        g.setColour (theme::granular.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xa, z.getY(), juce::jmax (xa + 1.0f, xb), z.getBottom()));
        g.setColour (theme::granular);
        g.drawLine (xb, z.getY(), xb, z.getBottom(), 1.5f);
        g.setFont (smallFont (10.0f));
        g.drawText ("Position " + formatValue (pid::grPos, pos * 1000.0f), juce::Rectangle<float> (xb + 4, z.getY() + 2, 140, 12), juce::Justification::centredLeft);
    }
    else
    {
        const float st0 = viz.grStutStartSec.load(), len = viz.grStutLenSec.load();
        const float xa = X (juce::jmin (zoomSec, st0)), xb = X (juce::jmax (0.0f, st0 - len));
        g.setColour (theme::granular.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xa, z.getY(), juce::jmax (xa + 2.0f, xb), z.getBottom()));
        g.setColour (theme::granular);
        g.setFont (smallFont (10.0f));
        g.drawText ("Stutter slice", juce::Rectangle<float> (xa + 4, z.getY() + 2, 120, 12), juce::Justification::centredLeft);
    }
    // grains
    if (! stutter)
        for (auto& gr : viz.grains)
        {
            const int fl = gr.flags.load();
            if ((fl & 1) == 0) continue;
            const float d = gr.delaySec.load();
            if (d > zoomSec) continue;
            const float age = gr.age.load();
            const float win = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, age));
            const bool rev = (fl & 2) != 0;
            const float x = X (d), y = Ysemi (gr.semis.load());
            g.setColour ((rev ? juce::Colour (0xfffdba74) : juce::Colour (0xffbbf7d0)).withAlpha (0.35f + 0.65f * win));
            const float rad = 2.5f + 3.0f * win;
            g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
        }
    // write head
    g.setColour (frozen ? theme::textDim : juce::Colour (0xfff87171));
    g.drawLine (z.getRight() - 1, z.getY(), z.getRight() - 1, z.getBottom(), 2.0f);
    g.setFont (smallFont (10.0f));
    g.drawText (frozen ? "Frozen" : "Write head", juce::Rectangle<float> (z.getRight() - 110, z.getBottom() - 14, 104, 12), juce::Justification::centredRight);
    g.setColour (theme::textDim);
    g.setFont (smallFont (9.0f));
    for (float sec : { 2.0f, 1.5f, 1.0f, 0.5f })
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (sec, 1) + " s",
                    juce::Rectangle<float> (X (sec) - 22, z.getBottom() + 1, 44, 11), juce::Justification::centred);
}

// ============================================================================ FilterCurve
FilterCurve::FilterCurve (SynthiaProcessor& p) : VizBox (p, "Filter", theme::filter)
{
    setLegend ({ { theme::filter, "Cutoff", 1 }, { theme::mod, "live", 2 } });
}

float FilterCurve::responseDb (int type, float hz, float fc, float res) const
{
    using C = std::complex<float>;
    const float x = hz / fc;
    const C s (0.0f, x);
    C h;
    switch (type)
    {
        case 0: // ladder
        {
            const float k = res * 4.2f;
            h = (1.0f + res * 1.2f) / ((1.0f + s) * (1.0f + s) * (1.0f + s) * (1.0f + s) + k);
            break;
        }
        case 1: case 2: case 3: case 4:
        {
            const float q = 0.5f + res * res * 24.0f;
            const C den = s * s + s / q + 1.0f;
            if (type == 1) h = 1.0f / den;
            else if (type == 2) h = s / den;            // bp (peak = Q, like the DSP)
            else if (type == 3) h = s * s / den;
            else h = (s * s + 1.0f) / den;
            break;
        }
        case 5: // comb
        {
            const float fb = res * 0.97f;
            const float w = juce::MathConstants<float>::twoPi * hz / fc; // delay = 1/fc
            const C z = std::polar (1.0f, -w);
            h = 0.6f * (1.0f + z / (1.0f - fb * z));
            break;
        }
        default: // formant: cutoff -> vowel position
        {
            const float pos = 4.0f * std::log (fc / 20.0f) / std::log (1000.0f);
            float f[3], gg[3], bw[3];
            dsp::FormantFilter::formants (pos, 0.0f, res, f, gg, bw);
            for (int i = 0; i < 3; ++i)
            {
                const C si (0.0f, hz / f[i]);
                const float q = juce::jmax (0.5f, f[i] / bw[i]);
                h += gg[i] * (si / q) / (si * si + si / q + 1.0f);
            }
            h *= 1.0f + res * 1.5f;
            break;
        }
    }
    return toDb (std::abs (h));
}

void FilterCurve::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    constexpr float lo = 20.0f, hi = 20000.0f, dbLo = -36.0f, dbHi = 18.0f;
    auto plot = r.withTrimmedBottom (12.0f).withTrimmedTop (2.0f);
    vizutil::drawFreqGrid (g, plot, lo, hi, dbLo, dbHi, false);
    g.setFont (smallFont (9.0f));
    g.setColour (theme::textDim);
    for (auto [hz, t] : { std::pair<float, const char*> { 100.f, "100" }, { 1000.f, "1k" }, { 10000.f, "10k" } })
        g.drawText (t, juce::Rectangle<float> (vizutil::freqToX (hz, lo, hi, plot) - 15, plot.getBottom() + 1, 30, 10), juce::Justification::centred);
    auto Y = [&] (float db) { return plot.getY() + plot.getHeight() * (dbHi - juce::jlimit (dbLo, dbHi, db)) / (dbHi - dbLo); };
    g.setColour (juce::Colour (0xff2a2e38));
    dashedLine (g, { plot.getX(), Y (0), plot.getRight(), Y (0) }, 1.0f, 2.0f, 3.0f);
    if (param (pid::fltOn) < 0.5f)
    {
        g.setColour (theme::filter.withAlpha (0.5f));
        g.drawLine (plot.getX(), Y (0), plot.getRight(), Y (0), 1.5f);
        drawNote (g, plot.withTrimmedTop (plot.getHeight() * 0.5f), "Filter off");
        return;
    }
    const int type = juce::roundToInt (param (pid::fltType));
    const float res = effective (pid::fltRes);
    auto curve = [&] (float fc)
    {
        juce::Path p;
        const int n = 160;
        for (int i = 0; i <= n; ++i)
        {
            const float hz = lo * std::pow (hi / lo, (float) i / (float) n);
            const float x = vizutil::freqToX (hz, lo, hi, plot), y = Y (responseDb (type, hz, fc, res));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };
    const float base = param (pid::fltCut);
    auto p = curve (base);
    juce::Path fill (p);
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setColour (theme::filter.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (theme::filter);
    g.strokePath (p, juce::PathStrokeType (1.6f));
    const float live = viz.fltCutoff.load();
    if (std::abs (std::log (live / base)) > 0.02f)
    {
        g.setColour (theme::mod);
        const float d[] = { 4.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.3f).createDashedStroke (dashed, curve (live), d, 2);
        g.fillPath (dashed);
    }
}

// ============================================================================ AdsrView
AdsrView::AdsrView (SynthiaProcessor& p) : VizBox (p, {}, theme::filter)
{
    caption = juce::CharPointer_UTF8 ("Pick ADSR \xc2\xb7 live");
}

void AdsrView::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced (2.0f, 4.0f);
    const float a = param (pid::adsrA), d = param (pid::adsrD), s = param (pid::adsrS), rl = param (pid::adsrR);
    auto seg = [] (float ms) { return std::log (1.0f + ms / 5.0f); };
    const float total = seg (a) + seg (d) + seg (rl);
    const float susW = r.getWidth() * 0.22f;
    const float scale = (r.getWidth() - susW) / juce::jmax (0.01f, total);
    const float xA = r.getX() + seg (a) * scale, xD = xA + seg (d) * scale, xS = xD + susW, xR = xS + seg (rl) * scale;
    const float yTop = r.getY(), yBot = r.getBottom(), yS = yBot - (yBot - yTop) * s;
    juce::Path p;
    p.startNewSubPath (r.getX(), yBot);
    p.lineTo (xA, yTop);
    p.quadraticTo (xA + (xD - xA) * 0.2f, yS, xD, yS);
    p.lineTo (xS, yS);
    p.quadraticTo (xS + (xR - xS) * 0.15f, yBot, xR, yBot);
    const float amt = param (pid::adsrAmt);
    juce::Path fill (p);
    fill.closeSubPath();
    g.setColour (theme::filter.withAlpha (amt > 0.001f ? 0.15f : 0.05f));
    g.fillPath (fill);
    g.setColour (theme::filter.withAlpha (amt > 0.001f ? 1.0f : 0.35f));
    g.strokePath (p, juce::PathStrokeType (1.8f));
    if (amt <= 0.001f)
    {
        drawNote (g, r, "Pick ADSR amount is 0");
        return;
    }
    // live position
    const int stage = viz.adsrStage.load();
    const float lvl = viz.adsrLevel.load();
    float x = -1.0f;
    switch (stage)
    {
        case 1: x = r.getX() + (xA - r.getX()) * lvl; break;
        case 2: x = xA + (xD - xA) * juce::jlimit (0.0f, 1.0f, (1.0f - lvl) / juce::jmax (0.001f, 1.0f - s)); break;
        case 3: x = (xD + xS) * 0.5f; break;
        case 4: x = xS + (xR - xS) * juce::jlimit (0.0f, 1.0f, 1.0f - lvl / juce::jmax (0.001f, s)); break;
        default: break;
    }
    if (x >= 0.0f)
    {
        const float y = yBot - (yBot - yTop) * lvl;
        g.setColour (theme::text);
        dashedLine (g, { x, yTop - 2, x, yBot }, 1.0f, 2.0f, 3.0f);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
    }
}

// ============================================================================ SpectrumView
SpectrumView::SpectrumView (SynthiaProcessor& p) : VizBox (p, {}, theme::spectral)
{
    caption = juce::CharPointer_UTF8 ("Spectrum \xc2\xb7 live");
    setLegend ({ { theme::spectral, "Live", 0 }, { theme::text, "Frozen", 1 }, { theme::mod, "Partials", 0 } });
}

void SpectrumView::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    constexpr float dbLo = -90.0f, dbHi = 0.0f;
    const float lo = dsp::VizData::kSpecLo, hi = dsp::VizData::kSpecHi;
    auto plot = r.withTrimmedLeft (26.0f).withTrimmedBottom (13.0f).withTrimmedTop (4.0f);
    vizutil::drawFreqGrid (g, plot, lo, hi, dbLo, dbHi, true);
    if (! layerOn (pid::spOn))
    {
        drawNote (g, plot, "Spectral is off");
        return;
    }
    const int B = dsp::VizData::kSpecBins;
    auto X = [&] (int b) { return plot.getX() + plot.getWidth() * ((float) b + 0.5f) / (float) B; };
    auto Y = [&] (float db) { return plot.getY() + plot.getHeight() * (dbHi - juce::jlimit (dbLo, dbHi, db)) / (dbHi - dbLo); };
    juce::Path live, frozen;
    live.startNewSubPath (plot.getX(), plot.getBottom());
    for (int b = 0; b < B; ++b)
        live.lineTo (X (b), Y (viz.spLive[(size_t) b].load()));
    juce::Path line (live);
    live.lineTo (plot.getRight(), plot.getBottom());
    live.closeSubPath();
    g.setColour (theme::spectral.withAlpha (0.28f));
    g.fillPath (live);
    g.setColour (theme::spectral);
    g.strokePath (line, juce::PathStrokeType (1.3f));
    const bool isFrozen = viz.spFrozenOn.load() != 0;
    if (isFrozen)
    {
        for (int b = 0; b < B; ++b)
        {
            const float y = Y (viz.spFrozen[(size_t) b].load());
            if (b == 0) frozen.startNewSubPath (X (b), y); else frozen.lineTo (X (b), y);
        }
        g.setColour (theme::text.withAlpha (0.85f));
        g.strokePath (frozen, juce::PathStrokeType (1.2f));
    }
    g.setColour (theme::mod);
    for (int b = 0; b < B; ++b)
        if (viz.spPeak[(size_t) b].load())
            g.fillEllipse (X (b) - 3.0f, Y (viz.spLive[(size_t) b].load()) - 3.0f, 6.0f, 6.0f);
    // badges
    juce::StringArray badges;
    if (isFrozen) badges.add (juce::String (juce::CharPointer_UTF8 ("FROZEN \xc2\xb7 Blend ")) + formatValue (pid::spFrzBlend, effective (pid::spFrzBlend)));
    const float shift = effective (pid::spShift), stretch = effective (pid::spStretch);
    if (std::abs (shift) > 0.5f) badges.add ("Shift " + formatValue (pid::spShift, shift));
    if (std::abs (stretch - 1.0f) > 0.002f) badges.add ("Stretch " + formatValue (pid::spStretch, stretch));
    float bx = plot.getX() + 8.0f;
    g.setFont (smallFont (10.0f, true));
    for (auto& b : badges)
    {
        const juce::String& txt = b;
        const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), txt) + 16.0f;
        g.setColour (theme::spectral);
        g.fillRoundedRectangle (bx, plot.getY() + 4, w, 16, 8.0f);
        g.setColour (juce::Colours::black);
        g.drawText (txt, juce::Rectangle<float> (bx, plot.getY() + 4, w, 16), juce::Justification::centred);
        bx += w + 6.0f;
    }
}

// ============================================================================ ResoVoices
ResoVoices::ResoVoices (SynthiaProcessor& p) : VizBox (p, {}, theme::reso)
{
    caption = juce::CharPointer_UTF8 ("Resonator voices \xc2\xb7 live");
}

void ResoVoices::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const bool resoMode = juce::roundToInt (param (pid::rsMode)) == 0;
    if (! layerOn (pid::rsOn) || ! resoMode)
    {
        drawNote (g, r, resoMode ? "Resonator layer is off" : "Only in mode Resonator");
        return;
    }
    const int slot = juce::jlimit (0, 3, juce::roundToInt (effective (pid::rsSlot)));
    const int b = pid::c1Root + slot * 4;
    float notes[8];
    const int n = dsp::chordNotes (juce::roundToInt (param (b)), juce::roundToInt (param (b + 1)), (int) param (b + 2), param (b + 3), notes);
    const float w = 38.0f;
    auto area = r.reduced (4.0f, 2.0f);
    for (int v = 0; v < n; ++v)
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, (toDb (viz.rsVoice[(size_t) v].load()) + 48.0f) / 48.0f);
        auto col = juce::Rectangle<float> (area.getX() + v * (w + 6.0f), area.getY(), w, area.getHeight());
        auto meter = col.withTrimmedBottom (14.0f).withSizeKeepingCentre (14.0f, col.getHeight() - 14.0f);
        g.setColour (theme::knobBg);
        g.fillRoundedRectangle (meter, 3.0f);
        g.setColour (theme::reso);
        g.fillRoundedRectangle (meter.withTop (meter.getBottom() - meter.getHeight() * lvl), 3.0f);
        const int m = juce::roundToInt (notes[v]);
        g.setColour (theme::textDim);
        g.setFont (smallFont (10.0f));
        g.drawText (juce::String (names[(m % 12 + 12) % 12]) + juce::String (m / 12 - 1), col.removeFromBottom (12.0f), juce::Justification::centred);
    }
}

// ============================================================================ VowelMap
VowelMap::VowelMap (SynthiaProcessor& p) : VizBox (p, "Vowel map (mode Formant)", theme::reso) {}

void VowelMap::tick()
{
    const bool formantMode = juce::roundToInt (param (pid::rsMode)) == 2;
    const float a = formantMode && layerOn (pid::rsOn) ? 1.0f : 0.45f;
    if (std::abs (getAlpha() - a) > 0.01f)
        setAlpha (a);
    VizBox::tick();
}

void VowelMap::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced (20.0f, 4.0f);
    const float shift = effective (pid::fmShift), res = effective (pid::fmReso);
    auto pt = [&] (float pos)
    {
        float f[3], gg[3], bw[3];
        dsp::FormantFilter::formants (pos, shift, res, f, gg, bw);
        const float x = r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, std::log (f[1] / 500.0f) / std::log (3200.0f / 500.0f));
        const float y = r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, std::log (f[0] / 220.0f) / std::log (1100.0f / 220.0f));
        return juce::Point<float> (x, y);
    };
    juce::Path path;
    for (int i = 0; i <= 40; ++i)
    {
        const auto p = pt (4.0f * (float) i / 40.0f);
        if (i == 0) path.startNewSubPath (p); else path.lineTo (p);
    }
    g.setColour (greyBar);
    const float d[] = { 4.0f, 3.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, path, d, 2);
    g.fillPath (dashed);
    const char* vowels[] = { "A", "E", "I", "O", "U" };
    g.setFont (smallFont (11.0f, true));
    for (int i = 0; i < 5; ++i)
    {
        const auto p = pt ((float) i);
        g.setColour (juce::Colour (0xff22252e));
        g.fillEllipse (p.x - 11, p.y - 11, 22, 22);
        g.setColour (greyBar);
        g.drawEllipse (p.x - 11, p.y - 11, 22, 22, 1.0f);
        g.setColour (theme::text);
        g.drawText (vowels[i], juce::Rectangle<float> (p.x - 11, p.y - 11, 22, 22), juce::Justification::centred);
    }
    const auto c = pt (effective (pid::fmVowel));
    g.setColour (theme::reso);
    g.fillEllipse (c.x - 6, c.y - 6, 12, 12);
}

// ============================================================================ ChordKeyboard
ChordKeyboard::ChordKeyboard (SynthiaProcessor& p, int s) : proc (p), slot (s) { setInterceptsMouseClicks (false, false); }

void ChordKeyboard::paint (juce::Graphics& g)
{
    const int b = pid::c1Root + slot * 4;
    float notes[8];
    const int n = dsp::chordNotes (juce::roundToInt (proc.getParam (b)), juce::roundToInt (proc.getParam (b + 1)), (int) proc.getParam (b + 2),
                                   proc.getParam (b + 3), notes);
    const float modN = proc.engine.params.uiModulatedNorm (pid::rsSlot);
    const int active = juce::jlimit (0, 3, juce::roundToInt (modN >= 0.0f ? spec (pid::rsSlot).fromNorm (modN) : proc.getParam (pid::rsSlot)));
    const bool isActive = active == slot;
    auto has = [&] (int m) { for (int i = 0; i < n; ++i) if (juce::roundToInt (notes[i]) == m) return true; return false; };
    const int lo = ((int) proc.getParam (b + 2) + 1) * 12; // C of the slot octave
    const int octs = 3;
    const int whiteSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
    auto r = getLocalBounds().toFloat();
    const float ww = r.getWidth() / (7.0f * octs);
    for (int o = 0, i = 0; o < octs; ++o)
        for (int ws : whiteSteps)
        {
            const bool on = has (lo + o * 12 + ws);
            auto key = juce::Rectangle<float> (r.getX() + i * ww, r.getY(), ww, r.getHeight()).reduced (0.5f, 0.0f);
            g.setColour (on ? theme::reso : juce::Colour (0xffd9dbe2));
            g.setOpacity (isActive ? 1.0f : 0.55f);
            g.fillRoundedRectangle (key, 2.0f);
            ++i;
        }
    for (int o = 0, i = 0; o < octs; ++o)
        for (int ws : whiteSteps)
        {
            if (ws == 0 || ws == 2 || ws == 5 || ws == 7 || ws == 9)
            {
                const bool on = has (lo + o * 12 + ws + 1);
                auto key = juce::Rectangle<float> (r.getX() + (i + 1) * ww - ww * 0.3f, r.getY(), ww * 0.6f, r.getHeight() * 0.6f);
                g.setColour (on ? theme::reso : juce::Colour (0xff22252e));
                g.setOpacity (isActive || on ? 1.0f : 0.7f);
                g.fillRoundedRectangle (key, 1.5f);
            }
            ++i;
        }
    if (isActive)
    {
        g.setColour (theme::reso);
        g.setOpacity (1.0f);
        g.drawRoundedRectangle (r.expanded (2.0f).reduced (0.5f), 3.0f, 1.5f);
    }
}

// ============================================================================ LfoPreview
LfoPreview::LfoPreview (SynthiaProcessor& p, int i) : VizBox (p, {}, theme::modc), lfo (i)
{
    caption = juce::CharPointer_UTF8 ("2 cycles \xc2\xb7 live");
}

void LfoPreview::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced (2.0f, 4.0f);
    const int shape = juce::roundToInt (param (lfo == 0 ? pid::lfo1Shape : pid::lfo2Shape));
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.5f;
    auto value = [&] (float t) -> float
    {
        const float p = t - std::floor (t);
        switch (shape)
        {
            case 0: return std::sin (juce::MathConstants<float>::twoPi * p);
            case 1: return 4.0f * std::abs (p - 0.5f) - 1.0f;
            case 2: return 2.0f * p - 1.0f;
            case 3: return 1.0f - 2.0f * p;
            case 4: return p < 0.5f ? 1.0f : -1.0f;
            default: return 0.0f;
        }
    };
    g.setColour (juce::Colour (0xff2a2e38));
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    juce::Path p;
    if (shape <= 4)
    {
        for (int i = 0; i <= 300; ++i)
        {
            const float t = 2.0f * (float) i / 300.0f;
            const float x = r.getX() + r.getWidth() * (float) i / 300.0f, y = mid - amp * value (t);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
    }
    else
    {
        // random shapes: an illustrative pattern (8 steps)
        juce::Random rnd (12345);
        float prev = rnd.nextFloat() * 2.0f - 1.0f;
        p.startNewSubPath (r.getX(), mid - amp * prev);
        for (int c = 0; c < 8; ++c)
        {
            const float v = rnd.nextFloat() * 2.0f - 1.0f;
            const float x0 = r.getX() + r.getWidth() * (float) c / 8.0f, x1 = r.getX() + r.getWidth() * (float) (c + 1) / 8.0f;
            if (shape == 5) { p.lineTo (x0, mid - amp * v); p.lineTo (x1, mid - amp * v); }
            else p.quadraticTo ((x0 + x1) * 0.5f, mid - amp * prev, x1, mid - amp * v);
            prev = v;
        }
    }
    g.setColour (theme::modc);
    g.strokePath (p, juce::PathStrokeType (1.6f));
    const float ph = lfo == 0 ? viz.lfo1Phase.load() : viz.lfo2Phase.load();
    const float x = r.getX() + r.getWidth() * ph * 0.5f;
    const float v = proc.engine.mod.uiSource[(size_t) (lfo == 0 ? modsrc::lfo1 : modsrc::lfo2)].load();
    g.setColour (theme::text);
    dashedLine (g, { x, r.getY(), x, r.getBottom() }, 1.0f, 2.0f, 3.0f);
    g.fillEllipse (x - 4.5f, mid - amp * v - 4.5f, 9.0f, 9.0f);
}

// ============================================================================ DelayView
DelayView::DelayView (SynthiaProcessor& p) : VizBox (p, "Echoes", theme::fx) {}

void DelayView::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced (4.0f, 2.0f).withTrimmedBottom (11.0f);
    const bool on = param (pid::dlyOn) >= 0.5f;
    const bool ping = param (pid::dlyPing) >= 0.5f;
    const float fb = effective (pid::dlyFb);
    const float mid = r.getCentreY();
    g.setColour (juce::Colour (0xff2a2e38));
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    if (! on)
    {
        drawNote (g, r, "Delay is off");
        return;
    }
    const float step = r.getWidth() / 9.0f;
    float a = 1.0f;
    for (int i = 0; i < 10 && a > 0.01f; ++i)
    {
        const float x = r.getX() + 4.0f + i * step;
        const float h = (r.getHeight() * 0.5f - 2.0f) * a;
        const bool left = ! ping || (i % 2) == 1;
        const bool right = ! ping || (i % 2) == 0;
        g.setColour (i == 0 ? theme::text : theme::fx.withAlpha (0.35f + 0.65f * a));
        if (i == 0)
            g.fillRoundedRectangle (x, mid - h, 6.0f, h * 2.0f, 2.0f);
        else
        {
            if (left) g.fillRoundedRectangle (x, mid - h, 6.0f, h, 2.0f);
            if (right) g.fillRoundedRectangle (x, mid, 6.0f, h, 2.0f);
        }
        if (i > 0) a *= juce::jmax (0.0f, fb);
    }
    const bool sync = param (pid::dlySync) >= 0.5f;
    juce::String t = sync ? choicesFor (ChoiceList::syncDiv)[juce::roundToInt (param (pid::dlyDiv))] : formatValue (pid::dlyTime, effective (pid::dlyTime));
    g.setColour (theme::textDim);
    g.setFont (smallFont (9.0f));
    if (ping) t << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ping-pong (L up, R down)"));
    g.drawText (t, juce::Rectangle<float> (r.getX(), r.getBottom() + 1, r.getWidth(), 10),
                juce::Justification::centredLeft);
}

// ============================================================================ ReverbView
ReverbView::ReverbView (SynthiaProcessor& p) : VizBox (p, "Reverb decay", theme::fx)
{
    setLegend ({ { theme::fx, "Tail", 0 }, { theme::mod, "Shimmer", 0 } });
}

void ReverbView::paintContent (juce::Graphics& g, juce::Rectangle<float> r)
{
    r = r.reduced (4.0f, 2.0f).withTrimmedBottom (11.0f);
    if (param (pid::rvOn) < 0.5f)
    {
        drawNote (g, r, "Reverb is off");
        return;
    }
    const int mode = juce::roundToInt (param (pid::rvMode));
    const bool inf = param (pid::rvInf) >= 0.5f;
    const float decay = effective (pid::rvDecay);
    const float pre = param (pid::rvPreSync) >= 0.5f ? 0.05f : effective (pid::rvPre) * 0.001f;
    const float span = juce::jlimit (1.0f, 60.0f, inf ? 12.0f : decay * 1.25f + pre);
    const float attack = mode == 3 ? effective (pid::rvBloom) * 0.5f : 0.02f + 0.06f * effective (pid::rvDiff);
    juce::Path p;
    p.startNewSubPath (r.getX(), r.getBottom());
    for (int i = 0; i <= 200; ++i)
    {
        const float t = span * (float) i / 200.0f;
        const float tt = juce::jmax (0.0f, t - pre);
        float env = t < pre ? 0.0f : (1.0f - std::exp (-tt / juce::jmax (0.005f, attack)));
        if (! inf) env *= std::pow (10.0f, -3.0f * tt / juce::jmax (0.1f, decay) * 0.5f); // amplitude in dB/20 -> shown on a dB-ish scale
        p.lineTo (r.getX() + r.getWidth() * (float) i / 200.0f, r.getBottom() - r.getHeight() * 0.9f * env);
    }
    juce::Path line (p);
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();
    g.setColour (theme::fx.withAlpha (0.22f));
    g.fillPath (p);
    g.setColour (theme::fx);
    g.strokePath (line, juce::PathStrokeType (1.4f));
    // shimmer tiers
    const float shim = effective (pid::rvShim);
    if (shim > 0.01f && ! inf)
    {
        static constexpr int semis[] = { 12, 7, 19, 24, -12 };
        int st = semis[juce::jlimit (0, 4, juce::roundToInt (param (pid::rvShimInt)))];
        if (mode == 2 && st > 0) st = -st; // Abyss shifts down
        g.setFont (smallFont (9.5f, true));
        for (int j = 1; j <= 3; ++j)
        {
            const float frac = 0.22f * (float) j;
            const float x = r.getX() + r.getWidth() * frac;
            const float y = r.getBottom() - r.getHeight() * 0.9f * std::pow (10.0f, -3.0f * span * frac / juce::jmax (0.1f, decay) * 0.5f) - 10.0f - 5.0f * j;
            const juce::String txt = (st * j > 0 ? "+" : "") + juce::String (st * j);
            g.setColour (theme::mod.withAlpha (juce::jlimit (0.25f, 1.0f, shim * (1.1f - 0.2f * j))));
            g.fillRoundedRectangle (x - 16, y - 8, 32, 15, 7.0f);
            g.setColour (juce::Colours::black);
            g.drawText (txt, juce::Rectangle<float> (x - 16, y - 8, 32, 15), juce::Justification::centred);
        }
    }
    g.setColour (theme::textDim);
    g.setFont (smallFont (9.0f));
    for (int i = 0; i <= 4; ++i)
    {
        const float t = span * (float) i / 4.0f;
        const float lx = r.getX() + r.getWidth() * (float) i / 4.0f;
        const auto box = i == 0 ? juce::Rectangle<float> (lx, r.getBottom() + 1, 40, 10)
                       : (i == 4 ? juce::Rectangle<float> (lx - 40, r.getBottom() + 1, 40, 10) : juce::Rectangle<float> (lx - 20, r.getBottom() + 1, 40, 10));
        g.drawText (juce::String (t, t < 10.0f ? 1 : 0) + " s", box,
                    i == 0 ? juce::Justification::centredLeft : (i == 4 ? juce::Justification::centredRight : juce::Justification::centred));
    }
    if (inf)
    {
        g.setColour (theme::text);
        g.setFont (smallFont (10.0f, true));
        g.drawText ("INFINITE", r.withTrimmedRight (6.0f).withHeight (14.0f).translated (0, 2), juce::Justification::centredRight);
    }
}
} // namespace synthia
