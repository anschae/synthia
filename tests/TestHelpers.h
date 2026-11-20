#pragma once
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PluginProcessor.h"

namespace testutil
{
using namespace synthia;

/** A processor prepared at `sr` with a fixed play head. */
struct Rig
{
    explicit Rig (double sampleRate = 48000.0, int block = 256, int inChannels = 2) : sr (sampleRate), blockSize (block)
    {
        proc.setPlayConfigDetails (inChannels, 2, sr, block);
        proc.prepareToPlay (sr, block);
    }
    void set (int index, float v) { proc.setParam (index, v); }
    void prepare() { proc.prepareToPlay (sr, blockSize); }

    /** Runs `input` (1 or 2 ch) through the processor with blocks of `block`; returns stereo output. */
    juce::AudioBuffer<float> run (const juce::AudioBuffer<float>& input, int block = -1)
    {
        if (block <= 0) block = blockSize;
        const int total = input.getNumSamples();
        juce::AudioBuffer<float> out (2, total);
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::AudioBuffer<float> b (2, n);
            for (int ch = 0; ch < 2; ++ch)
                b.copyFrom (ch, 0, input, std::min (ch, input.getNumChannels() - 1), pos, n);
            proc.processBlock (b, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, b, ch, 0, n);
        }
        return out;
    }

    double sr;
    int blockSize;
    SynthiaProcessor proc;
};

inline juce::AudioBuffer<float> sine (double sr, float hz, double seconds, float amp = 0.5f, int channels = 1)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (channels, n);
    for (int i = 0; i < n; ++i)
        for (int ch = 0; ch < channels; ++ch)
            b.setSample (ch, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / sr));
    return b;
}

/** Decaying plucks (sum of a few harmonics) every `period` seconds. */
inline juce::AudioBuffer<float> plucks (double sr, float hz, double seconds, double period = 0.5, float amp = 0.5f)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double tl = std::fmod (t, period);
        const double env = std::exp (-tl * 6.0);
        double v = 0;
        for (int h = 1; h <= 6; ++h)
            v += std::sin (2.0 * juce::MathConstants<double>::pi * hz * h * t) / h;
        b.setSample (0, i, (float) (amp * env * v * 0.6));
    }
    return b;
}

inline juce::AudioBuffer<float> silence (double sr, double seconds, int channels = 1)
{
    juce::AudioBuffer<float> b (channels, (int) (seconds * sr));
    b.clear();
    return b;
}

inline juce::AudioBuffer<float> concat (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    juce::AudioBuffer<float> r (a.getNumChannels(), a.getNumSamples() + b.getNumSamples());
    for (int ch = 0; ch < r.getNumChannels(); ++ch)
    {
        r.copyFrom (ch, 0, a, ch, 0, a.getNumSamples());
        r.copyFrom (ch, a.getNumSamples(), b, std::min (ch, b.getNumChannels() - 1), 0, b.getNumSamples());
    }
    return r;
}

inline bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

inline float peak (const juce::AudioBuffer<float>& b, int start = 0, int len = -1)
{
    if (len < 0) len = b.getNumSamples() - start;
    float p = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        p = std::max (p, b.getMagnitude (ch, start, len));
    return p;
}

inline double rms (const juce::AudioBuffer<float>& b, int ch, int start = 0, int len = -1)
{
    if (len < 0) len = b.getNumSamples() - start;
    double s = 0;
    for (int i = start; i < start + len; ++i)
        s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return std::sqrt (s / std::max (1, len));
}

/** Goertzel magnitude (amplitude) of frequency hz in channel ch over [start, start+len). */
inline double toneAmp (const juce::AudioBuffer<float>& b, int ch, double sr, double hz, int start, int len)
{
    double sre = 0, sim = 0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (len - 1));
        const double ph = 2.0 * juce::MathConstants<double>::pi * hz * i / sr;
        const double x = b.getSample (ch, start + i) * w;
        sre += x * std::cos (ph);
        sim -= x * std::sin (ph);
    }
    return 2.0 * std::sqrt (sre * sre + sim * sim) / (len * 0.5);
}

/** Turns everything off except the given layer (level 1, no filter/amp, no delay/reverb). */
inline void soloLayer (Rig& r, int onParam)
{
    for (int p : { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn, pid::dryOn })
        r.set (p, p == onParam ? 1.0f : 0.0f);
    r.set (onParam + 1, 1.0f); // level
    r.set (pid::rvOn, 0);
    r.set (pid::dlyOn, 0);
    r.set (pid::fltOn, 0);
    r.set (pid::gateOn, 0);
}
} // namespace testutil
