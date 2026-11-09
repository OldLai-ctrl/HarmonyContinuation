#pragma once
#include "core/ContextualContinuation.h"
#include "core/ChordVoicer.h"
#include "preview/PreviewSequence.h"
#include <cstddef>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace harmony::generation {
inline constexpr const char* generatorVersion="g13-relations-v1";
enum class VariationLevel { Conventional, Rich, Bold };
const char* name(VariationLevel) noexcept;
// Aggregated, tonic-relative corpus evidence. Mode is intentionally unknown
// when the source supplies tonic but no major/minor annotation.
struct StyleTransition { int previous2{-1}, previous{}, next{}; double count{}, songs{}; };
struct StyleDuration { double qn{}, count{}, songs{}; };
struct StylePosition { std::string phase; int root{}; double count{}, songs{}; };
struct StyleQualityTransition { int previous{}, next{}; std::string previousFamily, nextFamily; double count{}, songs{}; };
struct StylePrior {
    std::string id, source, license, scope;
    std::vector<StyleTransition> transitions;
    std::vector<StyleDuration> durations;
    std::vector<StylePosition> positions;
    std::vector<StyleQualityTransition> qualityTransitions;
};
StylePrior loadStylePrior(const std::filesystem::path&); // bounded TSV; throws on invalid evidence
struct PlannedSlot {
    double durationQN{};
    int rootDegree{}, anchorDegree{};
    bool appliedDominant{};
    std::string role, evidence;
    int bassDegree{}, bassMidi{};
};
struct PhrasePlan {
    std::string id, priorId, explanation;
    VariationLevel variation{};
    std::vector<PlannedSlot> slots;
    std::size_t outsideBudget{};
    int maximumBassLeap{7};
};
enum class RelationKind { Functional, VoiceLeading, Outside };
enum class DevelopmentIntent { Natural, Open, Tension, Color, BassLine };
const char* name(RelationKind) noexcept;
const char* name(DevelopmentIntent) noexcept;

struct Relation {
    ChordEvent chord; // complete sounding identity, including explicit bass
    RelationKind kind{};
    std::string mechanism, explanation;
    std::optional<int> expectedRoot; // an explicit dominant/leading-tone obligation
    std::optional<KeySignature> proposedCenter;
    int commonPitchClasses{}, bassClassStep{};
    float connection{}; // model evidence; not measured user voices
};
struct SuccessorContext {
    Progression recent; // chronological sounding history, final event is current
    KeySignature center;
    DevelopmentIntent intent{DevelopmentIntent::Open};
    HarmonicTendency tendency{HarmonicTendency::Balanced};
    int bassDirection{};
    std::optional<int> expectedRoot;
    bool allowCenterChange{true};
    std::size_t maximum{24};
    std::optional<int> establishedPedal; // grounded in input, not two generated coincidences
    std::optional<VariationLevel> variation; // experiment-only vocabulary, no product configuration
    std::optional<int> plannedRoot, plannedBass;
};
// Each operator can be called independently. All share the same sounding data.
std::vector<Relation> successors(const SuccessorContext&,RelationKind);

struct GenerationRequest {
    Progression input;
    AnalysisContext context;
    HarmonyConstraintSet constraints; // absolute QN, same convention as Preview
    HarmonicTendency tendency{HarmonicTendency::Balanced};
    std::vector<DevelopmentIntent> intents; // empty: all five
    std::size_t beamWidth{12}, successorLimit{16}, maximumExpansions{16000};
    std::size_t resultsPerIntent{2};
    std::size_t chordCount{}; // zero: derive a 4/6/8-event phrase plan
    std::optional<StylePrior> stylePrior;
    VariationLevel variation{VariationLevel::Conventional};
};
struct PhraseEvaluation {
    // Ordinal diagnostics. structure==1 means the explicit construction checks
    // passed, not that the generated music has passed human listening review.
    float connection{}, structure{}, development{}, novelty{}, tension{}, bassContinuity{};
    float recentSimilarity{}, historySimilarity{};
    std::optional<double> colorTensionTrend;
    std::size_t uniqueHarmonies{}, outsideChords{}, directedBassSteps{};
};
struct GeneratedPhrase {
    std::string id; // generated identity; never a Factory TemplateID
    std::string inputIdentity; // actual input signature, not a recovered source ID
    DevelopmentIntent intent{};
    KeySignature originCenter;
    std::optional<KeySignature> targetCenter;
    std::vector<KeySignature> contextInterpretations;
    std::string centerEvidence, structureExplanation, endingExplanation;
    PhraseEnding ending{PhraseEnding::Unknown};
    Progression phrase; // E only, complete known durations, absolute source QN
    std::vector<int> bassMidi; // performed register, rather than octave-equivalent direction alone
    std::vector<Relation> path; // one explanation per generated event
    HarmonyConstraintSet constraints;
    PhraseEvaluation evaluation;
    double seamQN{}, durationQN{}, inputOpenHoldQN{};
    bool hardConstraintsPassed{}, complete{};
    std::optional<PhrasePlan> plan;
};
struct GenerationResult {
    std::vector<GeneratedPhrase> phrases;
    std::vector<KeySignature> contextInterpretations;
    std::size_t expanded{}, completeExamined{}, constraintRejected{}, structureRejected{};
    // Style planning has fixed loop/beam limits, at most 4,096 examined states
    // in addition to maximumExpansions for relation realization.
    std::size_t skeletonExpanded{}, bassExpanded{};
    bool budgetExhausted{};
    double elapsedMs{};
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};
GenerationResult generate(const GenerationRequest&);
// Transient playback adapter only: no Factory ID, baseQualified=false.
// Never send this adapter to product presentation, Session or Snapshot storage.
ContinuationCandidate previewCandidate(const GeneratedPhrase&);
// Use this for generated performance; the generic candidate adapter alone
// cannot carry an explicit bass register and must not be persisted.
preview::BuildResult buildPerformance(const Progression&,const GeneratedPhrase&,double tempo=120.);
std::string performanceIdentity(const GeneratedPhrase&);
}
