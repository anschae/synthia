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
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (1);
    for (int b = 0; b < 100; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, (b % 20 < 3) ? (rng.nextFloat() - 0.5f) * 0.8f : 0.0f);
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<synthia::SynthiaEditor*> (ed.get());
    if (argc > 3)
        editor->setScale ((float) juce::String (argv[3]).getDoubleValue());
    editor->setVisible (true);
    pump (300);
    auto& v = editor->getView();
    for (int layer = 0; layer < 5; ++layer)
    {
        v.showPage (0);
        v.layers.selectLayer (layer);
        pump (100);
        save (*editor, out.getChildFile ("layers_" + juce::String (layer) + ".png"));
    }
    v.showPage (1);
    pump (100);
    save (*editor, out.getChildFile ("fx.png"));
    v.showPage (2);
    pump (100);
    save (*editor, out.getChildFile ("mod.png"));
    return 0;
}
