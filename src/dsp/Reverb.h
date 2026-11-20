#pragma once
// Reverb: 16-line modulated FDN (Hadamard feedback matrix) with an allpass
// input diffuser, pre-delay and five modes:
//   Cosmos  -- huge modulated hall
//   Shimmer -- pitch shifter (interval selectable) feeding back into the tank
//   Abyss   -- downward shimmer, darker tank
//   Bloom   -- long allpass chain before the tank: slowly swelling build-up
//   Cloud   -- large random (granular-like) delay jitter inside the tank
// Stability: every line input passes a soft limiter, the matrix is orthogonal
// and the line gains are <= 1, so nothing can run away (even with Infinite).
// Shimmer anti-chirp: long crossfade windows (90 ms), band-limited shifter
// output (HP 150 Hz / LP 5 kHz) and the loop damping on every pass.

#include "dsp/Common.h"

namespace synthia::dsp
{
namespace reverb_tuning
{
constexpr int kLines = 16;
constexpr float kBaseMs[kLines] = { 31.7f, 37.1f, 41.3f, 43.9f, 47.3f, 53.1f, 59.7f, 61.9f,
                                    67.3f, 71.9f, 73.7f, 79.1f, 83.9f, 89.3f, 97.1f, 103.7f };
constexpr float kSizeMin = 0.35f, kSizeMax = 1.8f;
constexpr float kDiffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
constexpr float kBloomFrac[4] = { 0.11f, 0.17f, 0.23f, 0.31f };
constexpr float kShiftWindowMs = 90.0f;
constexpr float kModMsCosmos = 7.0f, kModMsOther = 2.5f, kModMsCloud = 22.0f;
} // namespace reverb_tuning

/** Two-tap crossfading delay-line pitch shifter. */
class PitchShifter
{
public:
    void prepare (double sr)
    {
        line.prepare ((int) (0.25 * sr));
        window = (float) (reverb_tuning::kShiftWindowMs * 0.001 * sr);
        hp.setup (150.0f, 0.707f, sr);
        lp.setup (5000.0f, 0.707f, sr);
        reset();
    }
    void reset() noexcept { line.clear(); phase = 0.0f; hp.reset(); lp.reset(); }
    void setLowpass (float hz, double sr) noexcept { lp.setup (hz, 0.707f, sr); }
    float process (float x, float ratio) noexcept
    {
        line.push (x);
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        p2 -= std::floor (p2);
        const float y = line.readLinear (2.0f + phase * window) * sinCycle (0.5f * phase)
                      + line.readLinear (2.0f + p2 * window) * sinCycle (0.5f * p2);
        return lp.lp (hp.hp (y));
    }

private:
    DelayLine line;
    float window = 4000.0f, phase = 0.0f;
    Svf hp, lp;
};

class Allpass
{
public:
    void prepare (int maxSamples) { line.prepare (maxSamples); }
    void clear() noexcept { line.clear(); }
    float process (float x, float delaySamples, float g) noexcept
    {
        const float d = line.read (std::max (1.0f, delaySamples));
        const float v = x + g * d;
        line.push (v);
        return d - g * v;
    }

private:
    DelayLine line;
};

class Reverb
{
public:
    enum Mode { cosmos, shimmer, abyss, bloom, cloud };

    struct Settings
    {
        int mode = cosmos;
        float size = 0.7f, decay = 6.0f;
        bool infinite = false;
        float preSamples = 0;
        float diffusion = 0.75f, dampLo = 80, dampHi = 7000;
        float modDepth = 0.4f, modRate = 0.4f;
        float shimmer = 0.5f, shimRatio = 2.0f;
        float bloomSec = 1.0f, duck = 0.0f, width = 1.0f, mix = 0.3f;
        float env = 0.0f; // input envelope (for ducking)
    };

    void prepare (double sampleRate)
    {
        using namespace reverb_tuning;
        sr = sampleRate;
        const int lineMax = (int) ((kBaseMs[kLines - 1] * kSizeMax + kModMsCloud * 1.5f + 5.0f) * 0.001f * (float) sr);
        for (auto& l : lines) l.prepare (lineMax);
        for (auto& p : pre) p.prepare ((int) (2.5 * sr));
        for (int c = 0; c < 2; ++c)
        {
            for (auto& a : diff[c]) a.prepare ((int) (0.03 * sr * kSizeMax));
            for (auto& a : bloomAp[c]) a.prepare ((int) (4.0 * 0.32 * sr) + 8);
        }
        shifter.prepare (sr);
        rng.seed (0xBEEF1234u);
        for (int i = 0; i < kLines; ++i)
        {
            lfoPhase[i] = rng.uni();
            lfoRateMul[i] = 0.7f + 0.6f * rng.uni();
        }
        dc[0].setup (sr); dc[1].setup (sr);
        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lines) l.clear();
        for (auto& p : pre) p.clear();
        for (int c = 0; c < 2; ++c)
        {
            for (auto& a : diff[c]) a.clear();
            for (auto& a : bloomAp[c]) a.clear();
        }
        shifter.reset();
        for (int i = 0; i < reverb_tuning::kLines; ++i)
        {
            lpS[i] = hpS[i] = 0.0f;
            fb[i] = 0.0f;
            jit[i] = jitT[i] = 0.0f;
        }
        shimFb = 0.0f;
        tankGain = 1.0f;
        duckGain = 1.0f;
        inGain = 1.0f;
        dc[0].reset(); dc[1].reset();
        curScale = -1.0f;
    }

    void setChunk (const Settings& st, int n) noexcept
    {
        using namespace reverb_tuning;
        s = st;
        const float scale = lerp (kSizeMin, kSizeMax, s.size);
        if (curScale < 0.0f) curScale = scale;
        targetScale = scale;
        float hi = s.dampHi * (s.mode == abyss ? 0.5f : 1.0f);
        if (s.infinite) hi = 20000.0f;
        const float gHi = std::tan (kPi * clamp (hi, 200.0f, (float) sr * 0.45f) / (float) sr);
        lpG = gHi / (1.0f + gHi);
        const float gLo = std::tan (kPi * clamp (s.infinite ? 5.0f : s.dampLo, 5.0f, 2000.0f) / (float) sr);
        hpG = gLo / (1.0f + gLo);
        for (int i = 0; i < kLines; ++i)
        {
            baseDelay[i] = kBaseMs[i] * 0.001f * (float) sr;
            const float d = baseDelay[i] * curScale;
            gain[i] = s.infinite ? 1.0f : std::pow (10.0f, -3.0f * d / (std::max (0.1f, s.decay) * (float) sr));
        }
        modSamples = (s.mode == cosmos ? kModMsCosmos : (s.mode == cloud ? kModMsCloud : kModMsOther)) * s.modDepth * 0.001f * (float) sr;
        shifter.setLowpass (s.mode == abyss ? 3000.0f : 5000.0f, sr);
        // Cloud: new random jitter targets
        if (s.mode == cloud)
        {
            jitCounter -= n;
            if (jitCounter <= 0)
            {
                jitCounter = (int) (sr / (4.0 + 20.0 * s.modRate));
                const int i = (int) (rng.uni() * kLines) % kLines;
                jitT[i] = rng.bi();
            }
        }
    }

    void processSample (float& l, float& r) noexcept
    {
        using namespace reverb_tuning;
        curScale += (targetScale - curScale) * 0.0002f;
        // ---- input: pre-delay, diffusion, optional bloom chain
        const float inTarget = s.infinite ? 0.0f : 1.0f;
        inGain += (inTarget - inGain) * 0.001f;
        pre[0].push (l);
        pre[1].push (r);
        float x[2] = { pre[0].read (std::max (1.0f, s.preSamples)) * inGain, pre[1].read (std::max (1.0f, s.preSamples)) * inGain };
        const float dg = s.diffusion * 0.72f;
        const float dScale = std::sqrt (curScale);
        for (int c = 0; c < 2; ++c)
        {
            for (int k = 0; k < 4; ++k)
                x[c] = diff[c][k].process (x[c], kDiffMs[k] * dScale * 0.001f * (float) sr * (c ? 1.07f : 1.0f), dg);
            if (s.mode == bloom)
                for (int k = 0; k < 4; ++k)
                    x[c] = bloomAp[c][k].process (x[c], kBloomFrac[k] * s.bloomSec * (float) sr * (c ? 1.05f : 1.0f), 0.62f);
        }

        // ---- read lines (modulated)
        float y[kLines];
        const float rate = s.modRate / (float) sr;
        for (int i = 0; i < kLines; ++i)
        {
            lfoPhase[i] += rate * lfoRateMul[i];
            if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
            float m;
            if (s.mode == cloud)
            {
                jit[i] += (jitT[i] - jit[i]) * 0.0004f;
                m = modSamples * (0.5f + 0.5f * jit[i]) + 0.1f * modSamples * sinCycle (lfoPhase[i]);
            }
            else
                m = modSamples * (0.5f + 0.5f * sinCycle (lfoPhase[i]));
            const float d = std::min (curScale * baseDelay[i] + m + 2.0f, (float) lines[i].size() - 8.0f);
            y[i] = lines[i].read (d);
        }

        // ---- damping + gain
        for (int i = 0; i < kLines; ++i)
        {
            float v = y[i];
            // high damping (LP)
            const float a = (v - lpS[i]) * lpG;
            const float lpOut = a + lpS[i];
            lpS[i] = lpOut + a;
            // low damping (HP)
            const float b = (lpOut - hpS[i]) * hpG;
            const float lowPart = b + hpS[i];
            hpS[i] = lowPart + b;
            fb[i] = (lpOut - lowPart) * gain[i];
        }

        // ---- Hadamard 16 (fast Walsh-Hadamard, orthonormal)
        for (int h = 1; h < kLines; h <<= 1)
            for (int i = 0; i < kLines; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float u = fb[j], v = fb[j + h];
                    fb[j] = u + v;
                    fb[j + h] = u - v;
                }

        // ---- shimmer: pitch-shifted tank output back into the loop
        float shim = 0.0f;
        const bool shimOn = s.shimmer > 0.001f;
        if (shimOn)
        {
            float mono = 0.0f;
            for (int i = 0; i < kLines; ++i) mono += y[i];
            mono *= 0.25f;
            const float ratio = s.mode == abyss ? (s.shimRatio > 1.0f ? 1.0f / s.shimRatio : s.shimRatio) : s.shimRatio;
            shim = shifter.process (mono, ratio);
        }

        // ---- write lines. Shimmer is crossfaded into the loop (energy neutral),
        // and a slow tank-level limiter keeps Infinite + Shimmer from piling up.
        const float k = (shimOn && ! s.infinite) ? s.shimmer * 0.45f : 0.0f; // Infinite = pure freeze
        float tankPeak = 0.0f;
        for (int i = 0; i < kLines; ++i)
        {
            float v = fb[i] * 0.25f * (1.0f - k) * tankGain;
            if (shimOn)
                v += shim * 2.0f * k * ((i & 2) ? -1.0f : 1.0f) * gain[i] * tankGain;
            tankPeak = std::max (tankPeak, std::abs (v));
            v += x[i & 1] * 0.35f;
            lines[i].push (safeLimit (sanitize (v), 1.0f, 2.0f));
        }
        if (tankPeak > 0.7f)
            tankGain = std::max (0.5f, tankGain * 0.9997f);
        else
            tankGain = std::min (1.0f, tankGain + 0.00002f);

        // ---- outputs
        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < kLines; i += 2)
        {
            const float sg = (i & 4) ? -1.0f : 1.0f;
            wl += y[i] * sg;
            wr += y[i + 1] * sg;
        }
        wl = dc[0].process (wl * 0.42f);
        wr = dc[1].process (wr * 0.42f);
        const float m = 0.5f * (wl + wr), sd = 0.5f * (wl - wr) * s.width;
        wl = m + sd;
        wr = m - sd;
        // ducking by the input envelope
        const float duckT = 1.0f - s.duck * clamp (s.env * 1.5f, 0.0f, 1.0f);
        duckGain += (duckT - duckGain) * (duckT < duckGain ? 0.002f : 0.0002f);
        float dry, wet;
        mixLaw (s.mix, dry, wet);
        l = l * dry + wl * wet * duckGain;
        r = r * dry + wr * wet * duckGain;
    }

private:
    double sr = 48000.0;
    Settings s;
    DelayLine lines[reverb_tuning::kLines];
    DelayLine pre[2];
    Allpass diff[2][4], bloomAp[2][4];
    PitchShifter shifter;
    DcBlocker dc[2];
    float gain[reverb_tuning::kLines] {}, baseDelay[reverb_tuning::kLines] {};
    float lpS[reverb_tuning::kLines] {}, hpS[reverb_tuning::kLines] {}, fb[reverb_tuning::kLines] {};
    float lfoPhase[reverb_tuning::kLines] {}, lfoRateMul[reverb_tuning::kLines] {};
    float jit[reverb_tuning::kLines] {}, jitT[reverb_tuning::kLines] {};
    int jitCounter = 0;
    float lpG = 0.5f, hpG = 0.01f, modSamples = 0.0f;
    float curScale = -1.0f, targetScale = 1.0f;
    float shimFb = 0.0f, duckGain = 1.0f, inGain = 1.0f, tankGain = 1.0f;
    Rng rng;
};
} // namespace synthia::dsp
