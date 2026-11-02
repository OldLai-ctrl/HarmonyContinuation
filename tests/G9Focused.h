#pragma once

#include "ProgressionJson.h"
#include "core/ContextualContinuation.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace harmony;

Progression g9Progression(std::initializer_list<std::string_view> labels) {
    std::ostringstream json;
    json << '[';
    std::size_t index{};
    for (const auto label : labels) {
        if (index) json << ',';
        json << "{\"name\":\"" << label << "\",\"start\":" << (index * 4)
             << ",\"duration\":4}";
        ++index;
    }
    json << ']';
    auto parsed = dev::parseProgressionJson(json.str());
    require(static_cast<bool>(parsed) && sortAndInferDurations(parsed.events),
            "G9 chord fixture parse failed: " + parsed.error);
    return std::move(parsed.events);
}

MatchQuery g9Query(const Progression& input) {
    AnalysisContext context;
    context.forcedKey = KeySignature{PitchClass::C, Mode::Major};
    auto query = makeMatchQuery(input, recommendationContext(context));
    query.timeSigNumerator = 4;
    query.timeSigDenominator = 4;
    return query;
}

const ContinuationCandidate* g9FindCandidate(
    const RecommendationSet& recommendations, std::string_view source,
    RecommendationChannel channel, std::size_t contextLength = 0) {
    for (const auto& group : recommendations.groups) for (const auto& candidate : group)
        if (candidate.primaryTemplate == source && candidate.channel == channel &&
            (!contextLength || candidate.contextLength == contextLength)) return &candidate;
    return nullptr;
}

const ContinuationCandidate* g9FindCandidateForKey(
    const RecommendationSet& recommendations, std::string_view source,
    RecommendationChannel channel, KeySignature key) {
    for (const auto& group : recommendations.groups) for (const auto& candidate : group)
        if (candidate.primaryTemplate == source && candidate.channel == channel &&
            candidate.key.tonic == key.tonic && candidate.key.mode == key.mode) return &candidate;
    return nullptr;
}

std::string g9Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string g9KeyText(KeySignature key) {
    constexpr const char* names[]{"C", "Db", "D", "Eb", "E", "F",
                                  "Gb", "G", "Ab", "A", "Bb", "B"};
    const auto tonic = static_cast<int>(key.tonic);
    require(tonic >= 0 && tonic < 12, "G9 key has an invalid tonic");
    return std::string(names[tonic]) + (key.mode == Mode::Major ? ":major" : ":minor");
}

std::string g9ChordEvidence(const ChordPitchSet& chord) {
    std::ostringstream output;
    output << "root=" << chord.root << ",bass=" << chord.bass
           << ",quality=" << static_cast<int>(chord.quality)
           << ",mask=0x" << std::hex << g8AbsolutePitchMask(chord);
    return output.str();
}

struct G9MediaSample {
    std::string label, mechanism;
    int meterNumerator{4}, meterDenominator{4};
    G8RoundRecord round;
};

const ProgressionTemplate& g9FindTemplate(const library::LoadResult& factory,
                                           std::string_view id) {
    const auto found = std::find_if(factory.templates.begin(), factory.templates.end(),
        [&](const auto& item) { return item.id == id; });
    require(found != factory.templates.end(), "G9 check Factory lacks source " + std::string(id));
    return *found;
}

Progression g9ParseCompleteFingerprint(std::string_view fingerprint);

KeySignature g9KeyFromActualTail(const ProgressionTemplate& source,
                                 const Progression& actualImport) {
    require(source.rawChordKey.has_value() && !source.rawChords.empty(),
            "recorded G9 source lacks its original key or typed chords: " + source.id);
    require(actualImport.size() >= source.rawChords.size(),
            "recorded G9 import is shorter than its complete source: " + source.id);
    const auto sourceFirst = chordPitches(source.rawChords.front());
    const auto importedFirst = chordPitches(actualImport[actualImport.size() - source.rawChords.size()]);
    require(sourceFirst.root >= 0 && importedFirst.root >= 0,
            "cannot derive G9 transposition from actual imported pitches: " + source.id);
    const auto shift = (importedFirst.root - sourceFirst.root + 12) % 12;
    return {static_cast<PitchClass>((static_cast<int>(source.rawChordKey->tonic) + shift) % 12),
            source.rawChordKey->mode};
}

KeySignature g9RecordedKey(const ProgressionTemplate& source,
                           const G8RecordedRcrRound& record) {
    return g9KeyFromActualTail(source, g9ParseCompleteFingerprint(record.actualImportFingerprint));
}

Progression g9ParseCompleteFingerprint(std::string_view fingerprint) {
    require(!fingerprint.empty() && fingerprint.front() == '|',
            "complete G9 fingerprint must begin with a delimiter");
    auto tokens = g8Split(fingerprint.substr(1), '|');
    require(!tokens.empty(), "complete G9 fingerprint has no events");
    const auto at = tokens.back().find('@');
    require(at != std::string::npos, "complete G9 final event lacks duration");
    const auto finalDuration = g8ParseDouble(std::string_view(tokens.back()).substr(at + 1),
                                             "complete final duration");
    require(finalDuration > 0.0, "complete G9 final event has no positive duration");
    tokens.back() = tokens.back().substr(0, at) + "@0";
    std::ostringstream openFingerprint;
    for (const auto& token : tokens) openFingerprint << '|' << token;
    auto events = g8ParseRecordedFingerprint(openFingerprint.str());
    events.back().durationQN = finalDuration;
    events.back().openEnded = false;
    require(g8EventListFingerprint(events) == fingerprint,
            "complete G9 fingerprint changed during timeline reconstruction");
    return events;
}

void g9RequireSameTimeline(const Progression& expected, const Progression& actual,
                           std::string_view where) {
    require(expected.size() == actual.size(), std::string(where) + ": event count changed");
    for (std::size_t i = 0; i < expected.size(); ++i)
        require(g8SameHarmony(chordPitches(expected[i]), chordPitches(actual[i])) &&
                std::abs(expected[i].startQN - actual[i].startQN) <= 1e-9 &&
                expected[i].durationQN == actual[i].durationQN &&
                expected[i].openEnded == actual[i].openEnded,
                std::string(where) + ": chord identity or timing changed at event " +
                    std::to_string(i));
}

void g9WriteTop10(std::ostream& output, std::string_view queryId,
                  const RecommendationSet& recommendations) {
    const auto order = mixedInspirationOrder(recommendations);
    const auto count = std::min<std::size_t>(10, order.size());
    for (std::size_t visible = 0; visible < count; ++visible) {
        const auto* candidate = g8CandidateAt(recommendations, order[visible]);
        require(candidate != nullptr, "G9 top-10 contains an invalid recommendation position");
        output << g8Tsv(std::string(queryId)) << '\t' << visible + 1 << '\t'
               << visible / ui::recommendationPageSize + 1 << '\t'
               << recommendationChannelName(candidate->channel) << '\t'
               << g8Tsv(candidate->primaryTemplate) << '\t' << candidate->contextLength << '\t'
               << candidate->continuationStart << '\t' << (candidate->structureQualified ? 1 : 0)
               << '\t' << candidate->structurePriority << '\t' << candidate->seamQN << '\t'
               << g8Tsv(candidate->structureReason) << '\t'
               << g8Tsv(candidate->developmentMechanism) << '\t'
               << candidate->historySimilarity << '\t' << (candidate->historyFamilyPresent ? 1 : 0)
               << '\t' << (candidate->historyMechanismPresent ? 1 : 0) << '\t'
               << g8ContinuationLabels(*candidate) << '\n';
    }
    require(static_cast<bool>(output), "cannot write G9 actual top-10 candidates");
}

void g9CollectMediaSample(const ImportedProgressionSession& input,
                          const RecommendationSet& recommendations,
                          const ContinuationCandidate& candidate,
                          int meterNumerator, int meterDenominator,
                          std::set<std::string>& mechanisms,
                          std::vector<G9MediaSample>& samples) {
    if (!candidate.structureQualified || candidate.developmentMechanism.empty() ||
        samples.size() >= 3 || mechanisms.contains(candidate.developmentMechanism)) return;
    const auto order = mixedInspirationOrder(recommendations);
    std::size_t flatIndex{};
    bool visible{};
    for (std::size_t i = 0; i < order.size(); ++i)
        if (g8CandidateAt(recommendations, order[i]) == &candidate) {
            flatIndex = i;
            visible = true;
            break;
        }
    require(visible, "qualified G9 media candidate is absent from the actual recommendation order");

    const auto built = preview::buildSequence(input, &candidate, 120.0);
    require(static_cast<bool>(built), "G9 qualified Preview failed: " + built.error);
    const midi::CandidateExportContext context{input, 120.0,
        {meterNumerator, meterDenominator}, candidate.key,
        midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
    const auto payload = midi::candidatePayload(context, candidate);
    require(static_cast<bool>(payload) && !payload.payload.dawClipBytes.empty(),
            "G9 qualified production MIDI payload failed: " + payload.error);

    G8RoundRecord round;
    round.input = input;
    round.candidate = candidate;
    round.preview = built.sequence;
    round.payload = payload.payload;
    round.inputFingerprint = g8EventListFingerprint(input.events);
    round.realizedFingerprint = g8EventListFingerprint(built.sequence);
    round.flatIndex = flatIndex;
    round.page = flatIndex / ui::recommendationPageSize + 1;
    samples.push_back({"G9-" + std::to_string(samples.size() + 1) + "-" + candidate.primaryTemplate,
                       candidate.developmentMechanism, meterNumerator, meterDenominator,
                       std::move(round)});
    mechanisms.insert(candidate.developmentMechanism);
}

void g9CheckUnprovenSharedOneFive(const library::LoadResult& factory,
                                  std::ostream& report, std::ostream& top10) {
    auto parsed = dev::parseTemplateJson(R"([{
      "id":"G9_SHARED_1_5_SOURCE","name":"G9 shared 1-5 fixture",
      "nameZh":"G9共享1-5测试源","nameEn":"G9 shared 1-5 fixture",
      "mode":"major","sequence":"I V iii IV vi","rhythm":"4 4 4 4 4",
      "intent":"develop"
    }])");
    require(static_cast<bool>(parsed) && parsed.templates.size() == 1,
            "G9 shared-1-5 source fixture parse failed: " + parsed.error);

    std::vector<ProgressionTemplate> fixtureSources;
    fixtureSources.push_back(std::move(parsed.templates.front()));
    const CandidateIndex index(std::move(fixtureSources));
    auto input = g9Progression({"Am", "F", "C", "G"});
    const auto before = g8EventListFingerprint(input);
    const auto query = g9Query(input);
    const auto recommendations = g8NormalRecommendations(query, index);
    g9WriteTop10(top10, "shared-1-5", recommendations);
    require(g8EventListFingerprint(input) == before,
            "G9 structure assessment changed the original 6-4-1-5 input");

    const auto* candidate = g9FindCandidate(recommendations, "G9_SHARED_1_5_SOURCE",
                                             RecommendationChannel::ContextTwo, 2);
    require(candidate != nullptr,
            "normal recommendation did not retain the actual two-chord C-G overlap fixture");
    require(candidate->continuation.size() == 3,
            "structure annotation removed or truncated the C-G-Em-F-Am source continuation");
    const Progression expected = g9Progression({"Em", "F", "Am"});
    for (std::size_t i = 0; i < expected.size(); ++i)
        require(g8SameHarmony(candidate->continuation[i].harmonicData
                                  ? chordPitches(*candidate->continuation[i].harmonicData)
                                  : chordPitches(candidate->continuation[i].label,
                                                 candidate->continuation[i].quality),
                              chordPitches(expected[i])),
                "G9 structure annotation changed the retained source music");
    require(!candidate->structureQualified && candidate->structurePriority == 0,
            "shared 1-5 local overlap was incorrectly promoted to a structural continuation");
    const auto reason = g9Lower(candidate->structureReason);
    require(reason.find("unproven") != std::string::npos || reason.find("overlap") != std::string::npos,
            "shared 1-5 rejection lacks an unproven-overlap reason: " + candidate->structureReason);
    report << "shared-1-5\tPASS\tinput=Am-F-C-G\tsource=G9_SHARED_1_5_SOURCE"
           << "\tqualified=0\tpriority=" << candidate->structurePriority
           << "\treason=" << g8Tsv(candidate->structureReason) << '\n';
}

void g9CheckReviewedV8Phrase(const library::LoadResult& factory,
                             std::ostream& report, std::ostream& top10,
                             std::set<std::string>& mechanisms,
                             std::vector<G9MediaSample>& mediaSamples) {
    const CandidateIndex index(factory.templates);
    const auto input = g9Progression({"F", "C", "Dm", "Am"});
    const auto query = g9Query(input);
    const auto recommendations = g8NormalRecommendations(query, index);
    g9WriteTop10(top10, "reviewed-v8-phrase", recommendations);
    const auto* candidate = g9FindCandidate(recommendations, "V8_01_002",
                                             RecommendationChannel::PhraseTransition);
    require(candidate != nullptr,
            "normal recommendation omitted the complete V8_01_002 phrase for F-C-Dm-Am");
    require(candidate->structureQualified && candidate->structurePriority == 2,
            "reviewed complete V8_01_002 phrase did not receive structure priority 2");
    const auto reason = g9Lower(candidate->structureReason);
    require(reason.find("seam") != std::string::npos &&
            (reason.find("unreviewed") != std::string::npos ||
             reason.find("not independently") != std::string::npos ||
             reason.find("not independent") != std::string::npos ||
             reason.find("no independent") != std::string::npos),
            "V8_01_002 must distinguish reviewed source structure from an independently reviewed seam: " +
                candidate->structureReason);

    const auto sourceIt = std::find_if(factory.templates.begin(), factory.templates.end(),
        [](const auto& item) { return item.id == "V8_01_002"; });
    require(sourceIt != factory.templates.end(), "G9 check Factory lacks V8_01_002");
    const auto& source = *sourceIt;
    require(candidate->completeSourcePhrase && candidate->sourceEndPreserved &&
            candidate->continuation.size() == source.rawChords.size(),
            "V8_01_002 was not retained as its full source phrase");
    for (std::size_t i = 0; i < candidate->continuation.size(); ++i)
        require(std::abs(candidate->continuation[i].durationQN -
                         source.rawChords[i].durationQN.value_or(4.0)) <= 1e-9,
                "structure annotation changed a V8_01_002 source duration");
    ImportedProgressionSession imported;
    require(imported.replace(input, TimelineCoordinateMode::RelativeToSelection),
            "cannot construct G9 reviewed-phrase media input");
    g9CollectMediaSample(imported, recommendations, *candidate, 4, 4,
                         mechanisms, mediaSamples);
    report << "reviewed-v8-phrase\tPASS\tinput=F-C-Dm-Am\tsource=V8_01_002"
           << "\tqualified=1\tpriority=" << candidate->structurePriority
           << "\tseam_qn=" << candidate->seamQN
           << "\tmechanism=" << g8Tsv(candidate->developmentMechanism)
           << "\treason=" << g8Tsv(candidate->structureReason) << '\n';
}

void g9CheckLegacyChains(const library::LoadResult& factory,
                         const std::filesystem::path& roundsPath,
                         std::ostream& report, std::ostream& top10,
                         std::ostream& legacy,
                         std::set<std::string>& mechanisms,
                         std::vector<G9MediaSample>& mediaSamples) {
    const std::array<std::string_view, 3> starts{
        "V3_CYCLE_027", "V6_B3_02_054", "FUNCTIONAL_015"};
    const CandidateIndex index(factory.templates);
    for (const auto startId : starts) {
        const auto records = g8ReadRecordedRounds(roundsPath, startId);
        const auto& startSource = g9FindTemplate(factory, startId);
        const auto startKey = startSource.rawChordKey.value_or(
            KeySignature{PitchClass::C, startSource.mode});
        const auto startRoute = preview::templateRoute(startSource, startKey);
        require(static_cast<bool>(startRoute),
                "G9 legacy A route failed: " + startSource.id + ": " + startRoute.error);

        for (std::size_t round = 0; round < records.size(); ++round) {
            const auto& record = records[round];
            auto events = g8ParseRecordedFingerprint(record.queryInputFingerprint);
            require(events.size() == record.inputLength,
                    "G9 legacy query length differs from its frozen fingerprint");
            if (round > 0) {
                const auto previousComplete =
                    g9ParseCompleteFingerprint(records[round - 1].actualImportFingerprint);
                g8RequireRecordedCarry(previousComplete, events);
                auto reopenedTail = previousComplete;
                reopenedTail.back().durationQN.reset();
                reopenedTail.back().openEnded = true;
                require(g8EventListFingerprint(reopenedTail) == record.queryInputFingerprint,
                        "G9 frozen next query changed the previous phrase music or duration identity");
            }

            ImportedProgressionSession input;
            require(input.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
                    "cannot reconstruct G9 frozen query input");
            const auto inputBefore = input.events;
            const auto query = g8MakeQuery(input.events,
                std::pair{startRoute.meterNumerator, startRoute.meterDenominator});
            const auto queryStarted = G8Clock::now();
            const auto recommendations = g8NormalRecommendations(query, index);
            const auto recommendationMs = std::chrono::duration<double, std::milli>(
                G8Clock::now() - queryStarted).count();
            g9RequireSameTimeline(inputBefore, input.events,
                "G9 normal query " + std::string(startId) + " round " + std::to_string(round + 1));
            const auto queryId = std::string(startId) + ":round-" + std::to_string(round + 1);
            g9WriteTop10(top10, queryId, recommendations);

            const auto& source = g9FindTemplate(factory, record.primarySource);
            const auto actualImport = g9ParseCompleteFingerprint(record.actualImportFingerprint);
            const auto expectedKey = g9KeyFromActualTail(source, actualImport);
            const auto* candidate = g9FindCandidateForKey(recommendations, record.primarySource,
                RecommendationChannel::PhraseTransition, expectedKey);
            require(candidate != nullptr,
                    "G9 normal recommendation omitted frozen source/key " + record.primarySource +
                        "/" + g9KeyText(expectedKey) + " for " + queryId);
            require(!candidate->structureReason.empty(),
                    "G9 Inspiration candidate has no structural status reason: " + record.primarySource);

            const auto sourceRoute = preview::templateRoute(source, expectedKey);
            require(static_cast<bool>(sourceRoute),
                    "G9 typed source route failed for " + source.id + ": " + sourceRoute.error);
            const auto sourceLength = sourceRoute.imported.events.size();
            require(sourceLength == source.rawChords.size() &&
                    candidate->continuation.size() == sourceLength && actualImport.size() >= sourceLength,
                    "G9 frozen source no longer returns its full eight-event continuation: " +
                        record.primarySource);
            const auto actualTailStart = actualImport.size() - sourceLength;
            const auto actualTailStartQN = actualImport[actualTailStart].startQN;
            for (std::size_t i = 0; i < sourceLength; ++i) {
                const auto candidateChord = candidate->continuation[i].harmonicData
                    ? chordPitches(*candidate->continuation[i].harmonicData)
                    : chordPitches(candidate->continuation[i].label,
                                   candidate->continuation[i].quality);
                const auto sourceChord = chordPitches(sourceRoute.imported.events[i]);
                require(g8SameHarmony(candidateChord, sourceChord),
                        "G9 candidate differs from typed templateRoute at source chord " +
                            std::to_string(i) + " for " + record.primarySource + ": expected " +
                            g9ChordEvidence(sourceChord) + ", actual " + g9ChordEvidence(candidateChord));
                require(std::abs(candidate->continuation[i].durationQN -
                                 sourceRoute.imported.events[i].durationQN.value_or(4.0)) <= 1e-9,
                        "G9 candidate changed typed source duration at chord " +
                            std::to_string(i) + " for " + record.primarySource);

                const auto importedChord = chordPitches(actualImport[actualTailStart + i]);
                const auto sourceDuration = sourceRoute.imported.events[i].durationQN;
                const auto importedDuration = actualImport[actualTailStart + i].durationQN;
                const auto importedOffset = actualImport[actualTailStart + i].startQN - actualTailStartQN;
                require(importedChord.bass == sourceChord.bass &&
                        g8AbsolutePitchMask(importedChord) == g8AbsolutePitchMask(sourceChord) &&
                        sourceDuration && importedDuration &&
                        std::abs(*importedDuration - *sourceDuration) <= 0.5 / 480.0 &&
                        std::abs(importedOffset - sourceRoute.imported.events[i].startQN) <= 0.5 / 480.0,
                        "G9 recorded DAWClip tail differs in actual notes, bass, or time at chord " +
                            std::to_string(i) + " for " + record.primarySource + ": expected " +
                            g9ChordEvidence(sourceChord) + ", imported " + g9ChordEvidence(importedChord) +
                            ", duration=" + (sourceDuration ? std::to_string(*sourceDuration) : "unknown") +
                            "/" + (importedDuration ? std::to_string(*importedDuration) : "unknown"));
            }
            legacy << startId << '\t' << record.sampleOrder << '\t' << record.round << '\t'
                   << record.inputLength << '\t' << record.primarySource << '\t'
                   << g9KeyText(candidate->key) << '\t'
                   << (candidate->structureQualified ? 1 : 0) << '\t'
                   << candidate->structurePriority << '\t' << candidate->seamQN << '\t'
                   << g8Tsv(candidate->structureReason) << '\t'
                   << g8Tsv(candidate->developmentMechanism) << '\t'
                   << candidate->historySimilarity << '\t'
                   << (candidate->historyFamilyPresent ? 1 : 0) << '\t'
                   << (candidate->historyMechanismPresent ? 1 : 0) << '\t'
                   << std::fixed << std::setprecision(3) << recommendationMs << '\n';
            require(static_cast<bool>(legacy), "cannot write G9 legacy structure row");

            g9CollectMediaSample(input, recommendations, *candidate,
                startRoute.meterNumerator, startRoute.meterDenominator,
                mechanisms, mediaSamples);
        }
        report << "legacy-chain\tPASS\t" << startId << "\trounds=3;source_identity_and_timing=preserved\n";
    }
}

void g9CheckLongInput(const library::LoadResult& factory,
                      const std::filesystem::path& roundsPath,
                      std::ostream& report, std::ostream& top10,
                      std::ostream& longInputReport) {
    const auto firstChain = g8ReadRecordedRounds(roundsPath, "V3_CYCLE_027");
    require(firstChain.size() == 3 && firstChain[2].inputLength == 20,
            "G9 long query must begin from the frozen first-chain round-three 20-event input");
    const auto openBase = g8ParseRecordedFingerprint(firstChain[2].queryInputFingerprint);
    const auto completeBase = g9ParseCompleteFingerprint(firstChain[1].actualImportFingerprint);
    g8RequireRecordedCarry(completeBase, openBase);
    require(completeBase.size() == 20, "G9 long-query complete base is not 20 events");

    const auto& phraseSource = g9FindTemplate(factory, firstChain[0].primarySource);
    const auto phraseKey = g9RecordedKey(phraseSource, firstChain[0]);
    const auto phraseRoute = preview::templateRoute(phraseSource, phraseKey);
    require(static_cast<bool>(phraseRoute), "G9 long-query append route failed: " + phraseRoute.error);
    require(phraseRoute.imported.events.size() == phraseSource.rawChords.size() &&
            !phraseRoute.imported.events.empty(),
            "G9 long-query append source is not a complete authored phrase");

    auto events = openBase;
    events.back().durationQN = completeBase.back().durationQN;
    events.back().openEnded = false;
    double nextStartQN = events.back().startQN + events.back().durationQN.value_or(0.0);
    while (events.size() < 80) {
        for (const auto& sourceEvent : phraseRoute.imported.events) {
            auto event = sourceEvent;
            event.startQN = nextStartQN;
            event.openEnded = false;
            require(event.durationQN && *event.durationQN > 0.0,
                    "G9 long-query appended source has unknown duration");
            nextStartQN += *event.durationQN;
            events.push_back(std::move(event));
        }
    }
    require(events.size() >= 80, "G9 long-query construction did not reach 80 events");
    events.back().durationQN.reset();
    events.back().openEnded = true;
    Progression prefix(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(completeBase.size()));
    g9RequireSameTimeline(completeBase, prefix, "G9 long-query frozen 20-event prefix");

    ImportedProgressionSession imported;
    require(imported.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
            "G9 long-query session creation failed");
    const auto before = imported.events;
    const auto query = g8MakeQuery(imported.events,
        std::pair{phraseRoute.meterNumerator, phraseRoute.meterDenominator});
    const auto queryStarted = G8Clock::now();
    const auto recommendations = g8NormalRecommendations(query, CandidateIndex(factory.templates));
    const auto elapsedMs = std::chrono::duration<double, std::milli>(
        G8Clock::now() - queryStarted).count();
    g9RequireSameTimeline(before, imported.events, "G9 long recommendation query");
    g9WriteTop10(top10, "long-input-" + std::to_string(imported.events.size()), recommendations);
    std::size_t candidateCount{};
    for (const auto& group : recommendations.groups) candidateCount += group.size();
    longInputReport << "input_events\tqualified_candidates\trecommendation_ms\topen_tail\n"
                    << imported.events.size() << '\t' << candidateCount << '\t'
                    << std::fixed << std::setprecision(3) << elapsedMs << "\t1\n";
    require(static_cast<bool>(longInputReport), "cannot write G9 long-input result");
    report << "long-input\tPASS\t" << imported.events.size()
           << "\tnormal_recommendation_calls=1;elapsed_ms=" << std::fixed
           << std::setprecision(3) << elapsedMs << '\n';
}

void g9WriteMediaSamples(const library::LoadResult& factory,
                         const std::filesystem::path& outputDirectory,
                         const std::vector<G9MediaSample>& samples,
                         std::ostream& report) {
    require(!samples.empty(),
            "no qualified structural candidate with a distinct development mechanism for media verification");
    const auto mediaDirectory = outputDirectory / "structure-media";
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        const std::vector<G8RoundRecord> oneRound{sample.round};
        g8WriteRepresentative(mediaDirectory, sample.label, i, oneRound, factory,
                              sample.meterNumerator, sample.meterDenominator);
        const auto pathDirectory = mediaDirectory / "representative-paths" /
            (std::to_string(i + 1) + "-" + sample.label);
        report << "media\tPASS\t" << sample.label << "\tmechanism="
               << g8Tsv(sample.mechanism) << "\tpath=" << pathDirectory.string() << '\n';
    }
}

int runG9Checks(const std::filesystem::path& factoryPath,
                const std::filesystem::path& oldRoundsPath,
                const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G9 check Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G9 checks require Factory version 8 or newer");
    require(std::filesystem::is_regular_file(oldRoundsPath),
            "cannot find frozen G8 rounds TSV: " + oldRoundsPath.string());
    std::filesystem::create_directories(outputDirectory);
    const auto reportPath = outputDirectory / "g9-checks.tsv";
    std::ofstream report(reportPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot create G9 checks report");
    report << "check\tstatus\tcase\tdetail\n";
    std::ofstream top10(outputDirectory / "g9-top10.tsv", std::ios::binary | std::ios::trunc);
    std::ofstream legacy(outputDirectory / "g9-legacy-structure.tsv", std::ios::binary | std::ios::trunc);
    std::ofstream longInput(outputDirectory / "g9-long-input.tsv", std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(top10) && static_cast<bool>(legacy) && static_cast<bool>(longInput),
            "cannot create G9 diagnostic reports");
    top10 << "query\tvisible_rank\tpage\tchannel\tsource\tcontext_length\tcontinuation_start"
             "\tstructure_qualified\tstructure_priority\tseam_qn\tstructure_reason\tmechanism"
             "\thistory_similarity\thistory_family_present\thistory_mechanism_present\tcontinuation\n";
    legacy << "start_id\tsample_order\tround\tinput_length\trecorded_source\tactual_key"
              "\tstructure_qualified\tstructure_priority\tseam_qn\tstructure_reason\tmechanism"
              "\thistory_similarity\thistory_family_present\thistory_mechanism_present"
              "\trecommendation_ms\n";
    std::size_t failed{};
    const auto run = [&](std::string_view name, const auto& check) {
        try {
            check();
        } catch (const std::exception& error) {
            ++failed;
            report << name << "\tFAIL\t-\t" << g8Tsv(error.what()) << '\n';
        }
    };
    std::set<std::string> mechanisms;
    std::vector<G9MediaSample> mediaSamples;
    run("shared-1-5", [&] { g9CheckUnprovenSharedOneFive(factory, report, top10); });
    run("reviewed-v8-phrase", [&] {
        g9CheckReviewedV8Phrase(factory, report, top10, mechanisms, mediaSamples);
    });
    run("legacy-chains", [&] {
        g9CheckLegacyChains(factory, oldRoundsPath, report, top10, legacy,
                            mechanisms, mediaSamples);
    });
    run("long-input", [&] {
        g9CheckLongInput(factory, oldRoundsPath, report, top10, longInput);
    });
    run("qualified-media", [&] {
        g9WriteMediaSamples(factory, outputDirectory, mediaSamples, report);
    });
    report.flush();
    top10.flush();
    legacy.flush();
    longInput.flush();
    require(static_cast<bool>(report) && static_cast<bool>(top10) &&
            static_cast<bool>(legacy) && static_cast<bool>(longInput),
            "cannot finish G9 reports");
    std::cout << "g9_checks failed=" << failed << " report=" << reportPath.string() << '\n';
    require(failed == 0, "G9 focused checks failed; see " + reportPath.string());
    return 0;
}
} // namespace
