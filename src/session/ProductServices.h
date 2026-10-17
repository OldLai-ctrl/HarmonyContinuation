#pragma once
#include "PluginSessionState.h"
#include "core/ContinuationEngine.h"
#include "library/ProgressionLibrary.h"

namespace harmony::session {
struct RecommendationVisibilityPolicy {
    float relativeToGroupBest{0.f}; // 0 disables; otherwise max score drop
    std::size_t maxPerGroup{3};
    float minimumDiversityDistance{0.1f}; // default placement only; More keeps all qualified paths
    bool includeExploratory{}; // explicit saved-snapshot view only
    std::vector<std::size_t> visibleIndices(const std::vector<ContinuationCandidate>&) const;
};
using RecommendationPresentation = std::array<std::vector<std::size_t>, 4>;
RecommendationPresentation presentationIndices(const RecommendationSet&,
    const RecommendationVisibilityPolicy&);
// Called only by the explicit local export action, off the audio thread.
std::string recommendationDiagnostics(const Progression&,const AnalysisContext&,
    const RecommendationRequest&,const RecommendationSet&);
struct SaveMetadata {
    std::string name;
    std::optional<Style> style;
    PhraseIntent intent{PhraseIntent::Neutral};
    std::vector<std::string> tags;
    std::string note;
};
struct SaveResult { ProgressionTemplate item; std::string error; explicit operator bool() const noexcept { return error.empty(); } };
SaveResult makeUserProgression(const ImportedProgressionSession&, const ContinuationCandidate&, const SaveMetadata&);
bool saveRecommendation(library::UserLibrary&, const ImportedProgressionSession&,
                        const ContinuationCandidate&, const SaveMetadata&, std::string& error);
std::string continuationFingerprint(const ContinuationCandidate&);
std::string legacyContinuationFingerprint(const ContinuationCandidate&);
bool continuationReferenceMatches(const ContinuationCandidate&, std::string_view);
} // namespace harmony::session
