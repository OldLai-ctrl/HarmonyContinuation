#pragma once
#include "core/ContinuationEngine.h"
#include "core/ImportedProgression.h"
#include "core/ChordPitchSet.h"
#include <cstdint>
#include <string>
#include <vector>

namespace harmony::preview {
inline constexpr double defaultTempoBPM=120.0;
inline constexpr double minimumTempoBPM=20.0;
inline constexpr double maximumTempoBPM=400.0;
inline constexpr double defaultOpenQN=4.0;
enum class Segment { Existing, CurrentOpen, Recommended };
enum class PreviewTempoMode { FollowHost, Custom };
enum class PreviewScope { Full, Suffix };
using Chord=harmony::ChordPitchSet;
struct Event { Chord chord; double startQN{}, durationQN{}; Segment segment{}; std::optional<int> topVoice; };
struct Sequence {
    std::vector<Event> events;
    double tempoBPM{defaultTempoBPM};
    double totalQN{};
    std::size_t recommendationBoundary{};
    std::string sourceRecommendation;
};
struct PreviewRange { double startQN{}, endQN{}; };
struct BuildResult { Sequence sequence; std::string error; explicit operator bool() const noexcept { return error.empty(); } };
// Runtime output projection. The search query and ranked candidate stay unchanged.
struct SourceRouteResult {
    ImportedProgressionSession imported;
    ContinuationCandidate candidate;
    std::string error;
    int meterNumerator{4},meterDenominator{4};
    explicit operator bool() const noexcept { return error.empty(); }
};
double sanitizeTempo(double) noexcept;
double qnToSeconds(double qn,double bpm) noexcept;
std::int64_t qnToSamples(double qn,double bpm,double sampleRate) noexcept;
PreviewRange previewRange(const Sequence&,PreviewScope) noexcept;
BuildResult buildSequence(const ImportedProgressionSession&,const ContinuationCandidate*,double tempoBPM,
    const HarmonyConstraintSet& constraints={});
BuildResult buildTemplateSequence(const ProgressionTemplate&,KeySignature,double tempoBPM);
// Complete authored route without a query or a fabricated match; empty candidate suffix.
SourceRouteResult templateRoute(const ProgressionTemplate&,KeySignature);
SourceRouteResult sourceRoute(const ImportedProgressionSession&,const ContinuationCandidate&,const ProgressionTemplate&);
Chord chordFromLabel(const std::string&,ChordQuality hint=ChordQuality::Unknown);
} // namespace harmony::preview
