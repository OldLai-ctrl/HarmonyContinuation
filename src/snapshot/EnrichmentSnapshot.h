#pragma once
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "core/ImportedProgression.h"
#include <filesystem>

namespace harmony::snapshot {
struct EnrichmentSnapshot {
    static constexpr int currentSchemaVersion = 1;
    int schemaVersion{currentSchemaVersion};
    std::string productVersion;
    ImportedProgressionSession original;
    enrichment::EnrichmentCandidate candidate;
    double tempoBPM{120};
    int meterNumerator{4}, meterDenominator{4};
    std::optional<KeySignature> key;
    std::optional<Style> style;
};
struct EnrichmentDecodeResult {
    EnrichmentSnapshot value;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};
EnrichmentSnapshot captureEnrichment(const ImportedProgressionSession&,
    const enrichment::EnrichmentCandidate&,double tempo,int meterNumerator,int meterDenominator,
    std::optional<KeySignature> key={},std::optional<Style> style={});
std::string serialize(const EnrichmentSnapshot&);
EnrichmentDecodeResult deserializeEnrichment(std::string_view);
bool saveFile(const EnrichmentSnapshot&,const std::filesystem::path&,std::string& error);
EnrichmentDecodeResult loadEnrichmentFile(const std::filesystem::path&);
} // namespace harmony::snapshot
