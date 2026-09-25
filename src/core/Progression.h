#pragma once
#include "Chord.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace harmony {
using Progression = std::vector<ChordEvent>;
// Duplicate starts are retained as simultaneous events with zero duration.
inline bool sortAndInferDurations(Progression& events) {
    for (const auto& event : events)
        if (!std::isfinite(event.startQN)) return false;
    std::stable_sort(events.begin(), events.end(),
                     [](const auto& a, const auto& b) { return a.startQN < b.startQN; });
    for (std::size_t i = 0; i < events.size(); ++i) {
        auto& event = events[i];
        event.durationQN.reset();
        event.openEnded = i + 1 == events.size();
        if (!event.openEnded) {
            const auto duration = events[i + 1].startQN - event.startQN;
            if (!std::isfinite(duration)) return false;
            event.durationQN = duration;
        }
    }
    return true;
}
}
