#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace synthia::theme
{
inline const juce::Colour bg { 0xff121318 };
inline const juce::Colour panel { 0xff1b1d24 };
inline const juce::Colour panelEdge { 0xff2a2d37 };
inline const juce::Colour text { 0xffe6e8ef };
inline const juce::Colour textDim { 0xff8a8f9e };
inline const juce::Colour knobBg { 0xff2b2e39 };
inline const juce::Colour mod { 0xfff5d76e };

// one accent per section
inline const juce::Colour voicer { 0xff35d0e8 };
inline const juce::Colour spectral { 0xffa78bfa };
inline const juce::Colour reso { 0xfff59e0b };
inline const juce::Colour granular { 0xff4ade80 };
inline const juce::Colour dry { 0xffb8bcc8 };
inline const juce::Colour filter { 0xfff472b6 };
inline const juce::Colour fx { 0xff60a5fa };
inline const juce::Colour modc { 0xfffacc15 };
inline const juce::Colour input { 0xff94a3b8 };

constexpr int kDesignW = 1200;
constexpr int kDesignH = 790;
} // namespace synthia::theme
