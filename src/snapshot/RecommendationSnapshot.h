#pragma once
#include "core/ContinuationEngine.h"
#include "core/ImportedProgression.h"
#include <filesystem>
#include "library/ProgressionLibrary.h"

namespace harmony::snapshot {
struct RecommendationSnapshot {
    static constexpr int currentSchemaVersion=3;
    int schemaVersion{currentSchemaVersion};
    std::string productVersion;
    ImportedProgressionSession imported;
    ContinuationCandidate candidate;
    std::optional<MatchResult> match;
    double tempoBPM{120};
    int meterNumerator{4}, meterDenominator{4};
    std::optional<KeySignature> key;
    std::optional<Style> style;
    std::optional<PhraseIntent> intent;
    int factoryLibraryVersion{2};
    bool candidateAvailable{true}; // Runtime fallback; original phrase is still usable.
};
struct DecodeResult { RecommendationSnapshot value; std::string error; explicit operator bool() const noexcept {return error.empty();} };
RecommendationSnapshot capture(const ImportedProgressionSession&,const ContinuationCandidate&,
    const std::vector<MatchResult>&,double tempo,int meterNumerator,int meterDenominator,
    std::optional<KeySignature> key={},std::optional<Style> style={},std::optional<PhraseIntent> intent={});
std::string serialize(const RecommendationSnapshot&);
DecodeResult deserialize(std::string_view,const library::LoadResult* activeFactory=nullptr);
void restoreFactoryReferences(RecommendationSnapshot&,const library::LoadResult&);
bool saveFile(const RecommendationSnapshot&,const std::filesystem::path&,std::string& error);
DecodeResult loadFile(const std::filesystem::path&,const library::LoadResult* activeFactory=nullptr);
} // namespace harmony::snapshot
