#pragma once
// Main filter (stereo) with anti-aliased drive. Types: Ladder LP 24 (TPT
// ladder, tanh in the feedback path so self-oscillation stays tame), SVF
// LP/BP/HP/Notch 12 dB, Comb (feedback comb tuned to the cutoff) and Formant
// (cutoff = vowel position). No keytracking (there is no pitch).
//
// The drive uses first-order ADAA (antiderivative anti-aliasing) instead of
// oversampling, so this stage stays latency-free -- the dry guitar can go
// through it without comb-filtering against the global dry signal.

#include "dsp/Common.h"
#include "dsp/Formant.h"

namespace synthia::dsp
{
/** tanh with first-order antiderivative anti-aliasing. */
struct AdaaTanh
{
    float x1 = 0, F1 = 0;
    void reset() noexcept { x1 = 0; F1 = logCosh (0.0f); }
    static float logCosh (float x) noexcept
    {
        const float a = std::abs (x);
        return a + std::log1p (std::exp (-2.0f * a)) - 0.69314718f;
    }
    float process (float x) noexcept
    {
        const float F = logCosh (x);
        const float d = x - x1;
        float y;
        if (std::abs (d) < 1.0e-4f)
            y = std::tanh (0.5f * (x + x1));
        else
            y = (F - F1) / d;
        x1 = x;
        F1 = F;
        return y;
    }
};

class LadderFilter
{
public:
    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0; }
    float process (float x, float g, float k) noexcept
    {
        const float G = g / (1.0f + g);
        const float beta = 1.0f / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = beta * (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]);
        const float y4est = (G4 * x + S) / (1.0f + k * G4);
        // tamed resonance: saturate the feedback
        float u = x - k * fastTanh (y4est);
        u = fastTanh (u * 0.8f) * 1.25f;
        float y = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - s[i]) * G;
            const float o = v + s[i];
            s[i] = o + v;
            y = o;
        }
        return y;
    }

private:
    float s[4] {};
};

class MainFilter
{
public:
    enum Type { ladder, svfLp, svfBp, svfHp, svfNotch, comb, formant };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& d : combLine)
            d.prepare ((int) (sr / 20.0) + 4);
        formantL.prepare (sr);
        formantR.prepare (sr);
        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lad) l.reset();
        for (auto& v : svf) v.reset();
        for (auto& d : combLine) d.clear();
        for (auto& a : drive) a.reset();
        for (auto& d : combDamp) d = 0.0f;
        formantL.reset();
        formantR.reset();
    }

    /** cutoffHz per sample (already including env/pick modulation). */
    void processSample (float& l, float& r, int type, float cutoffHz, float res, float driveDb) noexcept
    {
        if (driveDb > 0.01f)
        {
            const float g = dbToGain (driveDb);
            const float comp = 1.0f / std::sqrt (g);
            l = drive[0].process (l * g) * comp * 1.2f;
            r = drive[1].process (r * g) * comp * 1.2f;
        }
        cutoffHz = clamp (cutoffHz, 20.0f, 20000.0f);
        switch (type)
        {
            case ladder:
            {
                const float g = std::tan (kPi * std::min (cutoffHz, (float) sr * 0.45f) / (float) sr);
                const float k = res * 4.2f;
                const float comp = 1.0f + res * 1.2f; // passband compensation
                l = lad[0].process (l, g, k) * comp;
                r = lad[1].process (r, g, k) * comp;
                break;
            }
            case svfLp: case svfBp: case svfHp: case svfNotch:
            {
                const float q = 0.5f + res * res * 24.0f;
                svf[0].setup (cutoffHz, q, sr);
                svf[1].g = svf[0].g; svf[1].k = svf[0].k; svf[1].a1 = svf[0].a1; svf[1].a2 = svf[0].a2; svf[1].a3 = svf[0].a3;
                auto o1 = svf[0].process (l);
                auto o2 = svf[1].process (r);
                // tame self-oscillation
                svf[0].ic1 = safeLimit (svf[0].ic1, 1.5f, 3.0f);
                svf[1].ic1 = safeLimit (svf[1].ic1, 1.5f, 3.0f);
                if (type == svfLp) { l = o1.lp; r = o2.lp; }
                else if (type == svfBp) { l = o1.bp; r = o2.bp; }
                else if (type == svfHp) { l = o1.hp; r = o2.hp; }
                else { l = o1.lp + o1.hp; r = o2.lp + o2.hp; }
                break;
            }
            case comb:
            {
                const float d = clamp ((float) sr / cutoffHz, 2.0f, (float) combLine[0].size() - 8.0f);
                const float fb = res * 0.97f;
                const float dampC = 0.35f;
                float* ch[2] = { &l, &r };
                for (int c = 0; c < 2; ++c)
                {
                    float y = combLine[c].read (d);
                    y = combDamp[c] = combDamp[c] + (y - combDamp[c]) * (1.0f - dampC);
                    const float in = *ch[c] + fb * safeLimit (y, 1.0f, 2.0f);
                    combLine[c].push (in);
                    *ch[c] = 0.6f * (*ch[c] + y);
                }
                break;
            }
            case formant:
            default:
            {
                // cutoff (log 20..20k) -> vowel position 0..4
                const float pos = 4.0f * std::log (cutoffHz / 20.0f) / std::log (1000.0f);
                if (std::abs (pos - lastVowel) > 0.002f || res != lastRes)
                {
                    lastVowel = pos; lastRes = res;
                    formantL.setVowel (pos, 0.0f, res);
                    formantR.setVowel (pos, 0.0f, res);
                }
                l = formantL.process (l);
                r = formantR.process (r);
                break;
            }
        }
        l = sanitize (l);
        r = sanitize (r);
    }

private:
    double sr = 48000.0;
    LadderFilter lad[2];
    Svf svf[2];
    DelayLine combLine[2];
    float combDamp[2] {};
    AdaaTanh drive[2];
    FormantFilter formantL, formantR;
    float lastVowel = -1, lastRes = -1;
};
} // namespace synthia::dsp
