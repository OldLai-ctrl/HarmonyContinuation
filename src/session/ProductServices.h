#pragma once
#include "PluginSessionState.h"
#include "core/ContinuationEngine.h"
#include "library/ProgressionLibrary.h"

namespace harmony::session {
struct RecommendationVisibilityPolicy {
    float absoluteMinimumScore{60.f};
    float relativeToGroupBest{0.f}; // 0 disables; otherwise max score drop
    std::size_t maxPerGroup{3};
    float minimumDiversityDistance{0.f}; // normalized path difference; 0 disables
    std::vector<std::size_t> visibleIndices(const std::vector<ContinuationCandidate>&) const;
};
struct SaveMetadata {
    std::string name;
    std::optional<Style> style;
    PhraseIntent intent{PhraseIntent::Neutral};
    std::vector<std::string> tags;
};
struct SaveResult { ProgressionTemplate item; std::string error; explicit operator bool() const noexcept { return error.empty(); } };
SaveResult makeUserProgression(const ImportedProgressionSession&, const ContinuationCandidate&, const SaveMetadata&);
bool saveRecommendation(library::UserLibrary&, const ImportedProgressionSession&,
                        const ContinuationCandidate&, const SaveMetadata&, std::string& error);
std::string continuationFingerprint(const ContinuationCandidate&);
} // namespace harmony::session
