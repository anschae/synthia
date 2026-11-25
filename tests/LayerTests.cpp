#include "TestHelpers.h"

using namespace testutil;

TEST_CASE ("voicer follows the input pitch (no detection, just the filter bank)", "[voicer]")
{
    Rig r;
    soloLayer (r, pid::vcOn);
    r.set (pid::vcWave, 0.0f); // sine
    r.set (pid::vcDetune, 0.0f);
    r.set (pid::vcSpread, 0.0f);
    r.prepare();
    auto out = r.run (sine (r.sr, 110.0f, 1.0, 0.5f));
    const int start = (int) (0.5 * r.sr), len = (int) (0.4 * r.sr);
    const double a110 = toneAmp (out, 0, r.sr, 110.0, start, len);
    const double a150 = toneAmp (out, 0, r.sr, 150.0, start, len);
    const double a55 = toneAmp (out, 0, r.sr, 55.0, start, len);
    INFO ("110: " << a110 << "  150: " << a150 << "  55: " << a55);
    REQUIRE (a110 > 0.05);
    REQUIRE (a110 > 10.0 * a150);
    REQUIRE (a110 > 10.0 * a55);
}

TEST_CASE ("voicer sub voice is an octave down", "[voicer]")
{
    Rig r;
    soloLayer (r, pid::vcOn);
    r.set (pid::vcWave, 0.0f);
    r.set (pid::vcMain, 0.0f);
    r.set (pid::vcSub, 1.0f);
    r.set (pid::vcDetune, 0.0f);
    r.prepare();
    auto out = r.run (sine (r.sr, 196.0f, 1.0, 0.5f));
    const int start = (int) (0.5 * r.sr), len = (int) (0.4 * r.sr);
    const double a98 = toneAmp (out, 0, r.sr, 98.0, start, len);
    const double a196 = toneAmp (out, 0, r.sr, 196.0, start, len);
    INFO ("98: " << a98 << "  196: " << a196);
    REQUIRE (a98 > 5.0 * a196);
}

TEST_CASE ("voicer handles a low 8-string note and a chord", "[voicer]")
{
    Rig r;
    soloLayer (r, pid::vcOn);
    r.set (pid::vcWave, 0.0f);
    r.set (pid::vcDetune, 0.0f);
    r.set (pid::vcSpread, 0.0f);
    r.set (pid::vcTilt, 0.0f);
    r.prepare();
    // F#1 (46.2 Hz) + A2 + E3 together
    auto in = sine (r.sr, 46.25f, 1.5, 0.3f);
    auto b = sine (r.sr, 110.0f, 1.5, 0.2f);
    auto c = sine (r.sr, 164.8f, 1.5, 0.2f);
    for (int i = 0; i < in.getNumSamples(); ++i)
        in.setSample (0, i, in.getSample (0, i) + b.getSample (0, i) + c.getSample (0, i));
    auto out = r.run (in);
    const int start = (int) (0.8 * r.sr), len = (int) (0.6 * r.sr);
    for (double hz : { 46.25, 110.0, 164.8 })
    {
        INFO (hz);
        REQUIRE (toneAmp (out, 0, r.sr, hz, start, len) > 0.03);
    }
}

TEST_CASE ("voicer quality settings stay finite with heavy drive", "[voicer]")
{
    for (int q = 0; q < 3; ++q)
    {
        Rig r;
        soloLayer (r, pid::vcOn);
        r.set (pid::quality, (float) q);
        r.set (pid::vcDrive, 36.0f);
        r.set (pid::vcFold, 1.0f);
        r.set (pid::vcRing, 0.5f);
        r.prepare();
        auto out = r.run (plucks (r.sr, 82.0f, 0.5), 17);
        REQUIRE (allFinite (out));
        REQUIRE (peak (out) < 4.0f);
    }
}

TEST_CASE ("spectral layer reconstructs the input with exactly N samples latency", "[spectral]")
{
    Rig r;
    soloLayer (r, pid::spOn);
    r.set (pid::spBlur, 0.0f);
    r.set (pid::spSize, 1.0f); // 2048
    r.set (pid::hpf, 20.0f);
    r.prepare();
    auto in = sine (r.sr, 440.0f, 0.5, 0.5f);
    auto out = r.run (in);
    const int N = 2048;
    for (int i = 6000; i < 20000; i += 37)
        REQUIRE (out.getSample (0, i) == Catch::Approx (in.getSample (0, i - N)).margin (0.06));
}

TEST_CASE ("align spectral reports the latency and delays the other paths", "[spectral][latency]")
{
    Rig r;
    REQUIRE (r.proc.engine.getDesiredLatency() == 0);
    r.set (pid::alignSpec, 1);
    r.set (pid::spSize, 2.0f); // 4096
    r.prepare();
    REQUIRE (r.proc.engine.getDesiredLatency() == 4096);
    REQUIRE (r.proc.getLatencySamples() == 4096);

    // dry path delayed by the same amount
    for (int p : { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn })
        r.set (p, 0);
    r.set (pid::dryOn, 1);
    r.set (pid::dryLevel, 1);
    r.set (pid::dryToFlt, 0);
    r.set (pid::rvOn, 0);
    r.set (pid::gateOn, 0);
    r.set (pid::hpf, 20);
    r.prepare();
    juce::AudioBuffer<float> imp (1, 10000);
    imp.clear();
    imp.setSample (0, 100, 1.0f);
    auto out = r.run (imp);
    int maxI = 0;
    for (int i = 0; i < out.getNumSamples(); ++i)
        if (std::abs (out.getSample (0, i)) > std::abs (out.getSample (0, maxI))) maxI = i;
    REQUIRE (maxI >= 100 + 4096);
    REQUIRE (maxI <= 100 + 4096 + 4);
}

TEST_CASE ("spectral freeze holds the sound after the input stops", "[spectral]")
{
    Rig r;
    soloLayer (r, pid::spOn);
    r.prepare();
    auto in = concat (sine (r.sr, 330.0f, 0.5, 0.5f), silence (r.sr, 1.5));
    // turn freeze on after 0.4 s
    juce::AudioBuffer<float> out (2, in.getNumSamples());
    const int split = (int) (0.4 * r.sr);
    juce::AudioBuffer<float> a (1, split), b (1, in.getNumSamples() - split);
    a.copyFrom (0, 0, in, 0, 0, split);
    b.copyFrom (0, 0, in, 0, split, b.getNumSamples());
    r.run (a);
    r.set (pid::spFreeze, 1);
    auto ob = r.run (b);
    const int late = ob.getNumSamples() - (int) (0.5 * r.sr);
    REQUIRE (toneAmp (ob, 0, r.sr, 330.0, late, (int) (0.4 * r.sr)) > 0.05);
}

TEST_CASE ("reverb stays bounded with infinite decay and full shimmer", "[reverb]")
{
    for (int mode = 0; mode < 5; ++mode)
    {
        Rig r;
        r.set (pid::rvOn, 1);
        r.set (pid::rvMode, (float) mode);
        r.set (pid::rvMix, 1.0f);
        r.set (pid::rvShim, 1.0f);
        r.set (pid::rvDecay, 60.0f);
        r.set (pid::rvSize, 1.0f);
        r.prepare();
        auto out = r.run (concat (plucks (r.sr, 110.0f, 1.0, 0.2, 0.9f), silence (r.sr, 1.0)));
        r.set (pid::rvInf, 1);
        auto out2 = r.run (silence (r.sr, 8.0));
        INFO ("mode " << mode);
        REQUIRE (allFinite (out2));
        REQUIRE (peak (out2) < 3.0f);
        // infinite keeps something in the tank
        REQUIRE (rms (out2, 0, out2.getNumSamples() - 48000, 48000) > 1.0e-4);
    }
}

TEST_CASE ("reverb decays roughly with the decay time", "[reverb]")
{
    Rig r;
    soloLayer (r, pid::dryOn);
    r.set (pid::rvOn, 1);
    r.set (pid::rvMix, 1.0f);
    r.set (pid::rvShim, 0.0f);
    r.set (pid::rvDecay, 2.0f);
    r.set (pid::rvDampLo, 20.0f);
    r.set (pid::rvDampHi, 20000.0f);
    r.prepare();
    auto out = r.run (concat (plucks (r.sr, 220.0f, 0.3, 1.0, 0.8f), silence (r.sr, 4.0)));
    const int s1 = (int) (0.8 * r.sr), s2 = (int) (2.8 * r.sr), len = (int) (0.2 * r.sr);
    const double drop = 20.0 * std::log10 (rms (out, 0, s1, len) / std::max (1e-12, rms (out, 0, s2, len)));
    INFO ("drop over 2 s: " << drop << " dB");
    REQUIRE (drop > 40.0); // RT60 = 2 s -> ~60 dB in 2 s (damping/modulation make it a bit irregular)
    REQUIRE (drop < 90.0);
}

TEST_CASE ("granular produces grains and stutter repeats", "[granular]")
{
    Rig r;
    soloLayer (r, pid::grOn);
    r.prepare();
    auto out = r.run (plucks (r.sr, 147.0f, 1.5, 0.3));
    REQUIRE (allFinite (out));
    REQUIRE (rms (out, 0, (int) (0.5 * r.sr), (int) r.sr) > 0.005);

    r.set (pid::grStutter, 1);
    r.prepare();
    out = r.run (plucks (r.sr, 147.0f, 1.0, 0.3));
    REQUIRE (allFinite (out));
    REQUIRE (peak (out) > 0.01f);
}

TEST_CASE ("granular feedback is limited", "[granular]")
{
    Rig r;
    soloLayer (r, pid::grOn);
    r.set (pid::grFb, 0.95f);
    r.set (pid::grDensity, 100.0f);
    r.set (pid::grSize, 500.0f);
    r.prepare();
    auto out = r.run (concat (plucks (r.sr, 100.0f, 1.0, 0.1, 1.0f), silence (r.sr, 6.0)));
    REQUIRE (allFinite (out));
    REQUIRE (peak (out) < 8.0f);
}

TEST_CASE ("resonator rings at the chord notes", "[reso]")
{
    Rig r;
    soloLayer (r, pid::rsOn);
    r.set (pid::rsMode, 0);
    r.set (pid::rsMat, 0);
    r.set (pid::c1Root, 9);  // A
    r.set (pid::c1Type, 12); // octaves
    r.set (pid::c1Oct, 2);   // A2 = 110 Hz
    r.set (pid::c1Spread, 0);
    r.set (pid::rsDecay, 3.0f);
    r.prepare();
    juce::AudioBuffer<float> imp (1, (int) r.sr);
    imp.clear();
    for (int i = 0; i < 200; ++i) imp.setSample (0, 1000 + i, (i % 2 ? 0.8f : -0.8f));
    auto out = r.run (imp);
    const int start = (int) (0.3 * r.sr), len = (int) (0.4 * r.sr);
    const double a110 = toneAmp (out, 0, r.sr, 110.0, start, len);
    const double a130 = toneAmp (out, 0, r.sr, 130.0, start, len);
    INFO ("110: " << a110 << " 130: " << a130);
    REQUIRE (a110 > 3.0 * a130);
}

TEST_CASE ("vocoder, formant and every material stay finite", "[reso]")
{
    for (int mode = 0; mode < 3; ++mode)
        for (int mat = 0; mat < 4; ++mat)
        {
            Rig r;
            soloLayer (r, pid::rsOn);
            r.set (pid::rsMode, (float) mode);
            r.set (pid::rsMat, (float) mat);
            r.set (pid::rsExcite, 12.0f);
            r.set (pid::rsDecay, 20.0f);
            r.prepare();
            auto out = r.run (plucks (r.sr, 73.0f, 0.6, 0.15, 1.0f), 13);
            INFO ("mode " << mode << " mat " << mat);
            REQUIRE (allFinite (out));
            REQUIRE (peak (out) < 8.0f);
            REQUIRE (peak (out) > 1.0e-3f);
        }
}

TEST_CASE ("chord slot can be switched by modulation", "[reso]")
{
    float notes[8];
    const int n = synthia::dsp::chordNotes (9, 2, 2, 0.0f, notes); // A minor, octave 2
    REQUIRE (n >= 3);
    REQUIRE (notes[0] == 45.0f); // A2
    REQUIRE (notes[1] == 48.0f); // C3
    REQUIRE (notes[2] == 52.0f); // E3
}

TEST_CASE ("layers that are off do not run", "[engine]")
{
    Rig r;
    r.set (pid::spOn, 0);
    r.set (pid::grOn, 0);
    r.prepare();
    r.run (sine (r.sr, 100.0f, 0.2));
    REQUIRE_FALSE (r.proc.engine.isLayerProcessing (synthia::dsp::Engine::layerSpectral));
    REQUIRE_FALSE (r.proc.engine.isLayerProcessing (synthia::dsp::Engine::layerGranular));
    REQUIRE (r.proc.engine.isLayerProcessing (synthia::dsp::Engine::layerVoicer));
}

TEST_CASE ("voicer unison thickens and widens the main voice", "[voicer]")
{
    auto render = [] (int voices, float width)
    {
        Rig r;
        soloLayer (r, pid::vcOn);
        r.set (pid::vcWave, 1.0f);
        r.set (pid::vcDetune, 0.0f);
        r.set (pid::vcSpread, 0.0f);
        r.set (pid::vcUniVoices, (float) voices);
        r.set (pid::vcUniDetune, 30.0f);
        r.set (pid::vcUniWidth, width);
        r.prepare();
        return r.run (sine (r.sr, 220.0f, 1.0, 0.5f), 17);
    };
    auto mono = render (1, 1.0f);
    auto uni = render (7, 1.0f);
    REQUIRE (allFinite (uni));
    REQUIRE (peak (uni) < 4.0f);
    const int start = 24000, len = 20000;
    // L == R without unison (spread 0), different with a wide unison
    double diffMono = 0, diffUni = 0;
    for (int i = start; i < start + len; ++i)
    {
        diffMono += std::abs (mono.getSample (0, i) - mono.getSample (1, i));
        diffUni += std::abs (uni.getSample (0, i) - uni.getSample (1, i));
    }
    REQUIRE (diffMono < 1.0e-3 * len);
    REQUIRE (diffUni > 0.01 * len);
    // level stays in the same ballpark (1/sqrt(N) normalisation)
    const double rm = rms (mono, 0, start, len), ru = rms (uni, 0, start, len);
    INFO ("rms mono " << rm << " unison " << ru);
    REQUIRE (ru > rm * 0.4);
    REQUIRE (ru < rm * 2.5);
}

namespace
{
/** Ratio (dB) between the level 0.1-0.3 s and 1.6-1.8 s after a single decaying pluck. */
double decayDb (const juce::AudioBuffer<float>& out, double sr)
{
    const double early = rms (out, 0, (int) (0.1 * sr), (int) (0.2 * sr));
    const double late = rms (out, 0, (int) (1.6 * sr), (int) (0.2 * sr));
    return 20.0 * std::log10 (early / std::max (1e-9, late));
}
} // namespace

TEST_CASE ("sustain compressor evens out a decaying note, picks still detected", "[input][comp]")
{
    auto render = [] (bool comp, int& picksOut)
    {
        Rig r;
        soloLayer (r, pid::dryOn);
        r.set (pid::hpf, 20.0f);
        r.set (pid::compOn, comp ? 1.0f : 0.0f);
        r.set (pid::compAmt, 0.8f);
        r.set (pid::compDry, 1.0f);
        r.prepare();
        const int before = r.proc.engine.pickCount.load();
        auto out = r.run (plucks (r.sr, 110.0f, 2.0, 2.0, 0.7f), 32);
        picksOut = r.proc.engine.pickCount.load() - before;
        REQUIRE (allFinite (out));
        REQUIRE (peak (out) < 1.5f);
        return out;
    };
    int pOff = 0, pOn = 0;
    auto off = render (false, pOff);
    auto on = render (true, pOn);
    const double dOff = decayDb (off, 48000.0), dOn = decayDb (on, 48000.0);
    INFO ("decay without comp " << dOff << " dB, with comp " << dOn << " dB");
    REQUIRE (dOn < dOff - 6.0);
    REQUIRE (pOn == pOff); // analysis hears the uncompressed signal
}

TEST_CASE ("voicer hold sustains bands while the input decays", "[voicer][hold]")
{
    auto render = [] (float hold)
    {
        Rig r;
        soloLayer (r, pid::vcOn);
        r.set (pid::vcWave, 0.0f);
        r.set (pid::vcDetune, 0.0f);
        r.set (pid::vcHold, hold);
        r.set (pid::vcHoldTime, 10.0f);
        r.prepare();
        auto out = r.run (plucks (r.sr, 110.0f, 2.0, 2.0, 0.7f), 32);
        REQUIRE (allFinite (out));
        REQUIRE (peak (out) < 4.0f);
        return out;
    };
    const double dOff = decayDb (render (0.0f), 48000.0), dOn = decayDb (render (1.0f), 48000.0);
    INFO ("decay without hold " << dOff << " dB, with hold " << dOn << " dB");
    REQUIRE (dOn < dOff - 6.0);
}
