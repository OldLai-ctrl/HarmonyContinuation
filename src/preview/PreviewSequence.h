#pragma once
#include "core/ContinuationEngine.h"
#include "core/ImportedProgression.h"
#include <cstdint>
#include <string>
#include <vector>

namespace harmony::preview {
inline constexpr double defaultTempoBPM=120.0;
inline constexpr double minimumTempoBPM=20.0;
inline constexpr double maximumTempoBPM=400.0;
inline constexpr double defaultOpenQN=4.0;
enum class Segment { Existing, CurrentOpen, Recommended };
struct Chord {
    std::string label;
    int root{}, bass{}; // pitch classes, C=0
    ChordQuality quality{ChordQuality::Unknown};
    std::uint16_t intervals{}; // relative to root
};
struct Event { Chord chord; double startQN{}, durationQN{}; Segment segment{}; };
struct Sequence {
    std::vector<Event> events;
    double tempoBPM{defaultTempoBPM};
    double totalQN{};
    std::size_t recommendationBoundary{};
    std::string sourceRecommendation;
};
struct BuildResult { Sequence sequence; std::string error; explicit operator bool() const noexcept { return error.empty(); } };
double sanitizeTempo(double) noexcept;
double qnToSeconds(double qn,double bpm) noexcept;
std::int64_t qnToSamples(double qn,double bpm,double sampleRate) noexcept;
BuildResult buildSequence(const ImportedProgressionSession&,const ContinuationCandidate*,double tempoBPM);
Chord chordFromLabel(const std::string&,ChordQuality hint=ChordQuality::Unknown);
} // namespace harmony::preview
