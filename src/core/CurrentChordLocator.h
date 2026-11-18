#pragma once

#include "ImportedProgression.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>

namespace harmony {

struct ChordLocation {
    std::optional<std::size_t> activeIndex;
    std::optional<std::size_t> previousIndex;
    std::optional<std::size_t> nextIndex;
    double qnIntoChord{};
};

// Stateless lookup: every position is resolved from the event starts, so
// seeking, looping, and reverse transport work without playback history.
inline ChordLocation locateCurrentChord(
    std::span<const ChordEvent> sortedEvents,
    std::optional<double> currentProjectQN) noexcept {
    ChordLocation result;
    if (sortedEvents.empty() || !currentProjectQN || !std::isfinite(*currentProjectQN))
        return result;

    const auto first = sortedEvents.begin();
    const auto last = std::upper_bound(first, sortedEvents.end(), *currentProjectQN,
        [](double qn, const ChordEvent& event) { return qn < event.startQN; });

    if (last == first) {
        result.nextIndex = 0;
        return result;
    }

    const auto active = static_cast<std::size_t>((last - first) - 1);
    result.activeIndex = active;
    if (active > 0) result.previousIndex = active - 1;
    if (active + 1 < sortedEvents.size()) result.nextIndex = active + 1;
    result.qnIntoChord = std::max(0.0, *currentProjectQN - sortedEvents[active].startQN);
    return result;
}

inline ChordLocation locateCurrentChord(
    const ImportedProgressionSession& session,
    std::optional<double> currentProjectQN) noexcept {
    if (session.coordinateMode != TimelineCoordinateMode::AbsoluteProjectQN)
        return {};
    return locateCurrentChord(session.events, currentProjectQN);
}

} // namespace harmony
