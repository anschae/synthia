#pragma once
// Small, header-only DSP helpers shared by all modules. Everything here is
// real-time safe (no allocation, no locks).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace synthia::dsp
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

/** Max chunk length: parameters/modulation are updated once per chunk. */
constexpr int kChunk = 32;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clamp (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

/** One-pole smoothing coefficient: y += (1-c)(x-y) per sample reaches 63 % after `ms`. */
inline float onePoleCoef (float ms, double sr) noexcept
{
    return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr));
}

/** Rational tanh approximation (accurate to ~0.1 %, saturates at +-1). */
inline float fastTanh (float x) noexcept
{
    x = clamp (x, -4.97f, 4.97f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Transparent below `knee`, smoothly limits to +-ceiling above. Used in every feedback loop. */
inline float safeLimit (float x, float knee = 0.9f, float ceiling = 1.5f) noexcept
{
    const float a = std::abs (x);
    if (a <= knee)
        return x;
    const float range = ceiling - knee;
    const float y = knee + range * fastTanh ((a - knee) / range);
    return x < 0.0f ? -y : y;
}

/** Replaces NaN/Inf with 0 (last line of defence on outputs and in loops). */
inline float sanitize (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }

/** Polynomial sine for t in cycles (any value; max error ~4e-6). */
inline float sinCycle (float t) noexcept
{
    t -= std::floor (t + 0.5f); // [-0.5, 0.5)
    float x = t * kTwoPi;
    if (x > 1.57079633f) x = kPi - x;
    else if (x < -1.57079633f) x = -kPi - x;
    const float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333333f + x2 * (-0.00019841270f + x2 * 2.7557319e-6f))));
}

/** atan2 with ~1e-5 rad max error. */
inline float fastAtan2 (float y, float x) noexcept
{
    const float ax = std::abs (x), ay = std::abs (y);
    const float mx = std::max (ax, ay);
    if (mx < 1.0e-30f)
        return 0.0f;
    const float a = std::min (ax, ay) / mx;
    const float s = a * a;
    float r = a * (0.99997726f + s * (-0.33262347f + s * (0.19354346f + s * (-0.11643287f + s * (0.05265332f + s * -0.01172120f)))));
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0.0f) r = 3.14159274f - r;
    if (y < 0.0f) r = -r;
    return r;
}

/** xorshift32 RNG. */
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    void seed (uint32_t v) noexcept { s = v ? v : 0x9E3779B9u; }
    uint32_t next() noexcept
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float bi() noexcept { return uni() * 2.0f - 1.0f; }                          // [-1,1)
};

/** 4-point Hermite interpolation. */
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

/** Power-of-two circular buffer with fractional reads ("delay samples behind the last write"). */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 8)
            n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }
    int size() const noexcept { return mask + 1; }
    void push (float x) noexcept
    {
        w = (w + 1) & mask;
        buf[(size_t) w] = x;
    }
    /** delay 0 = the sample just pushed. */
    float at (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }
    float read (float d) const noexcept
    {
        d = std::max (d, 1.0f);
        const int i = (int) d;
        const float t = d - (float) i;
        return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t);
    }
    float readLinear (float d) const noexcept
    {
        const int i = (int) d;
        const float t = d - (float) i;
        return at (i) + t * (at (i + 1) - at (i));
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

/** TPT state-variable filter (Simper/Zavalishin). */
struct Svf
{
    float ic1 = 0, ic2 = 0;
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void reset() noexcept { ic1 = ic2 = 0; }
    void setup (float fc, float q, double sr) noexcept
    {
        fc = clamp (fc, 5.0f, (float) sr * 0.49f);
        g = std::tan (kPi * fc / (float) sr);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    struct Out { float lp, bp, hp; };
    Out process (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }
    float lp (float x) noexcept { return process (x).lp; }
    float hp (float x) noexcept { return process (x).hp; }
    float bp (float x) noexcept { return process (x).bp; }
};

/** One-pole lowpass (TPT). */
struct OnePole
{
    float s = 0, G = 0;
    void reset() noexcept { s = 0; }
    void setup (float fc, double sr) noexcept
    {
        const float g = std::tan (kPi * clamp (fc, 1.0f, (float) sr * 0.49f) / (float) sr);
        G = g / (1.0f + g);
    }
    float lp (float x) noexcept
    {
        const float v = (x - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }
    float hp (float x) noexcept { return x - lp (x); }
};

/** DC blocker (one-pole highpass ~5 Hz). */
struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void setup (double sr) noexcept { r = (float) std::exp (-kTwoPi * 5.0 / sr); }
    void reset() noexcept { x1 = y1 = 0; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

/** Linear ramp towards a target with a fixed slew time (used for on/off fades). */
struct Fader
{
    float value = 0, target = 0, step = 0.001f;
    void setup (float ms, double sr) noexcept { step = 1.0f / std::max (1.0f, ms * 0.001f * (float) sr); }
    float next() noexcept
    {
        if (value < target) value = std::min (target, value + step);
        else if (value > target) value = std::max (target, value - step);
        return value;
    }
    bool silent() const noexcept { return value <= 0.0f && target <= 0.0f; }
};

/** Balance-style pan for -1..1: unity at the centre, equal-power curve towards the sides. */
inline void panGains (float pan, float& l, float& r) noexcept
{
    const float a = (clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    l = std::min (1.0f, std::cos (a) * 1.41421356f);
    r = std::min (1.0f, std::sin (a) * 1.41421356f);
}

/** Dry/wet law used by delay and reverb: dry stays at unity up to 50 %. */
inline void mixLaw (float mix, float& dry, float& wet) noexcept
{
    dry = std::min (1.0f, 2.0f * (1.0f - mix));
    wet = std::min (1.0f, 2.0f * mix);
}

/** Note frequency from semitones relative to A4 = 440 Hz (used only for chord slots, never for detection). */
inline float midiToHz (float midi) noexcept { return 440.0f * std::exp2 ((midi - 69.0f) / 12.0f); }
} // namespace synthia::dsp
