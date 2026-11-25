#pragma once
// Sustain compressor for the input stage (one-knob "Sustain" + attack/release).
// Feed-forward, stereo-linked peak detector smoothed in the dB domain, soft knee,
// automatic make-up so a typical DI level (-14 dBFS) passes at about unity.
// A soft limiter catches transients that pass during the attack time.

#include "dsp/Common.h"

namespace synthia::dsp
{
namespace comp_tuning
{
constexpr float kThrMax = -8.0f;    // threshold at Sustain 0 (dBFS)
constexpr float kThrRange = 40.0f;  // threshold at Sustain 1 = -48 dBFS
constexpr float kRatioMin = 1.5f, kRatioRange = 18.5f;
constexpr float kKneeDb = 8.0f;
constexpr float kMakeupRefDb = -14.0f; // input level that passes at ~unity
constexpr float kMaxMakeupDb = 30.0f;
} // namespace comp_tuning

class SustainCompressor
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { envDb = -120.0f; grDb = 0.0f; }

    void setChunk (float amount, float atkMs, float relMs) noexcept
    {
        using namespace comp_tuning;
        thr = kThrMax - kThrRange * amount;
        ratio = kRatioMin + kRatioRange * amount * std::sqrt (amount);
        atk = onePoleCoef (atkMs, sr);
        rel = onePoleCoef (relMs, sr);
        makeupDb = std::min (kMaxMakeupDb, reduction (kMakeupRefDb));
    }

    void process (float& l, float& r) noexcept
    {
        const float pk = std::max (std::abs (l), std::abs (r));
        const float db = 20.0f * std::log10 (pk + 1.0e-9f);
        envDb = db + (envDb - db) * (db > envDb ? atk : rel);
        grDb = reduction (envDb);
        const float g = dbToGain (makeupDb - grDb);
        l = safeLimit (l * g, 0.9f, 1.2f);
        r = safeLimit (r * g, 0.9f, 1.2f);
    }

    /** Current gain reduction in dB (>= 0, without make-up). */
    float gainReductionDb() const noexcept { return grDb; }

private:
    float reduction (float inDb) const noexcept
    {
        using namespace comp_tuning;
        const float over = inDb - thr;
        const float slope = 1.0f - 1.0f / ratio;
        if (over <= -kKneeDb * 0.5f) return 0.0f;
        if (over >= kKneeDb * 0.5f) return over * slope;
        const float x = over + kKneeDb * 0.5f;
        return slope * x * x / (2.0f * kKneeDb);
    }

    double sr = 48000.0;
    float thr = -28.0f, ratio = 4.0f, atk = 0.9f, rel = 0.999f, makeupDb = 0.0f;
    float envDb = -120.0f, grDb = 0.0f;
};
} // namespace synthia::dsp
