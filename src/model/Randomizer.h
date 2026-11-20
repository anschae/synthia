#pragma once
// Randomize per section or globally. "Chaos" = how far from the current
// settings: new = current + chaos * (random - current) in the normalised
// domain (choices/bools flip with probability chaos). Message thread only.

#include <juce_audio_processors/juce_audio_processors.h>
#include "params/Params.h"

namespace synthia
{
class Randomizer
{
public:
    /** True if `index` may be randomized (safety: no gain/mix/bypass/quality jumps). */
    static bool isRandomizable (int index)
    {
        switch (index)
        {
            case pid::inGain: case pid::outGain: case pid::mix: case pid::bypass: case pid::quality:
            case pid::alignSpec: case pid::gateOn: case pid::gateThr: case pid::hpf: case pid::spFreeze:
            case pid::rvInf: case pid::grFreeze:
                return false;
            default:
                break;
        }
        const auto& s = spec (index);
        if (s.section == Section::global || s.section == Section::input)
            return false;
        // keep mixer mute/solo/on untouched (they decide what you hear at all)
        if (s.section == Section::mix && s.kind == Kind::B)
            return false;
        return true;
    }

    /** section == Section::count: everything (except the matrix and mixer switches). */
    template <typename SetFn, typename GetFn>
    static void randomize (Section section, float chaos, juce::Random& rng, GetFn&& get, SetFn&& set)
    {
        chaos = juce::jlimit (0.0f, 1.0f, chaos);
        for (int i = 0; i < pid::count; ++i)
        {
            const auto& s = spec (i);
            if (! isRandomizable (i))
                continue;
            if (section == Section::count ? s.section == Section::matrix : s.section != section)
                continue;
            const float cur = get (i); // normalised 0..1
            float next = cur;
            if (s.kind == Kind::B || s.kind == Kind::C)
            {
                if (rng.nextFloat() < chaos)
                    next = rng.nextFloat();
            }
            else
                next = cur + chaos * (rng.nextFloat() - cur);
            set (i, juce::jlimit (0.0f, 1.0f, next));
        }
    }
};
} // namespace synthia
