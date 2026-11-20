// synthia-render: M0 placeholder (copies input to output). Replaced in M1.
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 4) { std::puts ("usage: synthia-render input.wav preset.xml output.wav [--bpm 120]"); return 1; }
    return 0;
}
