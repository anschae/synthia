#pragma once
// Modulation: sources (env follower, pick env, 2 LFOs, random, step sequencer,
// 4 macros) evaluated once per chunk, and the 16-slot matrix that turns them
// into normalised offsets per destination parameter.

#include <array>
#include <atomic>
#include "dsp/Common.h"
#include "dsp/ParamState.h"

namespace synthia::dsp
{
/** Transport position for one chunk. */
struct Transport
{
    double bpm = 120.0;
    double ppq = 0.0;            // quarter notes at the chunk start
    double quartersPerBar = 4.0;
    bool playing = false;        // host transport running (else: internal free-running clock)
    double samplesPerQuarter (double sr) const noexcept { return sr * 60.0 / std::max (1.0, bpm); }
};

class Lfo
{
public:
    enum Shape { sine, triangle, sawUp, sawDown, square, sampleHold, smoothRandom };

    void reset (uint32_t seed) noexcept
    {
        rng.seed (seed);
        cycles = 0.0;
        lastCycle = 0;
        a = rng.bi();
        b = rng.bi();
        value = 0.0f;
    }

    /** freeHz: rate in free mode; syncedQuarters > 0: cycle length in quarters (synced mode). */
    float advance (int n, double sr, int shape, double freeHz, double syncedQuarters, float phase, bool retrig,
                   const Transport& t) noexcept
    {
        if (syncedQuarters > 0.0)
        {
            if (t.playing)
                cycles = t.ppq / syncedQuarters;
            else
                cycles += (double) n / (t.samplesPerQuarter (sr) * syncedQuarters);
        }
        else
        {
            if (retrig)
                cycles = 0.0;
            cycles += freeHz * (double) n / sr;
        }
        const double c = cycles + (double) phase;
        const auto ci = (int64_t) std::floor (c);
        if (ci != lastCycle)
        {
            lastCycle = ci;
            a = b;
            b = rng.bi();
        }
        const float p = (float) (c - std::floor (c));
        switch (shape)
        {
            case sine:      value = sinCycle (p); break;
            case triangle:  value = 1.0f - 4.0f * std::abs (p - 0.5f); value = -value; break;
            case sawUp:     value = 2.0f * p - 1.0f; break;
            case sawDown:   value = 1.0f - 2.0f * p; break;
            case square:    value = p < 0.5f ? 1.0f : -1.0f; break;
            case sampleHold: value = b; break;
            case smoothRandom:
            default:
            {
                const float s = 0.5f - 0.5f * std::cos (kPi * p);
                value = a + (b - a) * s;
                break;
            }
        }
        return value;
    }

    float value = 0.0f;

private:
    Rng rng;
    double cycles = 0.0;
    int64_t lastCycle = 0;
    float a = 0, b = 0;
};

class StepSequencer
{
public:
    void reset() noexcept { freePos = 0.0; current = 0; }

    float advance (int n, double sr, int steps, double stepQuarters, bool sync, const float* values, const bool* glides,
                   const Transport& t) noexcept
    {
        steps = std::max (1, std::min (32, steps));
        double pos;
        if (sync && t.playing)
            pos = t.ppq / stepQuarters;
        else
        {
            freePos += (double) n / (t.samplesPerQuarter (sr) * stepQuarters);
            pos = freePos;
        }
        const double fl = std::floor (pos);
        auto idx = (int) ((int64_t) fl % steps);
        if (idx < 0) idx += steps;
        const float frac = (float) (pos - fl);
        current = idx;
        float v = values[idx];
        if (glides[idx])
        {
            const int prevIdx = (idx + steps - 1) % steps;
            const float s = frac * frac * (3.0f - 2.0f * frac);
            v = values[prevIdx] + (v - values[prevIdx]) * s;
        }
        return v;
    }

    int current = 0;

private:
    double freePos = 0.0;
};

class ModEngine
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        lfo1.reset (0x1234567u);
        lfo2.reset (0x89ABCDEu);
        rnd.reset (0x5EED1234u);
        seq.reset();
        src.fill (0.0f);
        offsets.fill (0.0f);
        touchedCount = 0;
    }

    /** Evaluate all sources for this chunk. env / pickEnv are 0..1. */
    void advance (int n, const ParamState& p, const Transport& t, bool picked, float env, float pickEnv) noexcept
    {
        src[modsrc::none] = 0.0f;
        src[modsrc::env] = env;
        src[modsrc::pickEnv] = pickEnv;

        auto lfo = [&] (Lfo& l, int shapeP, int syncP, int rateP, int divP, int phaseP, int retrigP)
        {
            const bool synced = p.on (syncP);
            return l.advance (n, sr, p.choice (shapeP), p.cur (rateP), synced ? syncDivQuarters (p.choice (divP)) : 0.0,
                              p.cur (phaseP), picked && p.on (retrigP), t);
        };
        src[modsrc::lfo1] = lfo (lfo1, pid::lfo1Shape, pid::lfo1Sync, pid::lfo1Rate, pid::lfo1Div, pid::lfo1Phase, pid::lfo1Retrig);
        src[modsrc::lfo2] = lfo (lfo2, pid::lfo2Shape, pid::lfo2Sync, pid::lfo2Rate, pid::lfo2Div, pid::lfo2Phase, pid::lfo2Retrig);

        const bool rsync = p.on (pid::rndSync);
        src[modsrc::random] = rnd.advance (n, sr, p.choice (pid::rndShape) == 0 ? Lfo::sampleHold : Lfo::smoothRandom,
                                           p.cur (pid::rndRate), rsync ? syncDivQuarters (p.choice (pid::rndDiv)) : 0.0, 0.0f,
                                           false, t);

        float values[32];
        bool glides[32];
        for (int i = 0; i < 32; ++i)
        {
            values[i] = p.cur (pid::seqV1 + i);
            glides[i] = p.on (pid::seqG1 + i);
        }
        src[modsrc::stepSeq] = seq.advance (n, sr, (int) p.cur (pid::seqSteps), syncDivQuarters (p.choice (pid::seqDiv)),
                                            p.on (pid::seqSync), values, glides, t);
        for (int m = 0; m < 4; ++m)
            src[(size_t) (modsrc::macro1 + m)] = p.cur (pid::macro1 + m);

        for (int i = 0; i < modsrc::count; ++i)
            uiSource[(size_t) i].store (src[(size_t) i], std::memory_order_relaxed);
        uiStep.store (seq.current, std::memory_order_relaxed);

        computeMatrix (p);
    }

    /** Normalised offsets per parameter index (valid after advance). */
    const float* modOffsets() const noexcept { return offsets.data(); }
    float source (int s) const noexcept { return src[(size_t) s]; }

    static bool isBipolar (int s) noexcept { return s == modsrc::lfo1 || s == modsrc::lfo2 || s == modsrc::random || s == modsrc::stepSeq; }

    // UI read-outs
    std::array<std::atomic<float>, modsrc::count> uiSource {};
    std::atomic<int> uiStep { 0 };

private:
    void computeMatrix (const ParamState& p) noexcept
    {
        for (int i = 0; i < touchedCount; ++i)
            offsets[(size_t) touched[(size_t) i]] = 0.0f;
        touchedCount = 0;
        const auto& dests = modDestinations();
        for (int s = 0; s < 16; ++s)
        {
            const int base = pid::m1Src + s * 4; // m{n}Src, m{n}Dst, m{n}Amt, m{n}Via are consecutive
            const int source = p.choice (base);
            const int dst = p.choice (base + 1);
            const float amt = p.cur (base + 2);
            if (source <= 0 || source >= modsrc::count || dst <= 0 || dst > (int) dests.size() || amt == 0.0f)
                continue;
            float v = src[(size_t) source] * amt;
            const int via = p.choice (base + 3);
            if (via > 0 && via < modsrc::count)
            {
                float vv = src[(size_t) via];
                if (isBipolar (via))
                    vv = 0.5f * (vv + 1.0f);
                v *= vv;
            }
            const int target = dests[(size_t) (dst - 1)];
            if (offsets[(size_t) target] == 0.0f)
                touched[(size_t) touchedCount++] = target;
            offsets[(size_t) target] += v;
            if (offsets[(size_t) target] == 0.0f)
                offsets[(size_t) target] = 1.0e-12f; // keep it in the touched list
        }
    }

    double sr = 48000.0;
    Lfo lfo1, lfo2, rnd;
    StepSequencer seq;
    std::array<float, modsrc::count> src {};
    std::array<float, pid::count> offsets {};
    std::array<int, 16> touched {};
    int touchedCount = 0;
};
} // namespace synthia::dsp
