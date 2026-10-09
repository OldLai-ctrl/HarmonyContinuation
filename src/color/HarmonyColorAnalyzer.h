#pragma once
#include "core/ChordPitchSet.h"
#include "core/ContinuationEngine.h"
#include <span>

namespace harmony::color {
enum class Status { Known, Unknown, Uncertain };
enum class Reason { None, UnsupportedPitchSet, UnsupportedGrade, WideSpan, MultipleDirections, MissingBass };
struct StaticColor {
    Status status{Status::Unknown};
    Reason reason{Reason::UnsupportedPitchSet};
    // Angles are radians in [0, 2*pi). Ambiguous directions are diagnostic only.
    std::vector<double> directions;
    std::optional<double> r,theta,thetaB,T,W,S;
};
struct DynamicColor { double deltaT{},deltaW{},Ts{},Ws{},Ti{}; };
struct TimedChord {
    ChordPitchSet chord;
    std::optional<double> startQN,durationQN;
};
struct PositionColor {
    std::optional<double> startQN,durationQN;
    StaticColor chord;
    std::optional<DynamicColor> fromPrevious;
};
struct PathColor {
    std::vector<PositionColor> positions;
    std::size_t recommendationBoundary{};
    // Whole-path summaries require complete reliable metrics and durations.
    std::optional<double> meanW,meanT,temperatureTrend,tensionTrend;
    std::optional<double> endingDeltaW,endingDeltaT,maxTs;
    bool warming{},cooling{},temperatureTurn{},tensionRising{},tensionReleasing{};
    Status status{Status::Unknown};
};
class HarmonyColorAnalyzer {
public:
    static StaticColor analyze(const ChordPitchSet&);
    static std::optional<DynamicColor> transition(const StaticColor&,const StaticColor&);
    static std::vector<TimedChord> prepare(const Progression&);
    static PathColor analyzePath(std::span<const TimedChord>,std::size_t boundary=0);
    static PathColor analyzeContinuation(std::span<const TimedChord> source,const ContinuationCandidate&);
};
}
