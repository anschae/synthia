#pragma once
// Input stage: gain -> HPF -> noise gate, plus the two analysis taps:
//   * envelope follower (attack/release)          -> mod source "Env Follower"
//   * pick/onset detector (HF transient detector)  -> trigger + "Pick Env"
// The onset detector only answers "something was just plucked" -- it never
// estimates pitch (hard rule of the project).

#include "dsp/Common.h"

namespace synthia::dsp
{
class InputStage
{
public:
    struct Settings
    {
        float gainDb = 0, hpfHz = 30;
        bool gateOn = true;
        float gateThrDb = -62, gateRelMs = 120;
    };
    struct AnalysisSettings
    {
        float envAtkMs = 5, envRelMs = 150;
        float pickSens = 0.6f, pickHoldMs = 60, pickDecayMs = 300;
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        for (auto& f : hpL) f.reset();
        for (auto& f : hpR) f.reset();
        onsetHp.setup (900.0f, sr);
        gateDetRel = onePoleCoef (60.0f, sr);
        gateAtk = onePoleCoef (0.8f, sr);
        fastAtk = onePoleCoef (0.3f, sr);
        fastRel = onePoleCoef (12.0f, sr);
        slowAtk = onePoleCoef (30.0f, sr);
        slowRel = onePoleCoef (80.0f, sr);
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : hpL) f.reset();
        for (auto& f : hpR) f.reset();
        onsetHp.reset();
        gateEnv = gateGain = 0.0f;
        gateOpen = false;
        env = fastEnv = slowEnv = pickEnv = 0.0f;
        holdCounter = 0;
        gainPrev = gainCur = 1.0f;
        peakIn = 0.0f;
    }

    /** Called once per chunk with the (smoothed) parameters. */
    void setChunk (const Settings& s, int n) noexcept
    {
        gainPrev = gainCur;
        gainCur = dbToGain (s.gainDb);
        chunkLen = n;
        if (s.hpfHz != lastHpf)
        {
            lastHpf = s.hpfHz;
            // 24 dB/oct Butterworth: two SVFs with Q 0.541 and 1.307
            hpL[0].setup (s.hpfHz, 0.5412f, sr); hpL[1].setup (s.hpfHz, 1.3066f, sr);
            hpR[0].setup (s.hpfHz, 0.5412f, sr); hpR[1].setup (s.hpfHz, 1.3066f, sr);
        }
        gateOn = s.gateOn;
        gateThr = dbToGain (s.gateThrDb);
        gateRel = onePoleCoef (s.gateRelMs * 0.4f, sr);
    }

    void setAnalysis (const AnalysisSettings& a) noexcept
    {
        envAtk = onePoleCoef (a.envAtkMs, sr);
        envRel = onePoleCoef (a.envRelMs, sr);
        // sensitivity 0..1 -> transient ratio 14 dB .. 3 dB and floor -36 .. -66 dBFS
        onsetRatio = dbToGain (14.0f - 11.0f * a.pickSens);
        onsetFloor = dbToGain (-36.0f - 30.0f * a.pickSens);
        holdSamples = (int) (a.pickHoldMs * 0.001f * (float) sr);
        pickDecay = onePoleCoef (a.pickDecayMs, sr);
    }

    /** Gain, HPF, gate on one stereo sample (in place). */
    void processSample (float& l, float& r, int s) noexcept
    {
        const float g = gainPrev + (gainCur - gainPrev) * (float) (s + 1) / (float) chunkLen;
        l *= g;
        r *= g;
        l = hpL[1].hp (hpL[0].hp (l));
        r = hpR[1].hp (hpR[0].hp (r));
        const float a = std::max (std::abs (l), std::abs (r));
        peakIn = std::max (peakIn, a);
        if (gateOn)
        {
            gateEnv = a > gateEnv ? a : gateEnv * gateDetRel + a * (1.0f - gateDetRel);
            if (gateOpen) { if (gateEnv < gateThr * 0.5f) gateOpen = false; }
            else if (gateEnv > gateThr) gateOpen = true;
            const float t = gateOpen ? 1.0f : 0.0f;
            const float c = t > gateGain ? gateAtk : gateRel;
            gateGain = t + (gateGain - t) * c;
            l *= gateGain;
            r *= gateGain;
        }
    }

    /** Envelope follower + onset detector on the mono analysis signal. Returns true on a pick. */
    bool analyse (float x) noexcept
    {
        const float a = std::abs (x);
        env = a + (env - a) * (a > env ? envAtk : envRel);

        const float h = std::abs (onsetHp.hp (x));
        fastEnv = h + (fastEnv - h) * (h > fastEnv ? fastAtk : fastRel);
        slowEnv = fastEnv + (slowEnv - fastEnv) * (fastEnv > slowEnv ? slowAtk : slowRel);
        bool picked = false;
        if (holdCounter > 0)
            --holdCounter;
        else if (fastEnv > onsetFloor && fastEnv > slowEnv * onsetRatio)
        {
            picked = true;
            holdCounter = holdSamples;
            pickEnv = 1.0f;
        }
        pickEnv *= pickDecay;
        return picked;
    }

    /** Envelope follower as 0..1 (-60 dBFS .. 0 dBFS). */
    float envNorm() const noexcept { return clamp ((gainToDb (env) + 60.0f) / 60.0f, 0.0f, 1.0f); }
    float envLinear() const noexcept { return env; }
    float pickEnvelope() const noexcept { return pickEnv; }

    /** Peak since the last call (for the input meter). */
    float takePeak() noexcept
    {
        const float p = peakIn;
        peakIn = 0.0f;
        return p;
    }

private:
    double sr = 48000.0;
    Svf hpL[2], hpR[2];
    OnePole onsetHp;
    float lastHpf = -1.0f;
    float gainPrev = 1, gainCur = 1;
    int chunkLen = 1;
    bool gateOn = true, gateOpen = false;
    float gateThr = 0.001f, gateEnv = 0, gateGain = 0, gateDetRel = 0.99f, gateAtk = 0.9f, gateRel = 0.99f;
    float env = 0, envAtk = 0.9f, envRel = 0.999f;
    float fastEnv = 0, slowEnv = 0, fastAtk = 0, fastRel = 0, slowAtk = 0, slowRel = 0;
    float onsetRatio = 2.0f, onsetFloor = 0.001f;
    int holdSamples = 2000, holdCounter = 0;
    float pickEnv = 0, pickDecay = 0.999f;
    float peakIn = 0;
};
} // namespace synthia::dsp
