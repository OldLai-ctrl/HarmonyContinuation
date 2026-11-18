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
} // namespace

IntentCompletionResult evaluateIntentCompletion(const ContinuationCandidate& candidate,
    std::optional<ScaleDegree> phraseStart, std::optional<ScaleDegree> current) {
    (void)phraseStart; // Exact phrase return is established from realized music, not degrees alone.
    if (!candidate.evidence.empty()) return candidate.completion;
    const auto& path = candidate.continuation;
    if (path.empty()) return {0.f, CompletionReason::NoEvidence};
    const auto& first = path.front();
    const auto& last = path.back();
    switch (candidate.intent) {
    case PhraseIntent::Resolve: {
        const bool tonicQuality = candidate.key.mode==Mode::Major
            ? last.quality==ChordQuality::Major || last.quality==ChordQuality::Major7
            : last.quality==ChordQuality::Minor || last.quality==ChordQuality::Minor7;
        if (!isDegree(last.degree, 1) || !tonicQuality) return {0.32f, CompletionReason::NoEvidence};
        const auto previous = path.size()>1?std::optional(path[path.size()-2].degree):current;
        if (!previous || previous->alteration!=0 ||
            (previous->degree!=4 && previous->degree!=5 && previous->degree!=7))
            return {0.32f, CompletionReason::NoEvidence};
        const bool dominantBefore = isDegree(*previous,5);
        if (dominantBefore)
            return {0.96f, CompletionReason::DominantTonic};
        return {0.84f, CompletionReason::StableTonic};
    }
    case PhraseIntent::Loop:
        if (candidate.actualPhraseReturn)
            return {0.95f, CompletionReason::PhraseReturn};
        if (candidate.preservedLoop)
            return {0.86f, CompletionReason::LoopClosure};
        return {0.40f, CompletionReason::NoEvidence};
    case PhraseIntent::Develop: {
        if (!candidate.pathContextReliable) return {0.38f, CompletionReason::NoEvidence};
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
    default:
        return {0.5f, CompletionReason::NoEvidence};
    }
}

float evaluateColorEvidence(const ContinuationCandidate& candidate, std::string* reason) {
    const auto& path = candidate.continuation;
    const auto note = [&](float score, const char* key) {
        if (reason) *reason = key;
        return score;
    };
    if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
            return hasRole(chord.roles, Role::SecondaryDominant) ||
                hasRole(chord.roles, Role::SecondaryLeadingTone); }))
        return note(0.95f, "secondary");
    if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
            return chord.quality == ChordQuality::Diminished ||
                chord.quality == ChordQuality::Diminished7 ||
                chord.quality == ChordQuality::HalfDiminished7 || chord.quality == ChordQuality::Augmented; }))
        return note(0.9f, "altered");
    if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
            return chord.harmonicData && chordPitches(*chord.harmonicData).colorMask; }))
        return note(0.85f, "tension");
    // A borrowed *minor* chord (minor iv in major) is a deliberate colour; a
    // borrowed major chord (bVII, bIII, bVI) is the everyday rock mixture.
    for (const auto& chord : path)
        if (hasRole(chord.roles, Role::Borrowed) &&
            (chord.quality == ChordQuality::Minor || chord.quality == ChordQuality::Minor7 ||
             chord.quality == ChordQuality::Diminished || chord.quality == ChordQuality::Diminished7 ||
             chord.quality == ChordQuality::HalfDiminished7 || chord.quality == ChordQuality::Augmented))
            return note(0.9f, "borrowed");
    // Modal mixture stays a tint: bVII is the default sound of rock, not a
    // colour accent, so it never fills the colour view on its own.
    if (std::any_of(path.begin(), path.end(), [](const auto& chord) {
            return hasRole(chord.roles, Role::Borrowed) || chord.degree.alteration != 0; }))
        return note(0.6f, "mixture");
    return note(0.f, "none");
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
