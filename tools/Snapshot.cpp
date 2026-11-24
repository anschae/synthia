// Developer tool: renders the plugin editor pages to PNG files, without a DAW.
//
//   SynthiaSnapshot <output-folder> [factory-preset-name] [scale]
//
// Build with -DSY_BUILD_TOOLS=ON. On Linux without a display, run it under xvfb-run.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>

#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
void pump (int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
}

bool save (juce::Component& c, const juce::File& f)
{
    const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream s (f);
    juce::PNGImageFormat png;
    const bool ok = s.openedOk() && png.writeImageToStream (img, s);
    std::cout << (ok ? "wrote " : "FAILED ") << f.getFullPathName() << std::endl;
    return ok;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 2)
    {
        std::cout << "usage: SynthiaSnapshot <output-folder> [factory-preset] [scale]" << std::endl;
        return 1;
    }
    const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    out.createDirectory();

    synthia::SynthiaProcessor proc;
    if (argc > 2)
        proc.presets.loadFactory (argv[2]);
    // dev tool: switch every layer on so each tab shows live data
    for (int prm : { synthia::pid::vcOn, synthia::pid::spOn, synthia::pid::rsOn, synthia::pid::grOn })
        proc.setParam (prm, 1.0f);
    proc.setParam (synthia::pid::spFreeze, 0.0f);
    proc.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<synthia::SynthiaEditor*> (ed.get());
    if (argc > 3)
        editor->setScale ((float) juce::String (argv[3]).getDoubleValue());
    editor->setVisible (true);

    // Feed plucked notes (F#1 power chord, then single notes) so the live displays have data.
    double t = 0.0;
    auto feed = [&] (double seconds)
    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        const double sr = 48000.0;
        const double notes[] = { 46.25, 69.3, 92.5 };
        for (int b = 0; b < (int) (seconds * sr / 512); ++b)
        {
            for (int i = 0; i < 512; ++i, t += 1.0 / sr)
            {
                const double period = 0.6;
                const double tl = std::fmod (t, period);
                const double env = std::exp (-tl * 3.0);
                double v = 0.0;
                for (double n0 : notes)
                    for (int h = 1; h <= 10; ++h)
                        v += std::sin (juce::MathConstants<double>::twoPi * n0 * h * t) / (h * 1.3);
                const float x = (float) (0.12 * env * v);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
    };
    pump (300);
    auto& v = editor->getView();
    for (int layer = 0; layer < 5; ++layer)
    {
        v.showPage (0);
        v.layers.selectLayer (layer);
        feed (2.5);
        pump (100);
        save (*editor, out.getChildFile ("layers_" + juce::String (layer) + ".png"));
    }
    v.showPage (1);
    feed (1.5);
    pump (100);
    save (*editor, out.getChildFile ("fx.png"));
    v.showPage (2);
    feed (1.5);
    pump (100);
    save (*editor, out.getChildFile ("mod.png"));

    // --bench: average paint time of each page with live data (software renderer)
    if (juce::String (argv[argc - 1]) == "--bench")
    {
        auto bench = [&] (const juce::String& name)
        {
            juce::Image img (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
            const int n = 60;
            double total = 0.0;
            for (int i = 0; i < n; ++i)
            {
                feed (1.0 / 30.0);
                juce::Graphics g (img);
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                editor->paintEntireComponent (g, true);
                total += juce::Time::getMillisecondCounterHiRes() - t0;
            }
            // only the live displays (what the 30 Hz timer actually repaints)
            std::vector<juce::Component*> displays;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                for (auto* ch : c.getChildren())
                {
                    if (! ch->isVisible()) continue;
                    if (dynamic_cast<synthia::VizBox*> (ch) != nullptr || dynamic_cast<synthia::ChordKeyboard*> (ch) != nullptr)
                        displays.push_back (ch);
                    walk (*ch);
                }
            };
            walk (*editor);
            double vizTotal = 0.0;
            for (int i = 0; i < n; ++i)
            {
                feed (1.0 / 30.0);
                for (auto* d : displays)
                {
                    juce::Image im (juce::Image::ARGB, juce::jmax (1, d->getWidth()), juce::jmax (1, d->getHeight()), true);
                    juce::Graphics g (im);
                    const auto t0 = juce::Time::getMillisecondCounterHiRes();
                    d->paintEntireComponent (g, true);
                    vizTotal += juce::Time::getMillisecondCounterHiRes() - t0;
                }
            }
            const double perTick = vizTotal / n;
            std::cout << "paint " << name << ": full frame " << total / n << " ms | " << displays.size() << " displays "
                      << perTick << " ms per tick = " << perTick * 30.0 / 10.0 << " % of one core at 30 fps" << std::endl;
        };
        for (int layer = 0; layer < 5; ++layer)
        {
            v.showPage (0);
            v.layers.selectLayer (layer);
            bench ("layers " + juce::String (layer));
        }
        v.showPage (1);
        bench ("fx");
        v.showPage (2);
        bench ("mod");
    }
    return 0;
}
