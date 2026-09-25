#pragma once
#include "preview/ChordVoicer.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace harmony::midi {
enum class ArrangementMode { BlockChords, VoiceLed };
enum class ExportScope { FullPhrase, CurrentOnly, ContinuationOnly };
enum class MarkerType { ChordLabel, RecommendedStart };
struct Meter { int numerator{4}, denominator{4}; };
struct Note { int midiNote{}; double startQN{}, durationQN{}; std::uint8_t velocity{80}, channel{}; bool bass{}; };
struct Marker { double qn{}; MarkerType type{}; std::string text; };
struct ExportSequence {
    std::vector<Note> notes;
    std::vector<Marker> markers;
    double tempoBPM{120}, totalQN{};
    Meter meter;
    std::optional<double> boundaryQN;
    std::optional<KeySignature> key;
    std::optional<PhraseIntent> intent;
    ArrangementMode mode{ArrangementMode::VoiceLed};
    ExportScope scope{ExportScope::FullPhrase};
    std::size_t chordCount{};
};
struct ExportConfig {
    std::uint16_t ppq{480};
    std::uint8_t upperVelocity{80}, bassVelocity{90}, channel{0};
};
struct BuildResult { ExportSequence sequence; std::string error; explicit operator bool() const noexcept {return error.empty();} };
BuildResult buildClip(const preview::Sequence&,ArrangementMode,ExportScope,Meter,
                      std::optional<KeySignature> key={},std::optional<PhraseIntent> intent={},
                      ExportConfig config={});
preview::BuildResult previewFromTemplate(const ProgressionTemplate&,KeySignature,double tempoBPM);
std::int64_t qnToTicks(double qn,std::uint16_t ppq) noexcept;
struct MidiClipPayload {
    std::vector<std::uint8_t> smfBytes;
    std::string suggestedFilename;
    std::size_t chordCount{}, noteCount{};
    std::optional<std::int64_t> boundaryTick;
};
std::string suggestedFilename(std::optional<PhraseIntent>,std::optional<KeySignature>,int candidateNumber=1);
} // namespace harmony::midi
