#pragma once
#include "core/ContinuationEngine.h"
#include "core/ImportedProgression.h"
#include <filesystem>

namespace harmony::snapshot {
struct RecommendationSnapshot {
    static constexpr int currentSchemaVersion=1;
    int schemaVersion{currentSchemaVersion};
    ImportedProgressionSession imported;
    ContinuationCandidate candidate;
    std::optional<MatchResult> match;
    double tempoBPM{120};
    int meterNumerator{4}, meterDenominator{4};
    std::optional<KeySignature> key;
    std::optional<Style> style;
    std::optional<PhraseIntent> intent;
};
struct DecodeResult { RecommendationSnapshot value; std::string error; explicit operator bool() const noexcept {return error.empty();} };
RecommendationSnapshot capture(const ImportedProgressionSession&,const ContinuationCandidate&,
    const std::vector<MatchResult>&,double tempo,int meterNumerator,int meterDenominator,
    std::optional<KeySignature> key={},std::optional<Style> style={},std::optional<PhraseIntent> intent={});
std::string serialize(const RecommendationSnapshot&);
DecodeResult deserialize(std::string_view);
bool saveFile(const RecommendationSnapshot&,const std::filesystem::path&,std::string& error);
DecodeResult loadFile(const std::filesystem::path&);
} // namespace harmony::snapshot
