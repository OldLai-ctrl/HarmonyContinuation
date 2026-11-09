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
enum class RecommendationChannel { Exact, ContextFull, ContextThree, ContextTwo, PhraseTransition, ContextFour, ContextPhrase };
enum class PhraseEnding {
    Unknown, Closed, ModulatedClosure, InterimPause, OpenUnresolved,
    Suspended, LoopReturn, ModalDevelopment, CenterTransition
};
enum class ConnectionProvenance { DirectSource, AlgorithmAssessed, EditorialReviewed };
enum class CreativeDirection { Stable, Rich, Bold };
enum class CenterRelation { Same, Relative, Modulation, Undetermined };
struct CreativeQuality {
    float boundary{}, context{}, center{}, source{}, playability{}, phrase{};
    bool qualified{};
    int tier{}; // comparable quality band; raw scores remain channel-specific
};
struct CreativeRouteProfile {
    bool ready{};
    std::array<std::uint16_t,8> tones{}; // actual pitch sets relative to first bass, not chord names
    std::array<float,8> bass{}, tension{};
    std::array<float,12> palette{}; // actual sounding pitch-class usage, time weighted
    float contextFit{}, historySimilarity{}, recentSimilarity{};
    float developmentHistorySimilarity{}; // relative sound/bass/tension contour, independent of absolute key palette
};
struct ContinuationBranching {
    bool assessed{};
    std::size_t legalRoutes{}, independentIdeas{}, families{}, centerDirections{}, freshIdeas{};
    float opportunity{}; // bounded quality-similar tie preference, not a future constraint guarantee
};
// Pitch-class model evidence, not measured voices in a user's arrangement.
// Runtime-only: no change to Session5/Snapshot3 or the neutral Unified score.
struct HarmonicDevelopmentEvidence {
    bool ready{};
    int commonTones{}, bassClassStep{}, maximumClassMotion{};
    float meanClassMotion{}, coherence{}, novelty{}, repetition{}, tensionChange{}, paletteChange{};
};
struct ContinuationCandidate {
    std::string id;
    TemplateID primaryTemplate;
    PhraseIntent intent{PhraseIntent::Neutral};
    KeySignature key;
    int keyOffsetFromInput{}; // transposition-invariant tie order, never a score
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
    RecommendationChannel channel{RecommendationChannel::Exact};
    ConnectionProvenance provenance{ConnectionProvenance::DirectSource};
    CreativeDirection direction{CreativeDirection::Stable};
    CenterRelation centerRelation{CenterRelation::Same};
    CreativeQuality creativeQuality;
    std::optional<KeySignature> originCenter, targetCenter, sourceCoordinateCenter;
    std::string connectionReason, family;
    std::size_t contextLength{};
    PhraseEnding ending{PhraseEnding::Unknown};
    bool reviewedEnding{}, sourceEndPreserved{}, completeSourcePhrase{};
    std::string endingEvidence;
    std::vector<TemplateID> nextPhraseSources; // actual bounded index links, not a promise under future constraints
    // Inspiration-only structural evidence. Not serialized; frozen performance
    // and the legacy Exact score/eligibility contract remain independent.
    bool structureQualified{}, historyFamilyPresent{}, historyMechanismPresent{};
    int structurePriority{}; // 0 unproven, 1 metric inference, 2 reviewed whole-phrase boundary
    double seamQN{};
    float historySimilarity{};
    std::string structureReason, developmentMechanism;
    CreativeRouteProfile routeProfile; // runtime ranking only; no persistence changes
    ContinuationBranching branching;
    HarmonicDevelopmentEvidence harmonicDevelopment;
};
inline bool representativeMatch(const ContinuationCandidate& c, const MatchResult& m) noexcept {
    return c.channel==RecommendationChannel::Exact && c.primaryTemplate==m.templateId && c.key.tonic==m.key.tonic && c.key.mode==m.key.mode &&
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
    std::size_t perGroup{3}; // zero keeps all qualified results within the existing retrieval budget
    bool diminishedPassingInsertionException{false}; // internal experiment, never persisted
    bool abstractChordCompatibility{false}; // internal retrieval experiment, never persisted
};
struct RecommendationRequest {
    std::optional<Style> style;
    std::optional<PhraseIntent> preferredIntent;
    HarmonyConstraintSet constraints;
    HarmonicTendency tendency{HarmonicTendency::Balanced};
    bool inspirationPriority{}; // runtime only; Session5 remains unchanged
    bool prepareBothViews{}; // worker request only; navigation reuses two immutable result banks
    bool diagnoseCreative{}; // bounded, opt-in editorial-route gate trace; not used by the worker
    bool harmonicReranking{}; // reversible dev.15 posterior ranking; false reproduces dev.14
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
struct RecommendationPosition { std::size_t group{}, index{}; };
struct CreativeTrial {
    TemplateID source;
    KeySignature key;
    std::string disposition;
    float boundary{};
};
struct RecommendationSet {
    RetrievalTimingMode retrievalTimingMode{RetrievalTimingMode::Unified};
    bool phase3aEnabled{}; // actual invocation, diagnostic only; never persisted
    std::array<std::vector<ContinuationCandidate>, 4> groups; // Resolve, Develop, Loop, Color
    std::vector<MatchResult> matches;
    MatchRunStats stats;
    std::vector<DiminishedInsertionDiagnostic> insertionDiagnostics; // enabled experiment only
    bool abstractChordCompatibility{}; // actual invocation, diagnostic only; never persisted
    std::size_t contextualExamined{}, contextualQualified{}, transitionExamined{}, transitionQualified{};
    double contextualMs{};
    std::vector<CreativeTrial> creativeTrials;
    std::vector<RecommendationPosition> inspirationOrder; // prepared by the worker, actual candidate references
    bool harmonicReranking{};
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
                                         const RecommendationWeights& = {},
                                         const RecommendationSet* exactBase = nullptr);
const char* intentName(PhraseIntent) noexcept;
const char* cadenceName(CadenceType) noexcept;
} // namespace harmony
