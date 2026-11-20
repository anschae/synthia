#pragma once
// Layer D -- GRANULAR: rolling 8 s input buffer + pool of 64 grains
// (all preallocated), plus a host-synced stutter.
//
// Grain reads never cross the write head: the start delay is clamped so that
// a forward grain at ratio r over L samples starts at least (r-1)L behind.
// Feedback writes the (soft-limited) grain output back into the buffer.
// Stutter: at every retrigger point (1/4, 1/2 or a bar) the next slice is
// recorded live and then repeated until the next retrigger point.

#include <array>
#include "dsp/Common.h"
#include "dsp/Layer.h"

namespace synthia::dsp
{
class Granular
{
public:
    static constexpr int kMaxGrains = 64;
    static constexpr float kBufferSeconds = 8.0f;
    static constexpr int kWindowTable = 1024;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufLen = (int) (kBufferSeconds * sr) + 16;
        buf.assign ((size_t) bufLen, 0.0f);
        for (int w = 0; w < 3; ++w)
            for (int i = 0; i <= kWindowTable; ++i)
            {
                const float t = (float) i / (float) kWindowTable;
                float v;
                if (w == 0) v = 0.5f - 0.5f * std::cos (kTwoPi * t);
                else if (w == 1) // Tukey (alpha 0.5)
                    v = t < 0.25f ? 0.5f - 0.5f * std::cos (kTwoPi * t * 2.0f)
                      : (t > 0.75f ? 0.5f - 0.5f * std::cos (kTwoPi * (1.0f - t) * 2.0f) : 1.0f);
                else // trapezoid (10 % ramps)
                    v = std::min (1.0f, std::min (t, 1.0f - t) * 10.0f);
                windows[w][(size_t) i] = v;
            }
        rng.seed (0x6A6A6A6Au);
        reset();
    }

    void reset() noexcept
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
        for (auto& g : grains) g.active = false;
        untilNext = 0.0;
        stutActive = false;
        lastStutBoundary = -1;
        fbEnv = 0.0f;
    }

    void process (const float* in, float* outL, float* outR, const ChunkContext& c) noexcept
    {
        const auto& p = c.p;
        const int n = c.n;
        const float sizeMs = p.cur (pid::grSize);
        const float density = p.cur (pid::grDensity);
        const float posMs = p.cur (pid::grPos);
        const float sprayMs = p.cur (pid::grSpray);
        const float pitch = p.cur (pid::grPitch);
        const int quant = p.choice (pid::grQuant);
        const float pjit = p.cur (pid::grPJit);
        const float panJit = p.cur (pid::grPanJit);
        const float revProb = p.cur (pid::grRev);
        const int window = juce::jlimit (0, 2, p.choice (pid::grWindow));
        const bool freeze = p.on (pid::grFreeze);
        const bool stutter = p.on (pid::grStutter);
        const float fb = p.cur (pid::grFb);

        const double spq = c.t.samplesPerQuarter (sr);
        const double sliceQ = stutDivQuarters (p.choice (pid::grStutDiv));
        const double holdQ = std::max (sliceQ, stutHoldQuarters (p.choice (pid::grStutHold), c.t.quartersPerBar));
        const int sliceLen = std::max (16, std::min (bufLen / 2, (int) (sliceQ * spq)));
        const float xfade = std::min ((float) sliceLen * 0.25f, 0.003f * (float) sr);

        const int grainLen = std::max (16, (int) (sizeMs * 0.001f * (float) sr));
        const double interval = sr / std::max (0.1f, density);
        const float norm = 1.0f / std::sqrt (std::max (1.0f, density * sizeMs * 0.001f));

        for (int s = 0; s < n; ++s)
        {
            // ---- stutter boundary detection
            if (stutter)
            {
                const double q = c.t.ppq + (double) s / spq;
                const auto boundary = (int64_t) std::floor (q / holdQ);
                if (boundary != lastStutBoundary)
                {
                    lastStutBoundary = boundary;
                    stutActive = true;
                    stutStart = writePos;
                    stutAge = 0;
                }
            }
            else
                stutActive = false;

            // ---- grain cloud
            float gl = 0.0f, gr = 0.0f;
            if (! stutActive)
            {
                untilNext -= 1.0;
                if (untilNext <= 0.0)
                {
                    untilNext += interval * (0.7 + 0.6 * rng.uni());
                    spawn (grainLen, posMs, sprayMs, pitch, quant, pjit, panJit, revProb, window, norm);
                }
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    const float t = (float) g.age / (float) g.len;
                    const float wi = windowAt (g.window, t);
                    const float v = readBuffer (g.pos) * wi * g.gain;
                    gl += v * g.panL;
                    gr += v * g.panR;
                    g.pos += g.inc;
                    if (++g.age >= g.len) g.active = false;
                }
            }

            // ---- stutter playback
            float sv = 0.0f;
            if (stutActive)
            {
                if (stutAge < sliceLen)
                    sv = in[s]; // first pass: live (being recorded)
                else
                {
                    const int ph = stutAge % sliceLen;
                    sv = buf[(size_t) wrap (stutStart + ph)];
                    // crossfade the loop seam with the continuation after the slice
                    if ((float) ph < xfade)
                    {
                        const float a = (float) ph / xfade;
                        sv = sv * a + buf[(size_t) wrap (stutStart + sliceLen + ph)] * (1.0f - a);
                    }
                }
                ++stutAge;
            }

            // ---- write (unless frozen; stutter keeps recording so the slice exists)
            if (! freeze || stutActive)
            {
                float w = in[s];
                if (fb > 0.0001f && ! stutActive)
                    w += fb * safeLimit (0.5f * (gl + gr), 0.7f, 1.0f);
                buf[(size_t) writePos] = sanitize (w);
                // The 8 s buffer is much longer than any slice, so the slice being
                // repeated is never overwritten while the stutter runs.
                writePos = wrap (writePos + 1);
            }

            if (stutActive)
            {
                outL[s] = sv;
                outR[s] = sv;
            }
            else
            {
                outL[s] = sanitize (gl);
                outR[s] = sanitize (gr);
            }
        }
    }

    int activeGrains() const noexcept
    {
        int a = 0;
        for (auto& g : grains) a += g.active ? 1 : 0;
        return a;
    }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0, inc = 1;
        int len = 1, age = 0, window = 0;
        float panL = 1, panR = 1, gain = 1;
    };

    int wrap (int64_t i) const noexcept
    {
        auto m = (int) (i % bufLen);
        return m < 0 ? m + bufLen : m;
    }

    float readBuffer (double pos) const noexcept
    {
        const auto i = (int64_t) std::floor (pos);
        const float t = (float) (pos - (double) i);
        return hermite (buf[(size_t) wrap (i - 1)], buf[(size_t) wrap (i)], buf[(size_t) wrap (i + 1)], buf[(size_t) wrap (i + 2)], t);
    }

    float windowAt (int w, float t) const noexcept
    {
        const float x = clamp (t, 0.0f, 1.0f) * (float) kWindowTable;
        const int i = std::min (kWindowTable - 1, (int) x);
        const float f = x - (float) i;
        return windows[w][(size_t) i] + f * (windows[w][(size_t) i + 1] - windows[w][(size_t) i]);
    }

    void spawn (int len, float posMs, float sprayMs, float pitch, int quant, float pjit, float panJit, float revProb, int window, float norm) noexcept
    {
        Grain* g = nullptr;
        for (auto& x : grains)
            if (! x.active) { g = &x; break; }
        if (g == nullptr)
            return;
        float st = pitch + pjit * rng.bi();
        switch (quant)
        {
            case 1: st = 12.0f * std::round (st / 12.0f); break;
            case 2: st = 7.0f * std::round (st / 7.0f); break; // fifth stack (+-7, +-14 ...)
            case 3: st = std::round (st); break;
            default: break;
        }
        const float ratio = std::exp2 (st / 12.0f);
        const bool rev = rng.uni() < revProb;
        float delay = (posMs + sprayMs * rng.uni()) * 0.001f * (float) sr;
        const float minD = rev ? (float) len * ratio + 4.0f : std::max (4.0f, (ratio - 1.0f) * (float) len + 4.0f);
        const float maxD = (float) bufLen - (float) len * ratio - 8.0f;
        delay = clamp (std::max (delay, minD), minD, std::max (minD, maxD));
        g->active = true;
        g->pos = (double) writePos - (double) delay;
        g->inc = rev ? -ratio : ratio;
        g->len = len;
        g->age = 0;
        g->window = window;
        g->gain = norm;
        panGains (panJit * rng.bi(), g->panL, g->panR);
    }

    double sr = 48000.0;
    std::vector<float> buf;
    int bufLen = 1, writePos = 0;
    Grain grains[kMaxGrains];
    std::array<float, kWindowTable + 1> windows[3];
    double untilNext = 0.0;
    bool stutActive = false;
    int64_t lastStutBoundary = -1;
    int stutStart = 0, stutAge = 0;
    float fbEnv = 0.0f;
    Rng rng;
};
} // namespace synthia::dsp
