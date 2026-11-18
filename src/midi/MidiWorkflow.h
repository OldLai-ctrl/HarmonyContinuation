#pragma once
#include "StandardMidiFileWriter.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
namespace harmony::midi {
struct CandidateExportContext {
    ImportedProgressionSession imported;
    double tempo{120};Meter meter;
    std::optional<KeySignature> key;
    ArrangementMode mode{ArrangementMode::VoiceLed};
    ExportScope scope{ExportScope::FullPhrase};
};
struct PayloadResult {MidiClipPayload payload;std::string error;explicit operator bool()const noexcept{return error.empty()&&!payload.smfBytes.empty();}};
PayloadResult candidatePayload(const CandidateExportContext&,const ContinuationCandidate&);
PayloadResult candidatePayload(const CandidateExportContext&,const enrichment::EnrichmentCandidate&);
struct DragFileResult {std::filesystem::path path;std::string error;explicit operator bool()const noexcept{return error.empty()&&!path.empty();}};
// UI/control thread only. Keep files 48 hours so hosts can read after drop completion.
DragFileResult createDragFile(const MidiClipPayload&,const std::filesystem::path& directory=
    std::filesystem::temp_directory_path()/"HarmonyContinuation"/"MidiDrag");
}
