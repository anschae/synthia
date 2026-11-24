// synthia-render: runs the complete Synthia processor offline over a WAV file.
//
//   synthia-render input.wav preset.xml output.wav [--bpm 120] [--tail 4] [--block 256]
//
// preset: a preset file saved by the plugin (*.synthia), any exported state XML,
//         "factory:<Name>" for a factory preset, or "init" for the defaults.
// Prints peak/RMS of the result and the realtime factor (useful for profiling).

#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "model/PresetManager.h"

namespace
{
class FixedPlayHead : public juce::AudioPlayHead
{
public:
    double bpm = 120.0, ppq = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (true);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

int usage()
{
    std::puts ("usage: synthia-render input.wav preset.xml|factory:<Name>|init output.wav [--bpm 120] [--tail 4] [--block 256] [--viz]\n"
               "       synthia-render --list   (lists the factory presets)");
    return 1;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    if (args.contains ("--list"))
    {
        synthia::SynthiaProcessor proc;
        for (auto& e : proc.presets.list())
            if (e.factory)
                std::printf ("%s  [%s]\n", e.name.toRawUTF8(), e.category.toRawUTF8());
        return 0;
    }

    juce::StringArray positional;
    double bpm = 120.0, tail = 4.0;
    int block = 256;
    bool viz = false;
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--bpm" && i + 1 < args.size()) bpm = args[++i].getDoubleValue();
        else if (args[i] == "--tail" && i + 1 < args.size()) tail = args[++i].getDoubleValue();
        else if (args[i] == "--block" && i + 1 < args.size()) block = juce::jmax (1, args[++i].getIntValue());
        else if (args[i] == "--viz") viz = true; // profile with the editor's live-display data path enabled
        else positional.add (args[i]);
    }
    if (positional.size() < 3)
        return usage();

    const juce::File inFile = juce::File::getCurrentWorkingDirectory().getChildFile (positional[0]);
    const juce::String presetArg = positional[1];
    const juce::File outFile = juce::File::getCurrentWorkingDirectory().getChildFile (positional[2]);

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (inFile));
    if (reader == nullptr)
    {
        std::fprintf (stderr, "cannot read %s\n", inFile.getFullPathName().toRawUTF8());
        return 2;
    }
    const double sr = reader->sampleRate;
    const int inCh = (int) juce::jlimit (1u, 2u, reader->numChannels);
    const auto inLen = (int) reader->lengthInSamples;
    const int tailLen = (int) (tail * sr);
    const int total = inLen + tailLen;

    juce::AudioBuffer<float> input (inCh, inLen);
    reader->read (&input, 0, inLen, 0, true, inCh > 1);

    synthia::SynthiaProcessor proc;
    proc.setPlayConfigDetails (inCh, 2, sr, block);
    // Load the preset before prepare so latency/settings are in place.
    if (presetArg.equalsIgnoreCase ("init"))
        proc.presets.loadInit();
    else if (presetArg.startsWithIgnoreCase ("factory:"))
    {
        if (! proc.presets.loadFactory (presetArg.fromFirstOccurrenceOf (":", false, false)))
        {
            std::fprintf (stderr, "unknown factory preset (see --list)\n");
            return 3;
        }
    }
    else
    {
        auto state = synthia::PresetManager::readStateFile (juce::File::getCurrentWorkingDirectory().getChildFile (presetArg));
        if (! state.isValid())
        {
            std::fprintf (stderr, "cannot read preset %s\n", presetArg.toRawUTF8());
            return 3;
        }
        proc.restoreState (state);
    }

    FixedPlayHead playHead;
    playHead.bpm = bpm;
    proc.setPlayHead (&playHead);
    proc.prepareToPlay (sr, block);
    const int latency = proc.engine.getDesiredLatency();
    proc.engine.viz.active.store (viz);

    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            const int src = juce::jmin (ch, inCh - 1);
            const int avail = juce::jlimit (0, n, inLen - pos);
            if (avail > 0 && (ch < inCh))
                buf.copyFrom (ch, 0, input, src, pos, avail);
        }
        playHead.ppq = (double) pos / (sr * 60.0 / bpm);
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, buf, ch, 0, n);
    }
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;

    float peak = 0.0f;
    double sumSq = 0.0;
    for (int ch = 0; ch < 2; ++ch)
    {
        peak = juce::jmax (peak, out.getMagnitude (ch, 0, total));
        auto* d = out.getReadPointer (ch);
        for (int i = 0; i < total; ++i)
            sumSq += (double) d[i] * d[i];
    }
    const double rms = std::sqrt (sumSq / (2.0 * juce::jmax (1, total)));

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = outFile.createOutputStream();
    if (stream == nullptr)
    {
        std::fprintf (stderr, "cannot write %s\n", outFile.getFullPathName().toRawUTF8());
        return 4;
    }
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, opts);
    if (writer == nullptr)
        return 4;
    writer->writeFromAudioSampleBuffer (out, 0, total);
    writer.reset();

    const double audioSec = (double) total / sr;
    std::printf ("picks detected: %d\n", proc.engine.pickCount.load());
    std::printf ("preset: %s\nlength: %.2f s (+%.1f s tail), %d ch in, %.0f Hz, block %d, latency %d\n"
                 "peak: %.2f dBFS, rms: %.2f dBFS\nrender: %.0f ms = %.1fx realtime (%.1f %% of one core)\n",
                 proc.getPresetName().toRawUTF8(), (double) inLen / sr, tail, inCh, sr, block, latency,
                 juce::Decibels::gainToDecibels (peak), juce::Decibels::gainToDecibels ((float) rms), ms,
                 audioSec * 1000.0 / juce::jmax (1.0, ms), 100.0 * ms / (audioSec * 1000.0));
    return 0;
}
