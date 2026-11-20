#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace synthia
{
SynthiaProcessor::SynthiaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      presets (*this)
{
    for (int i = 0; i < pid::count; ++i)
        engine.attachParameter (i, apvts.getRawParameterValue (paramId (i)));
    // Latency depends on these two parameters only (Align Spectral). No polling timer: on Linux
    // hosts a permanently running timer keeps the host run loop busy (pluginval crashes seen).
    apvts.addParameterListener (paramId (pid::alignSpec), this);
    apvts.addParameterListener (paramId (pid::spSize), this);
}

SynthiaProcessor::~SynthiaProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (paramId (pid::alignSpec), this);
    apvts.removeParameterListener (paramId (pid::spSize), this);
}

void SynthiaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    reportedLatency = dsp::Engine::latencyFor (getParam (pid::alignSpec) >= 0.5f, (int) getParam (pid::spSize), sampleRate);
    setLatencySamples (reportedLatency);
}

bool SynthiaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SynthiaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    dsp::HostTiming host;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            host.valid = true;
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) host.ppqAtBlockStart = *ppq;
            host.playing = pos->getIsPlaying();
            if (auto sig = pos->getTimeSignature())
                if (sig->denominator > 0)
                    host.quartersPerBar = 4.0 * sig->numerator / sig->denominator;
        }
    }
    engine.process (buffer, juce::jmax (1, numIn), host);
}

juce::AudioProcessorParameter* SynthiaProcessor::getBypassParameter() const
{
    return apvts.getParameter (paramId (pid::bypass));
}

void SynthiaProcessor::handleAsyncUpdate()
{
    // Latency changes (Align Spectral / FFT size) are reported from the message thread.
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const int want = dsp::Engine::latencyFor (getParam (pid::alignSpec) >= 0.5f, (int) getParam (pid::spSize), sr);
    if (want != reportedLatency)
    {
        reportedLatency = want;
        setLatencySamples (want);
    }
}

// ---------------------------------------------------------------------------
float SynthiaProcessor::getParam (int index) const
{
    return apvts.getRawParameterValue (paramId (index))->load();
}

void SynthiaProcessor::setParam (int index, float v)
{
    if (auto* prm = apvts.getParameter (paramId (index)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void SynthiaProcessor::resetToDefaults()
{
    for (int i = 0; i < pid::count; ++i)
        if (auto* prm = apvts.getParameter (paramId (i)))
            prm->setValueNotifyingHost (prm->getDefaultValue());
}

juce::ValueTree SynthiaProcessor::captureState() const
{
    juce::ValueTree root ("SynthiaState");
    root.setProperty ("stateVersion", kStateVersion, nullptr);
    root.setProperty ("pluginVersion", SY_VERSION_STRING, nullptr);
    root.setProperty ("presetName", presetName, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("chaos", chaos, nullptr);
    root.addChild (const_cast<juce::AudioProcessorValueTreeState&> (apvts).copyState(), -1, nullptr);
    return root;
}

juce::ValueTree SynthiaProcessor::migrate (juce::ValueTree state, int fromVersion)
{
    // Migration hook: when the state layout changes, bump kStateVersion and
    // convert older trees here (fromVersion < kStateVersion).
    juce::ignoreUnused (fromVersion);
    return state;
}

void SynthiaProcessor::restoreState (const juce::ValueTree& stateIn)
{
    if (! stateIn.hasType ("SynthiaState"))
        return;
    const int version = stateIn.getProperty ("stateVersion", 1);
    auto state = migrate (stateIn.createCopy(), version);
    auto params = state.getChildWithName ("PARAMS");
    if (params.isValid())
    {
        // Parameters missing from the stored tree (older versions) fall back to their defaults.
        resetToDefaults();
        for (int i = 0; i < params.getNumChildren(); ++i)
        {
            auto c = params.getChild (i);
            const auto id = c.getProperty ("id").toString();
            if (auto* prm = apvts.getParameter (id))
            {
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    rp->setValueNotifyingHost (rp->convertTo0to1 ((float) c.getProperty ("value")));
            }
        }
    }
    presetName = state.getProperty ("presetName", "Init").toString();
}

void SynthiaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = captureState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SynthiaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        restoreState (state);
        // UI settings come only from the host session (not from presets / A-B)
        uiScale = juce::jlimit (0.75f, 2.0f, (float) state.getProperty ("uiScale", uiScale));
        chaos = juce::jlimit (0.0f, 1.0f, (float) state.getProperty ("chaos", chaos));
    }
}

juce::AudioProcessorEditor* SynthiaProcessor::createEditor()
{
    return new SynthiaEditor (*this);
}
} // namespace synthia
