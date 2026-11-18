#include "core/CurrentChordLocator.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
int failures{};

void check(bool condition, std::string_view name) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << name << '\n';
}

harmony::ChordEvent event(double start, bool open = false) {
    harmony::ChordEvent value;
    value.startQN = start;
    value.openEnded = open;
    return value;
}

void checkActive(const harmony::ImportedProgressionSession& session, double qn,
                std::optional<std::size_t> expected, std::string_view name) {
    const auto location = harmony::locateCurrentChord(session, qn);
    check(location.activeIndex == expected, name);
}
}

int main() {
    using namespace harmony;
    ImportedProgressionSession session;
    check(session.empty(), "new session is empty");
    check(session.coordinateMode == TimelineCoordinateMode::Unknown, "new session coordinate is unknown");
    check(session.replace({event(32), event(36), event(38), event(39, true)},
                          TimelineCoordinateMode::AbsoluteProjectQN), "accept sorted absolute progression");
    check(session.revision == 1 && !session.empty(), "session revision advances on import");

    checkActive(session, 31.0, std::nullopt, "before first event has no active chord");
    check(locateCurrentChord(session, 31.0).nextIndex == 0, "before first event points to next chord");
    checkActive(session, 32.0, 0, "first event includes its start boundary");
    checkActive(session, 35.999, 0, "first event excludes the next boundary");
    checkActive(session, 36.0, 1, "second event includes its start boundary");
    checkActive(session, 37.9, 1, "second event uses the next start boundary");
    checkActive(session, 38.0, 2, "third event includes its start boundary");
    checkActive(session, 38.999, 2, "third event excludes the open event boundary");
    checkActive(session, 39.0, 3, "open event includes its start boundary");
    checkActive(session, 100.0, 3, "open event remains active after its start");

    const auto inside = locateCurrentChord(session, 37.25);
    check(inside.previousIndex == 0 && inside.activeIndex == 1 && inside.nextIndex == 2,
          "neighbor indices are reported around the active event");
    check(std::abs(inside.qnIntoChord - 1.25) < 1e-12, "QN into active chord is calculated");
    check(!locateCurrentChord(session, std::nullopt).activeIndex,
          "invalid ProjectTimeMusic clears the active event");
    check(!locateCurrentChord(session, std::numeric_limits<double>::infinity()).activeIndex,
          "non-finite position clears the active event");

    // Independent lookups prove that transport jumps in either direction do
    // not depend on the previous position.
    checkActive(session, 33.0, 0, "transport before forward seek");
    checkActive(session, 39.5, 3, "forward locate jumps directly to destination");
    checkActive(session, 34.0, 0, "backward locate jumps directly to destination");

    ImportedProgressionSession empty;
    check(!locateCurrentChord(empty, 50.0).activeIndex, "empty progression has no active chord");

    ImportedProgressionSession single;
    check(single.replace({event(-2.0, true)}, TimelineCoordinateMode::AbsoluteProjectQN),
          "accept single open event");
    check(!locateCurrentChord(single, -2.001).activeIndex, "single event is inactive before its start");
    checkActive(single, -2.0, 0, "single open event starts on its boundary");
    checkActive(single, 5.0, 0, "single open event remains active");
    check(!locateCurrentChord(single, -1.0).previousIndex &&
          !locateCurrentChord(single, -1.0).nextIndex,
          "single event has no neighboring events");

    ImportedProgressionSession negative;
    check(negative.replace({event(-8), event(-4, true)}, TimelineCoordinateMode::AbsoluteProjectQN),
          "accept negative event positions");
    checkActive(negative, -5.0, 0, "negative project QN is located");
    checkActive(negative, -4.0, 1, "negative event boundary is inclusive");

    ImportedProgressionSession unsorted;
    check(!unsorted.replace({event(36), event(32)}, TimelineCoordinateMode::AbsoluteProjectQN),
          "session rejects unsorted event arrays");
    check(unsorted.empty() && unsorted.revision == 0, "rejected import does not alter session");
    check(!unsorted.replace({event(32)}, TimelineCoordinateMode::Unknown),
          "session rejects unknown coordinate mode");

    ImportedProgressionSession relative;
    check(relative.replace({event(0), event(4, true)}, TimelineCoordinateMode::RelativeToSelection),
          "relative coordinate session is retained");
    check(!locateCurrentChord(relative, 1.0).activeIndex,
          "locator does not map relative coordinates to host project QN");

    if (failures) {
        std::cerr << failures << " timeline test(s) failed\n";
        return 1;
    }
    std::cout << "Core timeline tests passed\n";
    return 0;
}
