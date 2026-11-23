#pragma once
// Layer A -- VOICER: polyphonic synth tone from a filter bank (no pitch detection).
//
// The guitar is split into K log-spaced narrow bands by complex two-pole
// resonators (each band output is ~analytic). Per band we take
//   amplitude  a_k  = 2|y_k|
//   phase rate dphi_k = arg(y_k[n] * conj(y_k[n-1]))   (amplitude-weighted, smoothed)
// and drive a free-running phase accumulator with dphi_k. New band-limited
// waveforms (polyBLEP) are synthesised from that phase: main voice, sub
// (half rate), octave up (double rate) and an interval voice (rate * ratio).
// Every band works independently, so chords "just work".
//
// Shaping: Focus (lateral inhibition between neighbouring bands), Tilt,
// Smooth, Detune (per-band drift). Colour: ring (with the sub signal),
// drive + wavefolder (oversampled per Quality), crush, stereo spread.

#include <juce_dsp/juce_dsp.h>
#include "dsp/Common.h"
#include "dsp/Layer.h"
#include "dsp/VizData.h"

namespace synthia::dsp
{
namespace voicer_tuning
{
// Bandwidth of a band relative to the band spacing: Sharpness 0 -> kWideBw, 1 -> kNarrowBw.
constexpr float kWideBw = 2.2f;
constexpr float kNarrowBw = 0.45f;
// Two cascaded one-poles: per-stage bandwidth so that the cascade's -3 dB width matches.
constexpr float kCascadeBw = 1.554f;
// Smoothing of the phase-rate estimate.
constexpr float kPhaseRateMs = 1.5f;
// Output gain (partials caught by more than one band add up).
constexpr float kOutputGain = 2.2f;
// Focus exponent at 100 %.
constexpr float kFocusExp = 6.0f;
// Tilt range across the band range (dB at +-100 %).
constexpr float kTiltDb = 18.0f;
} // namespace voicer_tuning

class Voicer
{
public:
    static constexpr int kMaxBands = 72;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        os2 = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os4 = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os2->initProcessing ((size_t) kChunk);
        os4->initProcessing ((size_t) kChunk);
        rng.seed (0xA11CE5u);
        for (int k = 0; k < kMaxBands; ++k)
        {
            hashPan[k] = 0.6f + 0.4f * rng.uni();
            driftCounter[k] = (int) (rng.uni() * 0.5f * (float) sr);
        }
        catk = onePoleCoef (5.0f, sr);
        cz = onePoleCoef (voicer_tuning::kPhaseRateMs, sr);
        reset();
    }

    void reset() noexcept
    {
        for (int k = 0; k < kMaxBands; ++k)
        {
            y1r[k] = y1i[k] = y2r[k] = y2i[k] = 0.0f;
            zsr[k] = 1.0e-9f; zsi[k] = 0.0f;
            amp[k] = 0.0f; w[k] = 1.0f;
            th[k] = rng.uni(); ths[k] = rng.uni(); tho[k] = rng.uni(); thi[k] = rng.uni();
            drift[k] = driftT[k] = 0.0f;
        }
        os2->reset();
        os4->reset();
        carrierEnv = 0.0f;
        crushHeldL = crushHeldR = 0.0f;
        crushPhase = 0.0f;
        lastBands = -1;
        counter4 = 0;
    }

    /** Process one chunk: mono analysis input -> stereo output (overwrites out). */
    void process (const float* in, float* outL, float* outR, const ChunkContext& c)
    {
        const auto& p = c.p;
        const int n = c.n;
        updateBands (p);
        const int K = numBands;

        // ---- per-chunk settings
        const float wave = p.cur (pid::vcWave);
        const float pw = p.cur (pid::vcPw);
        const float lvMain = p.cur (pid::vcMain), lvSub = p.cur (pid::vcSub), lvOct = p.cur (pid::vcOct), lvInt = p.cur (pid::vcInt);
        static constexpr float intervals[] = { -12, -7, -5, 5, 7, 12, 19 };
        const float ratio = std::exp2 (intervals[juce::jlimit (0, 6, p.choice (pid::vcIntRatio))] / 12.0f);
        const float detCents = p.cur (pid::vcDetune);
        const float focus = p.cur (pid::vcFocus);
        const float tilt = p.cur (pid::vcTilt);
        const float smoothMs = p.cur (pid::vcSmooth);
        const float ring = p.cur (pid::vcRing);
        const float spread = p.cur (pid::vcSpread);
        catk = onePoleCoef (smoothMs, sr);
        crel = onePoleCoef (smoothMs * 3.0f, sr);

        if (tilt != lastTilt || spread != lastSpread || K != lastWeightsK)
        {
            lastTilt = tilt; lastSpread = spread; lastWeightsK = K;
            for (int k = 0; k < K; ++k)
            {
                const float pos = K > 1 ? (float) k / (float) (K - 1) - 0.5f : 0.0f;
                tiltW[k] = dbToGain (tilt * pos * voicer_tuning::kTiltDb);
                const float pan = spread * ((k & 1) ? 1.0f : -1.0f) * hashPan[k];
                panGains (pan, panL[k], panR[k]);
            }
        }
        // detune drift (per band slow random walk)
        for (int k = 0; k < K; ++k)
        {
            driftCounter[k] -= n;
            if (driftCounter[k] <= 0)
            {
                driftCounter[k] = (int) ((0.4f + rng.uni()) * (float) sr);
                driftT[k] = rng.bi();
            }
            drift[k] += (driftT[k] - drift[k]) * std::min (1.0f, (float) n / (0.3f * (float) sr));
            detF[k] = std::exp2 (detCents * drift[k] / 1200.0f);
        }

        const bool needSub = lvSub > 0.0001f || ring > 0.0001f;
        float ringBuf[kChunk];

        for (int s = 0; s < n; ++s)
        {
            const float x = in[s];
            // ---- analysis
            for (int k = 0; k < K; ++k)
            {
                const float pr = poleR[k], pi = poleI[k], g = bandG[k];
                const float a1r = pr * y1r[k] - pi * y1i[k] + g * x;
                const float a1i = pr * y1i[k] + pi * y1r[k];
                y1r[k] = a1r; y1i[k] = a1i;
                const float a2r = pr * y2r[k] - pi * y2i[k] + g * a1r;
                const float a2i = pr * y2i[k] + pi * y2r[k] + g * a1i;
                const float zr = a2r * y2r[k] + a2i * y2i[k];
                const float zi = a2i * y2r[k] - a2r * y2i[k];
                y2r[k] = a2r; y2i[k] = a2i;
                zsr[k] = zr + (zsr[k] - zr) * cz;
                zsi[k] = zi + (zsi[k] - zi) * cz;
                const float ar = 2.0f * std::sqrt (a2r * a2r + a2i * a2i);
                amp[k] = ar + (amp[k] - ar) * (ar > amp[k] ? catk : crel);
            }
            if ((counter4++ & 3) == 0)
            {
                for (int k = 0; k < K; ++k)
                {
                    float dphi = fastAtan2 (zsi[k], zsr[k]);
                    if (! (dphi > 1.0e-5f)) dphi = omega[k]; // noise / silence: fall back to the band centre
                    inc[k] = dphi * (1.0f / kTwoPi);
                }
                // focus: lateral inhibition (peaks keep their level, non-peaks are pushed down)
                if (focus > 0.001f)
                {
                    const float e = focus * voicer_tuning::kFocusExp;
                    for (int k = 0; k < K; ++k)
                    {
                        const float m = std::max (k > 0 ? amp[k - 1] : 0.0f, k < K - 1 ? amp[k + 1] : 0.0f);
                        w[k] = (amp[k] >= m || m <= 1.0e-9f) ? 1.0f : std::pow (amp[k] / m, e);
                    }
                }
                else
                    for (int k = 0; k < K; ++k) w[k] = 1.0f;
            }

            // ---- synthesis
            float l = 0.0f, r = 0.0f, sub = 0.0f;
            for (int k = 0; k < K; ++k)
            {
                const float a = amp[k] * w[k] * tiltW[k];
                if (a < 1.0e-7f)
                {
                    advancePhases (k, ratio);
                    continue;
                }
                const float dt = inc[k] * detF[k];
                float v = 0.0f;
                if (lvMain > 0.0001f) v += lvMain * osc (th[k], dt, wave, pw);
                if (needSub)
                {
                    const float ds = dt * 0.5f;
                    if (lvSub > 0.0001f) v += lvSub * osc (ths[k], ds, wave, pw);
                    if (ring > 0.0001f) sub += a * sinCycle (ths[k]);
                }
                if (lvOct > 0.0001f) v += lvOct * osc (tho[k], dt * 2.0f, wave, pw);
                if (lvInt > 0.0001f) v += lvInt * osc (thi[k], dt * ratio, wave, pw);
                advancePhases (k, ratio);
                v *= a;
                l += v * panL[k];
                r += v * panR[k];
            }
            outL[s] = l * voicer_tuning::kOutputGain;
            outR[s] = r * voicer_tuning::kOutputGain;
            ringBuf[s] = sub;
        }

        // ---- ring modulation with the (normalised) sub-octave signal
        if (ring > 0.0001f)
        {
            const float rel = onePoleCoef (80.0f, sr);
            for (int s = 0; s < n; ++s)
            {
                const float a = std::abs (ringBuf[s]);
                carrierEnv = a > carrierEnv ? a : carrierEnv * rel;
                const float cr = ringBuf[s] / std::max (carrierEnv, 1.0e-5f);
                outL[s] = lerp (outL[s], outL[s] * cr, ring);
                outR[s] = lerp (outR[s], outR[s] * cr, ring);
            }
        }

        // ---- drive + fold (oversampled)
        const float driveDb = p.cur (pid::vcDrive);
        const float fold = p.cur (pid::vcFold);
        if (driveDb > 0.01f || fold > 0.001f)
            colour (outL, outR, n, p.choice (pid::quality), driveDb, fold);

        // ---- crush (intentionally lo-fi, base rate)
        const float bits = p.cur (pid::vcBits);
        const float rate = p.cur (pid::vcRate);
        if (bits < 15.95f || rate > 0.001f)
        {
            const float q = std::exp2 (bits - 1.0f);
            const float hold = 1.0f + rate * rate * 47.0f;
            for (int s = 0; s < n; ++s)
            {
                crushPhase += 1.0f;
                if (crushPhase >= hold)
                {
                    crushPhase -= hold;
                    crushHeldL = outL[s];
                    crushHeldR = outR[s];
                }
                float l = rate > 0.001f ? crushHeldL : outL[s];
                float r = rate > 0.001f ? crushHeldR : outR[s];
                if (bits < 15.95f)
                {
                    l = std::round (l * q) / q;
                    r = std::round (r * q) / q;
                }
                outL[s] = l;
                outR[s] = r;
            }
        }
        for (int s = 0; s < n; ++s)
        {
            outL[s] = sanitize (outL[s]);
            outR[s] = sanitize (outR[s]);
        }

        if (viz != nullptr && viz->active.load (std::memory_order_relaxed))
        {
            viz->vcBands.store (K, std::memory_order_relaxed);
            viz->vcLo.store (lastLo, std::memory_order_relaxed);
            viz->vcHi.store (lastHi, std::memory_order_relaxed);
            for (int k = 0; k < K; ++k)
            {
                viz->vcAmp[(size_t) k].store (amp[k], std::memory_order_relaxed);
                viz->vcWeight[(size_t) k].store (w[k], std::memory_order_relaxed);
            }
        }
    }

    void setViz (VizData* v) noexcept { viz = v; }

    int getNumBands() const noexcept { return numBands; }
    float getBandCentreHz (int k) const noexcept { return centreHz[k]; }

private:
    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    static float saw (float t, float dt) noexcept { return 2.0f * t - 1.0f - polyBlep (t, dt); }
    static float pulse (float t, float dt, float d) noexcept
    {
        float t2 = t + 1.0f - d;
        t2 -= std::floor (t2);
        return (t < d ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt) - (2.0f * d - 1.0f);
    }
    /** Waveform morph sine -> saw -> square -> pulse, band-limited; silent above ~0.45 fs. */
    static float osc (float t, float dt, float wave, float pw) noexcept
    {
        if (dt >= 0.45f || dt <= 0.0f)
            return 0.0f;
        const float fade = dt > 0.4f ? (0.45f - dt) * 20.0f : 1.0f;
        float v;
        if (wave < 1.0f)
            v = lerp (sinCycle (t), saw (t, dt), wave);
        else if (wave < 2.0f)
            v = lerp (saw (t, dt), pulse (t, dt, 0.5f), wave - 1.0f);
        else
            v = pulse (t, dt, lerp (0.5f, pw, std::min (1.0f, wave - 2.0f)));
        return v * fade;
    }

    void advancePhases (int k, float ratio) noexcept
    {
        const float dt = inc[k] * detF[k];
        th[k] += dt;          th[k] -= (float) (int) th[k];
        ths[k] += dt * 0.5f;  ths[k] -= (float) (int) ths[k];
        tho[k] += dt * 2.0f;  tho[k] -= (float) (int) tho[k];
        thi[k] += dt * ratio; thi[k] -= (float) (int) thi[k];
    }

    void updateBands (const ParamState& p) noexcept
    {
        static constexpr int counts[] = { 24, 36, 48, 72 };
        const int K = counts[juce::jlimit (0, 3, p.choice (pid::vcBands))];
        const float lo = p.cur (pid::vcLow);
        const float hi = std::min (p.cur (pid::vcHigh), (float) sr * 0.42f);
        const float sharp = p.cur (pid::vcSharp);
        if (K == lastBands && lo == lastLo && hi == lastHi && sharp == lastSharp)
            return;
        if (K != lastBands)
            for (int k = 0; k < kMaxBands; ++k)
                y1r[k] = y1i[k] = y2r[k] = y2i[k] = amp[k] = 0.0f;
        lastBands = K; lastLo = lo; lastHi = hi; lastSharp = sharp;
        numBands = K;
        const float spacing = std::pow (hi / lo, 1.0f / (float) (K - 1));
        const float rel = lerp (voicer_tuning::kWideBw, voicer_tuning::kNarrowBw, sharp);
        for (int k = 0; k < K; ++k)
        {
            const float f = lo * std::pow (spacing, (float) k);
            centreHz[k] = f;
            const float bw = std::max (2.0f, f * (spacing - 1.0f) * rel) * voicer_tuning::kCascadeBw;
            const float rr = std::exp (-kPi * bw / (float) sr);
            omega[k] = kTwoPi * f / (float) sr;
            poleR[k] = rr * std::cos (omega[k]);
            poleI[k] = rr * std::sin (omega[k]);
            bandG[k] = 1.0f - rr;
            if (inc[k] <= 0.0f) inc[k] = omega[k] / kTwoPi;
        }
    }

    void colour (float* l, float* r, int n, int quality, float driveDb, float fold) noexcept
    {
        const float g = dbToGain (driveDb);
        const float comp = 1.0f / std::sqrt (g);
        const float foldGain = 1.0f + fold * 5.0f;
        const float foldMix = std::min (1.0f, fold * 3.0f);
        auto shape = [=] (float x)
        {
            float y = x;
            if (driveDb > 0.01f) y = fastTanh (y * g) * comp * 1.3f;
            if (fold > 0.001f)
            {
                const float f = sinCycle (0.25f * clamp (y * foldGain, -8.0f, 8.0f));
                y = lerp (y, f * 0.8f, foldMix);
            }
            return y;
        };
        float* chans[2] = { l, r };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        if (quality == 0)
        {
            for (int s = 0; s < n; ++s) { l[s] = shape (l[s]); r[s] = shape (r[s]); }
            return;
        }
        auto& os = quality == 1 ? *os2 : *os4;
        auto up = os.processSamplesUp (block);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = shape (d[i]);
        }
        os.processSamplesDown (block);
    }

    double sr = 48000.0;
    int numBands = 48, lastBands = -1;
    float lastLo = -1, lastHi = -1, lastSharp = -1, lastTilt = -99, lastSpread = -1;
    int lastWeightsK = -1;
    float catk = 0.9f, crel = 0.99f, cz = 0.9f;
    // band state (SoA)
    float poleR[kMaxBands] {}, poleI[kMaxBands] {}, bandG[kMaxBands] {}, omega[kMaxBands] {}, centreHz[kMaxBands] {};
    float y1r[kMaxBands] {}, y1i[kMaxBands] {}, y2r[kMaxBands] {}, y2i[kMaxBands] {};
    float zsr[kMaxBands] {}, zsi[kMaxBands] {}, amp[kMaxBands] {}, w[kMaxBands] {}, inc[kMaxBands] {};
    float th[kMaxBands] {}, ths[kMaxBands] {}, tho[kMaxBands] {}, thi[kMaxBands] {};
    float tiltW[kMaxBands] {}, panL[kMaxBands] {}, panR[kMaxBands] {}, hashPan[kMaxBands] {};
    float drift[kMaxBands] {}, driftT[kMaxBands] {}, detF[kMaxBands] {};
    int driftCounter[kMaxBands] {};
    int counter4 = 0;
    float carrierEnv = 0;
    float crushHeldL = 0, crushHeldR = 0, crushPhase = 0;
    Rng rng;
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    VizData* viz = nullptr;
};
} // namespace synthia::dsp
