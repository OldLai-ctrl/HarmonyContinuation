#pragma once
#include "ProgressionMatcher.h"
#include "HarmonyConstraints.h"
#include "HarmonicTendency.h"
#include <array>

namespace harmony {
struct ConcreteChordEvent {
    std::string label;
    double durationQN{};
    ScaleDegree degree{};
    ChordQuality quality{ChordQuality::Unknown};
    RoleFlags roles{};
    std::optional<ChordEvent> harmonicData;
};
struct RecommendationSubScores {
    float match{}, skeleton{}, style{}, intent{}, cadence{}, continuation{}, prior{}, rhythm{}, support{};
};
enum class CompletionReason {
    NoEvidence, DominantTonic, StableTonic, PhraseReturn, LoopClosure,
    DevelopedPath, FunctionalPivot, ShortDevelopment, AudibleColor,
    BorrowedColor, SecondaryColor, ExtendedColor
};
struct IntentCompletionResult {
    float score{}; // diagnostic 0..1, not a probability
    CompletionReason reason{CompletionReason::NoEvidence};
};
struct ContinuationEvidence {
    TemplateID templateId;
    KeySignature key;
    std::size_t matchStart{}, matchEnd{}, continuationStart{};
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
    float eligibilityScore{}; // neutral preference gate; HarmonyFirst keeps its rhythm-neutral scale
    bool hardConstraintsPassed{true};
    bool baseQualified{true}; // explicit gate, never inferred from rankingScore by consumers
    bool exploratory{}; // base-qualified, but no sufficient actual intent evidence
    bool pathContextReliable{true};
    bool preservedLoop{};
    bool actualPhraseReturn{};
    IntentCompletionResult completion;
    std::vector<ContinuationEvidence> evidence;
    RecommendationSubScores subscores;
    std::size_t matchStart{}, matchEnd{}, continuationStart{};
    CadenceType cadence{CadenceType::None};
    StyleFlags styles{};
    int supportCount{1};
    std::vector<TemplateID> supportingTemplates;
    HarmonyConstraintSet constraints;
    double sourceAnchorQN{}; // normalize realization constraints for music identity and snapshots
    MelodyCompatibility melodyCompatibility;
};
inline bool representativeMatch(const ContinuationCandidate& c, const MatchResult& m) noexcept {
    return c.primaryTemplate==m.templateId && c.key.tonic==m.key.tonic && c.key.mode==m.key.mode &&
        c.matchStart==m.templateMatchStart && c.matchEnd==m.templateMatchEnd && c.continuationStart==m.continuationStartIndex;
}
struct RecommendationWeights {
    float match{0.34f}, skeleton{0.11f}, style{0.08f}, intent{0.07f}, cadence{0.07f};
    float continuation{0.12f}, prior{0.08f}, rhythm{0.08f}, support{0.05f};
    float minimumMatch{0.60f};
    float minimumScore{60.f};
    float diversityPenalty{13.f}; // retained for historical experiment callers; no longer affects scores
    float unexplainedInsertionPenalty{8.f};
    float templateDeletionPenalty{2.f};
    float intentCompletionTieBreak{0.005f}; // at most 0.25 points in either direction
    std::size_t perGroup{3};
    bool diminishedPassingInsertionException{false}; // internal experiment, never persisted
    bool abstractChordCompatibility{false}; // internal retrieval experiment, never persisted
};
struct RecommendationRequest {
    std::optional<Style> style;
    std::optional<PhraseIntent> preferredIntent;
    HarmonyConstraintSet constraints;
    HarmonicTendency tendency{HarmonicTendency::Balanced};
};
// Output identity shared by ranking and presentation; omits explanatory key/intent.
std::string continuationMusicFingerprint(const ContinuationCandidate&);
struct DiminishedInsertionDiagnostic {
    TemplateID templateId;
    bool singleInsertion{}, reliableAlignment{}, diminished{}, chromaticMotion{};
    bool sharedTones{}, compatibleCore{}, validContinuation{}, alreadyExplained{};
    bool eligible{};
    float totalInsertionPenalty{}, originalPenalty{}, refund{}, scoreBefore{}, scoreAfter{};
};
struct RecommendationSet {
    RetrievalTimingMode retrievalTimingMode{RetrievalTimingMode::Legacy};
    bool phase3aEnabled{}; // actual invocation, diagnostic only; never persisted
    std::array<std::vector<ContinuationCandidate>, 4> groups; // Resolve, Develop, Loop, Color
    std::vector<MatchResult> matches;
    MatchRunStats stats;
    std::vector<DiminishedInsertionDiagnostic> insertionDiagnostics; // enabled experiment only
    bool abstractChordCompatibility{}; // actual invocation, diagnostic only; never persisted
};
DiminishedInsertionDiagnostic evaluateDiminishedPassingInsertion(
    const MatchQuery&, const MatchResult&, const ProgressionTemplate&);
struct RhythmScaleEstimate {
    float scale{1.f}; float confidence{}; std::size_t samples{};
    const char* source{"prototype-no-evidence"};
};
RhythmScaleEstimate estimateRhythmScale(const MatchResult&, const MatchQuery&, const ProgressionTemplate&);
ConcreteChordEvent realizeContinuation(const MatchEvent&, KeySignature, double durationQN);
IntentCompletionResult evaluateIntentCompletion(const ContinuationCandidate&,
    std::optional<ScaleDegree> phraseStart = {}, std::optional<ScaleDegree> current = {});
const char* completionReasonKey(CompletionReason) noexcept;
Progression continuationProgression(const Progression&,const ContinuationCandidate&);
RecommendationSet recommendContinuations(const MatchQuery&, const CandidateIndex&,
                                         const RecommendationRequest& = {},
                                         const RecommendationWeights& = {});
const char* intentName(PhraseIntent) noexcept;
const char* cadenceName(CadenceType) noexcept;
} // namespace harmony
