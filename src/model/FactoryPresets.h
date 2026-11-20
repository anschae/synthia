#pragma once
// Built-in presets, defined in code as differences to the defaults.

#include <vector>

namespace synthia
{
class SynthiaProcessor;

struct PresetValue
{
    const char* id;
    float value;             // real value (choice index, 0/1 for bools)
    const char* dst = nullptr; // for m{n}Dst: destination parameter id (value is ignored)
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<PresetValue> values;
};

const std::vector<FactoryPreset>& factoryPresets();

/** Resets to defaults and applies the preset (message thread). */
void applyFactoryPreset (SynthiaProcessor& proc, const FactoryPreset& preset);
} // namespace synthia
