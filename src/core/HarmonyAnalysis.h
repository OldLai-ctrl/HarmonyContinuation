#pragma once

#include "Progression.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace harmony {

enum class Mode : std::uint8_t { Major, Minor };
enum class HarmonicFunction : std::uint8_t {
    Unknown, Tonic, TonicProlongation, Predominant, Dominant,
    ChromaticPredominant, ChromaticDominant
};
enum class Role : std::uint32_t {
    Structural = 1u << 0, Embellishing = 1u << 1, Passing = 1u << 2,
    Approach = 1u << 3, SecondaryDominant = 1u << 4,
    SecondaryLeadingTone = 1u << 5, Borrowed = 1u << 6,
    Substitution = 1u << 7, Inversion = 1u << 8,
    Extension = 1u << 9, Cadential = 1u << 10
};
using RoleFlags = std::uint32_t;
constexpr RoleFlags flag(Role role) noexcept { return static_cast<RoleFlags>(role); }
constexpr bool hasRole(RoleFlags flags, Role role) noexcept { return (flags & flag(role)) != 0; }

struct KeySignature { PitchClass tonic{PitchClass::C}; Mode mode{Mode::Major}; };
struct KeyCandidate { KeySignature key; float score{}; float confidence{}; };
struct ScaleDegree { int degree{}; int alteration{}; }; // degree 1..7, semitone alteration

struct NormalizedChord {
    std::optional<PitchClass> root;
    std::optional<PitchClass> bass;
    ChordQuality quality{ChordQuality::Unknown};
    std::uint16_t intervalMask{}; // bits 0..11 relative to root
    std::uint16_t colorMask{};    // non-essential extension tones
    bool inversion{};
    float confidence{};
    std::string rootSpelling;
    std::vector<std::string> diagnostics;
};

struct HarmonicEvent {
    NormalizedChord chord;
    std::optional<ScaleDegree> degree;
    std::optional<ScaleDegree> target;
    std::optional<ChordQuality> targetQuality;
    HarmonicFunction function{HarmonicFunction::Unknown};
    RoleFlags roles{};
    float structuralWeight{};
    float analysisConfidence{};
    std::size_t sourceChordIndex{};
};

struct HarmonicAnalysisResult {
    std::vector<KeyCandidate> keyCandidates;
    std::optional<KeyCandidate> selectedKey;
    std::vector<HarmonicEvent> full;
    std::vector<std::size_t> skeletonIndices;
    float overallConfidence{};
    std::vector<std::string> warnings;
};

struct HarmonyAnalysisWeights {
    float exactDiatonic{2.0f};
    float compatibleDiatonic{1.3f};
    float inScaleRoot{0.35f};
    float chromaticPenalty{-1.25f};
    float secondaryDominant{1.7f};
    float secondaryLeadingTone{1.3f};
    float borrowedChord{1.45f};
    float tonicEvidence{0.9f};
    float firstTonic{0.45f};
    float lastClosedTonic{0.5f};
    float dominantResolution{1.5f};
    float cadence{1.1f};
    float structuralBase{0.66f};
    float relativeDurationEffect{0.12f};
    float structuralTonicEffect{0.10f};
    float structuralFunctionEffect{0.06f};
    float structuralUnknownPenalty{0.12f};
    float structuralEndpointEffect{0.06f};
    float structuralCadenceEffect{0.16f};
    float structuralBorrowedEffect{0.04f};
    float shortSecondaryDominantPenalty{0.13f};
    float shortSecondaryLeadingPenalty{0.17f};
    float shortApproachPenalty{0.13f};
    float barStartEffect{0.05f};
    float shortDurationRatio{0.65f};
};

struct AnalysisContext {
    std::optional<KeySignature> forcedKey;
    HarmonyAnalysisWeights weights;
    std::optional<int> timeSigNumerator; // used only when the project meter is known
    std::optional<int> timeSigDenominator;
    float skeletonThreshold{0.55f};
};

NormalizedChord normalizeChord(const ChordEvent& event);
HarmonicAnalysisResult analyzeHarmony(const Progression& events,
                                      const AnalysisContext& context = {});

// Pure presentation helpers. The model remains structured independently of these strings.
std::string formatKey(KeySignature key);
std::string formatDegree(const HarmonicEvent& event);
std::string formatFunction(HarmonicFunction function);

} // namespace harmony
