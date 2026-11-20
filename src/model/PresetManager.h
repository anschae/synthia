#pragma once
// Presets (factory + user files) and A/B compare. Message thread only.
// User presets are XML files (*.synthia) holding the full plugin state, in
//   ~/Documents/Synthia/Presets        (macOS)
//   %USERPROFILE%\Documents\Synthia\Presets   (Windows)
// The same files are accepted by synthia-render.

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace synthia
{
class SynthiaProcessor;

class PresetManager
{
public:
    explicit PresetManager (SynthiaProcessor& p);

    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
        juce::String category;
        juce::String displayName() const { return (factory ? "Factory / " : "User / ") + name; }
    };

    /** Factory presets first, then user presets (alphabetical). Rescans the folder. */
    std::vector<Entry> list() const;

    bool load (const Entry& e);
    bool loadFactory (const juce::String& name);
    void loadNext (int direction);
    bool saveUser (const juce::String& name); // overwrites an existing file of that name
    bool renameUser (const Entry& e, const juce::String& newName);
    bool deleteUser (const Entry& e);
    void loadInit();

    static juce::File getUserFolder();
    static void setUserFolderOverride (const juce::File& f);
    static constexpr const char* kExtension = ".synthia";

    /** Reads a preset/state XML file into a state tree (invalid tree on failure). */
    static juce::ValueTree readStateFile (const juce::File& f);

    // ---- A/B compare
    int getActiveAB() const noexcept { return activeAB; }
    void switchAB (int index);
    void copyActiveToOther();

    std::function<void()> onChange;

private:
    void changed() { if (onChange) onChange(); }
    SynthiaProcessor& proc;
    juce::ValueTree ab[2];
    int activeAB = 0;
};
} // namespace synthia
