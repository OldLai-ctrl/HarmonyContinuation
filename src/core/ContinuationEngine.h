#pragma once
#include "ProgressionMatcher.h"
#include <array>

namespace harmony {
struct ConcreteChordEvent {
    std::string label;
    double durationQN{};
    ScaleDegree degree{};
    ChordQuality quality{ChordQuality::Unknown};
    RoleFlags roles{};
};
struct RecommendationSubScores {
    float match{}, skeleton{}, style{}, intent{}, cadence{}, continuation{}, prior{}, rhythm{}, support{};
};
enum class CompletionReason {
    NoEvidence, DominantTonic, StableTonic, PhraseReturn, LoopClosure,
    DevelopedPath, FunctionalPivot, ShortDevelopment, AudibleColor
};
struct IntentCompletionResult {
    float score{}; // diagnostic 0..1, not a probability
    CompletionReason reason{CompletionReason::NoEvidence};
};
struct ContinuationCandidate {
    std::string id;
    TemplateID primaryTemplate;
    PhraseIntent intent{PhraseIntent::Neutral};
    KeySignature key;
    std::vector<ConcreteChordEvent> continuation;
    std::optional<double> suggestedCurrentChordDurationQN;
    float rhythmScale{1.f};
    float rhythmConfidence{};
    float matchSimilarity{};
    float rankingScore{}; // 0..100 ordinal score, not a probability
    RecommendationSubScores subscores;
    std::size_t matchStart{}, matchEnd{}, continuationStart{};
    CadenceType cadence{CadenceType::None};
    StyleFlags styles{};
    int supportCount{1};
    std::vector<TemplateID> supportingTemplates;
};
struct RecommendationWeights {
    float match{0.34f}, skeleton{0.11f}, style{0.08f}, intent{0.07f}, cadence{0.07f};
    float continuation{0.12f}, prior{0.08f}, rhythm{0.08f}, support{0.05f};
    float minimumMatch{0.60f};
    float minimumScore{60.f};
    float diversityPenalty{13.f};
    float unexplainedInsertionPenalty{8.f};
    float templateDeletionPenalty{2.f};
    float intentCompletionTieBreak{0.005f}; // at most 0.25 points in either direction
    std::size_t perGroup{3};
};
struct RecommendationRequest {
    std::optional<Style> style;
    std::optional<PhraseIntent> preferredIntent;
};
struct RecommendationSet {
    std::array<std::vector<ContinuationCandidate>, 4> groups; // Resolve, Develop, Loop, Color
    std::vector<MatchResult> matches;
    MatchRunStats stats;
};
struct RhythmScaleEstimate { float scale{1.f}; float confidence{}; std::size_t samples{}; };
RhythmScaleEstimate estimateRhythmScale(const MatchResult&, const MatchQuery&, const ProgressionTemplate&);
ConcreteChordEvent realizeContinuation(const MatchEvent&, KeySignature, double durationQN);
IntentCompletionResult evaluateIntentCompletion(const ContinuationCandidate&,
    std::optional<ScaleDegree> phraseStart = {}, std::optional<ScaleDegree> current = {}) noexcept;
const char* completionReasonKey(CompletionReason) noexcept;
RecommendationSet recommendContinuations(const MatchQuery&, const CandidateIndex&,
                                         const RecommendationRequest& = {},
                                         const RecommendationWeights& = {});
const char* intentName(PhraseIntent) noexcept;
const char* cadenceName(CadenceType) noexcept;
} // namespace harmony
