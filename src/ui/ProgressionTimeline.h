#pragma once
#include "core/Progression.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace harmony::ui {
struct TimelineBlock {
    double x{};
    double width{};
    std::size_t eventIndex{};
    bool openRightEdge{};
};
// Pure layout helper, not a VSTGUI view. The eventual view draws these blocks.
// Includes gaps/negative project positions via an origin at the first event.
// The final unknown duration occupies two visual QN without changing the model.
inline std::vector<TimelineBlock> layoutTimeline(const Progression& events, double width) {
    if (events.empty() || !std::isfinite(width) || width <= 0) return {};
    const auto origin = events.front().startQN;
    const auto span = events.back().startQN - origin + 2.0;
    if (!std::isfinite(span) || span <= 0) return {};
    std::vector<TimelineBlock> blocks;
    blocks.reserve(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        blocks.push_back({(event.startQN - origin) / span * width,
                          event.durationQN.value_or(2.0) / span * width,
                          i, event.openEnded});
    }
    return blocks;
}
}
