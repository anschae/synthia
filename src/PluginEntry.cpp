// Plugin entry point. Kept out of the shared code so the unit tests and the
// offline renderer can link the processor without the plugin wrappers.

#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new synthia::SynthiaProcessor();
}
