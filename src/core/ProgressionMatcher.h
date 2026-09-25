#pragma once

#include "HarmonyAnalysis.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace harmony {

using TemplateID = std::string;
enum class PhraseIntent : std::uint8_t { Unknown, Cadence, Loop, Departure };
enum class CadenceType : std::uint8_t { None, Authentic, Plagal, Half, Deceptive };
using StyleFlags = std::uint32_t;

// No absolute pitch class is stored in a template. Durations are relative QN
// values; the matcher normalizes them independently for each sequence.
struct MatchEvent {
    std::optional<ScaleDegree> degree;
    std::optional<ScaleDegree> target;
    ChordQuality quality{ChordQuality::Unknown};
    HarmonicFunction function{HarmonicFunction::Unknown};
    RoleFlags roles{};
    float structuralWeight{0.7f};
    std::optional<double> durationQN;
    std::size_t sourceIndex{};
};

struct ProgressionFingerprint {
    Mode mode{Mode::Major};
    std::size_t skeletonLength{};
    std::vector<int> skeletonDegrees;
    std::vector<int> skeletonBigrams;
    HarmonicFunction endingFunction{HarmonicFunction::Unknown};
    CadenceType cadence{CadenceType::None};
    std::vector<std::uint8_t> rhythmShape;
    std::size_t structuralCount{};
    RoleFlags roleSummary{};
};

struct ProgressionTemplate {
    TemplateID id;
    std::string name;
    Mode mode{Mode::Major};
    std::vector<MatchEvent> full;
    std::vector<std::size_t> skeletonIndices;
    PhraseIntent intent{PhraseIntent::Unknown};
    CadenceType cadence{CadenceType::None};
    StyleFlags styles{};
    bool loopable{};
    ProgressionFingerprint fingerprint;
};

struct KeyInterpretation {
    KeyCandidate key;
    std::vector<MatchEvent> full;
    std::vector<std::size_t> skeletonIndices;
    ProgressionFingerprint fingerprint;
};
struct MatchQuery {
    std::vector<KeyInterpretation> interpretations;
    std::optional<int> timeSigNumerator;
    std::optional<int> timeSigDenominator;
    std::optional<StyleFlags> styleHint;
    bool forcedKey{};
};

// All score-affecting constants live here. Values are dimensionless costs.
struct MatchWeights {
    std::size_t maxKeyCandidates{3};
    float minKeyConfidence{0.005f};
    float degreeDifference{0.9f};
    float alteredDegree{0.28f};
    float enharmonicDegreeDifference{0.06f};
    float qualityDifference{0.25f};
    float qualityExtensionDifference{0.07f};
    float functionDifference{0.48f};
    float sameFunctionDegreeFactor{0.52f};
    float roleDifference{0.22f};
    float secondaryTargetDifference{0.34f};
    float borrowedVariantFactor{0.58f};
    float substitutionMaxCost{1.30f};
    float rhythmWeight{0.18f};
    float gapBase{0.35f};
    float structuralImportanceWeight{0.44f};
    float passingGapFactor{0.36f};
    float approachGapFactor{0.55f};
    float secondaryGapFactor{0.72f};
    float tonicProlongationGapFactor{0.85f};
    float predominantGapFactor{1.30f};
    float dominantGapFactor{1.75f};
    float cadentialGapFactor{1.55f};
    float cadentialTonicGapFactor{1.35f};
    float queryTailStructuralFactor{1.85f};
    float templatePrefixSkipCost{0.05f};
    float skeletonWeight{0.68f};
    float fullWeight{0.32f};
    float keyPriorWeight{0.025f};
    float similarityScale{0.75f};
    float minimumMatchedFraction{0.75f};
    float lowCoveragePenalty{0.80f};
    std::size_t shortlistLimit{800};
};

enum class AlignmentOp : std::uint8_t { Match, Substitute, QueryInsertion, TemplateDeletion };
enum class MatchReason : std::uint32_t {
    SameDegree = 1u << 0, SameBaseDegree = 1u << 1, SameFunction = 1u << 2,
    QualityVariant = 1u << 3, BorrowedVariant = 1u << 4, SameRole = 1u << 5,
    SecondaryTargetMatch = 1u << 6, SecondaryTargetMismatch = 1u << 7,
    EmbellishingInsertion = 1u << 8, StructuralInsertion = 1u << 9,
    EmbellishingDeletion = 1u << 10, StructuralDeletion = 1u << 11,
    RhythmMismatch = 1u << 12, CadentialImportance = 1u << 13,
    LowStructuralPenalty = 1u << 14, ResolvesToMatchedTarget = 1u << 15,
    EnharmonicDegree = 1u << 16
};
using MatchReasonFlags = std::uint32_t;
constexpr MatchReasonFlags flag(MatchReason reason) noexcept { return static_cast<MatchReasonFlags>(reason); }
constexpr bool hasReason(MatchReasonFlags flags, MatchReason reason) noexcept { return (flags & flag(reason)) != 0; }

struct AlignmentStep {
    std::optional<std::size_t> queryIndex;
    std::optional<std::size_t> templateIndex;
    AlignmentOp operation{AlignmentOp::Match};
    float cost{};
    MatchReasonFlags reasons{};
};
struct MatchSubScores {
    float skeletonHarmony{};
    float fullHarmony{};
    float functionSimilarity{};
    float roleSimilarity{};
    float rhythmSimilarity{};
    float keyCompatibility{};
};
struct MatchResult {
    TemplateID templateId;
    std::string templateName;
    float similarity{}; // 0..1 similarity, not a probability
    float rawCost{};
    KeySignature key;
    float keyPrior{};
    std::size_t templateMatchStart{};
    std::size_t templateMatchEnd{}; // inclusive
    std::size_t continuationStartIndex{};
    bool hasContinuation{};
    std::size_t continuationLength{};
    std::vector<AlignmentStep> alignmentTrace;
    std::vector<std::string> templateLabels;
    MatchSubScores subScores;
};

struct MatchRunStats {
    double prefilterMs{};
    double alignmentMs{};
    double totalMs{};
    std::size_t shortlistSize{};
};

MatchQuery makeMatchQuery(const Progression&, const AnalysisContext& = {}, const MatchWeights& = {});
ProgressionFingerprint makeFingerprint(Mode, const std::vector<MatchEvent>&,
                                       const std::vector<std::size_t>&, CadenceType);
void prepareTemplate(ProgressionTemplate&);

class CandidateIndex {
public:
    explicit CandidateIndex(std::vector<ProgressionTemplate> templates = {});
    const std::vector<ProgressionTemplate>& templates() const noexcept { return templates_; }
    std::vector<std::size_t> shortlist(const MatchQuery&, const MatchWeights& = {}) const;
private:
    std::vector<ProgressionTemplate> templates_;
};

std::vector<MatchResult> matchProgression(const MatchQuery&, const CandidateIndex&,
                                          const MatchWeights& = {}, std::size_t topN = 5,
                                          MatchRunStats* stats = nullptr);

const char* alignmentOpName(AlignmentOp) noexcept;
std::string reasonNames(MatchReasonFlags);
std::string formatMatchEvent(const MatchEvent&);

} // namespace harmony
