#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
namespace harmony::midi {
struct MidiNoteEvent {
    int pitch{}, velocity{}, channel{}, track{};
    double startQN{}, durationQN{};
};
struct MidiTrack { int index{}; std::string name; std::size_t notes{}, percussionNotes{}; };
struct TempoEvent { double qn{}, bpm{}; };
struct TimeSignatureEvent { double qn{}; int numerator{}, denominator{}; };
struct MidiFile {
    int format{}, ppq{};
    double totalQN{};
    std::vector<MidiNoteEvent> notes;
    std::vector<MidiTrack> tracks;
    std::vector<TempoEvent> tempos;
    std::vector<TimeSignatureEvent> meters;
    std::vector<std::string> warnings;
};
enum class ReadStatus { Success, Malformed, Unsupported, ResourceLimit, FileError };
struct ReadResult {
    MidiFile file;
    ReadStatus status{ReadStatus::Success};
    std::string error;
    explicit operator bool() const noexcept { return status==ReadStatus::Success; }
};
ReadResult readFromMemory(std::span<const std::uint8_t>);
ReadResult readFromFile(const std::filesystem::path&);
}
