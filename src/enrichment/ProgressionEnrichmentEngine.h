#pragma once
#include "core/ProgressionMatcher.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace harmony::enrichment {
enum class ComplexityLevel { Basic, Rich, Advanced };
enum class Group { Polish, Rich, Advanced };
enum class TechniqueID {
    ChordExtension, SeventhColor, NinthColor, Inversion, BassConnection,
    SecondaryDominant, SecondaryLeadingTone, PassingDiminished,
    ChromaticApproach, BorrowedChord, PredominantSubstitution,
    CadentialExpansion, Turnaround
};
enum class OperationType {
    UpgradeQuality, InsertChord, ReplaceChord, ChangeInversion,
    ChangeBass, ExtendCadence
};
struct EnrichmentOperation {
    OperationType type{};
    TechniqueID technique{};
    std::size_t sourceIndex{};
    std::string before, after, reason;
};
struct EnrichmentOpportunity {
    TechniqueID technique{};
    std::size_t afterIndex{};
    std::string reason;
};
struct EnrichmentConfig {
    std::array<int, 3> maxOperations{2, 3, 5};
    std::array<int, 3> maxInsertions{0, 1, 2};
    std::array<int, 3> maxSubstitutions{0, 0, 1};
    double minimumSplitQN{0.5};
    float minimumSkeletonPreservation{0.75f};
    float voiceLeadingWeight{0.03f}; // bounded quality tie-break, after harmony/style checks
    int minimumBassImprovementSemitones{2};
    float toleratedVoiceLeadingLoss{0.02f};
};
struct EnrichmentCandidate {
    std::string id;
    Group group{};
    ComplexityLevel complexity{};
    Progression progression;
    std::vector<EnrichmentOperation> operations;
    std::vector<TechniqueID> techniques;
    float score{}, skeletonPreservation{}, styleCompatibility{}, complexityScore{};
    std::string fingerprint;
};
struct EnrichmentResult {
    std::array<std::vector<EnrichmentCandidate>, 3> groups;
    std::vector<EnrichmentOpportunity> opportunities;
    std::string error;
};
EnrichmentResult enrichProgression(const Progression&, const HarmonicAnalysisResult&,
                                   std::optional<Style> style = {},
                                   const EnrichmentConfig& = {});
const char* techniqueName(TechniqueID) noexcept;
const char* groupName(Group) noexcept;
} // namespace harmony::enrichment
