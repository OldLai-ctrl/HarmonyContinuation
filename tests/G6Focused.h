#pragma once

#include "ProgressionJson.h"
#include "core/ChordPitchSet.h"
#include "core/ContextualContinuation.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace {
using namespace harmony;
void require(bool condition, const std::string& message);
const char* groupName(std::size_t index);
void exactOutput(const preview::Sequence& sequence);
using G6Clock = std::chrono::steady_clock;

using G6MatchKey = std::tuple<TemplateID, int, int, std::size_t, std::size_t, std::size_t>;

G6MatchKey g6MatchKey(const ContinuationEvidence& evidence) {
    return {evidence.templateId, static_cast<int>(evidence.key.tonic),
            static_cast<int>(evidence.key.mode), evidence.matchStart,
            evidence.matchEnd, evidence.continuationStart};
}

G6MatchKey g6MatchKey(const MatchResult& match) {
    return {match.templateId, static_cast<int>(match.key.tonic),
            static_cast<int>(match.key.mode), match.templateMatchStart,
            match.templateMatchEnd, match.continuationStartIndex};
}

std::string g6TsvCell(std::string value) {
    for (auto& ch : value)
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    return value;
}

template <typename Range, typename Project>
std::string g6Join(const Range& values, std::string_view separator, Project project) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) result += separator;
        result += project(value);
    }
    return result;
}

const char* g6ModeName(Mode mode) {
    return mode == Mode::Major ? "major" : "minor";
}

std::vector<std::string> g6TemplateTail(const ProgressionTemplate& source,
                                        const MatchResult& match) {
    std::vector<std::string> notes;
    const auto add = [&](std::size_t i) {
        const auto& event = source.full[i];
        const auto realized = realizeContinuation(event, match.key, event.durationQN.value_or(4.0));
        notes.push_back(realized.label);
    };
    for (std::size_t i = match.continuationStartIndex; i < source.full.size(); ++i) add(i);
    if (source.loopable) {
        const auto wrapEnd = match.templateMatchStart == 0
            ? (match.hasContinuation ? 0 : source.full.size())
            : match.templateMatchStart;
        for (std::size_t i = 0; i < std::min(wrapEnd, source.full.size()); ++i) add(i);
    }
    return notes;
}

bool g6SameCore(const ChordPitchSet& left, const ChordPitchSet& right) {
    return left.root >= 0 && left.bass >= 0 &&
        left.root == right.root && left.bass == right.bass &&
        left.quality == right.quality && left.intervals == right.intervals &&
        left.colorMask == right.colorMask;
}

std::array<std::map<std::string, std::set<std::string>>, 3> g6StrictCoverage(
    const MatchQuery& query, const CandidateIndex& index, std::ostream& report,
    std::string_view caseId) {
    const auto& input = query.source;
    require(input.size() >= 3, "G6 coverage requires at least three input chords");
    const std::array<std::string_view, 3> windowNames{"full", "tail3", "tail2"};
    const std::array<std::size_t, 3> starts{0, input.size() - 3, input.size() - 2};
    const std::array<std::size_t, 3> lengths{input.size(), 3, 2};
    std::array<std::map<std::string, std::set<std::string>>, 3> sourceWitnesses;
    std::vector<ChordPitchSet> inputCore;
    inputCore.reserve(input.size());
    for (const auto& event : input) inputCore.push_back(chordPitches(event));

    for (const auto& source : index.templates()) {
        for (const auto& interpretation : query.interpretations) {
            const KeySignature candidateKey{interpretation.key.key.tonic, source.mode};
            std::vector<ChordPitchSet> candidateCore;
            candidateCore.reserve(source.full.size());
            for (const auto& event : source.full) {
                const auto realized = realizeContinuation(
                    event, candidateKey, event.durationQN.value_or(4.0));
                candidateCore.push_back(realized.harmonicData
                    ? chordPitches(*realized.harmonicData)
                    : chordPitches(realized.label, realized.quality));
            }
            const std::string queryKey =
                std::to_string(static_cast<int>(interpretation.key.key.tonic)) + ":" +
                g6ModeName(interpretation.key.key.mode);
            const std::string candidateKeyText =
                std::to_string(static_cast<int>(candidateKey.tonic)) + ":" +
                g6ModeName(candidateKey.mode);
            for (std::size_t window = 0; window < windowNames.size(); ++window) {
                const auto inputStart = starts[window];
                const auto length = lengths[window];
                if (length > candidateCore.size()) continue;
                for (std::size_t templateStart = 0;
                     templateStart + length <= candidateCore.size(); ++templateStart) {
                    bool same = true;
                    for (std::size_t offset = 0; offset < length; ++offset) {
                        if (!g6SameCore(inputCore[inputStart + offset],
                                        candidateCore[templateStart + offset])) {
                            same = false;
                            break;
                        }
                    }
                    if (!same) continue;
                    const auto witness = queryKey + "=>" + candidateKeyText +
                        "@" + std::to_string(templateStart);
                    sourceWitnesses[window][source.id].insert(witness);
                }
            }
        }
    }

    for (std::size_t window = 0; window < windowNames.size(); ++window) {
        for (const auto& [sourceId, witnesses] : sourceWitnesses[window]) {
            report << caseId << '\t' << windowNames[window] << '\t' << sourceId << '\t'
                   << g6Join(witnesses, ";", [](const auto& value) { return value; }) << '\n';
        }
    }
    return sourceWitnesses;
}

struct G6RouteInfo {
    std::size_t group{}, rank{};
    const ContinuationCandidate* candidate{};
};
using G6RouteMap = std::map<G6MatchKey, G6RouteInfo>;

G6RouteMap g6RouteMap(const RecommendationSet& results) {
    G6RouteMap routes;
    for (std::size_t group = 0; group < results.groups.size(); ++group) {
        const auto& candidates = results.groups[group];
        for (std::size_t rank = 0; rank < candidates.size(); ++rank)
            for (const auto& evidence : candidates[rank].evidence)
                routes[g6MatchKey(evidence)] = {group, rank + 1, &candidates[rank]};
    }
    return routes;
}

std::string g6ChannelName(RecommendationChannel channel) {
    return recommendationChannelName(channel);
}

const char* g6ProvenanceName(ConnectionProvenance provenance) {
    switch (provenance) {
        case ConnectionProvenance::DirectSource: return "direct-source";
        case ConnectionProvenance::AlgorithmAssessed: return "algorithm-assessed";
        case ConnectionProvenance::EditorialReviewed: return "editorial-reviewed";
    }
    return "unknown";
}

const char* g6DirectionName(CreativeDirection direction) {
    switch (direction) {
        case CreativeDirection::Stable: return "stable";
        case CreativeDirection::Rich: return "rich";
        case CreativeDirection::Bold: return "bold";
    }
    return "unknown";
}

std::string g6CenterName(const std::optional<KeySignature>& center) {
    if (!center) return {};
    return std::to_string(static_cast<int>(center->tonic)) + ":" + g6ModeName(center->mode);
}

bool g6CrossCenter(const ContinuationCandidate& candidate) {
    return candidate.centerRelation == CenterRelation::Modulation;
}

struct G6MatchAggregate {
    std::size_t rows{};
    std::set<std::string> keys, groups;
    std::map<std::string, std::size_t> dispositions;
    const MatchResult* best{};
    const G6RouteInfo* bestRoute{};
};

void g6WriteMatchSummary(std::ostream& report, std::string_view caseId,
                         std::string_view runName,
                         const RecommendationSet& results,
                         const std::unordered_map<TemplateID, const ProgressionTemplate*>& sources) {
    const auto routes = g6RouteMap(results);
    std::map<TemplateID, G6MatchAggregate> summary;
    for (const auto& match : results.matches) {
        auto& aggregate = summary[match.templateId];
        ++aggregate.rows;
        aggregate.keys.insert(std::to_string(static_cast<int>(match.key.tonic)) + ":" +
                              g6ModeName(match.key.mode));
        ++aggregate.dispositions[match.disposition];
        const auto route = routes.find(g6MatchKey(match));
        const auto* routeInfo = route == routes.end() ? nullptr : &route->second;
        if (routeInfo) aggregate.groups.insert(groupName(routeInfo->group));
        const auto better = [&] {
            if (!aggregate.best) return true;
            if (routeInfo && !aggregate.bestRoute) return true;
            if (!routeInfo && aggregate.bestRoute) return false;
            return match.candidateEligibilityScore > aggregate.best->candidateEligibilityScore ||
                (match.candidateEligibilityScore == aggregate.best->candidateEligibilityScore &&
                 match.similarity > aggregate.best->similarity);
        };
        if (better()) {
            aggregate.best = &match;
            aggregate.bestRoute = routeInfo;
        }
    }

    for (const auto& [sourceId, aggregate] : summary) {
        const auto sourceIt = sources.find(sourceId);
        require(sourceIt != sources.end() && aggregate.best,
                "G6 diagnostic source aggregation is incomplete: " + sourceId);
        const auto& best = *aggregate.best;
        std::string followup;
        if (aggregate.bestRoute) {
            followup = g6Join(aggregate.bestRoute->candidate->continuation, ";",
                [](const auto& event) { return event.label; });
        } else {
            followup = g6Join(g6TemplateTail(*sourceIt->second, best), ";",
                [](const auto& value) { return value; });
        }
        std::vector<std::string> dispositions;
        for (const auto& [name, count] : aggregate.dispositions)
            dispositions.push_back(name + ":" + std::to_string(count));
        report << caseId << '\t' << runName << '\t' << sourceId << '\t'
               << aggregate.rows << '\t' << aggregate.keys.size() << '\t'
               << static_cast<int>(best.key.tonic) << ':' << g6ModeName(best.key.mode) << '\t'
               << best.similarity << '\t' << g6TsvCell(best.disposition) << '\t'
               << g6Join(dispositions, ";", [](const auto& value) { return value; }) << '\t'
               << g6Join(aggregate.groups, ",", [](const auto& value) { return value; }) << '\t'
               << g6TsvCell(followup) << '\n';
    }
}

void g6WriteRoutes(std::ostream& report, std::string_view caseId, std::string_view runName,
                   const RecommendationSet& results, double elapsedMs) {
    for (std::size_t group = 0; group < results.groups.size(); ++group) {
        const auto& candidates = results.groups[group];
        for (std::size_t rank = 0; rank < candidates.size(); ++rank) {
            const auto& candidate = candidates[rank];
            report << caseId << '\t' << runName << '\t' << groupName(group) << '\t' << rank + 1
                   << '\t' << g6ChannelName(candidate.channel) << '\t'
                   << g6ProvenanceName(candidate.provenance) << '\t'
                   << candidate.primaryTemplate << '\t' << candidate.id << '\t'
                   << continuationMusicFingerprint(candidate) << '\t'
                   << static_cast<int>(candidate.key.tonic) << ':' << g6ModeName(candidate.key.mode) << '\t'
                   << g6Join(candidate.supportingTemplates, ",",
                       [](const auto& value) { return value; }) << '\t'
                   << g6Join(candidate.continuation, ";",
                       [](const auto& event) { return event.label; }) << '\t'
                   << candidate.matchSimilarity << '\t' << candidate.eligibilityScore << '\t'
                   << candidate.rankingScore << '\t' << candidate.contextLength << '\t'
                   << g6CenterName(candidate.originCenter) << '\t'
                   << g6CenterName(candidate.targetCenter) << '\t'
                   << g6DirectionName(candidate.direction) << '\t'
                   << candidate.creativeQuality.tier << '\t'
                   << candidate.creativeQuality.qualified << '\t'
                   << g6TsvCell(candidate.family) << '\t'
                   << g6TsvCell(candidate.connectionReason) << '\t' << elapsedMs << '\n';
        }
    }
}

std::array<std::size_t, 5> g6ChannelCounts(const RecommendationSet& results) {
    std::array<std::size_t, 5> counts{};
    for (const auto& group : results.groups)
        for (const auto& candidate : group) {
            const auto channel = static_cast<std::size_t>(candidate.channel);
            if (channel < counts.size()) ++counts[channel];
        }
    return counts;
}

std::array<std::size_t, 4> g6DisplayCounts(const RecommendationSet& results) {
    std::array<std::size_t, 4> counts{};
    for (std::size_t group = 0; group < results.groups.size(); ++group)
        for (const auto& candidate : results.groups[group])
            if (candidate.baseQualified && !candidate.exploratory) ++counts[group];
    return counts;
}

std::size_t g6AlgorithmSeamsInResolve(const RecommendationSet& results) {
    return static_cast<std::size_t>(std::count_if(results.groups[0].begin(), results.groups[0].end(),
        [](const auto& candidate) {
            return candidate.channel == RecommendationChannel::PhraseTransition ||
                   candidate.provenance == ConnectionProvenance::AlgorithmAssessed;
        }));
}

bool g6SameScores(const ContinuationCandidate& left, const ContinuationCandidate& right) {
    constexpr float tolerance = 0.0001f;
    return std::abs(left.matchSimilarity - right.matchSimilarity) <= tolerance &&
        std::abs(left.eligibilityScore - right.eligibilityScore) <= tolerance &&
        std::abs(left.rankingScore - right.rankingScore) <= tolerance &&
        left.baseQualified == right.baseQualified;
}

std::size_t g6VerifyExactPreserved(std::string_view caseId,
    const RecommendationSet& baseline, const RecommendationSet& inspiration, std::ostream& report) {
    using Ref = std::pair<std::size_t, const ContinuationCandidate*>;
    std::map<std::string, Ref> inspiredExact;
    for (std::size_t group = 0; group < inspiration.groups.size(); ++group)
        for (const auto& candidate : inspiration.groups[group])
            if (candidate.channel == RecommendationChannel::Exact)
                inspiredExact.emplace(continuationMusicFingerprint(candidate), Ref{group, &candidate});
    std::size_t preserved{};
    report << std::setprecision(8);
    for (std::size_t group = 0; group < baseline.groups.size(); ++group) {
        for (const auto& candidate : baseline.groups[group]) {
            const auto fingerprint = continuationMusicFingerprint(candidate);
            const auto found = inspiredExact.find(fingerprint);
            const bool present = found != inspiredExact.end();
            const bool sameGroup = present && found->second.first == group;
            const bool sameScores = present && g6SameScores(candidate, *found->second.second);
            require(present && sameGroup && sameScores,
                    "inspiration request changed an exact baseline route for " +
                    std::string(caseId) + ": " + candidate.primaryTemplate);
            report << caseId << '\t' << groupName(group) << '\t' << candidate.primaryTemplate << '\t'
                   << fingerprint << '\t' << candidate.matchSimilarity << '\t'
                   << candidate.eligibilityScore << '\t' << candidate.rankingScore << '\t'
                   << present << '\t' << sameGroup << '\t' << sameScores << '\n';
            ++preserved;
        }
    }
    return preserved;
}

ChordPitchSet g6Pitches(const ConcreteChordEvent& event) {
    return event.harmonicData ? chordPitches(*event.harmonicData)
                              : chordPitches(event.label, event.quality);
}

unsigned g6AbsolutePitchMask(const ChordPitchSet& chord) {
    unsigned mask{};
    if (chord.root < 0) return mask;
    for (int interval = 0; interval < 12; ++interval)
        if (chord.intervals & (1u << interval))
            mask |= 1u << ((chord.root + interval) % 12);
    return mask;
}

std::size_t g6VerifyContextBindings(const MatchQuery& query, const CandidateIndex& index,
                                   const RecommendationSet& results) {
    std::size_t verified{};
    for (const auto& group : results.groups) for (const auto& candidate : group) {
        std::size_t expectedLength{};
        switch (candidate.channel) {
            case RecommendationChannel::ContextFull: expectedLength = query.source.size(); break;
            case RecommendationChannel::ContextThree: expectedLength = 3; break;
            case RecommendationChannel::ContextTwo: expectedLength = 2; break;
            default: continue;
        }
        require(candidate.contextLength == expectedLength && expectedLength <= query.source.size(),
                "G6 context route has an invalid terminal window length: " + candidate.primaryTemplate);
        const auto source = std::find_if(index.templates().begin(), index.templates().end(),
            [&](const auto& item) { return item.id == candidate.primaryTemplate; });
        require(source != index.templates().end(),
                "G6 context route source missing from index: " + candidate.primaryTemplate);
        require(candidate.matchStart + expectedLength == candidate.continuationStart,
                "G6 context route window no longer maps to its source anchor: " + candidate.primaryTemplate);
        const auto sourceRoute = preview::templateRoute(*source, candidate.key);
        require(static_cast<bool>(sourceRoute),
                "G6 context source template route: " + sourceRoute.error);
        require(candidate.matchStart + expectedLength <= sourceRoute.imported.events.size(),
                "G6 context anchor exceeds the actual source route: " + candidate.primaryTemplate);

        const auto queryStart = query.source.size() - expectedLength;
        for (std::size_t offset = 0; offset < expectedLength; ++offset) {
            const auto& input = query.source[queryStart + offset];
            const auto& actualSource = sourceRoute.imported.events[candidate.matchStart + offset];
            const auto inputChord = chordPitches(input);
            const auto sourceChord = chordPitches(actualSource);
            const auto inputMask = g6AbsolutePitchMask(inputChord);
            const auto sourceMask = g6AbsolutePitchMask(sourceChord);
            require(inputMask != 0 && sourceMask != 0 && (inputMask & sourceMask) == inputMask,
                    "G6 context input pitch set is not a subset of its actual source anchor: " +
                        candidate.primaryTemplate);
            const auto inputHarmony = normalizeChord(input);
            const auto sourceHarmony = normalizeChord(actualSource);
            if (inputHarmony.explicitBass)
                require(inputHarmony.bass && sourceHarmony.bass &&
                        inputHarmony.bass == sourceHarmony.bass,
                        "G6 context explicit bass differs from its actual source anchor: " +
                            candidate.primaryTemplate);
            ++verified;
        }
    }
    return verified;
}

bool g6RejectHardMelody(std::string_view caseId, const MatchQuery& query,
    const CandidateIndex& index, const RecommendationRequest& inspirationRequest,
    const RecommendationWeights& weights, const RecommendationSet& inspiration,
    std::ostream& report) {
    const ContinuationCandidate* representative{};
    for (std::size_t pass = 0; pass < 2 && !representative; ++pass)
        for (const auto& group : inspiration.groups)
            for (const auto& candidate : group)
                if (!candidate.continuation.empty() &&
                    (pass == 1 || candidate.channel != RecommendationChannel::Exact)) {
                    representative = &candidate;
                    break;
                }
    if (!representative) {
        report << caseId << "\tSKIPPED\tno representative candidate\n";
        return false;
    }

    double onset = query.source.back().startQN + progressionOpenDuration(query.source);
    std::optional<MelodyConstraint> conflict;
    for (const auto& event : representative->continuation) {
        const auto chord = g6Pitches(event);
        for (int pitchClass = 0; pitchClass < 12; ++pitchClass) {
            if (classifyMelody(chord, pitchClass) != MelodyRelation::Conflict) continue;
            MelodyConstraint melody;
            melody.startQN = onset;
            melody.durationQN = event.durationQN;
            melody.pitchClass = pitchClass;
            melody.strictness = ConstraintStrictness::Hard;
            conflict = melody;
            break;
        }
        if (conflict) break;
        onset += event.durationQN;
    }
    if (!conflict) {
        report << caseId << "\tSKIPPED\tno definite chord-tone conflict\n";
        return false;
    }

    auto constrainedRequest = inspirationRequest;
    constrainedRequest.constraints.melody.push_back(*conflict);
    const auto constrained = recommendContinuations(query, index, constrainedRequest, weights);
    auto unconstrainedRepresentative = *representative;
    unconstrainedRepresentative.constraints = {};
    const auto fingerprint = continuationMusicFingerprint(unconstrainedRepresentative);
    bool survived{};
    for (const auto& group : constrained.groups)
        for (const auto& candidate : group) {
            require(candidate.hardConstraintsPassed && candidate.melodyCompatibility.hardSatisfied,
                    "a G6 hard melody conflict survived recommendation filtering");
            auto unconstrainedCandidate = candidate;
            unconstrainedCandidate.constraints = {};
            survived |= continuationMusicFingerprint(unconstrainedCandidate) == fingerprint;
        }
    require(!survived, "G6 hard melody conflict did not reject the representative route");
    report << caseId << '\t' << representative->primaryTemplate << '\t'
           << g6ChannelName(representative->channel) << '\t' << conflict->startQN << '\t'
           << conflict->durationQN << '\t' << conflict->pitchClass << "\tREJECTED\n";
    return true;
}

std::vector<std::size_t> g6SourceEventIndices(const ProgressionTemplate& source,
    const ContinuationCandidate& candidate) {
    std::vector<std::size_t> indices;
    const auto boundary = candidate.continuationStart;
    if (boundary > source.full.size()) return indices;
    for (std::size_t i = boundary; i < source.full.size(); ++i) indices.push_back(i);
    if (candidate.channel == RecommendationChannel::Exact && source.loopable) {
        if (candidate.continuation.size() < indices.size() || source.full.empty()) return {};
        const auto wrapped = candidate.continuation.size() - indices.size();
        for (std::size_t i = 0; i < wrapped; ++i) indices.push_back(i % source.full.size());
    }
    return indices;
}

std::string g6AnchorIds(const CandidateIndex& index, std::size_t sourceIndex,
    const ContinuationCandidate& candidate, std::size_t& matchingCount) {
    std::vector<std::string> ids;
    const bool transition = candidate.channel == RecommendationChannel::PhraseTransition;
    for (std::size_t i = 0; i < index.contextAnchors().size(); ++i) {
        const auto& anchor = index.contextAnchors()[i];
        if (anchor.templateIndex != sourceIndex ||
            anchor.continuationStart != candidate.continuationStart ||
            anchor.terminalPrototype.has_value() != transition) continue;
        ids.push_back(std::to_string(i));
    }
    matchingCount = ids.size();
    return g6Join(ids, ",", [](const auto& value) { return value; });
}

std::string g6IntervalText(const ChordPitchSet& chord) {
    return std::to_string(chord.intervals) + ":" + std::to_string(chord.colorMask);
}

void g6WriteSampleAudit(std::ostream& report, std::string_view sampleId,
    std::string_view caseId, std::string_view group, std::size_t rank,
    const ContinuationCandidate& candidate, const ProgressionTemplate& source,
    std::size_t sourceIndex, std::string_view anchorIds,
    const std::vector<std::size_t>& sourceIndices) {
    const auto tags = g6Join(source.tags, ";", [](const auto& value) { return value; });
    const auto center = candidate.key; // source projection coordinate, distinct from the eventual destination
    const auto writeRow = [&](std::string_view kind, std::string_view index,
                              std::string_view label, const ChordPitchSet* pitches,
                              double duration, int degree, int alteration, RoleFlags roles,
                              std::string_view templateIndex) {
        report << sampleId << '\t' << caseId << '\t' << group << '\t' << rank << '\t'
               << g6ChannelName(candidate.channel) << '\t'
               << g6ProvenanceName(candidate.provenance) << '\t' << source.id << '\t'
               << sourceIndex << '\t' << source.version << '\t' << g6TsvCell(source.sourceType) << '\t'
               << g6ModeName(source.mode) << '\t' << intentName(source.intent) << '\t'
               << source.rawChords.size() << '\t' << g6TsvCell(tags) << '\t'
               << g6TsvCell(candidate.family) << '\t' << g6DirectionName(candidate.direction) << '\t'
               << candidate.creativeQuality.tier << '\t' << candidate.contextLength << '\t'
               << g6CenterName(candidate.originCenter) << '\t' << g6CenterName(candidate.targetCenter) << '\t'
               << g6TsvCell(candidate.connectionReason) << '\t' << anchorIds << '\t'
               << kind << '\t' << index << '\t' << templateIndex << '\t' << g6TsvCell(std::string(label)) << '\t';
        if (pitches)
            report << pitches->root << '\t' << pitches->bass << '\t' << static_cast<int>(pitches->quality)
                   << '\t' << g6IntervalText(*pitches) << '\t' << duration << '\t'
                   << degree << '\t' << alteration << '\t' << roles;
        else
            report << "\t\t\t\t\t\t\t";
        report << '\n';
    };

    writeRow("route", "", "", nullptr, 0, 0, 0, 0,
             std::to_string(sourceIndices.size()) + " source events");
    for (std::size_t i = 0; i < source.full.size(); ++i) {
        const auto& event = source.full[i];
        const auto realized = realizeContinuation(event, center, event.durationQN.value_or(4.0));
        const auto chord = realized.harmonicData ? chordPitches(*realized.harmonicData)
                                                 : chordPitches(realized.label, realized.quality);
        const auto kind = i < candidate.continuationStart ? "source-context" : "source-suffix";
        const auto degree = event.degree ? event.degree->degree : 0;
        const auto alteration = event.degree ? event.degree->alteration : 0;
        writeRow(kind, std::to_string(i), realized.label, &chord,
                 event.durationQN.value_or(4.0), degree, alteration, event.roles, "");
    }
    for (std::size_t i = 0; i < candidate.continuation.size(); ++i) {
        const auto& event = candidate.continuation[i];
        const auto chord = g6Pitches(event);
        const auto sourceIndexForEvent = i < sourceIndices.size()
            ? std::to_string(sourceIndices[i]) : std::string{};
        writeRow("frozen-continuation", std::to_string(i), event.label, &chord,
                 event.durationQN, event.degree.degree, event.degree.alteration, event.roles,
                 sourceIndexForEvent);
    }
}

struct G6SampleCounts {
    std::array<std::size_t, 5> channels{};
    std::size_t crossCenter{};
    std::set<std::tuple<int, std::string, std::string>> seen;
};

void g6WriteMediaSample(const std::filesystem::path& mediaDirectory,
    std::ostream& mediaIndex, std::ostream& audit, std::string_view caseId,
    std::string_view group, std::size_t rank, const ImportedProgressionSession& imported,
    const RecommendationSet& results, const ContinuationCandidate& candidate,
    const CandidateIndex& index, const library::LoadResult& factory,
    G6SampleCounts& counts, bool mandatoryCase = false) {
    const auto channelIndex = static_cast<std::size_t>(candidate.channel);
    if (channelIndex >= counts.channels.size()) return;
    const bool crossCenter = g6CrossCenter(candidate);
    if (crossCenter) {
        if (counts.crossCenter >= 1) return;
    } else if (counts.channels[channelIndex] >= 2 && !mandatoryCase) return;
    const auto identity = continuationMusicFingerprint(candidate);
    if (!counts.seen.emplace(static_cast<int>(candidate.channel),
                             std::string(caseId), identity).second) return;
    const auto sourceIt = std::find_if(index.templates().begin(), index.templates().end(),
        [&](const auto& item) { return item.id == candidate.primaryTemplate; });
    require(sourceIt != index.templates().end(),
            "G6 sample source missing from index: " + candidate.primaryTemplate);
    const auto sourceIndex = static_cast<std::size_t>(
        std::distance(index.templates().begin(), sourceIt));
    const auto sourceIndices = g6SourceEventIndices(*sourceIt, candidate);
    require(sourceIndices.size() == candidate.continuation.size(),
            "G6 sample suffix no longer agrees with its source index: " + candidate.primaryTemplate);
    std::size_t anchorCount{};
    const auto anchors = g6AnchorIds(index, sourceIndex, candidate, anchorCount);
    if (candidate.channel == RecommendationChannel::ContextFull ||
        candidate.channel == RecommendationChannel::ContextThree ||
        candidate.channel == RecommendationChannel::ContextTwo ||
        candidate.channel == RecommendationChannel::PhraseTransition)
        require(anchorCount > 0, "G6 creative sample has no matching ContextAnchor identity");
    const auto center = candidate.key; // source projection coordinate, distinct from the eventual destination
    for (std::size_t i = 0; i < sourceIndices.size(); ++i) {
        const auto& sourceEvent = sourceIt->full[sourceIndices[i]];
        const auto& outputEvent = candidate.continuation[i];
        const auto expected = realizeContinuation(
            sourceEvent, center, outputEvent.durationQN);
        const auto expectedPitches = expected.harmonicData
            ? chordPitches(*expected.harmonicData)
            : chordPitches(expected.label, expected.quality);
        require(g6SameCore(expectedPitches, g6Pitches(outputEvent)),
                "G6 sample bass/extension differs from its source event: " + candidate.primaryTemplate);
    }

    const auto playback = preview::sourceRoute(imported, candidate, *sourceIt);
    require(static_cast<bool>(playback), "G6 source route: " + playback.error);
    const auto built = preview::buildSequence(playback.imported, &playback.candidate, 120.0);
    require(static_cast<bool>(built), "G6 sample Preview sequence: " + built.error);
    exactOutput(built.sequence);
    const auto audio = preview::renderOffline(built.sequence, 48000.0);
    require(!audio.left.empty() && audio.left.size() == audio.right.size() && audio.rms > 0,
            "G6 sample offline preview is silent");

    const midi::Meter meter{playback.meterNumerator, playback.meterDenominator};
    const auto clip = midi::buildClip(built.sequence, midi::ArrangementMode::VoiceLed,
                                      midi::ExportScope::FullPhrase, meter,
                                      playback.candidate.key, playback.candidate.intent);
    require(static_cast<bool>(clip), "G6 sample MIDI clip: " + clip.error);
    const auto midiBytes = midi::writeToMemory(clip.sequence);
    require(static_cast<bool>(midiBytes), "G6 sample MIDI serialization: " + midiBytes.error);

    const auto sampleId = std::string(caseId) + "-" + g6ChannelName(candidate.channel) + "-" +
        std::to_string(rank) + "-" + candidate.primaryTemplate;
    const auto midiPath = mediaDirectory / (sampleId + ".mid");
    const auto wavPath = mediaDirectory / (sampleId + ".wav");
    const auto snapshotPath = mediaDirectory / (sampleId + ".snapshot.json");
    std::string error;
    require(midi::writeToFile(midiBytes.bytes, midiPath, error), "G6 sample MIDI write: " + error);
    require(preview::writeWav16(audio, wavPath, error), "G6 sample WAV write: " + error);

    auto captured = snapshot::capture(playback.imported, playback.candidate, results.matches,
        120.0, playback.meterNumerator, playback.meterDenominator,
        playback.candidate.key, {}, playback.candidate.intent);
    captured.factoryLibraryVersion = 7;
    const auto frozenIdentity = continuationMusicFingerprint(playback.candidate);
    const auto snapshotText = snapshot::serialize(captured);
    require(snapshotText.find("\"channel\"") == std::string::npos &&
            snapshotText.find("\"provenance\"") == std::string::npos &&
            snapshotText.find("\"connectionReason\"") == std::string::npos,
            "Snapshot3 unexpectedly persisted runtime creative metadata");
    require(snapshot::saveFile(captured, snapshotPath, error), "G6 Snapshot write: " + error);
    const auto reopened = snapshot::loadFile(snapshotPath, &factory);
    require(static_cast<bool>(reopened), "G6 Snapshot readback: " + reopened.error);
    require(reopened.value.schemaVersion == snapshot::RecommendationSnapshot::currentSchemaVersion &&
            reopened.value.factoryLibraryVersion == 7 && reopened.value.candidateAvailable &&
            continuationMusicFingerprint(reopened.value.candidate) == frozenIdentity,
            "G6 Snapshot did not preserve frozen music identity");
    const auto reopenedPreview = preview::buildSequence(
        reopened.value.imported, &reopened.value.candidate, 120.0);
    require(static_cast<bool>(reopenedPreview), "G6 reopened Snapshot Preview: " + reopenedPreview.error);
    exactOutput(reopenedPreview.sequence);
    const auto reopenedAudio = preview::renderOffline(reopenedPreview.sequence, 48000.0);
    require(audio.left == reopenedAudio.left && audio.right == reopenedAudio.right,
            "G6 Snapshot changed the offline Preview waveform");
    const auto reopenedClip = midi::buildClip(reopenedPreview.sequence, midi::ArrangementMode::VoiceLed,
        midi::ExportScope::FullPhrase, meter, reopened.value.candidate.key,
        reopened.value.candidate.intent);
    require(static_cast<bool>(reopenedClip), "G6 reopened Snapshot MIDI clip: " + reopenedClip.error);
    const auto reopenedMidi = midi::writeToMemory(reopenedClip.sequence);
    require(reopenedMidi && reopenedMidi.bytes == midiBytes.bytes,
            "G6 Snapshot changed the exported MIDI bytes");

    g6WriteSampleAudit(audit, sampleId, caseId, group, rank, candidate, *sourceIt,
                       sourceIndex, anchors, sourceIndices);
    mediaIndex << sampleId << '\t' << caseId << '\t' << group << '\t' << rank << '\t'
               << g6ChannelName(candidate.channel) << '\t'
               << g6ProvenanceName(candidate.provenance) << '\t'
               << candidate.primaryTemplate << '\t' << midiPath.filename().string() << '\t'
               << wavPath.filename().string() << '\t' << snapshotPath.filename().string() << '\t'
               << frozenIdentity << '\t' << anchors << '\t' << (crossCenter ? "yes" : "no") << '\n';
    ++counts.channels[channelIndex];
    counts.crossCenter += crossCenter ? 1 : 0;
}

void g6SelectSamples(const std::filesystem::path& mediaDirectory, std::ostream& mediaIndex,
    std::ostream& audit, std::string_view caseId, const ImportedProgressionSession& imported,
    const RecommendationSet& results, const CandidateIndex& index,
    const library::LoadResult& factory, G6SampleCounts& counts) {
    const auto before = counts.seen.size();
    for (const bool chooseCrossCenter : {false, true}) {
        for (std::size_t group = 0; group < results.groups.size(); ++group) {
            const auto& candidates = results.groups[group];
            for (std::size_t rank = 0; rank < candidates.size(); ++rank) {
                const auto& candidate = candidates[rank];
                if (g6CrossCenter(candidate) != chooseCrossCenter) continue;
                g6WriteMediaSample(mediaDirectory, mediaIndex, audit, caseId,
                    groupName(group), rank + 1, imported, results, candidate, index,
                    factory, counts);
            }
        }
    }
    // The core input may fill every channel quota. Still exercise one actual
    // Preview/MIDI/Snapshot route for each of the other two requested inputs.
    if(counts.seen.size()==before)for(std::size_t group=0;group<results.groups.size();++group)
        for(std::size_t rank=0;rank<results.groups[group].size();++rank) {
            const auto& c=results.groups[group][rank];
            if(c.exploratory||g6CrossCenter(c))continue;
            g6WriteMediaSample(mediaDirectory,mediaIndex,audit,caseId,groupName(group),rank+1,
                imported,results,c,index,factory,counts,true);
            return;
        }
}

void g6WriteAnchors(std::ostream& report, const CandidateIndex& index) {
    for (std::size_t i = 0; i < index.contextAnchors().size(); ++i) {
        const auto& anchor = index.contextAnchors()[i];
        if (anchor.templateIndex >= index.templates().size()) continue;
        const auto& source = index.templates()[anchor.templateIndex];
        const auto& terminal = anchor.terminalPrototype;
        const auto degree = terminal && terminal->degree ? terminal->degree->degree : 0;
        const auto alteration = terminal && terminal->degree ? terminal->degree->alteration : 0;
        const auto quality = terminal ? static_cast<int>(terminal->quality) : -1;
        const auto roles = terminal ? terminal->roles : 0;
        report << i << '\t' << anchor.templateIndex << '\t' << source.id << '\t'
               << source.version << '\t' << intentName(source.intent) << '\t'
               << anchor.continuationStart << '\t'
               << (terminal ? "algorithm-seam-prototype" : "internal-source-anchor") << '\t'
               << g6TsvCell(anchor.basis) << '\t' << degree << '\t' << alteration << '\t'
               << quality << '\t' << roles << '\n';
    }
}

int runG6Focused(const std::filesystem::path& factoryPath,
                 const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G6 baseline Factory: " + factory.error);
    require(factory.libraryVersion == 7 && factory.templates.size() == 1272,
            "G6 baseline diagnostics require Factory 7/1272");
    std::filesystem::create_directories(outputDirectory);
    const auto mediaDirectory = outputDirectory / "media";
    std::filesystem::create_directories(mediaDirectory);
    CandidateIndex index(factory.templates);
    std::unordered_map<TemplateID, const ProgressionTemplate*> sources;
    sources.reserve(index.templates().size());
    for (const auto& source : index.templates()) sources.emplace(source.id, &source);

    const std::array<std::pair<std::string_view, std::string_view>, 3> cases{{
        {"F_G_Em_Am",
         R"([{"name":"F","start":0,"duration":4},{"name":"G","start":4,"duration":4},{"name":"Em","start":8,"duration":4},{"name":"Am","start":12}])"},
        {"Cmaj7_Dm7_Em7_Gsus4",
         R"([{"name":"Cmaj7","start":0,"duration":4},{"name":"Dm7","start":4,"duration":4},{"name":"Em7","start":8,"duration":4},{"name":"Gsus4","start":12}])"},
        {"Cmaj7_Fm6_Bb7_Ebmaj7",
         R"([{"name":"Cmaj7","start":0,"duration":4},{"name":"Fm6","start":4,"duration":4},{"name":"Bb7","start":8,"duration":4},{"name":"Ebmaj7","start":12}])"}
    }};
    const auto matchPath = outputDirectory / "g6-matches.tsv";
    const auto routePath = outputDirectory / "g6-routes.tsv";
    const auto exactPath = outputDirectory / "g6-exact-preservation.tsv";
    const auto coveragePath = outputDirectory / "g6-strict-coverage.tsv";
    const auto hardPath = outputDirectory / "g6-hard-melody.tsv";
    const auto anchorPath = outputDirectory / "g6-context-anchors.tsv";
    const auto mediaIndexPath = outputDirectory / "g6-media-index.tsv";
    const auto auditPath = outputDirectory / "g6-sample-audit.tsv";
    std::ofstream matchReport(matchPath, std::ios::binary);
    std::ofstream routeReport(routePath, std::ios::binary);
    std::ofstream exactReport(exactPath, std::ios::binary);
    std::ofstream coverageReport(coveragePath, std::ios::binary);
    std::ofstream hardReport(hardPath, std::ios::binary);
    std::ofstream anchorReport(anchorPath, std::ios::binary);
    std::ofstream mediaIndex(mediaIndexPath, std::ios::binary);
    std::ofstream auditReport(auditPath, std::ios::binary);
    require(static_cast<bool>(matchReport) && static_cast<bool>(routeReport) &&
            static_cast<bool>(exactReport) && static_cast<bool>(coverageReport) &&
            static_cast<bool>(hardReport) && static_cast<bool>(anchorReport) &&
            static_cast<bool>(mediaIndex) && static_cast<bool>(auditReport),
            "cannot create G6 diagnostic reports");
    matchReport << "case\trun\tsource\tkey_results\tunique_keys\tbest_key\tbest_similarity"
                   "\tbest_disposition\tdisposition_counts\tqualified_groups\trepresentative_followup\n";
    routeReport << "case\trun\tgroup\trank\tchannel\tprovenance\tsource\tcandidate_id"
                   "\tfingerprint\tkey\tsupporting_sources\tfollowup\tmatch_similarity"
                   "\teligibility_score\tranking_score\tcontext_length\torigin_center"
                   "\ttarget_center\tdirection\tcreative_tier\tcreative_qualified"
                   "\tfamily\tconnection_reason\telapsed_ms\n";
    exactReport << "case\tgroup\tsource\tfingerprint\tbaseline_match_similarity"
                   "\tbaseline_eligibility\tbaseline_ranking\tpresent\tgroup_preserved"
                   "\tscores_preserved\n";
    coverageReport << "case\twindow\tsource\twitnesses\n";
    hardReport << "case\tsource\tchannel\tstart_qn\tduration_qn\tpitch_class\tresult\n";
    anchorReport << "anchor_index\ttemplate_index\tsource\tfactory_version\tintent"
                    "\tcontinuation_start\tanchor_kind\tbasis\tterminal_degree"
                    "\tterminal_alteration\tterminal_quality\tterminal_roles\n";
    mediaIndex << "sample\tcase\tgroup\trank\tchannel\tprovenance\tsource\tmidi\twav"
                  "\tsnapshot\tfrozen_music_fingerprint\tanchor_ids\tcross_center\n";
    auditReport << "sample\tcase\tgroup\trank\tchannel\tprovenance\tsource\ttemplate_index"
                   "\tfactory_version\tsource_type\tsource_mode\tsource_intent\traw_chord_count"
                   "\ttags\tfamily\tdirection\tcreative_tier\tcontext_length\torigin_center"
                   "\ttarget_center\tconnection_reason\tanchor_ids\tevent_kind\tevent_index"
                   "\ttemplate_event_index\tlabel\troot_pc\tbass_pc\tquality\tinterval_color_masks"
                   "\tduration_qn\tdegree\talteration\troles\n";
    g6WriteAnchors(anchorReport, index);

    const auto weights = recommendationWeights();
    G6SampleCounts sampleCounts;
    std::size_t exactPreservedTotal{}, hardTests{};
    for (const auto& [caseId, json] : cases) {
        const auto parsed = dev::parseProgressionJson(json);
        require(static_cast<bool>(parsed), "invalid G6 input " + std::string(caseId) + ": " + parsed.error);
        ImportedProgressionSession imported;
        require(imported.replace(parsed.events, TimelineCoordinateMode::RelativeToSelection),
                "cannot create imported session for " + std::string(caseId));
        const auto query = makeMatchQuery(
            imported.events, recommendationContext(AnalysisContext{}));
        require(!query.interpretations.empty(), "G6 input has no key interpretations: " +
                    std::string(caseId));

        RecommendationRequest baselineRequest;
        const auto baselineStarted = G6Clock::now();
        const auto baseline = recommendContinuations(query, index, baselineRequest, weights);
        const auto baselineMs = std::chrono::duration<double, std::milli>(
            G6Clock::now() - baselineStarted).count();

        RecommendationRequest inspirationRequest;
        inspirationRequest.inspirationPriority = true;
        const auto inspirationStarted = G6Clock::now();
        const auto inspiration = recommendContinuations(query, index, inspirationRequest, weights);
        const auto inspirationMs = std::chrono::duration<double, std::milli>(
            G6Clock::now() - inspirationStarted).count();
        const auto contextBindings = g6VerifyContextBindings(query, index, inspiration);

        g6WriteMatchSummary(matchReport, caseId, "baseline", baseline, sources);
        g6WriteMatchSummary(matchReport, caseId, "inspiration", inspiration, sources);
        g6WriteRoutes(routeReport, caseId, "baseline", baseline, baselineMs);
        g6WriteRoutes(routeReport, caseId, "inspiration", inspiration, inspirationMs);
        const auto exactPreserved = g6VerifyExactPreserved(caseId, baseline, inspiration, exactReport);
        exactPreservedTotal += exactPreserved;
        const auto resolveSeams = g6AlgorithmSeamsInResolve(inspiration);
        require(resolveSeams == 0, "G6 algorithm-assessed seam entered Resolve");
        const auto coverage = g6StrictCoverage(query, index, coverageReport, caseId);
        const auto countCoverage = [&](std::size_t window) { return coverage[window].size(); };
        const auto hardTest = g6RejectHardMelody(caseId, query, index, inspirationRequest,
                                                  weights, inspiration, hardReport);
        hardTests += hardTest ? 1 : 0;

        const auto baseGroups = std::array<std::size_t, 4>{
            baseline.groups[0].size(), baseline.groups[1].size(),
            baseline.groups[2].size(), baseline.groups[3].size()};
        const auto baseDisplay = g6DisplayCounts(baseline);
        const auto inspiredGroups = std::array<std::size_t, 4>{
            inspiration.groups[0].size(), inspiration.groups[1].size(),
            inspiration.groups[2].size(), inspiration.groups[3].size()};
        const auto inspirationDisplay = g6DisplayCounts(inspiration);
        const auto channels = g6ChannelCounts(inspiration);
        std::set<TemplateID> matchedSources;
        for (const auto& match : baseline.matches) matchedSources.insert(match.templateId);
        std::set<TemplateID> outputSources;
        for (const auto& group : inspiration.groups)
            for (const auto& candidate : group)
                outputSources.insert(candidate.supportingTemplates.begin(),
                                     candidate.supportingTemplates.end());
        std::cout << "g6_baseline case=" << caseId
                  << " factory=7/1272 interpretations=" << query.interpretations.size()
                  << " shortlist=" << baseline.stats.shortlistSize
                  << " aligned=" << baseline.stats.aligned
                  << " similarity_passed=" << baseline.stats.similarityPassed
                  << " tail_passed=" << baseline.stats.tailPassed
                  << " realization_passed=" << baseline.stats.realizationPassed
                  << " path_passed=" << baseline.stats.pathPassed
                  << " distinct_routes=" << baseline.stats.distinct
                  << " score_passed=" << baseline.stats.scorePassed
                  << " output=" << baseline.stats.output
                  << " unique_match_sources=" << matchedSources.size()
                  << " retained_resolve=" << baseGroups[0] << " retained_develop=" << baseGroups[1]
                  << " retained_loop=" << baseGroups[2] << " retained_color=" << baseGroups[3]
                  << " display_resolve=" << baseDisplay[0] << " display_develop=" << baseDisplay[1]
                  << " display_loop=" << baseDisplay[2] << " display_color=" << baseDisplay[3]
                  << " matcher_ms=" << baseline.stats.totalMs << " elapsed_ms=" << baselineMs
                  << " strict_sources_full=" << countCoverage(0)
                  << " strict_sources_tail3=" << countCoverage(1)
                  << " strict_sources_tail2=" << countCoverage(2) << '\n';
        std::cout << "g6_inspiration case=" << caseId
                  << " exact_preserved=" << exactPreserved
                  << " unique_output_sources=" << outputSources.size()
                  << " retained_resolve=" << inspiredGroups[0] << " retained_develop=" << inspiredGroups[1]
                  << " retained_loop=" << inspiredGroups[2] << " retained_color=" << inspiredGroups[3]
                  << " display_resolve=" << inspirationDisplay[0] << " display_develop=" << inspirationDisplay[1]
                  << " display_loop=" << inspirationDisplay[2] << " display_color=" << inspirationDisplay[3]
                  << " exact=" << channels[0] << " context_full=" << channels[1]
                  << " context_three=" << channels[2] << " context_two=" << channels[3]
                  << " phrase_transition=" << channels[4]
                  << " contextual_examined=" << inspiration.contextualExamined
                  << " contextual_qualified=" << inspiration.contextualQualified
                  << " transition_examined=" << inspiration.transitionExamined
                  << " transition_qualified=" << inspiration.transitionQualified
                  << " context_binding_chords=" << contextBindings
                  << " resolve_algorithm_seams=" << resolveSeams
                  << " contextual_ms=" << inspiration.contextualMs
                  << " matcher_ms=" << inspiration.stats.totalMs
                  << " elapsed_ms=" << inspirationMs << '\n';

        // Process the core case first, then fill remaining per-channel slots from
        // later inputs. Same-center candidates are preferred; at most one
        // cross-center sample is retained across the whole run.
        g6SelectSamples(mediaDirectory, mediaIndex, auditReport, caseId, imported,
                        inspiration, index, factory, sampleCounts);
    }
    require(hardTests > 0, "G6 hard melody rejection was not exercised");
    require(static_cast<bool>(matchReport) && static_cast<bool>(routeReport) &&
            static_cast<bool>(exactReport) && static_cast<bool>(coverageReport) &&
            static_cast<bool>(hardReport) && static_cast<bool>(anchorReport) &&
            static_cast<bool>(mediaIndex) && static_cast<bool>(auditReport),
            "cannot finish G6 diagnostic reports");
    std::cout << "g6_exact_routes_preserved=" << exactPreservedTotal
              << " g6_hard_melody_cases=" << hardTests
              << " g6_context_anchor_count=" << index.contextAnchors().size()
              << " g6_samples_exact=" << sampleCounts.channels[0]
              << " g6_samples_context_full=" << sampleCounts.channels[1]
              << " g6_samples_context_three=" << sampleCounts.channels[2]
              << " g6_samples_context_two=" << sampleCounts.channels[3]
              << " g6_samples_phrase_transition=" << sampleCounts.channels[4]
              << " g6_samples_cross_center=" << sampleCounts.crossCenter << '\n'
              << "g6_matches=" << matchPath.string() << '\n'
              << "g6_routes=" << routePath.string() << '\n'
              << "g6_exact_preservation=" << exactPath.string() << '\n'
              << "g6_strict_coverage=" << coveragePath.string() << '\n'
              << "g6_hard_melody=" << hardPath.string() << '\n'
              << "g6_context_anchors=" << anchorPath.string() << '\n'
              << "g6_media_index=" << mediaIndexPath.string() << '\n'
              << "g6_sample_audit=" << auditPath.string() << '\n';
    return 0;
}
} // namespace
