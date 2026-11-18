#pragma once
#include "MidiHarmonyExtractor.h"
#include "core/ImportedProgression.h"
namespace harmony::midi {
struct ImportResult {
    ReadResult midi;
    ExtractionResult extraction;
    explicit operator bool() const noexcept {return midi&&extraction;}
};
ImportResult importFile(const std::filesystem::path&,bool openEnded=false);
// A single Session meter is meaningful only when it covers the imported region.
// Missing metadata and changes within that region remain unknown.
std::optional<std::pair<int,int>> uniformImportedMeter(const ImportResult&);
inline bool applyImport(const ImportResult& result,ImportedProgressionSession& session) {
    return result && session.replace(result.extraction.chords,TimelineCoordinateMode::RelativeToSelection);
}
std::optional<std::filesystem::path> chooseMidiFile(void* owner);
}
