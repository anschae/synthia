#include "params/Params.h"

namespace synthia
{
const std::array<ParamSpec, pid::count> kSpecs = { {
#define SY_X(id, name, kind, mn, mx, def, curve, unit, section, mod, choices, tip) \
    ParamSpec { #id, name, Kind::kind, mn, mx, def, Curve::curve, unit, Section::section, mod != 0, ChoiceList::choices, tip },
    SYNTHIA_PARAMS (SY_X)
#undef SY_X
} };

int indexOf (const juce::String& id)
{
    static const auto map = []
    {
        std::unordered_map<juce::String, int> m;
        for (int i = 0; i < pid::count; ++i)
            m[kSpecs[(size_t) i].id] = i;
        return m;
    }();
    auto it = map.find (id);
    return it == map.end() ? -1 : it->second;
}

const std::vector<int>& modDestinations()
{
    static const std::vector<int> list = []
    {
        std::vector<int> v;
        for (int i = 0; i < pid::count; ++i)
        {
            const auto& s = kSpecs[(size_t) i];
            if (s.modulatable && (s.kind == Kind::F || s.kind == Kind::I))
                v.push_back (i);
        }
        return v;
    }();
    return list;
}

// ---------------------------------------------------------------------------
// Note values. NEVER reorder or remove entries: the index is stored in projects.
namespace
{
struct Div { const char* name; double quarters; };
constexpr double Q5 = 4.0 / 5.0; // quintuplet: 5 in the time of 4
const Div kSyncDivs[] = {
    { "4/1", 16.0 }, { "2/1", 8.0 }, { "1/1", 4.0 }, { "1/1 D", 6.0 }, { "1/1 T", 4.0 * 2 / 3 },
    { "1/2", 2.0 }, { "1/2 D", 3.0 }, { "1/2 T", 2.0 * 2 / 3 },
    { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/4 T", 2.0 / 3 }, { "1/4 Q", Q5 },
    { "1/8", 0.5 }, { "1/8 D", 0.75 }, { "1/8 T", 1.0 / 3 }, { "1/8 Q", 0.5 * Q5 },
    { "1/16", 0.25 }, { "1/16 D", 0.375 }, { "1/16 T", 1.0 / 6 }, { "1/16 Q", 0.25 * Q5 },
    { "1/32", 0.125 }, { "1/32 D", 0.1875 }, { "1/32 T", 1.0 / 12 }, { "1/32 Q", 0.125 * Q5 },
    { "1/64", 0.0625 }, { "1/64 T", 1.0 / 24 },
};
const Div kStutDivs[] = {
    { "1/4", 1.0 }, { "1/4 D", 1.5 }, { "1/4 T", 2.0 / 3 }, { "1/8", 0.5 }, { "1/8 D", 0.75 }, { "1/8 T", 1.0 / 3 },
    { "1/16", 0.25 }, { "1/16 D", 0.375 }, { "1/16 T", 1.0 / 6 }, { "1/32", 0.125 }, { "1/32 T", 1.0 / 12 },
};

template <size_t N>
juce::StringArray divNames (const Div (&d)[N])
{
    juce::StringArray a;
    for (auto& x : d)
        a.add (x.name);
    return a;
}
} // namespace

double syncDivQuarters (int i) noexcept
{
    return kSyncDivs[(size_t) juce::jlimit (0, (int) std::size (kSyncDivs) - 1, i)].quarters;
}

double stutDivQuarters (int i) noexcept
{
    return kStutDivs[(size_t) juce::jlimit (0, (int) std::size (kStutDivs) - 1, i)].quarters;
}

double stutHoldQuarters (int i, double quartersPerBar) noexcept
{
    switch (i)
    {
        case 0: return 1.0;
        case 1: return 2.0;
        default: return juce::jmax (0.25, quartersPerBar);
    }
}

const juce::StringArray& choicesFor (ChoiceList list)
{
    // Append-only lists (stored by index).
    static const juce::StringArray empty;
    static const juce::StringArray quality { "Eco", "Normal (2x)", "High (4x)" };
    static const juce::StringArray vcBands { "24", "36", "48", "72" };
    static const juce::StringArray vcInterval { "-12", "-7", "-5", "+5", "+7", "+12", "+19" };
    static const juce::StringArray spSize { "1024", "2048", "4096" };
    static const juce::StringArray spMode { "Normal", "Robot", "Whisper" };
    static const juce::StringArray rsMode { "Resonator", "Vocoder", "Formant" };
    static const juce::StringArray root { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const juce::StringArray chordType { "Power", "Major", "Minor", "Sus2", "Sus4", "Maj7", "Min7", "Dom7",
                                               "Maj9", "Min9", "Add9", "Cluster", "Octaves", "Fifths" };
    static const juce::StringArray rsMaterial { "String", "Metal", "Glass", "Bowed" };
    static const juce::StringArray vcoBands { "16", "20", "24" };
    static const juce::StringArray vcoWave { "Saw", "Square", "Supersaw" };
    static const juce::StringArray grQuant { "Off", "Octaves", "Fifths", "Semitones" };
    static const juce::StringArray grWindow { "Hann", "Tukey", "Trapezoid" };
    static const juce::StringArray stutDiv = divNames (kStutDivs);
    static const juce::StringArray stutHold { "Every 1/4", "Every 1/2", "Every Bar" };
    static const juce::StringArray fltType { "Ladder LP 24", "SVF LP 12", "SVF BP 12", "SVF HP 12", "SVF Notch", "Comb", "Formant" };
    static const juce::StringArray syncDiv = divNames (kSyncDivs);
    static const juce::StringArray rvMode { "Cosmos", "Shimmer", "Abyss", "Bloom", "Cloud" };
    static const juce::StringArray shimInterval { "+12", "+7", "+19", "+24", "-12" };
    static const juce::StringArray lfoShape { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random" };
    static const juce::StringArray rndShape { "S&H", "Smooth" };
    static const juce::StringArray modSource { "None", "Env Follower", "Pick Env", "LFO 1", "LFO 2", "Random", "Step Seq",
                                               "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    static const juce::StringArray modDest = []
    {
        juce::StringArray a { "Off" };
        for (int i : modDestinations())
            a.add (kSpecs[(size_t) i].name);
        return a;
    }();

    switch (list)
    {
        case ChoiceList::quality: return quality;
        case ChoiceList::vcBands: return vcBands;
        case ChoiceList::vcInterval: return vcInterval;
        case ChoiceList::spSize: return spSize;
        case ChoiceList::spMode: return spMode;
        case ChoiceList::rsMode: return rsMode;
        case ChoiceList::root: return root;
        case ChoiceList::chordType: return chordType;
        case ChoiceList::rsMaterial: return rsMaterial;
        case ChoiceList::vcoBands: return vcoBands;
        case ChoiceList::vcoWave: return vcoWave;
        case ChoiceList::grQuant: return grQuant;
        case ChoiceList::grWindow: return grWindow;
        case ChoiceList::stutDiv: return stutDiv;
        case ChoiceList::stutHold: return stutHold;
        case ChoiceList::fltType: return fltType;
        case ChoiceList::syncDiv: return syncDiv;
        case ChoiceList::rvMode: return rvMode;
        case ChoiceList::shimInterval: return shimInterval;
        case ChoiceList::lfoShape: return lfoShape;
        case ChoiceList::rndShape: return rndShape;
        case ChoiceList::modSource: return modSource;
        case ChoiceList::modDest: return modDest;
        case ChoiceList::none: break;
    }
    return empty;
}

// ---------------------------------------------------------------------------
juce::String formatValue (int index, float v, int maxLen)
{
    const auto& s = spec (index);
    juce::String unit (s.unit);
    juce::String t;
    if (s.kind == Kind::B)
        return v >= 0.5f ? "On" : "Off";
    if (s.kind == Kind::C)
        return choicesFor (s.choices)[juce::roundToInt (v)];
    if (index == pid::rsSlot)
        return juce::String (juce::roundToInt (v) + 1);
    if (index == pid::spPartials && v < 0.5f)
        return "Off";
    if (index == pid::vcBits && v >= 15.95f)
        return "Off";
    if (index == pid::vcWave)
    {
        static const char* names[] = { "Sine", "Saw", "Square", "Pulse" };
        const int k = juce::jlimit (0, 3, juce::roundToInt (v));
        if (std::abs (v - (float) k) < 0.04f)
            return names[k];
        return juce::String (v, 2);
    }
    if (unit == "%")
    {
        const float lo = s.min < 0 ? -100.0f : 0.0f;
        juce::ignoreUnused (lo);
        t = juce::String (juce::roundToInt (v * 100.0f)) + " %";
    }
    else if (unit == "Hz")
        t = v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                         : juce::String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
    else if (unit == "ms")
        t = v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    else if (unit == "s")
        t = juce::String (v, v < 10.0f ? 2 : 1) + " s";
    else if (unit == "dB" || unit == "st" || unit == "ct")
        t = (v > 0.0f ? "+" : "") + juce::String (v, unit == "dB" ? 1 : 1) + " " + unit;
    else if (s.kind == Kind::I)
        t = juce::String (juce::roundToInt (v));
    else if (unit.isNotEmpty())
        t = juce::String (v, 2) + " " + unit;
    else
        t = juce::String (v, 2);
    return t.substring (0, maxLen);
}

// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < pid::count; ++i)
    {
        const auto& s = kSpecs[(size_t) i];
        const juce::ParameterID id { s.id, 1 };
        switch (s.kind)
        {
            case Kind::F:
            case Kind::I:
            {
                const ParamSpec* sp = &s;
                juce::NormalisableRange<float> range (
                    s.min, s.max,
                    [sp] (float, float, float n) { return sp->fromNorm (n); },
                    [sp] (float, float, float v) { return sp->toNorm (v); },
                    [sp] (float, float, float v)
                    {
                        v = juce::jlimit (sp->min, sp->max, v);
                        return sp->kind == Kind::I ? std::round (v) : v;
                    });
                if (s.kind == Kind::I)
                    range.interval = 1.0f;
                auto attrs = juce::AudioParameterFloatAttributes()
                                 .withLabel (s.unit)
                                 .withStringFromValueFunction ([i] (float v, int len) { return formatValue (i, v, len > 0 ? len : 16); });
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, range, s.def, attrs));
                break;
            }
            case Kind::B:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, s.name, s.def >= 0.5f));
                break;
            case Kind::C:
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, s.name, choicesFor (s.choices), juce::roundToInt (s.def)));
                break;
        }
    }
    return layout;
}
} // namespace synthia
