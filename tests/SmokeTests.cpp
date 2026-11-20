#include <catch2/catch_test_macros.hpp>
#include "PluginProcessor.h"

TEST_CASE ("processor passes audio through", "[smoke]")
{
    synthia::SynthiaProcessor p;
    p.prepareToPlay (48000.0, 64);
    juce::AudioBuffer<float> b (2, 64);
    for (int i = 0; i < 64; ++i)
        b.setSample (0, i, 0.5f), b.setSample (1, i, -0.5f);
    juce::MidiBuffer m;
    p.processBlock (b, m);
    REQUIRE (b.getSample (0, 10) == 0.5f);
}
