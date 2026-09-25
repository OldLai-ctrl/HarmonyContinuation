#pragma once
#include "core/Progression.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace harmony::ui {
struct TimelineBlock {
    double x{};
    double width{};
    std::size_t eventIndex{};
    bool openRightEdge{};
};
inline double timelineSpanQN(const Progression& events) {
    if(events.empty())return 0;
    return events.back().startQN-events.front().startQN+events.back().durationQN.value_or(2.0);
}
// Pure layout helper, not a VSTGUI view. The eventual view draws these blocks.
// Includes gaps/negative project positions via an origin at the first event.
// The final unknown duration occupies two visual QN without changing the model.
inline std::vector<TimelineBlock> layoutTimeline(const Progression& events, double width) {
    if (events.empty() || !std::isfinite(width) || width <= 0) return {};
    const auto origin = events.front().startQN;
    const auto span = timelineSpanQN(events);
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
struct ScrollableTimelineLayout {
    double contentWidth{};
    std::vector<TimelineBlock> blocks;
};
inline ScrollableTimelineLayout layoutScrollableTimeline(const Progression& events,double viewportWidth,
                                                          double minimumChordWidth=56.0) {
    ScrollableTimelineLayout out;
    if(events.empty()||!std::isfinite(viewportWidth)||viewportWidth<=0)return out;
    out.contentWidth=viewportWidth;
    const double span=timelineSpanQN(events);
    double shortest=std::numeric_limits<double>::infinity();
    for(const auto& event:events) {
        const auto duration=event.durationQN.value_or(2.0);
        if(std::isfinite(duration)&&duration>0)shortest=std::min(shortest,duration);
    }
    if(std::isfinite(span)&&span>0&&std::isfinite(shortest))
        out.contentWidth=std::max(viewportWidth,std::min(100000.,span*minimumChordWidth/shortest));
    out.blocks=layoutTimeline(events,out.contentWidth);
    return out;
}
}
