#pragma once
// Clean stereo delay: ms or host-synced time, feedback with low/high cut and
// a safe limiter in the loop, ping-pong, subtle tape wobble, width.
// Time changes (knob, modulation, host tempo) are followed by a smoothed,
// interpolated read position -> a short tape-like glide instead of clicks.

#include "dsp/Common.h"

namespace synthia::dsp
{
class StereoDelay
{
public:
    static constexpr float kMaxSeconds = 6.0f;

    struct Settings
    {
        float timeSamples = 18000;
        float feedback = 0.35f, mix = 0.25f;
        bool pingPong = false;
        float lowCut = 120, highCut = 7000;
        float modRate = 0.6f, modDepth = 0.15f, width = 1.0f;
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& d : line)
            d.prepare ((int) (kMaxSeconds * sr) + 64);
        timeCoef = onePoleCoef (120.0f, sr);
        reset();
    }

    void reset() noexcept
    {
        for (auto& d : line) d.clear();
        for (auto& f : lc) f.reset();
        for (auto& f : hc) f.reset();
        curTime = -1.0f;
        lfoPhase = 0.0f;
        drift = driftTarget = 0.0f;
    }

    void setChunk (const Settings& st, int n) noexcept
    {
        s = st;
        s.timeSamples = clamp (s.timeSamples, 1.0f, kMaxSeconds * (float) sr);
        if (curTime < 0.0f)
            curTime = s.timeSamples;
        if (s.lowCut != lastLc || s.highCut != lastHc)
        {
            lastLc = s.lowCut; lastHc = s.highCut;
            for (auto& f : lc) f.setup (s.lowCut, 0.707f, sr);
            for (auto& f : hc) f.setup (s.highCut, 0.707f, sr);
        }
        // slow random drift for the "tape" feel
        driftCounter -= n;
        if (driftCounter <= 0)
        {
            driftCounter = (int) (0.25 * sr);
            driftTarget = rng.bi();
        }
    }

    /** In-place stereo processing of one sample. */
    void processSample (float& l, float& r) noexcept
    {
        curTime = s.timeSamples + (curTime - s.timeSamples) * timeCoef;
        drift += (driftTarget - drift) * 0.00005f;
        lfoPhase += s.modRate / (float) sr;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float modMs = s.modDepth * (2.5f * sinCycle (lfoPhase) + 1.5f * drift);
        const float mod = modMs * 0.001f * (float) sr;
        const float dl = clamp (curTime + mod, 2.0f, (float) line[0].size() - 8.0f);
        const float dr = clamp (curTime - mod * 0.7f, 2.0f, (float) line[0].size() - 8.0f);
        float yl = line[0].read (dl);
        float yr = line[1].read (dr);

        // feedback path filtering
        float fl = hc[0].lp (lc[0].hp (yl));
        float fr = hc[1].lp (lc[1].hp (yr));
        const float fb = s.feedback;
        float inL, inR;
        if (s.pingPong)
        {
            inL = 0.5f * (l + r) + fb * fr;
            inR = fb * fl;
        }
        else
        {
            inL = l + fb * fl;
            inR = r + fb * fr;
        }
        line[0].push (safeLimit (sanitize (inL), 0.8f, 1.6f));
        line[1].push (safeLimit (sanitize (inR), 0.8f, 1.6f));

        // width on the wet signal
        const float m = 0.5f * (yl + yr), sd = 0.5f * (yl - yr) * s.width;
        yl = m + sd;
        yr = m - sd;
        float dry, wet;
        mixLaw (s.mix, dry, wet);
        l = l * dry + yl * wet;
        r = r * dry + yr * wet;
    }

private:
    double sr = 48000.0;
    Settings s;
    DelayLine line[2];
    Svf lc[2], hc[2];
    float lastLc = -1, lastHc = -1;
    float curTime = -1.0f, timeCoef = 0.999f;
    float lfoPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    Rng rng;
};
} // namespace synthia::dsp
