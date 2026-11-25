#include "model/FactoryPresets.h"
#include "PluginProcessor.h"

namespace synthia
{
// Values are real parameter values (choice = index, bool = 0/1). Parameters not
// listed keep their defaults. Choice indices: see Params.cpp choicesFor().
// clang-format off
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", "Basic", {} },

        // ------------------------------------------------------------ leads
        { "Poly Saw Lead", "Lead", {
            { "vcWave", 1 }, { "vcSub", 0.25f }, { "vcDetune", 10 }, { "vcFocus", 0.6f }, { "vcSpread", 0.5f },
            { "fltCut", 3500 }, { "fltRes", 0.3f }, { "fltPick", 0.35f },
            { "dlyOn", 1 }, { "dlyDiv", 13 }, { "dlyMix", 0.2f }, { "rvMix", 0.2f } } },
        { "Supersaw Unison Lead", "Lead", { { "outGain", 1.5f },
            { "vcWave", 1 }, { "vcUniVoices", 5 }, { "vcUniDetune", 22 }, { "vcUniWidth", 0.8f }, { "vcSub", 0.2f }, { "vcFocus", 0.65f },
            { "vcSpread", 0.3f }, { "fltCut", 4500 }, { "fltRes", 0.2f }, { "fltPick", 0.25f },
            { "dlyOn", 1 }, { "dlyDiv", 13 }, { "dlyMix", 0.18f }, { "rvMix", 0.22f } } },
        { "80s Brass", "Lead", { { "outGain", 2.5f },
            { "vcWave", 1 }, { "vcDetune", 14 }, { "vcOct", 0.2f }, { "fltCut", 900 }, { "fltRes", 0.15f },
            { "fltEnv", 0.45f }, { "fltPick", 0.4f }, { "adsrAmt", 0.6f }, { "adsrA", 40 }, { "adsrD", 600 }, { "adsrS", 0.7f },
            { "rvMix", 0.25f } } },
        { "Octave Fuzz Synth", "Lead", { { "outGain", -6.0f },
            { "vcWave", 1.4f }, { "vcOct", 0.6f }, { "vcDrive", 18 }, { "vcFold", 0.2f }, { "fltCut", 5000 }, { "quality", 2 },
            { "dlyOn", 1 }, { "dlyMix", 0.15f } } },
        { "Talking Wah Synth", "Lead", { { "outGain", 6.0f },
            { "vcWave", 1 }, { "fltType", 6 }, { "fltCut", 120 }, { "fltRes", 0.7f }, { "fltEnv", 0.8f }, { "rvMix", 0.2f } } },

        // ------------------------------------------------------------ plucks
        { "Bright Saw Pluck", "Pluck", { { "outGain", 6.5f },
            { "vcWave", 1 }, { "vcUniVoices", 3 }, { "vcUniDetune", 12 }, { "adsrAmt", 1 }, { "adsrA", 1 }, { "adsrD", 350 },
            { "adsrS", 0 }, { "adsrR", 200 }, { "fltCut", 900 }, { "fltRes", 0.35f }, { "fltPick", 0.7f }, { "pickDecay", 250 },
            { "dlyOn", 1 }, { "dlyDiv", 13 }, { "dlyMix", 0.22f }, { "rvMix", 0.2f } } },
        { "Soft Sine Pluck", "Pluck", { { "outGain", 6.5f },
            { "vcWave", 0.2f }, { "adsrAmt", 1 }, { "adsrA", 2 }, { "adsrD", 700 }, { "adsrS", 0.05f }, { "fltCut", 2500 },
            { "fltPick", 0.3f }, { "rvMix", 0.3f } } },
        { "Square Pluck Bass", "Pluck", { { "outGain", 3.0f },
            { "vcWave", 2 }, { "vcSub", 0.6f }, { "vcHigh", 2500 }, { "vcSpread", 0 }, { "adsrAmt", 1 }, { "adsrA", 0.5f },
            { "adsrD", 250 }, { "adsrS", 0 }, { "fltCut", 500 }, { "fltRes", 0.4f }, { "fltPick", 0.6f }, { "pickDecay", 180 },
            { "rvOn", 0 } } },
        { "Glass Pluck", "Pluck", { { "outGain", 2.5f },
            { "vcWave", 0 }, { "vcMain", 0.5f }, { "vcOct", 0.6f }, { "vcRing", 0.1f }, { "adsrAmt", 1 }, { "adsrA", 0.5f },
            { "adsrD", 500 }, { "adsrS", 0 }, { "fltOn", 0 }, { "rvMode", 1 }, { "rvShim", 0.4f }, { "rvMix", 0.35f } } },
        { "Muted Funk Pluck", "Pluck", { { "outGain", 5.0f },
            { "vcWave", 1.5f }, { "adsrAmt", 1 }, { "adsrA", 0.5f }, { "adsrD", 120 }, { "adsrS", 0 }, { "fltType", 2 },
            { "fltCut", 700 }, { "fltRes", 0.5f }, { "fltPick", 0.5f }, { "pickDecay", 120 }, { "rvMix", 0.12f } } },
        { "Ping-Pong Arp Pluck", "Pluck", { { "outGain", 3.5f },
            { "vcWave", 2.6f }, { "vcPw", 0.2f }, { "adsrAmt", 1 }, { "adsrD", 300 }, { "adsrS", 0 }, { "fltCut", 1500 },
            { "fltPick", 0.4f }, { "dlyOn", 1 }, { "dlyPing", 1 }, { "dlyDiv", 12 }, { "dlyFb", 0.5f }, { "dlyMix", 0.35f },
            { "rvMix", 0.25f } } },
        { "Djent Pluck Layer", "Pluck", { { "outGain", -1.5f },
            { "vcWave", 1 }, { "vcSub", 0.4f }, { "vcSharp", 0.3f }, { "vcSmooth", 1.5f }, { "vcLevel", 0.6f },
            { "adsrAmt", 1 }, { "adsrA", 0.5f }, { "adsrD", 180 }, { "adsrS", 0.1f }, { "fltCut", 1200 }, { "fltPick", 0.6f },
            { "pickDecay", 150 }, { "dryOn", 1 }, { "dryLevel", 0.8f }, { "dryToFlt", 0 }, { "rvMix", 0.1f } } },
        { "Granular Pluck Cloud", "Pluck", { { "outGain", 6.0f },
            { "vcWave", 0.6f }, { "adsrAmt", 1 }, { "adsrD", 400 }, { "adsrS", 0 }, { "grOn", 1 }, { "grLevel", 0.5f },
            { "grPitch", 12 }, { "grQuant", 1 }, { "grSize", 80 }, { "grDensity", 20 }, { "grPos", 100 }, { "grToFlt", 0 },
            { "rvMode", 1 }, { "rvMix", 0.4f } } },

        // ------------------------------------------------------------ instrument imitations (from guitar, approximate)
        { "Piano (approx.)", "Instrument", { { "outGain", 8.0f },
            { "vcWave", 0.35f }, { "vcOct", 0.15f }, { "vcSharp", 0.6f }, { "vcUniVoices", 2 }, { "vcUniDetune", 4 },
            { "compOn", 1 }, { "compAmt", 0.3f }, { "adsrAmt", 0.7f }, { "adsrA", 1 }, { "adsrD", 1200 }, { "adsrS", 0.35f },
            { "adsrR", 400 }, { "fltCut", 3500 }, { "fltPick", 0.35f }, { "pickDecay", 400 },
            { "rvSize", 0.4f }, { "rvDecay", 1.8f }, { "rvMix", 0.18f } } },
        { "Electric Piano", "Instrument", { { "outGain", 1.5f },
            { "vcWave", 0.05f }, { "vcOct", 0.12f }, { "vcRing", 0.12f }, { "adsrAmt", 0.6f }, { "adsrA", 1 }, { "adsrD", 1500 },
            { "adsrS", 0.4f }, { "fltCut", 4000 }, { "compOn", 1 }, { "compAmt", 0.3f },
            { "lfo1Rate", 4.5f }, { "m1Src", 3 }, { "m1Dst", 0, "vcPan" }, { "m1Amt", 0.4f }, { "rvMix", 0.2f } } },
        { "Flute", "Instrument", { { "outGain", -4.5f },
            { "vcWave", 0.1f }, { "vcFocus", 0.8f }, { "vcTilt", -0.4f }, { "vcHigh", 4000 }, { "vcHold", 0.7f }, { "vcHoldTime", 6 },
            { "compOn", 1 }, { "compAmt", 0.6f }, { "swellOn", 1 }, { "swellTime", 90 }, { "fltCut", 3200 },
            { "spOn", 1 }, { "spMode", 2 }, { "spLevel", 0.12f }, { "spBlur", 0.3f },
            { "lfo1Rate", 5 }, { "m1Src", 3 }, { "m1Dst", 0, "fltCut" }, { "m1Amt", 0.04f },
            { "rvDecay", 2.5f }, { "rvMix", 0.28f } } },
        { "Clarinet", "Instrument", { { "outGain", -10.0f },
            { "vcWave", 2 }, { "vcTilt", -0.3f }, { "vcFocus", 0.7f }, { "vcHold", 0.6f }, { "compOn", 1 }, { "compAmt", 0.5f },
            { "swellOn", 1 }, { "swellTime", 60 }, { "fltType", 1 }, { "fltCut", 1800 }, { "fltRes", 0.1f }, { "rvMix", 0.2f } } },
        { "String Ensemble", "Instrument", {
            { "vcWave", 1 }, { "vcUniVoices", 5 }, { "vcUniDetune", 14 }, { "vcUniWidth", 0.9f }, { "vcHold", 0.6f },
            { "compOn", 1 }, { "compAmt", 0.5f }, { "swellOn", 1 }, { "swellTime", 350 }, { "fltCut", 3800 },
            { "rvDecay", 4 }, { "rvMix", 0.35f } } },
        { "Cello Section", "Instrument", { { "outGain", 8.5f },
            { "vcWave", 1.3f }, { "vcSub", 0.3f }, { "vcTilt", -0.5f }, { "vcUniVoices", 3 }, { "vcUniDetune", 8 }, { "vcHold", 0.5f },
            { "compOn", 1 }, { "compAmt", 0.5f }, { "swellOn", 1 }, { "swellTime", 200 }, { "fltCut", 2200 },
            { "rvDecay", 3.5f }, { "rvMix", 0.3f } } },
        { "Church Organ", "Instrument", { { "outGain", -15.0f },
            { "vcWave", 0 }, { "vcSub", 0.7f }, { "vcOct", 0.5f }, { "vcInt", 0.3f }, { "vcIntRatio", 6 }, { "vcFocus", 0.8f },
            { "vcHold", 1 }, { "vcHoldTime", 10 }, { "compOn", 1 }, { "compAmt", 0.7f }, { "fltOn", 0 },
            { "rvSize", 0.95f }, { "rvDecay", 5 }, { "rvMix", 0.35f } } },
        { "Rock Organ (Leslie)", "Instrument", { { "outGain", -11.0f },
            { "vcWave", 0 }, { "vcSub", 0.6f }, { "vcOct", 0.4f }, { "vcHold", 0.9f }, { "compOn", 1 }, { "compAmt", 0.6f },
            { "vcDrive", 6 }, { "fltCut", 6000 }, { "lfo1Rate", 6 }, { "m1Src", 3 }, { "m1Dst", 0, "vcPan" }, { "m1Amt", 0.5f },
            { "m2Src", 3 }, { "m2Dst", 0, "fltCut" }, { "m2Amt", 0.06f }, { "rvMix", 0.15f } } },
        { "Trumpet", "Instrument", {
            { "vcWave", 1 }, { "vcFocus", 0.6f }, { "swellOn", 1 }, { "swellTime", 40 }, { "fltCut", 1200 }, { "fltRes", 0.2f },
            { "fltEnv", 0.55f }, { "adsrAmt", 0.4f }, { "adsrA", 25 }, { "adsrD", 300 }, { "adsrS", 0.8f }, { "vcHold", 0.4f },
            { "compOn", 1 }, { "compAmt", 0.4f }, { "rvMix", 0.2f } } },
        { "Choir Aah", "Instrument", { { "outGain", 4.0f },
            { "vcWave", 0.8f }, { "vcUniVoices", 5 }, { "vcUniDetune", 10 }, { "vcHold", 0.7f }, { "compOn", 1 }, { "compAmt", 0.5f },
            { "swellOn", 1 }, { "swellTime", 250 }, { "fltType", 6 }, { "fltCut", 20 }, { "fltRes", 0.6f },
            { "lfo1Rate", 0.2f }, { "m1Src", 3 }, { "m1Dst", 0, "fltCut" }, { "m1Amt", 0.05f }, { "rvDecay", 6 }, { "rvMix", 0.45f } } },
        { "Music Box", "Instrument", {
            { "vcWave", 0 }, { "vcMain", 0.4f }, { "vcOct", 1 }, { "vcRing", 0.15f }, { "adsrAmt", 1 }, { "adsrA", 0.5f },
            { "adsrD", 900 }, { "adsrS", 0 }, { "fltOn", 0 }, { "rvMode", 1 }, { "rvShim", 0.3f }, { "rvMix", 0.35f },
            { "dlyOn", 1 }, { "dlyMix", 0.15f } } },
        { "Marimba", "Instrument", { { "outGain", 8.0f },
            { "vcWave", 0 }, { "vcTilt", -0.3f }, { "vcInt", 0.15f }, { "vcIntRatio", 5 }, { "adsrAmt", 1 }, { "adsrA", 0.5f },
            { "adsrD", 350 }, { "adsrS", 0 }, { "fltCut", 2500 }, { "rvMix", 0.15f } } },
        { "Harp", "Instrument", { { "outGain", 13.0f },
            { "vcWave", 0.5f }, { "adsrAmt", 0.8f }, { "adsrA", 1 }, { "adsrD", 1500 }, { "adsrS", 0.1f }, { "fltCut", 3000 },
            { "fltPick", 0.3f }, { "rvDecay", 3 }, { "rvMix", 0.3f } } },
        { "Sitar Drone (E)", "Instrument", {
            { "vcOn", 0 }, { "dryOn", 1 }, { "dryLevel", 0.8f }, { "dryToFlt", 1 }, { "fltType", 5 }, { "fltCut", 1200 }, { "fltRes", 0.6f },
            { "rsOn", 1 }, { "rsMat", 0 }, { "rsLevel", 0.4f }, { "rsDecay", 3 }, { "rsBright", 0.8f }, { "rsInharm", 0.2f },
            { "c1Root", 4 }, { "c1Type", 13 }, { "c1Oct", 2 }, { "rvMix", 0.2f } } },

        // ------------------------------------------------------------ bass / djent
        { "Square Bass", "Bass", {
            { "vcWave", 2 }, { "vcHigh", 2500 }, { "vcSub", 0.8f }, { "vcMain", 0.6f }, { "vcTilt", -0.7f }, { "vcSharp", 0.6f },
            { "vcSpread", 0 }, { "fltCut", 1200 }, { "fltRes", 0.25f }, { "fltPick", 0.3f }, { "rvOn", 0 } } },
        { "Sub Octave Doom", "Bass", {
            { "vcSub", 1 }, { "vcMain", 0.3f }, { "vcWave", 0.2f }, { "vcHigh", 2000 }, { "vcTilt", -0.6f },
            { "dryOn", 1 }, { "dryLevel", 0.8f }, { "dryToFlt", 0 }, { "fltCut", 900 }, { "rvMix", 0.12f } } },
        { "Djent Synth Layer", "Djent", { { "outGain", -1.5f },
            { "vcWave", 1.6f }, { "vcSub", 0.5f }, { "vcSharp", 0.35f }, { "vcSmooth", 2 }, { "vcFocus", 0.7f }, { "vcTilt", -0.5f },
            { "vcDrive", 6 }, { "vcLevel", 0.6f }, { "dryOn", 1 }, { "dryLevel", 0.8f }, { "dryToFlt", 0 },
            { "fltCut", 2500 }, { "fltPick", 0.3f }, { "adsrAmt", 0.5f }, { "adsrA", 1 }, { "adsrD", 200 }, { "adsrS", 0.4f },
            { "rvMix", 0.1f } } },
        { "Bit Crushed Arp", "Lo-Fi", { { "outGain", -5.5f },
            { "vcWave", 2 }, { "vcBits", 6 }, { "vcRate", 0.3f }, { "fltCut", 6000 },
            { "dlyOn", 1 }, { "dlyPing", 1 }, { "dlyDiv", 12 }, { "dlyFb", 0.45f }, { "dlyMix", 0.3f } } },

        // ------------------------------------------------------------ pads / ambient
        { "Glass Pad", "Pad", { { "outGain", 8.0f },
            { "vcLevel", 0.25f }, { "vcWave", 0 }, { "spOn", 1 }, { "spLevel", 0.8f }, { "spPartials", 16 }, { "spBlur", 0.7f },
            { "swellOn", 1 }, { "swellTime", 800 }, { "rvMode", 1 }, { "rvDecay", 10 }, { "rvMix", 0.45f } } },
        { "Frozen Choir", "Pad", { { "outGain", 6.5f },
            { "vcOn", 0 }, { "spOn", 1 }, { "spLevel", 0.8f }, { "spBlur", 0.8f }, { "spDrift", 0.5f },
            { "rsOn", 1 }, { "rsMode", 2 }, { "rsLevel", 0.5f }, { "fmVowel", 3 }, { "fmReso", 0.7f },
            { "lfo1Rate", 0.15f }, { "m1Src", 3 }, { "m1Dst", 0, "fmVowel" }, { "m1Amt", 0.25f },
            { "swellOn", 1 }, { "swellTime", 900 }, { "rvDecay", 14 }, { "rvMix", 0.5f } } },
        { "Bloom Pad", "Pad", { { "outGain", 18.0f },
            { "vcWave", 0.5f }, { "vcDetune", 18 }, { "swellOn", 1 }, { "swellTime", 1200 }, { "fltCut", 3000 },
            { "rvMode", 3 }, { "rvBloom", 2.5f }, { "rvDecay", 12 }, { "rvMix", 0.6f } } },
        { "Swell Violin", "Pad", { { "outGain", 8.5f },
            { "vcWave", 0.8f }, { "vcDetune", 15 }, { "vcTilt", -0.3f }, { "swellOn", 1 }, { "swellTime", 350 },
            { "fltCut", 4000 }, { "rvMix", 0.35f } } },
        { "Shimmer Cathedral", "Ambient", { { "outGain", -2.0f },
            { "vcOn", 0 }, { "dryOn", 1 }, { "dryLevel", 0.8f }, { "spOn", 1 }, { "spLevel", 0.3f }, { "spBlur", 0.8f },
            { "dlyOn", 1 }, { "dlyMix", 0.2f }, { "dlyFb", 0.45f },
            { "rvMode", 1 }, { "rvShim", 0.6f }, { "rvDecay", 18 }, { "rvSize", 0.95f }, { "rvMix", 0.55f } } },
        { "Abyss Drone", "Ambient", { { "outGain", 12.0f },
            { "vcSub", 0.8f }, { "vcMain", 0.3f }, { "vcWave", 0.3f }, { "spOn", 1 }, { "spBlur", 0.9f }, { "spLevel", 0.4f },
            { "swellOn", 1 }, { "swellTime", 1500 }, { "rvMode", 2 }, { "rvShim", 0.7f }, { "rvDecay", 30 }, { "rvDampHi", 4000 },
            { "rvMix", 0.6f } } },
        { "Granular Cloud", "Ambient", { { "outGain", 1.5f },
            { "vcOn", 0 }, { "grOn", 1 }, { "grLevel", 0.9f }, { "grSize", 250 }, { "grDensity", 25 }, { "grPos", 600 },
            { "grSpray", 400 }, { "grPanJit", 1 }, { "grRev", 0.3f }, { "dryOn", 1 }, { "dryLevel", 0.5f },
            { "rvMode", 4 }, { "rvMix", 0.45f } } },
        { "Octave Shimmer Grains", "Ambient", {
            { "vcOn", 0 }, { "grOn", 1 }, { "grPitch", 12 }, { "grQuant", 1 }, { "grPJit", 12 }, { "grDensity", 18 },
            { "grSize", 180 }, { "dryOn", 1 }, { "dryLevel", 0.7f }, { "rvMode", 1 }, { "rvMix", 0.4f } } },
        { "Reverse Swells", "Ambient", { { "outGain", 3.5f },
            { "vcOn", 0 }, { "grOn", 1 }, { "grRev", 1 }, { "grSize", 400 }, { "grDensity", 8 }, { "grPos", 300 },
            { "dryOn", 1 }, { "dryLevel", 0.4f }, { "swellOn", 1 }, { "rvMode", 3 }, { "rvMix", 0.5f } } },
        { "Infinite Bed", "Ambient", { { "outGain", 5.0f },
            { "vcWave", 0.2f }, { "vcLevel", 0.5f }, { "spOn", 1 }, { "spBlur", 0.95f }, { "spLevel", 0.5f },
            { "rvMode", 0 }, { "rvDecay", 60 }, { "rvSize", 1 }, { "rvMix", 0.7f }, { "rvModDepth", 0.7f } } },

        // ------------------------------------------------------------ resonator / vocoder
        { "Metallic Resonator Riff", "Resonator", { { "outGain", -2.0f },
            { "vcOn", 0 }, { "rsOn", 1 }, { "rsLevel", 0.9f }, { "rsMat", 1 }, { "rsDecay", 1.2f }, { "rsBright", 0.7f },
            { "rsInharm", 0.3f }, { "c1Root", 8 }, { "c1Type", 0 }, { "c1Oct", 1 }, { "dryOn", 1 }, { "dryLevel", 0.5f },
            { "rvMix", 0.15f } } },
        { "Bowed Strings", "Resonator", { { "outGain", 2.0f },
            { "vcOn", 0 }, { "rsOn", 1 }, { "rsMat", 3 }, { "rsDecay", 4 }, { "c1Root", 4 }, { "c1Type", 1 }, { "c1Oct", 2 },
            { "swellOn", 1 }, { "swellTime", 300 }, { "rvMix", 0.35f } } },
        { "Glass Harp", "Resonator", { { "outGain", 5.0f },
            { "vcOn", 0 }, { "rsOn", 1 }, { "rsMat", 2 }, { "rsDecay", 6 }, { "rsBright", 0.8f }, { "c1Root", 2 }, { "c1Type", 10 },
            { "c1Oct", 3 }, { "c1Spread", 0.8f }, { "dryOn", 1 }, { "dryLevel", 0.4f }, { "rvMode", 1 }, { "rvMix", 0.4f } } },
        { "Chord Vocoder Strums", "Vocoder", {
            { "vcOn", 0 }, { "rsOn", 1 }, { "rsMode", 1 }, { "rsLevel", 0.9f }, { "vcoWave", 2 }, { "c1Root", 9 }, { "c1Type", 9 },
            { "c1Oct", 3 }, { "dlyOn", 1 }, { "dlyMix", 0.18f }, { "rvMix", 0.3f } } },
        { "Chord Sequencer", "Vocoder", { { "outGain", 4.0f },
            { "vcOn", 0 }, { "rsOn", 1 }, { "rsMode", 1 }, { "vcoWave", 0 },
            { "c1Root", 9 }, { "c1Type", 2 }, { "c2Root", 5 }, { "c2Type", 1 }, { "c3Root", 0 }, { "c3Type", 1 }, { "c4Root", 7 }, { "c4Type", 1 },
            { "seqSteps", 4 }, { "seqDiv", 2 }, { "seqV1", 0 }, { "seqV2", 0.34f }, { "seqV3", 0.67f }, { "seqV4", 1 },
            { "m1Src", 6 }, { "m1Dst", 0, "rsSlot" }, { "m1Amt", 1 }, { "rvMix", 0.3f } } },

        // ------------------------------------------------------------ rhythmic / mod
        { "Stutter Glitch", "Rhythmic", { { "outGain", 2.5f },
            { "vcLevel", 0.5f }, { "grOn", 1 }, { "grLevel", 0.9f }, { "grStutter", 1 }, { "grStutDiv", 6 }, { "grStutHold", 0 },
            { "chopOn", 1 }, { "chopDiv", 16 }, { "rvMix", 0.15f } } },
        { "Chopper Pad", "Rhythmic", { { "outGain", 2.5f },
            { "vcWave", 1 }, { "vcDetune", 12 }, { "chopOn", 1 }, { "chopDiv", 14 }, { "chopDepth", 0.9f },
            { "chop1", 1 }, { "chop2", 1 }, { "chop3", 0 }, { "chop4", 1 }, { "chop5", 0 }, { "chop6", 1 }, { "chop7", 1 }, { "chop8", 0 },
            { "rvMix", 0.3f } } },
        { "LFO Filter Sweep", "Mod", { { "outGain", 4.0f },
            { "vcWave", 1 }, { "fltCut", 800 }, { "fltRes", 0.5f }, { "lfo1Sync", 1 }, { "lfo1Div", 5 },
            { "m1Src", 3 }, { "m1Dst", 0, "fltCut" }, { "m1Amt", 0.35f }, { "rvMix", 0.25f } } },
        { "Polymeter 7", "Mod", { { "outGain", 9.5f },
            { "vcWave", 1.5f }, { "fltCut", 600 }, { "fltRes", 0.45f }, { "seqSteps", 7 }, { "seqDiv", 16 },
            { "seqV1", 1 }, { "seqV2", -0.4f }, { "seqV3", 0.3f }, { "seqV4", -1 }, { "seqV5", 0.6f }, { "seqV6", -0.2f }, { "seqV7", 0.1f },
            { "m1Src", 6 }, { "m1Dst", 0, "fltCut" }, { "m1Amt", 0.4f }, { "dlyOn", 1 }, { "dlyMix", 0.2f } } },

        // ------------------------------------------------------------ spectral FX
        { "Robot Voice", "FX", { { "outGain", 12.5f },
            { "vcOn", 0 }, { "spOn", 1 }, { "spMode", 1 }, { "spSize", 0 }, { "spBlur", 0 }, { "spLevel", 0.9f }, { "rvMix", 0.15f } } },
        { "Whisper Texture", "FX", { { "outGain", 5.5f },
            { "vcOn", 0 }, { "spOn", 1 }, { "spMode", 2 }, { "spBlur", 0.4f }, { "dryOn", 1 }, { "dryLevel", 0.3f }, { "rvMix", 0.4f } } },
        { "Inharmonic Shift", "FX", { { "outGain", 2.0f },
            { "vcOn", 0 }, { "spOn", 1 }, { "spShift", 220 }, { "spBlur", 0.2f }, { "dryOn", 1 }, { "dryLevel", 0.4f },
            { "dlyOn", 1 }, { "dlyMix", 0.25f } } },
        { "Ring Bells", "FX", { { "outGain", 2.5f },
            { "vcRing", 0.8f }, { "vcWave", 0 }, { "vcSub", 0.2f }, { "rsOn", 1 }, { "rsMat", 2 }, { "rsLevel", 0.3f }, { "rvMix", 0.35f } } },
    };
    return presets;
}
// clang-format on

void applyFactoryPreset (SynthiaProcessor& proc, const FactoryPreset& preset)
{
    proc.resetToDefaults();
    const auto& dests = modDestinations();
    for (const auto& v : preset.values)
    {
        const int idx = indexOf (v.id);
        jassert (idx >= 0); // typo in a preset
        if (idx < 0)
            continue;
        float value = v.value;
        if (v.dst != nullptr)
        {
            const int d = indexOf (v.dst);
            auto it = std::find (dests.begin(), dests.end(), d);
            jassert (it != dests.end());
            value = it == dests.end() ? 0.0f : (float) (1 + (it - dests.begin()));
        }
        proc.setParam (idx, value);
    }
}
} // namespace synthia
