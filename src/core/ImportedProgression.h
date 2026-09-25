#pragma once

#include "Progression.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace harmony {

enum class TimelineCoordinateMode : std::uint8_t {
    Unknown,
    AbsoluteProjectQN,
    RelativeToSelection
};

// The latest valid progression imported by the user. Coordinates are retained
// exactly as supplied; no analysis or inferred anchor belongs in this session.
struct ImportedProgressionSession {
    Progression events;
    TimelineCoordinateMode coordinateMode{TimelineCoordinateMode::Unknown};
    std::uint64_t revision{};

    bool empty() const noexcept { return events.empty(); }

    // Callers provide the already normalized/sorted parser result. Refuse an
    // invalid order instead of silently changing event identity or timing.
    bool replace(Progression next, TimelineCoordinateMode mode) {
        if (mode == TimelineCoordinateMode::Unknown) return false;
        if (!std::all_of(next.begin(), next.end(), [](const ChordEvent& event) {
                return std::isfinite(event.startQN);
            })) return false;
        if (!std::is_sorted(next.begin(), next.end(), [](const auto& a, const auto& b) {
                return a.startQN < b.startQN;
            })) return false;

        events = std::move(next);
        coordinateMode = mode;
        ++revision;
        if (revision == 0) ++revision;
        return true;
    }
};

} // namespace harmony
