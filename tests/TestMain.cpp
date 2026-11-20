// Catch2 runner. JUCE needs to be initialised (message manager etc.) before
// any processor or component is created, so we provide our own main().
#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    return Catch::Session().run (argc, argv);
}
