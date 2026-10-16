#include "ContinuationEngine.h"
#include <algorithm>

namespace harmony {
namespace {
bool isDegree(const ScaleDegree& value, int degree, int alteration = 0) noexcept {
    return value.degree == degree && value.alteration == alteration;
}
bool sameDegree(const ScaleDegree& left, const ScaleDegree& right) noexcept {
    return left.degree == right.degree && left.alteration == right.alteration;
}
bool hasExtensionColor(const ConcreteChordEvent& chord) noexcept {
    switch (chord.quality) {
    case ChordQuality::Major7: case ChordQuality::Minor7: case ChordQuality::Dominant7:
    case ChordQuality::Sus2: case ChordQuality::Sus4: return true;
    default: return false;
    }
}
} // namespace

IntentCompletionResult evaluateIntentCompletion(const ContinuationCandidate& candidate,
    std::optional<ScaleDegree> phraseStart, std::optional<ScaleDegree> current) noexcept {
    const auto& path = candidate.continuation;
    if (path.empty()) return {0.f, CompletionReason::NoEvidence};
    const auto& first = path.front();
    const auto& last = path.back();
    switch (candidate.intent) {
    case PhraseIntent::Resolve: {
        if (!isDegree(last.degree, 1)) return {0.32f, CompletionReason::NoEvidence};
        const bool dominantBefore = path.size() > 1 ? isDegree(path[path.size()-2].degree, 5) :
            current && isDegree(*current, 5);
        if (dominantBefore)
            return {0.96f, CompletionReason::DominantTonic};
        return {0.84f, CompletionReason::StableTonic};
    }
    case PhraseIntent::Loop:
        if (phraseStart && sameDegree(last.degree, *phraseStart))
            return {0.95f, CompletionReason::PhraseReturn};
        if (candidate.cadence == CadenceType::LoopClosure)
            return {0.86f, CompletionReason::LoopClosure};
        return {0.40f, CompletionReason::NoEvidence};
    case PhraseIntent::Develop: {
        const bool pivot = std::any_of(path.begin(), path.end(), [](const auto& chord) {
            return hasRole(chord.roles, Role::SecondaryDominant) ||
                hasRole(chord.roles, Role::SecondaryLeadingTone) ||
                hasRole(chord.roles, Role::Borrowed) || chord.degree.alteration != 0;
        });
        if (pivot) return {0.80f, CompletionReason::FunctionalPivot};
        const bool distinct = std::any_of(path.begin()+1, path.end(), [&](const auto& chord) {
            return !sameDegree(chord.degree, first.degree);
        });
        if (path.size() >= 2 && distinct) return {0.78f, CompletionReason::DevelopedPath};
        return {0.38f, CompletionReason::ShortDevelopment};
    }
    case PhraseIntent::Color:
        if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
                return hasRole(chord.roles, Role::Borrowed); }))
            return {0.90f, CompletionReason::BorrowedColor};
        if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
                return hasRole(chord.roles, Role::SecondaryDominant) ||
                    hasRole(chord.roles, Role::SecondaryLeadingTone); }))
            return {0.90f, CompletionReason::SecondaryColor};
        if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
                return chord.degree.alteration != 0 || chord.quality == ChordQuality::Diminished ||
                    chord.quality == ChordQuality::Diminished7 ||
                    chord.quality == ChordQuality::HalfDiminished7 || chord.quality == ChordQuality::Augmented; }))
            return {0.90f, CompletionReason::AudibleColor};
        if (std::any_of(path.begin(), path.end(), hasExtensionColor))
            return {0.90f, CompletionReason::ExtendedColor};
        return {0.34f, CompletionReason::NoEvidence};
    default:
        return {0.5f, CompletionReason::NoEvidence};
    }
}

const char* completionReasonKey(CompletionReason reason) noexcept {
    switch (reason) {
    case CompletionReason::DominantTonic: return "completion.dominantTonic";
    case CompletionReason::StableTonic: return "completion.stableTonic";
    case CompletionReason::PhraseReturn: return "completion.phraseReturn";
    case CompletionReason::LoopClosure: return "completion.loopClosure";
    case CompletionReason::DevelopedPath: return "completion.developedPath";
    case CompletionReason::FunctionalPivot: return "completion.functionalPivot";
    case CompletionReason::ShortDevelopment: return "completion.shortDevelopment";
    case CompletionReason::AudibleColor: return "completion.audibleColor";
    case CompletionReason::BorrowedColor: return "completion.borrowedColor";
    case CompletionReason::SecondaryColor: return "completion.secondaryColor";
    case CompletionReason::ExtendedColor: return "completion.extendedColor";
    default: return "completion.noEvidence";
    }
}
} // namespace harmony
