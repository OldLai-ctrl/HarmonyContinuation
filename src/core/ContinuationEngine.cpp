#include "ContinuationEngine.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace harmony {
namespace {
const KeyInterpretation* interpretationFor(const MatchQuery& query, KeySignature key) {
    for (const auto& candidate : query.interpretations)
        if (candidate.key.key.tonic == key.tonic && candidate.key.key.mode == key.mode) return &candidate;
    return nullptr;
}
int positiveMod(int x, int divisor) { return (x % divisor + divisor) % divisor; }
float clampScore(float x) { return std::clamp(x, 0.f, 1.f); }
std::string suffixFingerprint(const ContinuationCandidate& candidate, bool coarse) {
    std::ostringstream out;
    out << static_cast<int>(candidate.key.tonic) << ':' << static_cast<int>(candidate.key.mode) << ':'
        << static_cast<int>(candidate.intent) << ':';
    if (!coarse) {
        out << static_cast<int>(candidate.cadence) << ':';
        if (candidate.suggestedCurrentChordDurationQN)
            out << std::round(*candidate.suggestedCurrentChordDurationQN * 4.0) / 4.0 << ':';
    }
    double average{};
    for (const auto& event : candidate.continuation) average += event.durationQN;
    average /= std::max<std::size_t>(1, candidate.continuation.size());
    for (const auto& event : candidate.continuation) {
        if (coarse) out << event.degree.degree << '/' << event.degree.alteration << ':'
                        << (event.durationQN / std::max(0.001, average) < 0.7 ? 0 :
                            event.durationQN / std::max(0.001, average) > 1.4 ? 2 : 1);
        else out << event.label << '@' << std::round(event.durationQN * 4.0) / 4.0;
        out << '|';
    }
    return out.str();
}
PhraseIntent effectiveIntent(const ProgressionTemplate& item, const std::vector<MatchEvent>& suffix) {
    if (item.intent != PhraseIntent::Neutral) return item.intent;
    if (item.loopable) return PhraseIntent::Loop;
    for (const auto& event : suffix) if (hasRole(event.roles, Role::Borrowed) ||
        hasRole(event.roles, Role::SecondaryLeadingTone)) return PhraseIntent::Color;
    if (item.cadence == CadenceType::Authentic || item.cadence == CadenceType::PerfectAuthentic ||
        item.cadence == CadenceType::Plagal) return PhraseIntent::Resolve;
    return PhraseIntent::Develop;
}
std::size_t groupIndex(PhraseIntent intent) {
    switch (intent) {
        case PhraseIntent::Resolve: return 0; case PhraseIntent::Loop: return 2;
        case PhraseIntent::Color: return 3; default: return 1;
    }
}
float styleScore(const ProgressionTemplate& item, std::optional<Style> hint) {
    if (!hint) return 0.5f;
    for (const auto& [style, weight] : item.styleWeights) if (style == *hint) return weight;
    return 0.25f; // a strong harmonic match can still rank
}
float cadenceScore(PhraseIntent intent, CadenceType cadence) {
    if (intent == PhraseIntent::Loop) return cadence == CadenceType::LoopClosure ? 1.f : 0.5f;
    if (intent == PhraseIntent::Resolve) return cadence == CadenceType::Authentic ||
        cadence == CadenceType::PerfectAuthentic || cadence == CadenceType::Plagal ? 1.f : 0.4f;
    return 0.5f;
}
} // namespace

RhythmScaleEstimate estimateRhythmScale(const MatchResult& match, const MatchQuery& query,
                                        const ProgressionTemplate& item) {
    RhythmScaleEstimate result;
    const auto* selected = interpretationFor(query, match.key);
    if (!selected) return result;
    std::vector<float> ratios;
    for (const auto& step : match.alignmentTrace) {
        if (!step.queryIndex || !step.templateIndex ||
            (step.operation != AlignmentOp::Match && step.operation != AlignmentOp::Substitute) ||
            *step.queryIndex >= selected->full.size() || *step.templateIndex >= item.full.size()) continue;
        const auto& current = selected->full[*step.queryIndex];
        const auto& reference = item.full[*step.templateIndex];
        if (current.durationQN && reference.durationQN && current.structuralWeight >= 0.5f &&
            reference.structuralWeight >= 0.5f && *reference.durationQN > 0)
            ratios.push_back(static_cast<float>(*current.durationQN / *reference.durationQN));
    }
    result.samples = ratios.size();
    if (ratios.size() < 2) return result;
    std::sort(ratios.begin(), ratios.end());
    const auto middle = ratios.size() / 2;
    const auto median = ratios.size() % 2 ? ratios[middle] : (ratios[middle - 1] + ratios[middle]) * 0.5f;
    if (ratios.size() == 2 && ratios.back() / std::max(0.01f, ratios.front()) > 1.3f)
        return result;
    result.scale = std::clamp(median, 0.25f, 4.f);
    std::vector<float> deviation;
    for (auto ratio : ratios) deviation.push_back(std::abs(ratio - median));
    std::sort(deviation.begin(), deviation.end());
    const float spread = deviation[deviation.size() / 2] / std::max(0.01f, median);
    result.confidence = clampScore(std::min(1.f, static_cast<float>(ratios.size()) / 4.f) * (1.f - spread));
    return result;
}

ConcreteChordEvent realizeContinuation(const MatchEvent& event, KeySignature key, double durationQN) {
    constexpr int major[]{0, 2, 4, 5, 7, 9, 11};
    constexpr int minor[]{0, 2, 3, 5, 7, 8, 10};
    constexpr const char* flat[]{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    constexpr const char* sharp[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    ConcreteChordEvent concrete;
    concrete.durationQN = durationQN;
    concrete.quality = event.quality;
    concrete.roles = event.roles;
    if (!event.degree || event.degree->degree < 1 || event.degree->degree > 7) {
        concrete.label = "?"; return concrete;
    }
    concrete.degree = *event.degree;
    const auto* scale = key.mode == Mode::Major ? major : minor;
    const int pc = positiveMod(static_cast<int>(key.tonic) + scale[event.degree->degree - 1] +
                               event.degree->alteration, 12);
    const bool flatKey = key.tonic == PitchClass::Db || key.tonic == PitchClass::Eb ||
        key.tonic == PitchClass::F || key.tonic == PitchClass::Gb || key.tonic == PitchClass::Ab ||
        key.tonic == PitchClass::Bb || event.degree->alteration < 0 ||
        hasRole(event.roles, Role::Borrowed);
    concrete.label = flatKey ? flat[pc] : sharp[pc];
    switch (event.quality) {
        case ChordQuality::Minor: concrete.label += "m"; break;
        case ChordQuality::Dominant7: concrete.label += "7"; break;
        case ChordQuality::Major7: concrete.label += "maj7"; break;
        case ChordQuality::Minor7: concrete.label += "m7"; break;
        case ChordQuality::Diminished: concrete.label += "dim"; break;
        case ChordQuality::Diminished7: concrete.label += "dim7"; break;
        case ChordQuality::HalfDiminished7: concrete.label += "m7b5"; break;
        case ChordQuality::Sus2: concrete.label += "sus2"; break;
        case ChordQuality::Sus4: concrete.label += "sus4"; break;
        case ChordQuality::Augmented: concrete.label += "aug"; break;
        default: break;
    }
    return concrete;
}

RecommendationSet recommendContinuations(const MatchQuery& query, const CandidateIndex& index,
                                         const RecommendationRequest& request, const RecommendationWeights& w) {
    RecommendationSet result;
    result.matches = matchProgression(query, index, {}, index.templates().size(), &result.stats);
    std::map<std::string, ContinuationCandidate> distinct;
    std::unordered_map<TemplateID, const ProgressionTemplate*> byId;
    byId.reserve(index.templates().size());
    for (const auto& item : index.templates()) byId.emplace(item.id, &item);
    for (const auto& match : result.matches) {
        if (match.similarity < w.minimumMatch) continue;
        const auto found = byId.find(match.templateId);
        if (found == byId.end()) continue;
        const auto& item = *found->second;
        const auto* selected = interpretationFor(query, match.key);
        if (!selected || selected->full.empty()) continue;
        const auto lastQuery = selected->full.size() - 1;
        const bool tailAligned = std::any_of(match.alignmentTrace.begin(), match.alignmentTrace.end(),
            [&](const AlignmentStep& step) {
                return step.queryIndex == lastQuery && step.templateIndex == match.templateMatchEnd &&
                    (step.operation == AlignmentOp::Match || step.operation == AlignmentOp::Substitute);
            });
        if (!tailAligned) continue;
        if (!match.hasContinuation && !item.loopable) continue;
        std::vector<MatchEvent> suffix;
        for (std::size_t i = match.continuationStartIndex; i < item.full.size(); ++i) suffix.push_back(item.full[i]);
        if (item.loopable) {
            const auto wrapEnd = match.templateMatchStart == 0 ? 1 : match.templateMatchStart;
            for (std::size_t i = 0; i < std::min(wrapEnd, item.full.size()); ++i) suffix.push_back(item.full[i]);
        }
        if (suffix.empty()) continue;
        auto rhythm = estimateRhythmScale(match, query, item);
        ContinuationCandidate candidate;
        candidate.id = item.id;
        candidate.primaryTemplate = item.id;
        candidate.key = match.key;
        candidate.intent = effectiveIntent(item, suffix);
        candidate.cadence = item.cadence;
        candidate.styles = item.styles;
        candidate.matchStart = match.templateMatchStart;
        candidate.matchEnd = match.templateMatchEnd;
        candidate.continuationStart = match.continuationStartIndex;
        candidate.rhythmScale = rhythm.scale;
        candidate.rhythmConfidence = rhythm.confidence;
        candidate.matchSimilarity = match.similarity;
        candidate.supportingTemplates.push_back(item.id);
        for (const auto& event : suffix)
            candidate.continuation.push_back(realizeContinuation(event, match.key,
                event.durationQN.value_or(4.0) * rhythm.scale));
        // A repeated copy of the current open chord offers no new harmonic step.
        const auto currentLabel = realizeContinuation(selected->full.back(), match.key, 1.0).label;
        if (!candidate.continuation.empty() && candidate.continuation.front().label == currentLabel)
            continue;
        if (!selected->full.back().durationQN) {
            for (const auto& step : match.alignmentTrace) {
                if (step.queryIndex == lastQuery && step.templateIndex && *step.templateIndex < item.full.size() &&
                    item.full[*step.templateIndex].durationQN)
                    candidate.suggestedCurrentChordDurationQN =
                        *item.full[*step.templateIndex].durationQN * rhythm.scale;
            }
        }
        auto& s = candidate.subscores;
        s.match = match.similarity;
        s.skeleton = match.subScores.skeletonHarmony;
        s.style = styleScore(item, request.style);
        s.intent = request.preferredIntent && *request.preferredIntent == candidate.intent ? 1.f : 0.5f;
        s.cadence = cadenceScore(candidate.intent, item.cadence);
        s.continuation = clampScore(0.45f + 0.1f * static_cast<float>(std::min<std::size_t>(5, suffix.size())));
        s.prior = item.priorWeight;
        s.rhythm = rhythm.samples >= 2 ? clampScore(0.5f * match.subScores.rhythmSimilarity +
                                                      0.5f * rhythm.confidence) : 0.5f;
        s.support = 0.f;
        candidate.rankingScore = 100.f * (w.match*s.match + w.skeleton*s.skeleton + w.style*s.style +
            w.intent*s.intent + w.cadence*s.cadence + w.continuation*s.continuation + w.prior*s.prior +
            w.rhythm*s.rhythm);
        for (const auto& step : match.alignmentTrace) {
            if (step.operation == AlignmentOp::QueryInsertion &&
                !hasReason(step.reasons, MatchReason::ResolvesToMatchedTarget))
                candidate.rankingScore -= w.unexplainedInsertionPenalty;
            else if (step.operation == AlignmentOp::TemplateDeletion)
                candidate.rankingScore -= w.templateDeletionPenalty;
        }
        const auto fingerprint = suffixFingerprint(candidate, false);
        auto [it, inserted] = distinct.try_emplace(fingerprint, std::move(candidate));
        if (!inserted) {
            // A selected style can make a later source the better explanation.
            const auto previousCount = it->second.supportCount;
            auto sources = std::move(it->second.supportingTemplates);
            sources.push_back(item.id);
            if (candidate.rankingScore > it->second.rankingScore) {
                candidate.supportCount = previousCount + 1;
                candidate.supportingTemplates = std::move(sources);
                it->second = std::move(candidate);
            } else {
                it->second.supportCount = previousCount + 1;
                it->second.supportingTemplates = std::move(sources);
            }
        }
    }
    for (auto& [key, candidate] : distinct) {
        candidate.subscores.support = clampScore(static_cast<float>(candidate.supportCount - 1) / 4.f);
        candidate.rankingScore += 100.f * w.support * candidate.subscores.support;
        if (candidate.rankingScore >= w.minimumScore) result.groups[groupIndex(candidate.intent)].push_back(std::move(candidate));
    }
    for (auto& group : result.groups) {
        std::sort(group.begin(), group.end(), [](const auto& a, const auto& b) {
            return a.rankingScore == b.rankingScore ? a.id < b.id : a.rankingScore > b.rankingScore;
        });
        std::vector<ContinuationCandidate> diverse;
        for (auto& candidate : group) {
            const auto coarse = suffixFingerprint(candidate, true);
            bool near{};
            for (const auto& chosen : diverse) {
                if (suffixFingerprint(chosen, true) == coarse) { near = true; break; }
                std::size_t shared{};
                const auto common = std::min(chosen.continuation.size(), candidate.continuation.size());
                while (shared < common && chosen.continuation[shared].degree.degree == candidate.continuation[shared].degree.degree &&
                       chosen.continuation[shared].degree.alteration == candidate.continuation[shared].degree.alteration)
                    ++shared;
                if (shared >= 2 && shared * 2 >= common) candidate.rankingScore -= w.diversityPenalty;
            }
            if (!near && candidate.rankingScore >= w.minimumScore) diverse.push_back(std::move(candidate));
        }
        std::sort(diverse.begin(), diverse.end(), [](const auto& a, const auto& b) { return a.rankingScore > b.rankingScore; });
        if (diverse.size() > w.perGroup) diverse.resize(w.perGroup);
        group = std::move(diverse);
    }
    return result;
}
const char* intentName(PhraseIntent value) noexcept {
    switch (value) {
        case PhraseIntent::Resolve: return "RESOLVE"; case PhraseIntent::Develop: return "DEVELOP";
        case PhraseIntent::Loop: return "LOOP"; case PhraseIntent::Color: return "COLOR";
        default: return "NEUTRAL";
    }
}
const char* cadenceName(CadenceType value) noexcept {
    switch (value) {
        case CadenceType::Authentic: return "authentic";
        case CadenceType::PerfectAuthentic: return "perfect authentic";
        case CadenceType::ImperfectAuthentic: return "imperfect authentic";
        case CadenceType::Plagal: return "plagal"; case CadenceType::Half: return "half";
        case CadenceType::Deceptive: return "deceptive"; case CadenceType::Modal: return "modal";
        case CadenceType::LoopClosure: return "loop closure";
        case CadenceType::Unknown: return "unknown"; default: return "none";
    }
}
} // namespace harmony
