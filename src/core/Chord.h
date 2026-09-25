#pragma once
#include "MusicTypes.h"
#include <optional>
#include <string>

namespace harmony {
struct ChordExtensions {
    // Raw until the producer's bit/pitch encodings have been verified.
    std::optional<std::string> pitches;
    std::optional<std::string> mask;
    std::optional<std::string> type;
    std::optional<std::string> color;
};
struct ChordEvent {
    // PitchClass fields are retained for the synthetic-v1 fixture only. Clipboard
    // VST-XML uses producer note numbers, stored losslessly below.
    std::optional<PitchClass> root;
    std::optional<PitchClass> bass; // Missing bass is not silently invented.
    std::optional<std::int32_t> keyNoteValue;
    std::optional<std::int32_t> bassNoteValue;
    std::optional<std::string> rawId;
    std::optional<std::string> rawName;
    std::optional<std::string> rawKeyNote;
    std::optional<std::string> rawBassNote;
    std::optional<std::string> rawProjectTime;
    std::optional<std::string> rawTimeDomain;
    ChordQuality quality{ChordQuality::Unknown};
    ChordExtensions extensions;
    std::string name;
    double startQN{};
    std::optional<double> durationQN;
    bool openEnded{true};
    ChordSource source{ChordSource::SyntheticFixture};
};
}
