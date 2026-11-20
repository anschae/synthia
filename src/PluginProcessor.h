#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"
#include "model/PresetManager.h"

namespace synthia
{
/** Thin wrapper: APVTS + Engine + state + presets. All DSP lives in dsp::Engine. */
class SynthiaProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SynthiaProcessor();
    ~SynthiaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return SY_PLUGIN_NAME; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return engine.getTailSeconds(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // ---- state helpers (message thread)
    /** Full state: <SynthiaState stateVersion presetName> + APVTS child. */
    juce::ValueTree captureState() const;
    void restoreState (const juce::ValueTree& state);
    /** Resets every parameter to its default (used by presets). */
    void resetToDefaults();
    void setParam (int index, float realValue);
    float getParam (int index) const;

    static juce::ValueTree migrate (juce::ValueTree state, int fromVersion);

    juce::String getPresetName() const { return presetName; }
    void setPresetName (const juce::String& n) { presetName = n; }

    // UI settings (not automatable; stored in the state)
    float chaos = 0.35f;
    float uiScale = 1.0f;

    juce::AudioProcessorValueTreeState apvts;
    dsp::Engine engine;
    PresetManager presets;

private:
    void timerCallback() override;

    juce::String presetName { "Init" };
    int reportedLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthiaProcessor)
};
} // namespace synthia
