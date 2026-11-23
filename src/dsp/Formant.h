#pragma once
// Three-formant vowel filter with continuous A-E-I-O-U morph.
// Used by the main filter (type "Formant") and by layer C (mode "Formant").

#include "dsp/Common.h"

namespace synthia::dsp
{
class FormantFilter
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); setVowel (0, 0, 0.5f); }
    void reset() noexcept { for (auto& f : bands) f.reset(); }

    /** Formant frequencies (Hz), linear gains and bandwidths (Hz) for a vowel position. Also used by the UI. */
    static void formants (float pos, float shiftSemis, float res, float* freq, float* gain, float* bandwidth) noexcept
    {
        // Formant frequencies / gains (dB) / bandwidths, roughly after Peterson & Barney
        // (male voice), softened a bit so they work musically on guitar.
        static constexpr float F[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 280, 2250, 2950 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        static constexpr float G[5][3] = { { 0, -4, -18 }, { 0, -9, -14 }, { 0, -8, -13 }, { 0, -5, -17 }, { 0, -9, -19 } };
        static constexpr float B[5][3] = { { 80, 90, 120 }, { 60, 90, 120 }, { 50, 100, 120 }, { 70, 80, 100 }, { 50, 60, 120 } };
        pos = clamp (pos, 0.0f, 4.0f);
        const int i0 = std::min (3, (int) pos);
        const float t = pos - (float) i0;
        const float shift = std::exp2 (shiftSemis / 12.0f);
        const float bwScale = lerp (2.6f, 0.35f, clamp (res, 0.0f, 1.0f));
        for (int k = 0; k < 3; ++k)
        {
            freq[k] = std::exp (lerp (std::log (F[i0][k]), std::log (F[i0 + 1][k]), t)) * shift;
            gain[k] = dbToGain (lerp (G[i0][k], G[i0 + 1][k], t));
            bandwidth[k] = lerp (B[i0][k], B[i0 + 1][k], t) * bwScale * std::sqrt (shift);
        }
    }

    /** pos 0..4 (A E I O U), shift in semitones, res 0..1. */
    void setVowel (float pos, float shiftSemis, float res) noexcept
    {
        float fr[3], g[3], bw[3];
        formants (pos, shiftSemis, res, fr, g, bw);
        for (int k = 0; k < 3; ++k)
        {
            bands[k].setup (fr[k], std::max (0.5f, fr[k] / bw[k]), sr);
            gains[k] = g[k] * bands[k].k; // k * bp = unity peak
        }
        makeup = 1.0f + res * 1.5f;
    }

    float process (float x) noexcept
    {
        float y = 0.0f;
        for (int k = 0; k < 3; ++k)
            y += bands[k].bp (x) * gains[k];
        return y * makeup;
    }

private:
    double sr = 48000.0;
    Svf bands[3];
    float gains[3] { 1, 1, 1 };
    float makeup = 1.0f;
};
} // namespace synthia::dsp
