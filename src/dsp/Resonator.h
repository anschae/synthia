#pragma once
// Layer C -- RESONATOR / VOCODER / FORMANT. The guitar drives something tuned.
// Tuning comes from 4 chord slots (root, type, octave, spread); the active slot
// is a (modulatable) parameter, so e.g. the step sequencer can switch chords.
// Chords are *set by the user* -- nothing is detected from the input.
//   Resonator: up to 8 tuned resonators. String = comb (Karplus-Strong style)
//              with damping + dispersion allpass; Metal / Glass = modal banks;
//              Bowed = string excited by noise shaped by the input envelope.
//   Vocoder:   16-24 band channel vocoder, modulator = guitar, carrier =
//              internal chord oscillators (saw / square / supersaw) + noise on picks.
//   Formant:   A-E-I-O-U vowel filter morph.

#include "dsp/Common.h"
#include "dsp/Formant.h"
#include "dsp/Layer.h"
#include "dsp/VizData.h"

namespace synthia::dsp
{
namespace reso_tuning
{
constexpr int kMaxVoices = 8;
constexpr int kModes = 4;
constexpr float kMetalRatios[kModes] = { 1.0f, 2.756f, 5.404f, 8.933f };
constexpr float kGlassRatios[kModes] = { 1.0f, 2.32f, 4.25f, 6.63f };
constexpr float kExciteGain = 0.35f;
constexpr float kVocoderGain = 20.0f;
constexpr float kVocLo = 120.0f, kVocHi = 7500.0f;
} // namespace reso_tuning

/** Chord slot -> list of MIDI note numbers (up to 8). */
inline int chordNotes (int root, int type, int octave, float spread, float* out) noexcept
{
    static const std::vector<std::vector<int>> types = {
        { 0, 7, 12 },          // Power
        { 0, 4, 7 },           // Major
        { 0, 3, 7 },           // Minor
        { 0, 2, 7 },           // Sus2
        { 0, 5, 7 },           // Sus4
        { 0, 4, 7, 11 },       // Maj7
        { 0, 3, 7, 10 },       // Min7
        { 0, 4, 7, 10 },       // Dom7
        { 0, 4, 7, 11, 14 },   // Maj9
        { 0, 3, 7, 10, 14 },   // Min9
        { 0, 4, 7, 14 },       // Add9
        { 0, 2, 3, 5 },        // Cluster
        { 0, 12, 24 },         // Octaves
        { 0, 7, 14, 21 },      // Fifths
    };
    const auto& iv = types[(size_t) juce::jlimit (0, (int) types.size() - 1, type)];
    const int base = (octave + 1) * 12 + root;
    const int count = std::min (reso_tuning::kMaxVoices, (int) iv.size() + 3);
    for (int i = 0; i < count; ++i)
    {
        const int n = (int) iv.size();
        float semis = (float) (iv[(size_t) (i % n)] + 12 * (i / n));
        // spread: open the voicing (odd voices up an octave, then every 4th again)
        if (i > 0 && (i & 1) && spread > 0.25f + 0.1f * (float) (i % 3)) semis += 12.0f;
        if (i > 1 && (i % 4) == 3 && spread > 0.75f) semis += 12.0f;
        out[i] = (float) base + semis;
    }
    return count;
}

class ResonatorLayer
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& d : comb) d.prepare ((int) (sr / 20.0) + 16);
        formL.prepare (sr);
        formR.prepare (sr);
        exciteHp.setup (80.0f, sr);
        voiceDecay = onePoleCoef (120.0f, sr);
        glideCoef = onePoleCoef (20.0f, sr);
        rng.seed (0xC0FFEEu);
        reset();
    }

    void reset() noexcept
    {
        for (auto& d : comb) d.clear();
        for (int v = 0; v < reso_tuning::kMaxVoices; ++v)
        {
            combLp[v] = apState[v] = 0.0f;
            curHz[v] = -1.0f;
            for (int m = 0; m < reso_tuning::kModes; ++m) mz1[v][m] = mz2[v][m] = 0.0f;
            for (int o = 0; o < 3; ++o) oscPh[v][o] = rng.uni();
        }
        for (int b = 0; b < 24; ++b)
        {
            for (auto& f : modBp[b]) f.reset();
            for (auto& f : carBp[b]) f.reset();
            vocEnv[b] = 0.0f;
        }
        formL.reset();
        formR.reset();
        exciteHp.reset();
        lastMode = -1;
        fade = 0.0f;
        lastVocBands = -1;
    }

    void process (const float* in, float* outL, float* outR, const ChunkContext& c) noexcept
    {
        const auto& p = c.p;
        const int n = c.n;
        const int mode = p.choice (pid::rsMode);
        if (mode != lastMode)
        {
            if (lastMode >= 0 && fade > 0.0f)
                pendingMode = mode, fadingOut = true;
            else
            {
                lastMode = mode;
                fadingOut = false;
            }
        }

        // ---- chord
        const int slot = juce::jlimit (0, 3, (int) std::lround (p.cur (pid::rsSlot)));
        const int base = pid::c1Root + slot * 4; // cNRoot, cNType, cNOct, cNSpread consecutive
        float notes[reso_tuning::kMaxVoices];
        numVoices = chordNotes (p.choice (base), p.choice (base + 1), (int) p.cur (base + 2), p.cur (base + 3), notes);
        for (int v = 0; v < numVoices; ++v)
        {
            targetHz[v] = std::min (midiToHz (notes[v]), (float) sr * 0.4f);
            if (curHz[v] < 0.0f) curHz[v] = targetHz[v];
        }

        const float fadeStep = 1.0f / (0.008f * (float) sr);
        float tmpL[kChunk], tmpR[kChunk];
        switch (lastMode)
        {
            case 0: processResonator (in, tmpL, tmpR, c); break;
            case 1: processVocoder (in, tmpL, tmpR, c); break;
            default: processFormant (in, tmpL, tmpR, c); break;
        }
        for (int s = 0; s < n; ++s)
        {
            if (fadingOut)
            {
                fade -= fadeStep;
                if (fade <= 0.0f)
                {
                    fade = 0.0f;
                    fadingOut = false;
                    lastMode = pendingMode;
                    softResetModeState();
                }
            }
            else
                fade = std::min (1.0f, fade + fadeStep);
            outL[s] = sanitize (tmpL[s] * fade);
            outR[s] = sanitize (tmpR[s] * fade);
        }
        if (viz != nullptr && viz->active.load (std::memory_order_relaxed))
        {
            viz->rsVoices.store (lastMode == 0 ? numVoices : 0, std::memory_order_relaxed);
            for (int v = 0; v < reso_tuning::kMaxVoices; ++v)
            {
                if (lastMode != 0) voiceLevel[v] = 0.0f;
                viz->rsVoice[(size_t) v].store (voiceLevel[v] * fade, std::memory_order_relaxed);
            }
        }
    }

    void setViz (VizData* v) noexcept { viz = v; }

private:
    void softResetModeState() noexcept
    {
        for (auto& d : comb) d.clear();
        for (int v = 0; v < reso_tuning::kMaxVoices; ++v)
            for (int m = 0; m < reso_tuning::kModes; ++m) mz1[v][m] = mz2[v][m] = 0.0f;
        formL.reset(); formR.reset();
    }

    void glide() noexcept
    {
        for (int v = 0; v < numVoices; ++v)
            curHz[v] = targetHz[v] + (curHz[v] - targetHz[v]) * glideCoef;
    }

    // ------------------------------------------------------------------ resonator
    void processResonator (const float* in, float* outL, float* outR, const ChunkContext& c) noexcept
    {
        using namespace reso_tuning;
        const auto& p = c.p;
        const int mat = p.choice (pid::rsMat);
        const float decay = p.cur (pid::rsDecay);
        const float bright = p.cur (pid::rsBright);
        const float inh = p.cur (pid::rsInharm);
        const float exc = dbToGain (p.cur (pid::rsExcite)) * kExciteGain / std::sqrt ((float) std::max (1, numVoices));
        const bool modal = mat == 1 || mat == 2;
        const bool bowed = mat == 3;
        const float* ratios = mat == 2 ? kGlassRatios : kMetalRatios;
        const float lpFc = 600.0f * std::exp2 (bright * 5.0f); // 600 Hz .. 19 kHz
        const float lpG0 = std::tan (kPi * std::min (lpFc, (float) sr * 0.45f) / (float) sr);
        const float lpG = lpG0 / (1.0f + lpG0);
        const float apA = -0.65f * inh;
        const float apDelay = (1.0f - apA) / (1.0f + apA);

        // modal coefficients (per chunk; frequencies glide slowly)
        float mc[kMaxVoices][kModes], mr2[kMaxVoices][kModes], mg[kMaxVoices][kModes];
        for (int s = 0; s < c.n; ++s) glide();
        float combG[kMaxVoices], combD[kMaxVoices];
        for (int v = 0; v < numVoices; ++v)
        {
            const float f = curHz[v];
            combG[v] = std::pow (10.0f, -3.0f / (f * std::max (0.05f, bowed ? decay * 2.0f : decay)));
            combD[v] = clamp ((float) sr / f - 0.5f - apDelay, 2.0f, (float) comb[v].size() - 8.0f);
        }
        if (modal)
            for (int v = 0; v < numVoices; ++v)
                for (int m = 0; m < kModes; ++m)
                {
                    const float ratio = 1.0f + (ratios[m] - 1.0f) * (0.8f + 0.6f * inh);
                    const float f = std::min (curHz[v] * ratio, (float) sr * 0.45f);
                    const float t60 = decay / (1.0f + (float) m * (1.0f - bright) * 2.5f);
                    const float r = std::exp (-6.9078f / (std::max (0.02f, t60) * (float) sr));
                    mc[v][m] = 2.0f * r * std::cos (kTwoPi * f / (float) sr);
                    mr2[v][m] = r * r;
                    mg[v][m] = (1.0f - r) * (m == 0 ? 1.0f : std::pow (0.35f + 0.5f * bright, (float) m)) * 6.0f;
                }

        for (int s = 0; s < c.n; ++s)
        {
            float x = exciteHp.hp (in[s]) * exc;
            float l = 0.0f, r = 0.0f;
            for (int v = 0; v < numVoices; ++v)
            {
                float y = 0.0f;
                if (modal)
                {
                    for (int m = 0; m < kModes; ++m)
                    {
                        const float o = mc[v][m] * mz1[v][m] - mr2[v][m] * mz2[v][m] + mg[v][m] * x;
                        mz2[v][m] = mz1[v][m];
                        mz1[v][m] = o;
                        y += o;
                    }
                    y *= 0.5f;
                }
                else
                {
                    const float g = combG[v];
                    float d = comb[v].read (combD[v]);
                    // damping lowpass
                    const float a = (d - combLp[v]) * lpG;
                    const float lpOut = a + combLp[v];
                    combLp[v] = lpOut + a;
                    // dispersion allpass (inharmonicity)
                    const float apOut = apA * lpOut + apState[v];
                    apState[v] = lpOut - apA * apOut;
                    float e = x;
                    if (bowed)
                        e += rng.bi() * c.envLinear * 0.6f * exc / kExciteGain;
                    comb[v].push (safeLimit (e + g * apOut, 1.0f, 2.0f));
                    y = apOut;
                }
                voiceLevel[v] = std::max (std::abs (y), voiceLevel[v] * voiceDecay);
                const float pan = (v & 1) ? 0.6f : -0.6f;
                l += y * (1.0f - std::max (0.0f, pan));
                r += y * (1.0f + std::min (0.0f, pan));
            }
            outL[s] = l;
            outR[s] = r;
        }
    }

    // ------------------------------------------------------------------ vocoder
    void setupVocoder (int bands, float shiftSemis) noexcept
    {
        using namespace reso_tuning;
        lastVocBands = bands;
        lastVocShift = shiftSemis;
        vocBands = bands;
        const float spacing = std::pow (kVocHi / kVocLo, 1.0f / (float) (bands - 1));
        const float q = 1.0f / (spacing - 1.0f) * 1.2f;
        const float shift = std::exp2 (shiftSemis / 12.0f);
        for (int b = 0; b < bands; ++b)
        {
            const float f = kVocLo * std::pow (spacing, (float) b);
            for (auto& flt : modBp[b]) flt.setup (f, q, sr);
            for (auto& flt : carBp[b]) flt.setup (std::min (f * shift, (float) sr * 0.45f), q, sr);
        }
    }

    void processVocoder (const float* in, float* outL, float* outR, const ChunkContext& c) noexcept
    {
        using namespace reso_tuning;
        const auto& p = c.p;
        static constexpr int bandCounts[] = { 16, 20, 24 };
        const int bands = bandCounts[juce::jlimit (0, 2, p.choice (pid::vcoBands))];
        const float shiftSemis = p.cur (pid::vcoFormant);
        if (bands != lastVocBands || std::abs (shiftSemis - lastVocShift) > 0.01f)
            setupVocoder (bands, shiftSemis);
        const int wave = p.choice (pid::vcoWave);
        const float detune = p.cur (pid::vcoDetune);
        const float noise = p.cur (pid::vcoNoise);
        const float relC = onePoleCoef (p.cur (pid::vcoRelease), sr);
        const float atkC = onePoleCoef (2.0f, sr);
        const float inGain = dbToGain (p.cur (pid::rsExcite));
        const int voices = std::min (numVoices, 6);
        const float oscNorm = 1.0f / std::sqrt ((float) std::max (1, voices) * (wave == 2 ? 3.0f : 1.0f));
        for (int s = 0; s < c.n; ++s)
        {
            glide();
            // carrier
            float car = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                const int nOsc = wave == 2 ? 3 : 1;
                for (int o = 0; o < nOsc; ++o)
                {
                    const float det = wave == 2 ? (float) (o - 1) * detune * 0.012f : (o == 0 ? detune * 0.002f * (float) (v - 2) : 0.0f);
                    const float dt = curHz[v] * (1.0f + det) / (float) sr;
                    float& ph = oscPh[v][o];
                    float t = ph;
                    float y;
                    if (wave == 1)
                    {
                        float t2 = t + 0.5f; t2 -= std::floor (t2);
                        y = (t < 0.5f ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt);
                    }
                    else
                        y = 2.0f * t - 1.0f - blep (t, dt);
                    car += y;
                    ph += dt;
                    ph -= std::floor (ph);
                }
            }
            car = car * oscNorm + rng.bi() * noise * (0.1f + 0.9f * c.pickEnv);
            const float x = in[s] * inGain;
            float l = 0.0f, r = 0.0f;
            for (int b = 0; b < vocBands; ++b)
            {
                const float m = modBp[b][1].bp (modBp[b][0].bp (x) * modBp[b][0].k) * modBp[b][1].k;
                const float a = std::abs (m);
                vocEnv[b] = a + (vocEnv[b] - a) * (a > vocEnv[b] ? atkC : relC);
                const float cb = carBp[b][1].bp (carBp[b][0].bp (car) * carBp[b][0].k) * carBp[b][1].k;
                const float y = cb * vocEnv[b];
                if (b & 1) { l += y * 0.7f; r += y; }
                else { l += y; r += y * 0.7f; }
            }
            outL[s] = l * kVocoderGain;
            outR[s] = r * kVocoderGain;
        }
    }

    static float blep (float t, float dt) noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // ------------------------------------------------------------------ formant
    void processFormant (const float* in, float* outL, float* outR, const ChunkContext& c) noexcept
    {
        const auto& p = c.p;
        const float vowel = p.cur (pid::fmVowel);
        const float res = p.cur (pid::fmReso);
        const float shift = p.cur (pid::fmShift);
        formL.setVowel (vowel - 0.12f, shift, res);
        formR.setVowel (vowel + 0.12f, shift, res);
        const float g = dbToGain (p.cur (pid::rsExcite));
        for (int s = 0; s < c.n; ++s)
        {
            const float x = in[s] * g;
            outL[s] = formL.process (x);
            outR[s] = formR.process (x);
        }
    }

    double sr = 48000.0;
    int numVoices = 3;
    float targetHz[reso_tuning::kMaxVoices] {}, curHz[reso_tuning::kMaxVoices] {};
    float glideCoef = 0.99f;
    DelayLine comb[reso_tuning::kMaxVoices];
    float combLp[reso_tuning::kMaxVoices] {}, apState[reso_tuning::kMaxVoices] {};
    float mz1[reso_tuning::kMaxVoices][reso_tuning::kModes] {}, mz2[reso_tuning::kMaxVoices][reso_tuning::kModes] {};
    float oscPh[reso_tuning::kMaxVoices][3] {};
    Svf modBp[24][2], carBp[24][2];
    float vocEnv[24] {};
    int vocBands = 20, lastVocBands = -1;
    float lastVocShift = 0.0f;
    FormantFilter formL, formR;
    OnePole exciteHp;
    int lastMode = -1, pendingMode = 0;
    bool fadingOut = false;
    float fade = 0.0f;
    Rng rng;
    VizData* viz = nullptr;
    float voiceLevel[reso_tuning::kMaxVoices] {};
    float voiceDecay = 0.999f;
};
} // namespace synthia::dsp
