#pragma once
#include "HarmonyAnalysis.h"

namespace harmony {
struct ChordPitchSet {
    std::string label;
    int root{}, bass{};
    ChordQuality quality{ChordQuality::Unknown};
    std::uint16_t intervals{};
    std::uint16_t colorMask{};
    bool exactIntervals{};
};
ChordPitchSet chordPitches(const ChordEvent&);
ChordPitchSet chordPitches(const std::string&, ChordQuality hint=ChordQuality::Unknown);
}
