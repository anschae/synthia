#include "TestHelpers.h"
#include "model/FactoryPresets.h"
#include "model/Randomizer.h"

using namespace testutil;

TEST_CASE ("parameter table is consistent", "[params]")
{
    std::set<juce::String> ids;
    for (int i = 0; i < pid::count; ++i)
    {
        const auto& s = spec (i);
        REQUIRE (ids.insert (s.id).second);
        REQUIRE (juce::String (s.tooltip).isNotEmpty());
        if (s.kind == Kind::F || s.kind == Kind::I)
        {
            REQUIRE (s.min < s.max);
            REQUIRE (s.def >= s.min);
            REQUIRE (s.def <= s.max);
            for (float n : { 0.0f, 0.13f, 0.5f, 0.77f, 1.0f })
            {
                const float v = s.fromNorm (n);
                if (s.kind == Kind::F)
                    REQUIRE (s.toNorm (v) == Catch::Approx (n).margin (1e-4));
            }
        }
        if (s.kind == Kind::C)
        {
            const auto& c = choicesFor (s.choices);
            REQUIRE (c.size() > 0);
            REQUIRE ((int) s.def < c.size());
        }
    }
    REQUIRE (indexOf ("vcLevel") == pid::vcLevel);
    REQUIRE (indexOf ("nope") == -1);
    REQUIRE (choicesFor (ChoiceList::modDest).size() == (int) modDestinations().size() + 1);
}

TEST_CASE ("note divisions", "[params]")
{
    REQUIRE (syncDivQuarters (8) == Catch::Approx (1.0));   // 1/4
    REQUIRE (syncDivQuarters (13) == Catch::Approx (0.75)); // 1/8 D
    REQUIRE (syncDivQuarters (14) == Catch::Approx (1.0 / 3)); // 1/8 T
    REQUIRE (syncDivQuarters (19) == Catch::Approx (0.2));  // 1/16 Q
    REQUIRE (choicesFor (ChoiceList::syncDiv)[13] == "1/8 D");
}

TEST_CASE ("state round trip", "[state]")
{
    Rig a;
    juce::Random rng (42);
    for (int i = 0; i < pid::count; ++i)
        if (auto* prm = a.proc.apvts.getParameter (paramId (i)))
            prm->setValueNotifyingHost (rng.nextFloat());
    a.proc.setPresetName ("User / Test");
    juce::MemoryBlock mb;
    a.proc.getStateInformation (mb);

    Rig b;
    b.proc.setStateInformation (mb.getData(), (int) mb.getSize());
    for (int i = 0; i < pid::count; ++i)
        REQUIRE (b.proc.getParam (i) == Catch::Approx (a.proc.getParam (i)).margin (1e-5));
    REQUIRE (b.proc.getPresetName() == "User / Test");
}

TEST_CASE ("passes audio with every layer off (dry path, mix)", "[engine]")
{
    Rig r;
    for (int p : { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn })
        r.set (p, 0);
    r.set (pid::dryOn, 1);
    r.set (pid::dryLevel, 1);
    r.set (pid::dryToFlt, 0);
    r.set (pid::rvOn, 0);
    r.set (pid::gateOn, 0);
    r.set (pid::hpf, 20);
    r.prepare();
    auto in = sine (r.sr, 440.0f, 0.5);
    auto out = r.run (in);
    const int n = in.getNumSamples();
    REQUIRE (rms (out, 0, n / 2, n / 2) == Catch::Approx (rms (in, 0, n / 2, n / 2)).epsilon (0.05));

    // global mix 0 = exactly the input
    r.set (pid::mix, 0);
    r.set (pid::dryOn, 0);
    r.prepare();
    out = r.run (in);
    for (int i = 100; i < n; i += 97)
        REQUIRE (out.getSample (0, i) == Catch::Approx (in.getSample (0, i)).margin (1e-5));
}

TEST_CASE ("fuzz: random parameters, block sizes and sample rates stay finite and bounded", "[engine][fuzz]")
{
    juce::Random rng (1234);
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Rig r (sr, 512);
        for (int round = 0; round < 6; ++round)
        {
            for (int i = 0; i < pid::count; ++i)
            {
                if (i == pid::bypass || i == pid::outGain || i == pid::inGain) continue;
                if (auto* prm = r.proc.apvts.getParameter (paramId (i)))
                    prm->setValueNotifyingHost (rng.nextFloat());
            }
            // all layers on, so everything runs
            for (int p : { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn, pid::dryOn })
                r.set (p, 1);
            auto in = plucks (sr, 55.0f + 300.0f * rng.nextFloat(), 0.4, 0.13, 0.9f);
            for (int block : { 1, 7, 64, 333, 512 })
            {
                auto out = r.run (in, block);
                REQUIRE (allFinite (out));
                REQUIRE (peak (out) < 8.01f);
                if (sr > 100000.0) break; // keep the test fast
            }
        }
    }
}

TEST_CASE ("silence in -> output decays to silence", "[engine]")
{
    Rig r;
    for (int p : { pid::vcOn, pid::spOn, pid::rsOn, pid::grOn, pid::dryOn })
        r.set (p, 1);
    r.set (pid::dryLevel, 0.7f);
    r.set (pid::rvDecay, 3.0f);
    r.set (pid::dlyOn, 1);
    r.set (pid::dlyFb, 0.6f);
    r.set (pid::rsDecay, 2.0f);
    r.prepare();
    auto in = concat (plucks (r.sr, 82.0f, 1.0), silence (r.sr, 14.0));
    auto out = r.run (in);
    REQUIRE (allFinite (out));
    REQUIRE (peak (out, 0, (int) r.sr) > 0.01f);
    const int lastSecond = out.getNumSamples() - (int) r.sr;
    REQUIRE (peak (out, lastSecond, (int) r.sr) < 1.0e-3f);
}

TEST_CASE ("pick detector finds plucks", "[input]")
{
    Rig r;
    r.prepare();
    const int before = r.proc.engine.pickCount.load();
    auto in = plucks (r.sr, 110.0f, 2.0, 0.25, 0.5f);
    r.run (in, 32);
    const int picks = r.proc.engine.pickCount.load() - before;
    REQUIRE (picks >= 7);
    REQUIRE (picks <= 9);
}

TEST_CASE ("factory presets load and render", "[presets]")
{
    Rig r (48000.0, 128);
    const auto& fp = factoryPresets();
    REQUIRE (fp.size() >= 25);
    for (auto& p : fp)
    {
        applyFactoryPreset (r.proc, p);
        r.prepare();
        auto out = r.run (plucks (r.sr, 98.0f, 0.6, 0.3));
        INFO (p.name);
        REQUIRE (allFinite (out));
        REQUIRE (peak (out) < 8.0f);
    }
}

TEST_CASE ("user presets: save, list, rename, delete", "[presets]")
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("synthia-test-presets");
    dir.deleteRecursively();
    PresetManager::setUserFolderOverride (dir);
    Rig r;
    r.set (pid::vcWave, 2.5f);
    REQUIRE (r.proc.presets.saveUser ("My Sound"));
    auto list = r.proc.presets.list();
    auto it = std::find_if (list.begin(), list.end(), [] (auto& e) { return ! e.factory && e.name == "My Sound"; });
    REQUIRE (it != list.end());
    r.set (pid::vcWave, 0.0f);
    REQUIRE (r.proc.presets.load (*it));
    REQUIRE (r.proc.getParam (pid::vcWave) == Catch::Approx (2.5f));
    REQUIRE (r.proc.presets.renameUser (*it, "Renamed"));
    list = r.proc.presets.list();
    it = std::find_if (list.begin(), list.end(), [] (auto& e) { return ! e.factory && e.name == "Renamed"; });
    REQUIRE (it != list.end());
    REQUIRE (r.proc.presets.deleteUser (*it));
    dir.deleteRecursively();
    PresetManager::setUserFolderOverride ({});
}

TEST_CASE ("A/B compare", "[presets]")
{
    Rig r;
    r.set (pid::fltCut, 500.0f);
    r.proc.presets.switchAB (1);
    REQUIRE (r.proc.getParam (pid::fltCut) == Catch::Approx (500.0f));
    r.set (pid::fltCut, 3000.0f);
    r.proc.presets.switchAB (0);
    REQUIRE (r.proc.getParam (pid::fltCut) == Catch::Approx (500.0f));
    r.proc.presets.switchAB (1);
    REQUIRE (r.proc.getParam (pid::fltCut) == Catch::Approx (3000.0f));
}

TEST_CASE ("randomize respects sections and chaos", "[presets]")
{
    Rig r;
    juce::Random rng (7);
    auto get = [&] (int i) { return r.proc.apvts.getParameter (paramId (i))->getValue(); };
    auto set = [&] (int i, float v) { r.proc.apvts.getParameter (paramId (i))->setValueNotifyingHost (v); };
    const float outBefore = r.proc.getParam (pid::outGain);
    const float cutBefore = get (pid::fltCut);
    Randomizer::randomize (Section::voicer, 1.0f, rng, get, set);
    REQUIRE (get (pid::fltCut) == cutBefore); // other section untouched
    Randomizer::randomize (Section::count, 0.0f, rng, get, set);
    REQUIRE (get (pid::fltCut) == cutBefore); // chaos 0 = no change
    Randomizer::randomize (Section::count, 1.0f, rng, get, set);
    REQUIRE (r.proc.getParam (pid::outGain) == outBefore); // never randomized
}

TEST_CASE ("modulation matrix moves the destination", "[mod]")
{
    Rig r;
    r.set (pid::macro1, 1.0f);
    r.set (pid::m1Src, (float) modsrc::macro1);
    const auto& d = modDestinations();
    const int dst = (int) (std::find (d.begin(), d.end(), (int) pid::fltCut) - d.begin()) + 1;
    r.set (pid::m1Dst, (float) dst);
    r.set (pid::m1Amt, -0.5f);
    r.set (pid::fltCut, 20000.0f);
    r.prepare();
    r.run (silence (r.sr, 0.2));
    const float eff = r.proc.engine.params.cur (pid::fltCut);
    REQUIRE (eff == Catch::Approx (spec (pid::fltCut).fromNorm (0.5f)).epsilon (0.02));
    REQUIRE (r.proc.engine.params.uiModulatedNorm (pid::fltCut) == Catch::Approx (0.5f).margin (0.01));
}

TEST_CASE ("LFO synced to the host follows the position", "[mod]")
{
    synthia::dsp::Lfo lfo;
    lfo.reset (1);
    synthia::dsp::Transport t;
    t.playing = true;
    t.ppq = 0.5; // half a quarter into a 1/4 cycle
    const float v = lfo.advance (32, 48000.0, synthia::dsp::Lfo::sawUp, 1.0, 1.0, 0.0f, false, t);
    REQUIRE (v == Catch::Approx (0.0f).margin (1e-4));
}

TEST_CASE ("step sequencer polymeter", "[mod]")
{
    synthia::dsp::StepSequencer seq;
    float values[32] {};
    bool glides[32] {};
    for (int i = 0; i < 32; ++i) values[i] = (float) i;
    synthia::dsp::Transport t;
    t.playing = true;
    t.ppq = 7.0 * 0.25 + 0.01; // step 7 at 1/16 -> wraps to 0 with 7 steps
    REQUIRE (seq.advance (32, 48000.0, 7, 0.25, true, values, glides, t) == 0.0f);
    t.ppq = 3.0 * 0.25 + 0.01;
    REQUIRE (seq.advance (32, 48000.0, 7, 0.25, true, values, glides, t) == 3.0f);
}

TEST_CASE ("bypass returns the input", "[engine]")
{
    Rig r;
    r.set (pid::bypass, 1);
    r.prepare();
    auto in = sine (r.sr, 300.0f, 0.3);
    auto out = r.run (in);
    for (int i = 2000; i < in.getNumSamples(); i += 101)
        REQUIRE (out.getSample (1, i) == Catch::Approx (in.getSample (0, i)).margin (1e-5));
}
