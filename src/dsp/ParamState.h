#pragma once
// Audio-thread view of all parameters. Once per chunk (<= 32 samples):
//   raw APVTS value -> normalised target -> one-pole smoothing (normalised
//   domain, so log parameters glide in log space) -> + modulation offset ->
//   effective real value.
// DSP code reads `cur(i)` (value at the end of the chunk) or `interp(i, s, n)`
// for per-sample linear interpolation from the previous chunk's value.
// Bool/choice parameters are read directly (no smoothing, no modulation);
// stepped integer parameters (Kind::I) are modulated but not smoothed.

#include <array>
#include <atomic>
#include "params/Params.h"

namespace synthia::dsp
{
class ParamState
{
public:
    ParamState()
    {
        raw.fill (nullptr);
        for (int i = 0; i < pid::count; ++i)
            if (spec (i).kind == Kind::F || spec (i).kind == Kind::I)
                floatParams.push_back (i);
            else
                discreteParams.push_back (i);
        for (auto& a : uiNorm)
            a.store (-1.0f, std::memory_order_relaxed);
    }

    void attach (int i, std::atomic<float>* p) noexcept { raw[(size_t) i] = p; }

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        coefFull = std::exp (-(double) 32 / (kSmoothSeconds * sr));
        snapToTargets();
    }

    /** Jump to the current raw values (no smoothing), e.g. after prepare or state load. */
    void snapToTargets() noexcept
    {
        for (int i = 0; i < pid::count; ++i)
        {
            const float r = readRaw (i);
            lastRaw[(size_t) i] = r;
            const auto& s = spec (i);
            if (s.kind == Kind::F || s.kind == Kind::I)
            {
                target[(size_t) i] = smooth[(size_t) i] = s.toNorm (r);
                lastMod[(size_t) i] = 0.0f;
                effNorm[(size_t) i] = target[(size_t) i];
                cur_[(size_t) i] = prv[(size_t) i] = s.fromNorm (target[(size_t) i]);
            }
            else
                cur_[(size_t) i] = prv[(size_t) i] = r;
        }
    }

    /** `mod` holds normalised offsets per parameter index (may be nullptr). */
    void beginChunk (int n, const float* mod) noexcept
    {
        const double c = n == 32 ? coefFull : std::exp (-(double) n / (kSmoothSeconds * sr));
        const float a = (float) c;
        for (int i : floatParams)
        {
            const size_t k = (size_t) i;
            const auto& s = kSpecs[k];
            bool dirty = false;
            const float r = readRaw (i);
            if (r != lastRaw[k])
            {
                lastRaw[k] = r;
                target[k] = s.toNorm (r);
                dirty = true;
            }
            prv[k] = cur_[k];
            if (smooth[k] != target[k])
            {
                if (s.kind == Kind::I)
                    smooth[k] = target[k];
                else
                {
                    smooth[k] = target[k] + (smooth[k] - target[k]) * a;
                    if (std::abs (smooth[k] - target[k]) < 1.0e-6f)
                        smooth[k] = target[k];
                }
                dirty = true;
            }
            const float m = mod != nullptr ? mod[k] : 0.0f;
            if (m != lastMod[k])
            {
                lastMod[k] = m;
                dirty = true;
            }
            if (dirty)
            {
                const float nrm = std::min (1.0f, std::max (0.0f, smooth[k] + m));
                effNorm[k] = nrm;
                cur_[k] = s.fromNorm (nrm);
                if (m != 0.0f || uiWritten[k])
                {
                    uiNorm[k].store (m != 0.0f ? nrm : -1.0f, std::memory_order_relaxed);
                    uiWritten[k] = m != 0.0f;
                }
            }
        }
        for (int i : discreteParams)
            prv[(size_t) i] = cur_[(size_t) i] = readRaw (i);
    }

    float cur (int i) const noexcept { return cur_[(size_t) i]; }
    float prev (int i) const noexcept { return prv[(size_t) i]; }
    /** Linear interpolation inside the chunk: sample s of n. */
    float interp (int i, int s, int n) const noexcept
    {
        const float p = prv[(size_t) i], c = cur_[(size_t) i];
        return p + (c - p) * (float) (s + 1) / (float) n;
    }
    bool on (int i) const noexcept { return cur_[(size_t) i] >= 0.5f; }
    int choice (int i) const noexcept { return (int) (cur_[(size_t) i] + 0.5f); }
    float norm (int i) const noexcept { return effNorm[(size_t) i]; }

    /** For the UI: effective normalised value of a modulated parameter, -1 if unmodulated. */
    float uiModulatedNorm (int i) const noexcept { return uiNorm[(size_t) i].load (std::memory_order_relaxed); }

    static constexpr double kSmoothSeconds = 0.02;

private:
    float readRaw (int i) const noexcept
    {
        auto* p = raw[(size_t) i];
        return p != nullptr ? p->load (std::memory_order_relaxed) : spec (i).def;
    }

    double sr = 48000.0, coefFull = 0.9;
    std::array<std::atomic<float>*, pid::count> raw;
    std::array<float, pid::count> lastRaw {}, target {}, smooth {}, cur_ {}, prv {}, lastMod {}, effNorm {};
    std::array<bool, pid::count> uiWritten {};
    std::array<std::atomic<float>, pid::count> uiNorm;
    std::vector<int> floatParams, discreteParams;
};
} // namespace synthia::dsp
