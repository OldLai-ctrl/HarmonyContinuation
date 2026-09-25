#include "ProgressionMatcher.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <tuple>

namespace harmony {
namespace {
constexpr float epsilon = 0.0001f;
int relativeSemitone(ScaleDegree degree, Mode mode) {
    constexpr int major[]{0, 2, 4, 5, 7, 9, 11};
    constexpr int minor[]{0, 2, 3, 5, 7, 8, 10};
    if (degree.degree < 1 || degree.degree > 7) return -1;
    const auto* tones = mode == Mode::Major ? major : minor;
    return (tones[degree.degree - 1] + degree.alteration + 12) % 12;
}

bool sameDegree(const std::optional<ScaleDegree>& a, const std::optional<ScaleDegree>& b) {
    return a && b && a->degree == b->degree && a->alteration == b->alteration;
}
bool sameBase(const std::optional<ScaleDegree>& a, const std::optional<ScaleDegree>& b) {
    return a && b && a->degree == b->degree;
}
bool isDominant(HarmonicFunction f) {
    return f == HarmonicFunction::Dominant || f == HarmonicFunction::ChromaticDominant;
}
bool isPredominant(HarmonicFunction f) {
    return f == HarmonicFunction::Predominant || f == HarmonicFunction::ChromaticPredominant;
}
bool isTonic(HarmonicFunction f) {
    return f == HarmonicFunction::Tonic || f == HarmonicFunction::TonicProlongation;
}
bool extensionVariant(ChordQuality a, ChordQuality b) {
    using Q = ChordQuality;
    return (a == Q::Major && b == Q::Major7) || (b == Q::Major && a == Q::Major7) ||
           (a == Q::Minor && b == Q::Minor7) || (b == Q::Minor && a == Q::Minor7) ||
           (a == Q::Major && b == Q::Dominant7) || (b == Q::Major && a == Q::Dominant7) ||
           (a == Q::Diminished && b == Q::Diminished7) || (b == Q::Diminished && a == Q::Diminished7);
}
float median(std::vector<double> values) {
    values.erase(std::remove_if(values.begin(), values.end(), [](double v) { return !std::isfinite(v) || v <= 0; }), values.end());
    if (values.empty()) return 1.f;
    std::sort(values.begin(), values.end());
    const auto mid = values.size() / 2;
    return static_cast<float>(values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2.0);
}
std::vector<float> normalizedDurations(const std::vector<MatchEvent>& events) {
    std::vector<double> known;
    for (const auto& e : events) if (e.durationQN) known.push_back(*e.durationQN);
    const auto scale = std::max(median(std::move(known)), epsilon);
    std::vector<float> result;
    result.reserve(events.size());
    for (const auto& e : events) result.push_back(e.durationQN && *e.durationQN > 0
        ? static_cast<float>(*e.durationQN) / scale : 0.f);
    return result;
}
std::vector<MatchEvent> skeleton(const std::vector<MatchEvent>& full, const std::vector<std::size_t>& indices) {
    std::vector<MatchEvent> result;
    result.reserve(indices.size());
    for (std::size_t n = 0; n < indices.size(); ++n) {
        const auto i = indices[n];
        if (i >= full.size()) continue;
        auto event = full[i];
        if (n + 1 < indices.size() && indices[n + 1] < full.size()) {
            double duration{};
            bool known = true;
            for (auto k = i; k < indices[n + 1]; ++k) {
                if (!full[k].durationQN) { known = false; break; }
                duration += *full[k].durationQN;
            }
            event.durationQN = known ? std::optional<double>(duration) : std::nullopt;
        }
        result.push_back(event);
    }
    return result;
}
float gapCost(const MatchEvent& e, const MatchWeights& w, MatchReasonFlags& reasons, bool insertion) {
    const bool light = hasRole(e.roles, Role::Passing) || hasRole(e.roles, Role::Embellishing) ||
                       hasRole(e.roles, Role::Approach);
    reasons |= flag(insertion ? (light ? MatchReason::EmbellishingInsertion : MatchReason::StructuralInsertion)
                              : (light ? MatchReason::EmbellishingDeletion : MatchReason::StructuralDeletion));
    float factor = 1.f;
    if (hasRole(e.roles, Role::Passing) || hasRole(e.roles, Role::Embellishing)) factor *= w.passingGapFactor;
    else if (hasRole(e.roles, Role::Approach)) factor *= w.approachGapFactor;
    else if (hasRole(e.roles, Role::SecondaryDominant) || hasRole(e.roles, Role::SecondaryLeadingTone))
        factor *= w.secondaryGapFactor;
    if (e.function == HarmonicFunction::TonicProlongation) factor *= w.tonicProlongationGapFactor;
    if (isPredominant(e.function) && !light) factor *= w.predominantGapFactor;
    if (e.function == HarmonicFunction::Dominant && !light) factor *= w.dominantGapFactor;
    if (hasRole(e.roles, Role::Cadential)) {
        factor *= w.cadentialGapFactor;
        if (isTonic(e.function)) factor *= w.cadentialTonicGapFactor;
        reasons |= flag(MatchReason::CadentialImportance);
    }
    if (light || e.structuralWeight < 0.4f) reasons |= flag(MatchReason::LowStructuralPenalty);
    return (w.gapBase + w.structuralImportanceWeight * std::clamp(e.structuralWeight, 0.f, 1.f)) * factor;
}

float pairCost(const MatchEvent& q, const MatchEvent& t, float qRhythm, float tRhythm,
               const MatchWeights& w, MatchReasonFlags& reasons, bool queryFinal, Mode mode) {
    float cost{};
    if (sameDegree(q.degree, t.degree)) reasons |= flag(MatchReason::SameDegree);
    else if (q.degree && t.degree && relativeSemitone(*q.degree, mode) == relativeSemitone(*t.degree, mode)) {
        reasons |= flag(MatchReason::EnharmonicDegree);
        cost += w.enharmonicDegreeDifference;
    }
    else if (sameBase(q.degree, t.degree)) {
        reasons |= flag(MatchReason::SameBaseDegree);
        cost += w.alteredDegree;
    } else if (q.degree && t.degree) {
        cost += w.degreeDifference * (q.function == t.function && q.function != HarmonicFunction::Unknown
                                         ? w.sameFunctionDegreeFactor : 1.f);
    } else cost += w.degreeDifference * 0.7f;

    if (q.quality != t.quality && q.quality != ChordQuality::Unknown && t.quality != ChordQuality::Unknown) {
        reasons |= flag(MatchReason::QualityVariant);
        cost += extensionVariant(q.quality, t.quality) ? w.qualityExtensionDifference : w.qualityDifference;
    }
    if (q.function == t.function && q.function != HarmonicFunction::Unknown) reasons |= flag(MatchReason::SameFunction);
    else if (q.function != t.function) {
        cost += (isPredominant(q.function) && isPredominant(t.function)) ||
                (isDominant(q.function) && isDominant(t.function)) ||
                (isTonic(q.function) && isTonic(t.function)) ? w.functionDifference * 0.32f : w.functionDifference;
    }
    const RoleFlags important = flag(Role::SecondaryDominant) | flag(Role::SecondaryLeadingTone) |
                                flag(Role::Borrowed) | flag(Role::Cadential) | flag(Role::Passing);
    auto qRoles = q.roles & important;
    auto tRoles = t.roles & important;
    // A phrase stopped on V has not yet shown whether it will resolve to I.
    if (queryFinal && sameDegree(q.degree, t.degree) && q.function == t.function) {
        qRoles &= ~flag(Role::Cadential);
        tRoles &= ~flag(Role::Cadential);
    }
    if (qRoles == tRoles) reasons |= flag(MatchReason::SameRole);
    else cost += w.roleDifference;
    if (hasRole(q.roles, Role::Borrowed) != hasRole(t.roles, Role::Borrowed) && sameBase(q.degree, t.degree)) {
        reasons |= flag(MatchReason::BorrowedVariant);
        cost *= w.borrowedVariantFactor;
    }
    if (q.target || t.target) {
        if (sameDegree(q.target, t.target)) reasons |= flag(MatchReason::SecondaryTargetMatch);
        else { reasons |= flag(MatchReason::SecondaryTargetMismatch); cost += w.secondaryTargetDifference; }
    }
    // A single implausible chord is still one substitution, rather than an
    // artificially cheap delete-plus-insert detour through the same position.
    cost = std::min(cost, w.substitutionMaxCost);
    // OPEN query durations never add a rhythmic penalty; their harmonic data remains.
    if (qRhythm > 0.f && tRhythm > 0.f) {
        const auto deviation = std::min(2.f, std::abs(std::log2(qRhythm / tRhythm)));
        if (deviation > 0.25f) reasons |= flag(MatchReason::RhythmMismatch);
        cost += w.rhythmWeight * deviation;
    }
    return cost;
}

struct LayerResult {
    float cost{};
    float similarity{};
    std::size_t start{};
    std::size_t end{};
    std::size_t pairCount{};
    std::vector<AlignmentStep> trace;
    float functionScore{};
    float roleScore{};
    float rhythmScore{};
};
struct Cell { float cost{std::numeric_limits<float>::infinity()}; std::uint8_t predecessor{}; std::size_t pairs{}; };

LayerResult align(const std::vector<MatchEvent>& q, const std::vector<MatchEvent>& t,
                  const MatchWeights& w, Mode mode) {
    LayerResult out;
    if (q.empty() || t.empty()) return out;
    const auto n = q.size(), m = t.size();
    const auto qr = normalizedDurations(q), tr = normalizedDurations(t);
    std::vector<Cell> dp((n + 1) * (m + 1));
    const auto at = [m, &dp](std::size_t i, std::size_t j) -> Cell& { return dp[i * (m + 1) + j]; };
    for (std::size_t j = 0; j <= m; ++j)
        at(0, j) = {w.templatePrefixSkipCost * static_cast<float>(j), 0, 0}; // very low-cost template prefix
    for (std::size_t i = 1; i <= n; ++i) {
        MatchReasonFlags reasons{};
        auto gap = gapCost(q[i - 1], w, reasons, true);
        if (i == n && !hasReason(reasons, MatchReason::EmbellishingInsertion)) gap *= w.queryTailStructuralFactor;
        at(i, 0) = {at(i - 1, 0).cost + gap, 2, 0};
    }
    for (std::size_t i = 1; i <= n; ++i) for (std::size_t j = 1; j <= m; ++j) {
        MatchReasonFlags reasons{};
        const float pair = at(i - 1, j - 1).cost + pairCost(q[i - 1], t[j - 1], qr[i - 1], tr[j - 1], w, reasons, i == n, mode);
        auto insertGap = gapCost(q[i - 1], w, reasons, true);
        if (i == n && !hasReason(reasons, MatchReason::EmbellishingInsertion)) insertGap *= w.queryTailStructuralFactor;
        const auto insert = at(i - 1, j).cost + insertGap;
        const auto erase = at(i, j - 1).cost + gapCost(t[j - 1], w, reasons, false);
        if (pair <= insert + epsilon && pair <= erase + epsilon)
            at(i, j) = {pair, 1, at(i - 1, j - 1).pairs + 1};
        else if (insert <= erase + epsilon)
            at(i, j) = {insert, 2, at(i - 1, j).pairs};
        else at(i, j) = {erase, 3, at(i, j - 1).pairs};
    }
    // The complete query is consumed. Template suffix is free; prefer higher
    // pair coverage when endpoints have equal cost.
    std::size_t end = 1;
    float best = std::numeric_limits<float>::infinity();
    for (std::size_t j = 1; j <= m; ++j) {
        const auto& cell = at(n, j);
        const float coverage = static_cast<float>(cell.pairs) / static_cast<float>(n);
        const float adjusted = cell.cost + w.lowCoveragePenalty *
            std::max(0.f, w.minimumMatchedFraction - coverage) * static_cast<float>(n);
        if (adjusted < best - epsilon || (std::abs(adjusted - best) <= epsilon && cell.pairs > at(n, end).pairs)) {
            best = adjusted;
            end = j;
        }
    }
    out.cost = best;
    out.similarity = std::clamp(std::exp(-best / (std::max(1.f, static_cast<float>(n)) * w.similarityScale)), 0.f, 1.f);
    out.end = end - 1;
    std::size_t i = n, j = end;
    float functionSum{}, roleSum{}, rhythmSum{};
    std::size_t rhythmCount{};
    std::vector<std::optional<std::pair<std::size_t, std::size_t>>> pairPositions;
    while (i > 0) {
        const auto pred = at(i, j).predecessor;
        AlignmentStep step;
        if (pred == 1 && j > 0) {
            --i; --j;
            pairPositions.emplace_back(std::pair{i, j});
            step.queryIndex = q[i].sourceIndex;
            step.templateIndex = t[j].sourceIndex;
            step.cost = pairCost(q[i], t[j], qr[i], tr[j], w, step.reasons, i + 1 == n, mode);
            step.operation = step.cost <= epsilon ? AlignmentOp::Match : AlignmentOp::Substitute;
            ++out.pairCount;
            functionSum += q[i].function == t[j].function ? 1.f : 0.f;
            roleSum += (step.reasons & flag(MatchReason::SameRole)) ? 1.f : 0.f;
        } else if (pred == 2 || j == 0) {
            --i;
            pairPositions.emplace_back(std::nullopt);
            step.queryIndex = q[i].sourceIndex;
            step.operation = AlignmentOp::QueryInsertion;
            step.cost = gapCost(q[i], w, step.reasons, true);
            if (i + 1 == n && !hasReason(step.reasons, MatchReason::EmbellishingInsertion))
                step.cost *= w.queryTailStructuralFactor;
        } else {
            --j;
            pairPositions.emplace_back(std::nullopt);
            step.templateIndex = t[j].sourceIndex;
            step.operation = AlignmentOp::TemplateDeletion;
            step.cost = gapCost(t[j], w, step.reasons, false);
        }
        out.trace.push_back(step);
    }
    out.start = j;
    std::reverse(out.trace.begin(), out.trace.end());
    std::reverse(pairPositions.begin(), pairPositions.end());
    for (std::size_t k = 0; k < out.trace.size(); ++k) {
        auto& step = out.trace[k];
        if (step.operation != AlignmentOp::QueryInsertion || !step.queryIndex) continue;
        const auto inserted = std::find_if(q.begin(), q.end(), [&](const auto& event) {
            return event.sourceIndex == *step.queryIndex;
        });
        if (inserted == q.end() || !inserted->target) continue;
        for (std::size_t next = k + 1; next < out.trace.size(); ++next) {
            if (!out.trace[next].queryIndex) continue;
            if (out.trace[next].templateIndex) {
                const auto resolved = std::find_if(q.begin(), q.end(), [&](const auto& event) {
                    return event.sourceIndex == *out.trace[next].queryIndex;
                });
                if (resolved != q.end() && sameDegree(inserted->target, resolved->degree))
                    step.reasons |= flag(MatchReason::ResolvesToMatchedTarget);
            }
            break;
        }
    }
    // Estimate one global tempo ratio from aligned, known-duration pairs.
    // This excludes an OPEN final chord and unmatched passing events, so
    // missing durations cannot shift the normalization of earlier chords.
    std::vector<double> ratios;
    for (const auto& pair : pairPositions) if (pair) {
        const auto [qi, ti] = *pair;
        if (q[qi].durationQN && t[ti].durationQN && *q[qi].durationQN > 0 && *t[ti].durationQN > 0)
            ratios.push_back(*t[ti].durationQN / *q[qi].durationQN);
    }
    const auto tempoScale = median(std::move(ratios));
    for (std::size_t k = 0; k < out.trace.size(); ++k) if (pairPositions[k]) {
        const auto [qi, ti] = *pairPositions[k];
        if (!q[qi].durationQN || !t[ti].durationQN || *q[qi].durationQN <= 0 || *t[ti].durationQN <= 0) continue;
        const auto oldDeviation = qr[qi] > 0.f && tr[ti] > 0.f
            ? std::min(2.f, std::abs(std::log2(qr[qi] / tr[ti]))) : 0.f;
        const auto newDeviation = std::min(2.f, static_cast<float>(
            std::abs(std::log2(*q[qi].durationQN * tempoScale / *t[ti].durationQN))));
        const auto correction = w.rhythmWeight * (newDeviation - oldDeviation);
        out.cost += correction;
        out.trace[k].cost += correction;
        out.trace[k].reasons &= ~flag(MatchReason::RhythmMismatch);
        if (newDeviation > 0.25f) out.trace[k].reasons |= flag(MatchReason::RhythmMismatch);
        if (out.trace[k].cost <= epsilon) {
            out.trace[k].cost = 0.f;
            out.trace[k].operation = AlignmentOp::Match;
        }
        rhythmSum += std::max(0.f, 1.f - newDeviation / 2.f);
        ++rhythmCount;
    }
    out.similarity = std::clamp(std::exp(-out.cost / (std::max(1.f, static_cast<float>(n)) * w.similarityScale)), 0.f, 1.f);
    out.functionScore = out.pairCount ? functionSum / static_cast<float>(out.pairCount) : 0.f;
    out.roleScore = out.pairCount ? roleSum / static_cast<float>(out.pairCount) : 0.f;
    out.rhythmScore = rhythmCount ? rhythmSum / static_cast<float>(rhythmCount) : 1.f;
    return out;
}

float fingerprintScore(const ProgressionFingerprint& q, const ProgressionFingerprint& t) {
    float score{};
    if (q.mode == t.mode) score += 2.f;
    const auto lengthDifference = q.skeletonLength > t.skeletonLength
        ? q.skeletonLength - t.skeletonLength : t.skeletonLength - q.skeletonLength;
    score += 1.f / (1.f + static_cast<float>(lengthDifference) * 0.25f);
    for (int d : q.skeletonDegrees) if (std::find(t.skeletonDegrees.begin(), t.skeletonDegrees.end(), d) != t.skeletonDegrees.end()) score += 0.5f;
    for (int pair : q.skeletonBigrams) if (std::find(t.skeletonBigrams.begin(), t.skeletonBigrams.end(), pair) != t.skeletonBigrams.end()) score += 0.9f;
    if (q.endingFunction == t.endingFunction) score += 0.35f;
    if (q.cadence == t.cadence) score += 0.15f;
    if (q.roleSummary & t.roleSummary) score += 0.1f;
    return score;
}
} // namespace

MatchQuery makeMatchQuery(const Progression& source, const AnalysisContext& context, const MatchWeights& w) {
    MatchQuery query;
    query.forcedKey = context.forcedKey.has_value();
    query.timeSigNumerator = context.timeSigNumerator;
    query.timeSigDenominator = context.timeSigDenominator;
    if (source.empty()) return query;
    const auto initial = analyzeHarmony(source, context);
    std::vector<KeyCandidate> keys;
    if (context.forcedKey && initial.selectedKey) keys.push_back(*initial.selectedKey);
    else for (const auto& key : initial.keyCandidates) {
        if (keys.size() >= w.maxKeyCandidates) break;
        if (key.confidence >= w.minKeyConfidence || keys.empty()) keys.push_back(key);
    }
    for (const auto& key : keys) {
        auto selectedContext = context;
        selectedContext.forcedKey = key.key;
        const auto analysis = analyzeHarmony(source, selectedContext);
        KeyInterpretation interpretation;
        interpretation.key = key;
        interpretation.skeletonIndices = analysis.skeletonIndices;
        for (std::size_t i = 0; i < analysis.full.size(); ++i) {
            const auto& e = analysis.full[i];
            interpretation.full.push_back({e.degree, e.target, e.chord.quality, e.function, e.roles,
                                           e.structuralWeight, source[i].durationQN, i});
        }
        interpretation.fingerprint = makeFingerprint(key.key.mode, interpretation.full,
                                                     interpretation.skeletonIndices, CadenceType::None);
        query.interpretations.push_back(std::move(interpretation));
    }
    return query;
}

ProgressionFingerprint makeFingerprint(Mode mode, const std::vector<MatchEvent>& full,
                                        const std::vector<std::size_t>& indices, CadenceType cadence) {
    ProgressionFingerprint result;
    result.mode = mode;
    result.skeletonLength = indices.size();
    result.cadence = cadence;
    const auto skel = skeleton(full, indices);
    const auto rhythm = normalizedDurations(skel);
    for (std::size_t i = 0; i < skel.size(); ++i) {
        const auto& e = skel[i];
        if (e.degree) result.skeletonDegrees.push_back(relativeSemitone(*e.degree, mode));
        result.rhythmShape.push_back(rhythm[i] < 0.7f ? 0 : rhythm[i] > 1.4f ? 2 : 1);
    }
    for (std::size_t i = 1; i < result.skeletonDegrees.size(); ++i)
        result.skeletonBigrams.push_back((result.skeletonDegrees[i - 1] + 16) * 32 + result.skeletonDegrees[i] + 16);
    if (!skel.empty()) result.endingFunction = skel.back().function;
    for (const auto& e : full) {
        if (e.structuralWeight >= 0.55f) ++result.structuralCount;
        result.roleSummary |= e.roles;
    }
    return result;
}
void prepareTemplate(ProgressionTemplate& value) {
    if (value.skeletonIndices.empty()) {
        for (std::size_t i = 0; i < value.full.size(); ++i)
            if (value.full[i].structuralWeight >= 0.55f || i == 0 || i + 1 == value.full.size())
                value.skeletonIndices.push_back(i);
    }
    for (std::size_t i = 0; i < value.full.size(); ++i) value.full[i].sourceIndex = i;
    value.fingerprint = makeFingerprint(value.mode, value.full, value.skeletonIndices, value.cadence);
}
CandidateIndex::CandidateIndex(std::vector<ProgressionTemplate> templates) : templates_(std::move(templates)) {
    for (auto& item : templates_) prepareTemplate(item);
}
std::vector<std::size_t> CandidateIndex::shortlist(const MatchQuery& query, const MatchWeights& w) const {
    std::vector<std::size_t> result;
    if (query.interpretations.empty()) return result;
    result.reserve(templates_.size());
    if (templates_.size() <= w.shortlistLimit) {
        for (std::size_t i = 0; i < templates_.size(); ++i) result.push_back(i);
        return result;
    }
    std::vector<std::pair<float, std::size_t>> ranked;
    ranked.reserve(templates_.size());
    for (std::size_t i = 0; i < templates_.size(); ++i) {
        float best{};
        for (const auto& interpretation : query.interpretations)
            best = std::max(best, fingerprintScore(interpretation.fingerprint, templates_[i].fingerprint));
        ranked.emplace_back(best, i);
    }
    const auto limit = std::min(w.shortlistLimit, ranked.size());
    std::partial_sort(ranked.begin(), ranked.begin() + limit, ranked.end(), [](const auto& a, const auto& b) {
        return a.first == b.first ? a.second < b.second : a.first > b.first;
    });
    for (std::size_t i = 0; i < limit; ++i) result.push_back(ranked[i].second);
    return result;
}

std::vector<MatchResult> matchProgression(const MatchQuery& query, const CandidateIndex& index,
                                          const MatchWeights& w, std::size_t topN, MatchRunStats* stats) {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    const auto candidates = index.shortlist(query, w);
    const auto pref = Clock::now();
    std::vector<MatchResult> results;
    results.reserve(candidates.size());
    for (const auto ti : candidates) {
        const auto& t = index.templates()[ti];
        if (t.full.empty()) continue;
        std::optional<MatchResult> best;
        const auto ts = skeleton(t.full, t.skeletonIndices);
        for (const auto& q : query.interpretations) {
            if (q.key.key.mode != t.mode || q.full.empty()) continue;
            const auto qs = skeleton(q.full, q.skeletonIndices);
            const auto full = align(q.full, t.full, w, t.mode);
            const auto structural = align(qs, ts, w, t.mode);
            if (!full.pairCount || !structural.pairCount) continue;
            MatchResult r;
            r.templateId = t.id;
            r.templateName = t.name;
            r.key = q.key.key;
            r.keyPrior = q.key.confidence;
            r.rawCost = full.cost;
            r.subScores.skeletonHarmony = structural.similarity;
            r.subScores.fullHarmony = full.similarity;
            r.subScores.functionSimilarity = full.functionScore;
            r.subScores.roleSimilarity = full.roleScore;
            r.subScores.rhythmSimilarity = full.rhythmScore;
            r.subScores.keyCompatibility = q.key.confidence;
            const auto harmonyScore = (w.skeletonWeight * structural.similarity + w.fullWeight * full.similarity) /
                                      std::max(epsilon, w.skeletonWeight + w.fullWeight);
            r.similarity = std::clamp((1.f - w.keyPriorWeight) * harmonyScore +
                                      w.keyPriorWeight * q.key.confidence, 0.f, 1.f);
            r.templateMatchStart = full.start;
            r.templateMatchEnd = full.end;
            r.continuationStartIndex = std::min(t.full.size(), full.end + 1);
            r.hasContinuation = r.continuationStartIndex < t.full.size();
            r.continuationLength = t.full.size() - r.continuationStartIndex;
            r.alignmentTrace = full.trace;
            for (const auto& event : t.full) r.templateLabels.push_back(formatMatchEvent(event));
            if (!best || r.similarity > best->similarity) best = std::move(r);
        }
        if (best) results.push_back(std::move(*best));
    }
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.similarity == b.similarity ? a.templateId < b.templateId : a.similarity > b.similarity;
    });
    if (results.size() > topN) results.resize(topN);
    const auto finished = Clock::now();
    if (stats) {
        stats->prefilterMs = std::chrono::duration<double, std::milli>(pref - started).count();
        stats->alignmentMs = std::chrono::duration<double, std::milli>(finished - pref).count();
        stats->totalMs = std::chrono::duration<double, std::milli>(finished - started).count();
        stats->shortlistSize = candidates.size();
    }
    return results;
}

const char* alignmentOpName(AlignmentOp op) noexcept {
    switch (op) {
        case AlignmentOp::Match: return "MATCH";
        case AlignmentOp::Substitute: return "SUBSTITUTE";
        case AlignmentOp::QueryInsertion: return "QUERY_INSERTION";
        case AlignmentOp::TemplateDeletion: return "TEMPLATE_DELETION";
    }
    return "?";
}
std::string reasonNames(MatchReasonFlags flags) {
    const std::pair<MatchReason, const char*> names[] = {
        {MatchReason::SameDegree,"SameDegree"}, {MatchReason::SameBaseDegree,"SameBaseDegree"},
        {MatchReason::SameFunction,"SameFunction"}, {MatchReason::QualityVariant,"QualityVariant"},
        {MatchReason::BorrowedVariant,"BorrowedVariant"}, {MatchReason::SameRole,"SameRole"},
        {MatchReason::SecondaryTargetMatch,"SecondaryTargetMatch"}, {MatchReason::SecondaryTargetMismatch,"SecondaryTargetMismatch"},
        {MatchReason::EmbellishingInsertion,"EmbellishingInsertion"}, {MatchReason::StructuralInsertion,"StructuralInsertion"},
        {MatchReason::EmbellishingDeletion,"EmbellishingDeletion"}, {MatchReason::StructuralDeletion,"StructuralDeletion"},
        {MatchReason::RhythmMismatch,"RhythmMismatch"}, {MatchReason::CadentialImportance,"CadentialImportance"},
        {MatchReason::LowStructuralPenalty,"LowStructuralPenalty"}, {MatchReason::ResolvesToMatchedTarget,"ResolvesToMatchedTarget"},
        {MatchReason::EnharmonicDegree,"EnharmonicDegree"}
    };
    std::string out;
    for (const auto& [bit, name] : names) if (hasReason(flags, bit)) {
        if (!out.empty()) out += '|';
        out += name;
    }
    return out;
}
std::string formatMatchEvent(const MatchEvent& e) {
    if (!e.degree) return "?";
    constexpr const char* upper[]{"?", "I", "II", "III", "IV", "V", "VI", "VII"};
    constexpr const char* lower[]{"?", "i", "ii", "iii", "iv", "v", "vi", "vii"};
    std::string text;
    const bool localFunctionSymbol = e.target &&
        (hasRole(e.roles, Role::SecondaryDominant) || hasRole(e.roles, Role::SecondaryLeadingTone));
    if (!localFunctionSymbol) {
        if (e.degree->alteration < 0) text += 'b';
        else if (e.degree->alteration > 0) text += '#';
    }
    if (e.degree->degree < 1 || e.degree->degree > 7) return "?";
    const bool minor = e.quality == ChordQuality::Minor || e.quality == ChordQuality::Minor7 ||
                       e.quality == ChordQuality::Diminished || e.quality == ChordQuality::Diminished7 ||
                       e.quality == ChordQuality::HalfDiminished7;
    if (e.target && hasRole(e.roles, Role::SecondaryDominant)) text += 'V';
    else if (e.target && hasRole(e.roles, Role::SecondaryLeadingTone)) text += "vii";
    else text += minor ? lower[e.degree->degree] : upper[e.degree->degree];
    switch (e.quality) {
        case ChordQuality::Minor: case ChordQuality::Minor7: break;
        case ChordQuality::Diminished: case ChordQuality::Diminished7: text += "dim"; break;
        case ChordQuality::HalfDiminished7: text += "m7b5"; break;
        default: break;
    }
    if (e.quality == ChordQuality::Dominant7 || e.quality == ChordQuality::Minor7 || e.quality == ChordQuality::Diminished7) text += '7';
    else if (e.quality == ChordQuality::Major7) text += "maj7";
    if (e.target && e.target->degree >= 1 && e.target->degree <= 7) {
        text += '/';
        if (e.target->alteration < 0) text += 'b';
        else if (e.target->alteration > 0) text += '#';
        text += (e.target->degree == 2 || e.target->degree == 3 || e.target->degree == 6 || e.target->degree == 7)
            ? lower[e.target->degree] : upper[e.target->degree];
    }
    return text;
}
} // namespace harmony
