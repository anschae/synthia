#pragma once
// Layer B -- SPECTRAL: STFT (Hann, hop N/4, overlap-add) with
//   Freeze (crossfaded capture, per-bin phase advance + random drift),
//   Blur (per-bin temporal magnitude smoothing), Partials (keep the N strongest
//   peaks), Shift (linear Hz) and Stretch (bin scaling) via a phase vocoder,
//   Mode Normal / Robot (zero phase) / Whisper (random phase).
// Latency is exactly N samples (getLatency()). The engine either ignores it
// (default, "Align Spectral" off) or delays every other path to match.
// FFT size scales with the sample rate (x2 at 88.2/96 k, x4 at 176.4/192 k).
// Uses the complex FFT with preallocated buffers (no alloca/heap on the audio thread).

#include <juce_dsp/juce_dsp.h>
#include "dsp/Common.h"
#include "dsp/Layer.h"
#include "dsp/VizData.h"

namespace synthia::dsp
{
class Spectral
{
public:
    static constexpr int kMaxOrder = 14; // 16384

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        srMul = sr > 150000.0 ? 4 : (sr > 70000.0 ? 2 : 1);
        for (int o = 10; o <= kMaxOrder; ++o)
            ffts[(size_t) (o - 10)] = std::make_unique<juce::dsp::FFT> (o);
        const size_t maxN = (size_t) 1 << kMaxOrder;
        inRing.assign (maxN, 0.0f);
        outRing.assign (maxN * 2, 0.0f);
        window.assign (maxN, 0.0f);
        cbufA.assign (maxN, {});
        cbufB.assign (maxN, {});
        const size_t bins = maxN / 2 + 1;
        for (auto* v : { &mag, &phase, &prevPhase, &trueBin, &blurMag, &frzMag, &frzBin, &synL, &synF,
                         &outMagL, &outBinL, &outMagF, &outBinF, &tmpMag })
            v->assign (bins, 0.0f);
        peakIdx.assign (bins, 0);
        keep.assign (bins, 0);
        rng.seed (0x51ECu);
        configure (2048 * srMul);
    }

    /** FFT size from the parameter (index 0..2). Call on the audio thread at chunk start. */
    int sizeForChoice (int choice) const noexcept { return (1024 << juce::jlimit (0, 2, choice)) * srMul; }
    static int sizeForChoice (int choice, double sampleRate) noexcept
    {
        const int mul = sampleRate > 150000.0 ? 4 : (sampleRate > 70000.0 ? 2 : 1);
        return (1024 << juce::jlimit (0, 2, choice)) * mul;
    }
    int getLatency() const noexcept { return N; }

    void reset() noexcept
    {
        std::fill (inRing.begin(), inRing.end(), 0.0f);
        std::fill (outRing.begin(), outRing.end(), 0.0f);
        for (auto* v : { &mag, &phase, &prevPhase, &trueBin, &blurMag, &frzMag, &frzBin, &synL, &synF })
            std::fill (v->begin(), v->end(), 0.0f);
        t = 0;
        frozen = false;
        freezeFade = 0.0f;
        remapWasActive = false;
    }

    void process (const float* in, float* outL, float* outR, const ChunkContext& c) noexcept
    {
        const auto& p = c.p;
        const int wantN = sizeForChoice (p.choice (pid::spSize));
        if (wantN != N)
            configure (wantN);

        st.freeze = p.on (pid::spFreeze);
        st.blend = p.cur (pid::spFrzBlend);
        st.drift = p.cur (pid::spDrift);
        st.blur = p.cur (pid::spBlur);
        st.partials = (int) p.cur (pid::spPartials);
        st.shiftHz = p.cur (pid::spShift);
        st.stretch = p.cur (pid::spStretch);
        st.mode = p.choice (pid::spMode);

        const int ringMask = N - 1;
        const int outMask = 2 * N - 1;
        for (int s = 0; s < c.n; ++s)
        {
            inRing[(size_t) (t & ringMask)] = in[s];
            const size_t oi = (size_t) (t & outMask);
            const float y = outRing[oi];
            outRing[oi] = 0.0f;
            outL[s] = outR[s] = sanitize (y);
            ++t;
            if ((t % hop) == 0)
                processFrame();
        }
    }

private:
    struct Settings
    {
        bool freeze = false;
        float blend = 1, drift = 0.3f, blur = 0.5f;
        int partials = 0;
        float shiftHz = 0, stretch = 1;
        int mode = 0;
    };

    void configure (int newN) noexcept
    {
        N = newN;
        order = 0;
        while ((1 << order) < N) ++order;
        hop = N / 4;
        fft = ffts[(size_t) (order - 10)].get();
        // display bins (log spaced) -> FFT bin ranges for this N
        for (int b = 0; b <= VizData::kSpecBins; ++b)
        {
            const double hz = VizData::kSpecLo * std::pow ((double) VizData::kSpecHi / VizData::kSpecLo, (double) b / VizData::kSpecBins);
            dispEdge[(size_t) b] = juce::jlimit (1, N / 2 - 1, (int) std::lround (hz * N / sr));
            const double hzC = VizData::kSpecLo * std::pow ((double) VizData::kSpecHi / VizData::kSpecLo, ((double) b + 0.5) / VizData::kSpecBins);
            dispCentre[(size_t) std::min (b, VizData::kSpecBins - 1)] = (float) juce::jlimit (1.0, (double) N / 2 - 2, hzC * N / sr);
        }
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) N);
        reset();
    }

    static float wrapPi (float x) noexcept
    {
        x = std::fmod (x + kPi, kTwoPi);
        if (x < 0) x += kTwoPi;
        return x - kPi;
    }

    void processFrame() noexcept
    {
        const int bins = N / 2 + 1;
        const int ringMask = N - 1;
        // ---- analysis: last N input samples, windowed
        for (int i = 0; i < N; ++i)
        {
            const int64_t idx = t - N + i;
            cbufA[(size_t) i] = { inRing[(size_t) (idx & ringMask)] * window[(size_t) i], 0.0f };
        }
        fft->perform (cbufA.data(), cbufB.data(), false);

        const float hopF = (float) hop;
        const float expectStep = kTwoPi * hopF / (float) N;
        const float blurCoef = st.blur > 0.0f ? std::pow (st.blur, 0.35f) * 0.985f : 0.0f;
        for (int k = 0; k < bins; ++k)
        {
            const auto z = cbufB[(size_t) k];
            float m = std::abs (z);
            if (! std::isfinite (m)) m = 0.0f;
            const float ph = std::isfinite (z.real()) && std::isfinite (z.imag()) ? std::arg (z) : 0.0f;
            const float dphi = wrapPi (ph - prevPhase[(size_t) k] - expectStep * (float) k);
            prevPhase[(size_t) k] = ph;
            phase[(size_t) k] = ph;
            trueBin[(size_t) k] = (float) k + dphi / expectStep;
            mag[(size_t) k] = m;
            blurMag[(size_t) k] = m + (blurMag[(size_t) k] - m) * blurCoef;
        }

        // ---- freeze capture / fade
        if (st.freeze && ! frozen)
        {
            frozen = true;
            for (int k = 0; k < bins; ++k)
            {
                frzMag[(size_t) k] = blurMag[(size_t) k];
                frzBin[(size_t) k] = trueBin[(size_t) k];
                synF[(size_t) k] = phase[(size_t) k];
            }
        }
        else if (! st.freeze && freezeFade <= 0.0f)
            frozen = false;
        const float fadeStep = 1.0f / 4.0f; // ~4 hops crossfade
        freezeFade = clamp (freezeFade + (st.freeze ? fadeStep : -fadeStep), 0.0f, 1.0f);
        const float b = frozen ? st.blend * freezeFade : 0.0f;

        // ---- live path
        const bool remap = std::abs (st.stretch - 1.0f) > 1.0e-3f || std::abs (st.shiftHz) > 0.5f;
        shape (blurMag.data(), trueBin.data(), outMagL.data(), outBinL.data(), remap);
        if (remap && ! remapWasActive)
            for (int k = 0; k < bins; ++k) synL[(size_t) k] = phase[(size_t) k];
        remapWasActive = remap;

        // ---- frozen path
        if (b > 0.0f)
            shape (frzMag.data(), frzBin.data(), outMagF.data(), outBinF.data(), true);

        // ---- synthesis
        const float binHz = (float) sr / (float) N;
        juce::ignoreUnused (binHz);
        for (int k = 0; k < bins; ++k)
        {
            std::complex<float> zl {}, zf {};
            if (b < 1.0f)
            {
                float phL;
                if (st.mode == 1) phL = 0.0f;
                else if (st.mode == 2) phL = rng.uni() * kTwoPi;
                else if (remap)
                {
                    synL[(size_t) k] = wrapPi (synL[(size_t) k] + expectStep * outBinL[(size_t) k]);
                    phL = synL[(size_t) k];
                }
                else
                    phL = phase[(size_t) k];
                zl = std::polar (outMagL[(size_t) k] * (1.0f - b), phL);
            }
            if (b > 0.0f)
            {
                synF[(size_t) k] = wrapPi (synF[(size_t) k] + expectStep * outBinF[(size_t) k] + st.drift * 0.6f * rng.bi());
                const float phF = st.mode == 1 ? 0.0f : (st.mode == 2 ? rng.uni() * kTwoPi : synF[(size_t) k]);
                zf = std::polar (outMagF[(size_t) k] * b, phF);
            }
            cbufA[(size_t) k] = zl + zf;
        }
        if (viz != nullptr && viz->active.load (std::memory_order_relaxed))
            publish (b);

        cbufA[0] = { cbufA[0].real(), 0.0f };
        cbufA[(size_t) (N / 2)] = { cbufA[(size_t) (N / 2)].real(), 0.0f };
        for (int k = N / 2 + 1; k < N; ++k)
            cbufA[(size_t) k] = std::conj (cbufA[(size_t) (N - k)]);
        fft->perform (cbufA.data(), cbufB.data(), true);

        // ---- overlap-add, output delayed by exactly N samples
        const float scale = 1.0f / 1.5f; // sum of Hann^2 at hop N/4
        const int outMask = 2 * N - 1;
        for (int i = 0; i < N; ++i)
        {
            const int64_t outTime = t - N + i + N; // input time + latency N
            outRing[(size_t) (outTime & outMask)] += cbufB[(size_t) i].real() * window[(size_t) i] * scale;
        }
    }

    void publish (float frozenBlend) noexcept
    {
        const float norm = 4.0f / (float) N; // Hann: a sine of amplitude A peaks at A*N/4
        auto db = [norm] (float m) { return 20.0f * std::log10 (std::max (m * norm, 1.0e-6f)); };
        for (int b = 0; b < VizData::kSpecBins; ++b)
        {
            const int k0 = dispEdge[(size_t) b], k1 = std::max (k0, dispEdge[(size_t) b + 1] - 1);
            float ml = 0.0f, mf = 0.0f;
            uint8_t pk = 0;
            if (k1 - k0 < 2)
            {
                // display bin narrower than the FFT resolution: interpolate between FFT bins
                const float c = dispCentre[(size_t) b];
                const int i = (int) c;
                const float t = c - (float) i;
                ml = outMagL[(size_t) i] + t * (outMagL[(size_t) i + 1] - outMagL[(size_t) i]);
                mf = outMagF[(size_t) i] + t * (outMagF[(size_t) i + 1] - outMagF[(size_t) i]);
            }
            else
                for (int k = k0; k <= k1; ++k)
                {
                    ml = std::max (ml, outMagL[(size_t) k]);
                    mf = std::max (mf, outMagF[(size_t) k]);
                }
            viz->spLive[(size_t) b].store (db (ml), std::memory_order_relaxed);
            viz->spFrozen[(size_t) b].store (db (mf), std::memory_order_relaxed);
            viz->spPeak[(size_t) b].store (pk, std::memory_order_relaxed);
        }
        // one marker per kept partial (display bin of its FFT peak)
        if (st.partials > 0)
        {
            const double lo = VizData::kSpecLo, hi = VizData::kSpecHi;
            for (int i = 0; i < keptPeaks; ++i)
            {
                const double hz = (double) keptPeakBins[(size_t) i] * sr / N;
                if (hz <= lo || hz >= hi) continue;
                const int b = (int) (VizData::kSpecBins * std::log (hz / lo) / std::log (hi / lo));
                viz->spPeak[(size_t) juce::jlimit (0, VizData::kSpecBins - 1, b)].store (1, std::memory_order_relaxed);
            }
        }
        viz->spFrozenOn.store (frozenBlend > 0.0f ? 1 : 0, std::memory_order_relaxed);
        viz->spFrameCounter.fetch_add (1, std::memory_order_relaxed);
    }

public:
    void setViz (VizData* v) noexcept { viz = v; }

private:
    /** Partials selection + shift/stretch remap: (mIn, binIn) -> (mOut, binOut). */
    void shape (const float* mIn, const float* binIn, float* mOut, float* binOut, bool remap) noexcept
    {
        const int bins = N / 2 + 1;
        const float* m = mIn;
        if (st.partials > 0)
        {
            int np = 0;
            for (int k = 2; k < bins - 2; ++k)
                if (m[k] > m[k - 1] && m[k] >= m[k + 1] && m[k] > 1.0e-6f)
                    peakIdx[(size_t) np++] = k;
            const int keepN = std::min (np, st.partials);
            if (mIn == blurMag.data())
            {
                keptPeaks = std::min (keepN, (int) keptPeakBins.size());
                for (int i = 0; i < keptPeaks; ++i) keptPeakBins[(size_t) i] = peakIdx[(size_t) i];
            }
            if (keepN < np)
                std::nth_element (peakIdx.begin(), peakIdx.begin() + keepN, peakIdx.begin() + np,
                                  [m] (int a, int bb) { return m[a] > m[bb]; });
            std::fill (keep.begin(), keep.begin() + bins, (uint8_t) 0);
            for (int i = 0; i < keepN; ++i)
                for (int d = -2; d <= 2; ++d)
                    keep[(size_t) (peakIdx[(size_t) i] + d)] = 1;
            for (int k = 0; k < bins; ++k)
                tmpMag[(size_t) k] = keep[(size_t) k] ? m[k] : 0.0f;
            m = tmpMag.data();
        }
        if (! remap)
        {
            for (int k = 0; k < bins; ++k) { mOut[k] = m[k]; binOut[k] = binIn[k]; }
            return;
        }
        std::fill (mOut, mOut + bins, 0.0f);
        const float shiftBins = st.shiftHz * (float) N / (float) sr;
        for (int k = 0; k < bins; ++k)
            binOut[k] = (float) k;
        for (int k = 1; k < bins - 1; ++k)
        {
            if (m[k] <= 0.0f) continue;
            const float nb = (float) k * st.stretch + shiftBins;
            const int j = (int) std::lround (nb);
            if (j < 1 || j >= bins - 1) continue;
            if (m[k] > mOut[j])
                binOut[j] = binIn[k] * st.stretch + shiftBins;
            mOut[j] += m[k];
        }
    }

    double sr = 48000.0;
    int srMul = 1;
    int N = 2048, order = 11, hop = 512;
    int64_t t = 0;
    juce::dsp::FFT* fft = nullptr;
    std::array<std::unique_ptr<juce::dsp::FFT>, kMaxOrder - 9> ffts;
    std::vector<float> inRing, outRing, window;
    std::vector<std::complex<float>> cbufA, cbufB;
    std::vector<float> mag, phase, prevPhase, trueBin, blurMag, frzMag, frzBin, synL, synF, outMagL, outBinL, outMagF, outBinF, tmpMag;
    std::vector<int> peakIdx;
    std::vector<uint8_t> keep;
    Settings st;
    bool frozen = false, remapWasActive = false;
    float freezeFade = 0.0f;
    Rng rng;
    VizData* viz = nullptr;
    std::array<int, VizData::kSpecBins + 1> dispEdge {};
    std::array<float, VizData::kSpecBins> dispCentre {};
    int keptPeaks = 0;
    std::array<int, 64> keptPeakBins {};
};
} // namespace synthia::dsp
