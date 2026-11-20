#include "model/PresetManager.h"
#include "model/FactoryPresets.h"
#include "PluginProcessor.h"

namespace synthia
{
namespace
{
juce::File& folderOverride()
{
    static juce::File f;
    return f;
}
} // namespace

PresetManager::PresetManager (SynthiaProcessor& p) : proc (p) {}

juce::File PresetManager::getUserFolder()
{
    if (folderOverride() != juce::File())
        return folderOverride();
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Synthia").getChildFile ("Presets");
}

void PresetManager::setUserFolderOverride (const juce::File& f) { folderOverride() = f; }

std::vector<PresetManager::Entry> PresetManager::list() const
{
    std::vector<Entry> out;
    const auto& fp = factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        out.push_back ({ fp[(size_t) i].name, true, i, {}, fp[(size_t) i].category });
    auto folder = getUserFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0; });
        for (auto& f : files)
            out.push_back ({ f.getFileNameWithoutExtension(), false, -1, f, "User" });
    }
    return out;
}

void PresetManager::loadInit()
{
    proc.resetToDefaults();
    proc.setPresetName ("Init");
    changed();
}

bool PresetManager::load (const Entry& e)
{
    if (e.factory)
    {
        const auto& fp = factoryPresets();
        if (e.factoryIndex < 0 || e.factoryIndex >= (int) fp.size())
            return false;
        applyFactoryPreset (proc, fp[(size_t) e.factoryIndex]);
        proc.setPresetName (e.displayName());
        changed();
        return true;
    }
    auto state = readStateFile (e.file);
    if (! state.isValid())
        return false;
    proc.restoreState (state);
    proc.setPresetName (e.displayName());
    changed();
    return true;
}

bool PresetManager::loadFactory (const juce::String& name)
{
    for (auto& e : list())
        if (e.factory && e.name.equalsIgnoreCase (name))
            return load (e);
    return false;
}

void PresetManager::loadNext (int direction)
{
    auto entries = list();
    if (entries.empty())
        return;
    const auto current = proc.getPresetName();
    int idx = -1;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].displayName() == current)
            idx = i;
    const int n = (int) entries.size();
    idx = idx < 0 ? (direction > 0 ? 0 : n - 1) : ((idx + direction) % n + n) % n;
    load (entries[(size_t) idx]);
}

juce::ValueTree PresetManager::readStateFile (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.hasType ("SynthiaState"))
            return t;
    }
    return {};
}

bool PresetManager::saveUser (const juce::String& nameIn)
{
    const auto name = juce::File::createLegalFileName (nameIn.trim());
    if (name.isEmpty())
        return false;
    auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;
    proc.setPresetName ("User / " + name);
    auto state = proc.captureState();
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (name + kExtension)))
        return false;
    changed();
    return true;
}

bool PresetManager::renameUser (const Entry& e, const juce::String& newNameIn)
{
    const auto newName = juce::File::createLegalFileName (newNameIn.trim());
    if (e.factory || newName.isEmpty())
        return false;
    auto target = e.file.getSiblingFile (newName + kExtension);
    if (target.exists() || ! e.file.moveFileTo (target))
        return false;
    if (proc.getPresetName() == e.displayName())
        proc.setPresetName ("User / " + newName);
    changed();
    return true;
}

bool PresetManager::deleteUser (const Entry& e)
{
    if (e.factory || ! e.file.existsAsFile())
        return false;
    const bool ok = e.file.deleteFile();
    changed();
    return ok;
}

void PresetManager::switchAB (int index)
{
    index = juce::jlimit (0, 1, index);
    if (index == activeAB)
        return;
    ab[activeAB] = proc.captureState();
    if (! ab[index].isValid())
        ab[index] = ab[activeAB].createCopy();
    activeAB = index;
    proc.restoreState (ab[index]);
    changed();
}

void PresetManager::copyActiveToOther()
{
    ab[1 - activeAB] = proc.captureState();
    changed();
}
} // namespace synthia
