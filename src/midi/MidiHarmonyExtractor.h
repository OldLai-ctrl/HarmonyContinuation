#pragma once
#include "StandardMidiFileReader.h"
#include "core/ImportedProgression.h"
#include <optional>
namespace harmony::midi {
struct MidiHarmonyExtractionConfig {
    double onsetToleranceQN{0.125}, minimumPersistenceQN{0.125};
    bool ignoreDrums{true}, openEnded{};
    std::optional<int> selectedTrack;
};
struct SliceDiagnostic {double startQN{},durationQN{},confidence{};std::string chord;};
struct TrackEvidence {int track{};std::string name;double chordLikeness{};std::size_t noteCount{};};
struct ExtractionResult {
    Progression chords;
    std::vector<SliceDiagnostic> slices;
    std::vector<TrackEvidence> tracks;
    std::vector<std::string> warnings;
    int selectedTrack{-1};std::string selectedTrackName,error;
    explicit operator bool() const noexcept {return error.empty()&&!chords.empty();}
};
ExtractionResult extractHarmony(const MidiFile&,MidiHarmonyExtractionConfig={});
}
