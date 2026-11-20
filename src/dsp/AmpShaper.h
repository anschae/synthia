#pragma once
// Amp shaper (VCA on the layers routed to the filter):
//   * Pick ADSR -- triggered by the pick detector. Sustain is held while the
//     string keeps ringing (input envelope above 25 % of the level at the
//     pick), then the release starts. No note-off needed, no pitch involved.
//   * Swell -- every pick restarts a slow S-shaped volume ramp.
//   * Chopper -- 16-step tempo-synced gate with smoothed edges.

#include "dsp/Common.h"

namespace synthia::dsp
{
class AmpShaper
{
public:
    struct Settings
    {
        float adsrAmt = 0, a = 5, d = 300, s = 0.6f, r = 250;
        bool swellOn = false;
        float swellMs = 400;
        bool chopOn = false;
        float chopDepth = 1, chopSmoothMs = 3;
        double stepQuarters = 0.25;
        uint32_t steps = 0xFFFF; // bit i = step i open
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        dipStep = 1.0f / (0.003f * (float) sr);
        reset();
    }

    void reset() noexcept
    {
        stage = idle;
        level = 0.0f;
        peakAtPick = 0.0f;
        swell = 1.0f;
        swellPhase = 2;
        chopGain = 1.0f;
        freeQuarters = 0.0;
    }

    void setChunk (const Settings& st) noexcept
    {
        s = st;
        atkStep = 1.0f / std::max (1.0f, s.a * 0.001f * (float) sr);
        decCoef = onePoleCoef (s.d / 3.0f, sr);
        relCoef = onePoleCoef (s.r / 3.0f, sr);
        swellStep = 1.0f / std::max (1.0f, s.swellMs * 0.001f * (float) sr);
        chopCoef = onePoleCoef (s.chopSmoothMs, sr);
    }

    /** Gain for one sample. `quarters`: musical position of this sample. */
    float next (bool picked, float envLinear, double quarters) noexcept
    {
        // ---- ADSR
        if (picked)
        {
            stage = attack;
            peakAtPick = envLinear;
            if (swellPhase == 2 || swellPhase == 1)
                swellPhase = 0; // dip, then rise
        }
        switch (stage)
        {
            case attack:
                level += atkStep;
                peakAtPick = std::max (peakAtPick, envLinear);
                if (level >= 1.0f) { level = 1.0f; stage = decay; }
                break;
            case decay:
                level = s.s + (level - s.s) * decCoef;
                peakAtPick = std::max (peakAtPick, envLinear);
                if (level - s.s < 0.001f) stage = sustain;
                if (envLinear < peakAtPick * 0.25f) stage = release;
                break;
            case sustain:
                level = s.s;
                if (envLinear < peakAtPick * 0.25f) stage = release;
                break;
            case release:
                level *= relCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = idle; }
                break;
            case idle:
            default:
                level = 0.0f;
                break;
        }
        float g = 1.0f - s.adsrAmt + s.adsrAmt * level;

        // ---- swell
        if (s.swellOn)
        {
            if (swellPhase == 0)
            {
                swell -= dipStep;
                if (swell <= 0.0f) { swell = 0.0f; swellPhase = 1; swellRamp = 0.0f; }
            }
            else if (swellPhase == 1)
            {
                swellRamp = std::min (1.0f, swellRamp + swellStep);
                swell = swellRamp * swellRamp * (3.0f - 2.0f * swellRamp);
                if (swellRamp >= 1.0f) swellPhase = 2;
            }
            g *= swell;
        }
        else
        {
            swell = 1.0f;
            swellPhase = 2;
        }

        // ---- chopper
        if (s.chopOn)
        {
            const double stepPos = quarters / s.stepQuarters;
            auto idx = (int64_t) std::floor (stepPos) % 16;
            if (idx < 0) idx += 16;
            const float open = ((s.steps >> idx) & 1u) ? 1.0f : 0.0f;
            const float t = 1.0f - s.chopDepth * (1.0f - open);
            chopGain = t + (chopGain - t) * chopCoef;
            g *= chopGain;
        }
        else
            chopGain = 1.0f;
        return g;
    }

    float adsrLevel() const noexcept { return level; }
    double freeQuarters = 0.0;

private:
    enum Stage { idle, attack, decay, sustain, release };
    double sr = 48000.0;
    Settings s;
    Stage stage = idle;
    float level = 0, peakAtPick = 0;
    float atkStep = 0.01f, decCoef = 0.999f, relCoef = 0.999f;
    float swell = 1, swellRamp = 0, swellStep = 0.001f, dipStep = 0.01f;
    int swellPhase = 2;
    float chopGain = 1, chopCoef = 0.9f;
};
} // namespace synthia::dsp
