#pragma once
#include "MusicIdentity.h"
#include <filesystem>
#include <span>

namespace harmony::wish {
enum class MusicalMode {
    Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian,
    HarmonicMinor, MelodicMinorAscending, ClassicalMelodicMinor,
    NaturalMajor=Ionian, NaturalMinor=Aeolian
};
enum class EditorialUsage { High, Medium, Low, Unrated };
enum class ReviewStatus { Pending, Reviewed, Unconfirmed };
enum class AnnotationState { Unannotated, Unreviewed, SourceMismatch, Unconfirmed, Reviewed, Invalid };
struct ModeMembership {
    int tonicOffset{}; // semitones from the template's reference tonic; NOT a chord-root offset
    MusicalMode mode{MusicalMode::Ionian};
    EditorialUsage usage{EditorialUsage::Unrated};
    std::string usageBasis, musicalEvidence;
    ReviewStatus review{ReviewStatus::Pending};
    std::string reviewer, melodicContext;
};
struct ModeAnnotation {
    TemplateID templateId;
    std::string sourceFingerprint;
    std::vector<ModeMembership> memberships;
};
struct AnnotationCatalog { int version{1}; std::vector<ModeAnnotation> entries; };
struct AnnotationLoadResult {
    AnnotationCatalog catalog;
    std::string error;
    explicit operator bool() const noexcept {return error.empty();}
};
struct AnnotationResolution {
    AnnotationState state{AnnotationState::Unannotated};
    std::vector<ModeMembership> memberships;
    std::string diagnostic;
};
const char* modeName(MusicalMode) noexcept;
std::optional<MusicalMode> parseMode(std::string_view) noexcept;
std::uint16_t modePitchMask(MusicalMode) noexcept; // classical melodic minor has no static mask
bool validMembership(const ModeMembership&, std::string& error);
AnnotationLoadResult parseAnnotations(std::string_view);
AnnotationLoadResult loadAnnotations(const std::filesystem::path&);
std::string serializeAnnotations(const AnnotationCatalog&);
AnnotationResolution resolveAnnotations(const ProgressionTemplate&, const AnnotationCatalog&);
bool strictlyCompatible(const preview::Sequence&, MusicalMode, int tonicPitchClass) noexcept;
}
