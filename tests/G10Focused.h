#pragma once

#include <map>
#include <set>
#include <tuple>

namespace {
using namespace harmony;

RecommendationSet g10DiagnosticRecommendations(const MatchQuery& query,
                                                const CandidateIndex& index) {
    const auto weights = recommendationWeights();
    RecommendationRequest exactRequest;
    exactRequest.inspirationPriority = false;
    const auto exact = recommendContinuations(query, index, exactRequest, weights);

    RecommendationRequest inspirationRequest;
    inspirationRequest.inspirationPriority = true;
    inspirationRequest.preferredIntent.reset();
    inspirationRequest.diagnoseCreative = true;
    auto inspiration = recommendContinuations(query, index, inspirationRequest, weights, &exact);
    const auto analysis = analyzeHarmony(query.source, recommendationContext(AnalysisContext{}));
    const auto inputCenter = analysis.selectedKey
        ? std::optional(analysis.selectedKey->key) : std::nullopt;
    for (auto& group : inspiration.groups) for (auto& candidate : group) {
        if (candidate.channel != RecommendationChannel::Exact && !candidate.continuation.empty())
            require(candidate.sourceEndPreserved,
                    "G10 Inspiration candidate does not preserve its complete source ending: " +
                        candidate.primaryTemplate);
        if (candidate.ending == PhraseEnding::CenterTransition)
            require(!candidate.targetCenter && candidate.centerRelation == CenterRelation::Undetermined,
                    "G10 center-transition ending claims an established destination: " +
                        candidate.primaryTemplate);
        candidate.direction = inspirationDirection(candidate, inputCenter);
        if (candidate.channel == RecommendationChannel::Exact) candidate.originCenter = inputCenter;
    }
    inspiration.inspirationOrder = mixedInspirationOrder(inspiration);
    return inspiration;
}

Progression g10ParseFrozenFingerprint(std::string_view fingerprint) {
    require(!fingerprint.empty() && fingerprint.front() == '|',
            "frozen G10 fingerprint must begin with a delimiter");
    const auto tokens = g8Split(fingerprint.substr(1), '|');
    require(!tokens.empty(), "frozen G10 fingerprint has no events");
    constexpr const char* pitchNames[]{"C", "Db", "D", "Eb", "E", "F",
                                       "Gb", "G", "Ab", "A", "Bb", "B"};
    Progression result;
    result.reserve(tokens.size());
    double startQN{};
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto at = tokens[i].find('@');
        require(at != std::string::npos, "frozen G10 event lacks duration");
        const auto parts = g8Split(std::string_view(tokens[i]).substr(0, at), ':');
        require(parts.size() == 4, "frozen G10 event must contain root, bass, quality, and mask");
        const auto root = g8ParseInt(parts[0], "frozen root");
        const auto bass = g8ParseInt(parts[1], "frozen bass");
        const auto quality = g8ParseInt(parts[2], "frozen quality");
        const auto absoluteMask = g8ParseInt(parts[3], "frozen actual pitch mask");
        const auto duration = g8ParseDouble(std::string_view(tokens[i]).substr(at + 1),
                                            "frozen duration");
        require(root >= 0 && root < 12 && bass >= 0 && bass < 12 &&
                quality >= static_cast<int>(ChordQuality::Unknown) &&
                quality <= static_cast<int>(ChordQuality::Augmented) &&
                absoluteMask > 0 && absoluteMask < (1 << 12) && duration >= 0.0,
                "frozen G10 event is outside supported bounds");
        const bool open = duration == 0.0;
        require(!open || i + 1 == tokens.size(),
                "only the final frozen G10 query event may be OPEN");

        std::uint16_t relativeMask{};
        for (int pitch = 0; pitch < 12; ++pitch)
            if (absoluteMask & (1 << pitch))
                relativeMask |= static_cast<std::uint16_t>(1u << ((pitch - root + 12) % 12));
        const auto writeMask = [](ChordEvent& event, std::uint16_t mask) {
            std::ostringstream text;
            text << "0x" << std::hex << mask;
            event.extensions.mask = text.str();
        };
        ChordEvent event;
        event.name = pitchNames[root];
        if (bass != root) event.name += "/" + std::string(pitchNames[bass]);
        event.root = static_cast<PitchClass>(root);
        event.bass = static_cast<PitchClass>(bass);
        event.quality = static_cast<ChordQuality>(quality);
        writeMask(event, relativeMask);

        auto normalized = chordPitches(event);
        if (static_cast<int>(normalized.quality) != quality && bass != root) {
            const auto bassInterval = (bass - root + 12) % 12;
            const auto upperMask = static_cast<std::uint16_t>(
                relativeMask & ~(1u << bassInterval));
            writeMask(event, upperMask);
            normalized = chordPitches(event);
        }
        require(static_cast<int>(normalized.root) == root &&
                static_cast<int>(normalized.bass) == bass &&
                static_cast<int>(normalized.quality) == quality,
                "frozen G10 event cannot preserve recorded root, bass, and quality");

        event.startQN = startQN;
        event.openEnded = open;
        if (!open) {
            event.durationQN = duration;
            startQN += duration;
        }
        result.push_back(std::move(event));
    }
    require(result.front().startQN == 0.0, "frozen G10 query must start at QN zero");
    require(result.back().openEnded,
            "frozen G10 query fingerprint must end with an OPEN event");
    require(g8EventListFingerprint(result) == fingerprint,
            "frozen G10 fingerprint did not round-trip all recorded fields");
    return result;
}

Progression g10ParseCompleteFrozenFingerprint(std::string_view fingerprint) {
    require(!fingerprint.empty() && fingerprint.front() == '|',
            "complete frozen G10 fingerprint must begin with a delimiter");
    auto tokens = g8Split(fingerprint.substr(1), '|');
    require(!tokens.empty(), "complete frozen G10 fingerprint has no events");
    const auto at = tokens.back().find('@');
    require(at != std::string::npos, "complete frozen G10 final event lacks duration");
    const auto finalDuration = g8ParseDouble(std::string_view(tokens.back()).substr(at + 1),
                                             "complete frozen final duration");
    require(finalDuration > 0.0, "complete frozen G10 final event has no positive duration");
    tokens.back() = tokens.back().substr(0, at) + "@0";
    std::ostringstream openFingerprint;
    for (const auto& token : tokens) openFingerprint << '|' << token;
    auto events = g10ParseFrozenFingerprint(openFingerprint.str());
    events.back().durationQN = finalDuration;
    events.back().openEnded = false;
    require(g8EventListFingerprint(events) == fingerprint,
            "complete frozen G10 fingerprint changed during timeline reconstruction");
    return events;
}

std::vector<G8RecordedRcrRound> g10ReadSuccessfulFrozenRounds(
    const std::filesystem::path& roundsPath) {
    std::ifstream input(roundsPath, std::ios::binary);
    require(static_cast<bool>(input), "cannot open frozen G9 rounds TSV: " + roundsPath.string());
    std::string line;
    require(static_cast<bool>(std::getline(input, line)), "frozen G9 rounds TSV is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto header = g8Split(line, '\t');
    std::unordered_map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < header.size(); ++i)
        require(columns.emplace(header[i], i).second, "duplicate column in frozen G9 rounds TSV");
    require(columns.contains("start_id") && columns.contains("sample_order") && columns.contains("round"),
            "frozen G9 rounds TSV lacks start_id, sample_order, or round");
    const auto cell = [&](const std::vector<std::string>& cells, std::string_view name) -> const std::string& {
        require(cells.size() == header.size(), "frozen G9 round has an invalid column count");
        return cells[columns.at(std::string(name))];
    };

    struct ChainIndex {
        std::size_t sampleOrder{};
        std::vector<std::size_t> rounds;
    };
    std::map<std::string, ChainIndex> chains;
    std::size_t rowCount{};
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto cells = g8Split(line, '\t');
        const auto& startId = cell(cells, "start_id");
        const auto sampleOrder = static_cast<std::size_t>(
            g8ParseInt(cell(cells, "sample_order"), "G9 sample order"));
        const auto round = static_cast<std::size_t>(g8ParseInt(cell(cells, "round"), "G9 round"));
        require(sampleOrder > 0 && round > 0 && round <= 3,
                "frozen G9 round is outside the expected sample or round bounds");
        const auto inserted = chains.try_emplace(startId, ChainIndex{sampleOrder, {}});
        require(inserted.first->second.sampleOrder == sampleOrder,
                "frozen G9 chain changes its original sample order: " + startId);
        inserted.first->second.rounds.push_back(round);
        ++rowCount;
    }
    require(input.eof(), "cannot finish reading frozen G9 rounds TSV");
    require(rowCount == 36 && chains.size() == 12,
            "G10 diagnosis requires the 36 rounds from the 12 frozen G9 successful chains");

    std::vector<std::pair<std::size_t, std::string>> orderedStarts;
    orderedStarts.reserve(chains.size());
    for (const auto& [startId, chain] : chains) {
        auto rounds = chain.rounds;
        std::sort(rounds.begin(), rounds.end());
        require(rounds == std::vector<std::size_t>{1, 2, 3},
                "frozen G9 success chain must contain exactly rounds 1, 2, and 3: " + startId);
        orderedStarts.emplace_back(chain.sampleOrder, startId);
    }
    std::sort(orderedStarts.begin(), orderedStarts.end());
    for (std::size_t i = 1; i < orderedStarts.size(); ++i)
        require(orderedStarts[i - 1].first < orderedStarts[i].first,
                "frozen G9 successful chains repeat a sample order");

    std::vector<G8RecordedRcrRound> result;
    result.reserve(36);
    for (const auto& [sampleOrder, startId] : orderedStarts) {
        auto chain = g8ReadRecordedRounds(roundsPath, startId);
        require(chain.front().sampleOrder == sampleOrder,
                "G8 recorded-round parser changed the frozen G9 sample order: " + startId);
        result.insert(result.end(), chain.begin(), chain.end());
    }
    require(result.size() == 36, "G10 diagnosis did not reconstruct all frozen G9 rounds");
    return result;
}

int runG10Diagnose(const std::filesystem::path& factoryPath,
                   const std::filesystem::path& roundsPath,
                   const std::filesystem::path& outputDirectory, std::size_t skipQueries = 0) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G10 diagnostic Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G10 diagnostics require Factory version 8 or newer");
    require(std::filesystem::is_regular_file(roundsPath),
            "cannot find frozen G9 rounds TSV: " + roundsPath.string());
    std::filesystem::create_directories(outputDirectory);

    const auto rounds = g10ReadSuccessfulFrozenRounds(roundsPath);
    const CandidateIndex index(factory.templates);
    const auto gatesPath = outputDirectory / "g10-gates.tsv";
    const auto candidatesPath = outputDirectory / "g10-candidates.tsv";
    const auto summaryPath = outputDirectory / "g10-summary.tsv";
    const auto parserNotePath = outputDirectory / "g10-parser-scope.txt";
    std::ofstream gates(gatesPath, std::ios::binary | std::ios::trunc);
    std::ofstream candidates(candidatesPath, std::ios::binary | std::ios::trunc);
    std::ofstream summary(summaryPath, std::ios::binary | std::ios::trunc);
    std::ofstream parserNote(parserNotePath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(gates) && static_cast<bool>(candidates) && static_cast<bool>(summary) &&
            static_cast<bool>(parserNote),
            "cannot create G10 diagnostic TSV outputs");
    parserNote << "Frozen fingerprints retain the recorded root, quality, explicit bass, actual pitch-class set, and timing.\n"
                  "The actual pitch-class set may include the bass. When its full mask changes the normalized quality, "
                  "the parser may remove the explicit-bass pitch class from the upper-interval mask while retaining "
                  "that pitch as the explicit bass; it accepts the event only if every recorded fingerprint field "
                  "round-trips exactly. This recovers the frozen harmonic identity, actual pitch-class set, and timing. "
                  "The fingerprint cannot distinguish upper-note assignment from bass membership and does not recover "
                  "the original note configuration or source identity.\n";
    gates << "query\tsample_order\tstart_id\tround\tsource\tkey\tdisposition\tboundary\n";
    candidates << "query\tsample_order\tstart_id\tround\tinput_length\tvisible_rank\tpage"
                  "\tsource\tkey\tchannel\tfamily\tending\tstructure_qualified"
                  "\tstructure_priority\tstructure_reason\thistory_family_present"
                  "\thistory_mechanism_present\thistory_similarity\tranking_score"
                  "\tnext_phrase_source_count\tcontinuation\tcandidate_music_fingerprint\n";
    summary << "query\tsample_order\tstart_id\tround\tinput_length"
               "\tpre_structure_qualified_source_key_count"
               "\tstructure_qualified_music_count\tstructure_qualified_family_count"
               "\tindependent_complete_open_direction_count\n";

    std::size_t writtenQueries{};
    for (std::size_t chainStart = 0; chainStart < rounds.size(); chainStart += 3) {
        const auto& chainFirst = rounds[chainStart];
        const auto& source = g9FindTemplate(factory, chainFirst.startId);
        const auto startKey = source.rawChordKey.value_or(KeySignature{PitchClass::C, source.mode});
        const auto startRoute = preview::templateRoute(source, startKey);
        require(static_cast<bool>(startRoute),
                "G10 frozen start route failed: " + source.id + ": " + startRoute.error);

        for (std::size_t chainRound = 0; chainRound < 3; ++chainRound) {
            if(chainStart+chainRound<skipQueries)continue;
            const auto& record = rounds[chainStart + chainRound];
            require(record.startId == chainFirst.startId && record.round == chainRound + 1,
                    "G10 frozen rounds are not grouped in original chain order");
            auto events = g10ParseFrozenFingerprint(record.queryInputFingerprint);
            require(events.size() == record.inputLength,
                    "G10 frozen query length differs from its recorded input fingerprint");
            if (chainRound > 0) {
                const auto previousComplete =
                    g10ParseCompleteFrozenFingerprint(
                        rounds[chainStart + chainRound - 1].actualImportFingerprint);
                g8RequireRecordedCarry(previousComplete, events);
                auto reopenedTail = previousComplete;
                reopenedTail.back().durationQN.reset();
                reopenedTail.back().openEnded = true;
                require(g8EventListFingerprint(reopenedTail) == record.queryInputFingerprint,
                        "G10 frozen next query changed previous-round MIDI music or duration identity");
            }

            ImportedProgressionSession input;
            require(input.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
                    "cannot reconstruct G10 frozen query input");
            const auto inputBefore = input.events;
            const auto query = g8MakeQuery(input.events,
                std::pair{startRoute.meterNumerator, startRoute.meterDenominator});
            const auto recommendations = g10DiagnosticRecommendations(query, index);
            const auto queryId = record.startId + ":round-" + std::to_string(record.round);
            g9RequireSameTimeline(inputBefore, input.events, "G10 " + queryId);

            std::set<std::tuple<std::string, int, int, std::string>> uniqueTrials;
            std::set<std::pair<std::string, std::string>> preStructureQualifiedSourceKeys;
            for (const auto& trial : recommendations.creativeTrials) {
                const auto key = std::tuple(trial.source, static_cast<int>(trial.key.tonic),
                                            static_cast<int>(trial.key.mode), trial.disposition);
                if (!uniqueTrials.insert(key).second) continue;
                gates << g8Tsv(queryId) << '\t' << record.sampleOrder << '\t' << g8Tsv(record.startId)
                      << '\t' << record.round << '\t' << g8Tsv(trial.source) << '\t'
                      << g9KeyText(trial.key) << '\t' << g8Tsv(trial.disposition) << '\t'
                      << std::fixed << std::setprecision(3) << trial.boundary << '\n';
                if (trial.disposition == "qualified-before-structure")
                    preStructureQualifiedSourceKeys.emplace(trial.source, g9KeyText(trial.key));
            }

            std::set<std::string> structureQualifiedMusic,
                structureQualifiedFamilies, independentOpenMusic;
            const auto order = mixedInspirationOrder(recommendations);
            for (std::size_t visible = 0; visible < order.size(); ++visible) {
                const auto* candidate = g8CandidateAt(recommendations, order[visible]);
                require(candidate != nullptr, "G10 mixed order contains an invalid candidate position");
                const auto identity = g8CandidateMusicFingerprint(*candidate);
                if (candidate->structureQualified) {
                    structureQualifiedMusic.insert(identity);
                    structureQualifiedFamilies.insert(g8SourceFamily(*candidate, index));
                }
                if (candidate->structureQualified && !candidate->exploratory &&
                    candidate->completeSourcePhrase && candidate->reviewedEnding &&
                    unterminatedEnding(candidate->ending))
                    independentOpenMusic.insert(identity);

                candidates << g8Tsv(queryId) << '\t' << record.sampleOrder << '\t'
                           << g8Tsv(record.startId) << '\t' << record.round << '\t'
                           << record.inputLength << '\t' << visible + 1 << '\t'
                           << visible / ui::recommendationPageSize + 1 << '\t'
                           << g8Tsv(candidate->primaryTemplate) << '\t' << g9KeyText(candidate->key) << '\t'
                           << recommendationChannelName(candidate->channel) << '\t'
                           << g8Tsv(g8SourceFamily(*candidate, index)) << '\t'
                           << phraseEndingName(candidate->ending) << '\t'
                           << (candidate->structureQualified ? 1 : 0) << '\t'
                           << candidate->structurePriority << '\t' << g8Tsv(candidate->structureReason) << '\t'
                           << (candidate->historyFamilyPresent ? 1 : 0) << '\t'
                           << (candidate->historyMechanismPresent ? 1 : 0) << '\t'
                           << std::fixed << std::setprecision(3) << candidate->historySimilarity << '\t'
                           << candidate->rankingScore << '\t' << candidate->nextPhraseSources.size() << '\t'
                           << g8Tsv(g8ContinuationLabels(*candidate)) << '\t'
                           << g8Tsv(identity) << '\n';
            }
            summary << g8Tsv(queryId) << '\t' << record.sampleOrder << '\t' << g8Tsv(record.startId)
                    << '\t' << record.round << '\t' << record.inputLength << '\t'
                    << preStructureQualifiedSourceKeys.size() << '\t'
                    << structureQualifiedMusic.size() << '\t' << structureQualifiedFamilies.size() << '\t'
                    << independentOpenMusic.size() << '\n';
            ++writtenQueries;
        }
    }
    gates.flush();
    candidates.flush();
    summary.flush();
    parserNote.flush();
    require(static_cast<bool>(gates) && static_cast<bool>(candidates) && static_cast<bool>(summary),
            "cannot finish G10 diagnostic TSV outputs");
    require(static_cast<bool>(parserNote), "cannot finish G10 frozen parser scope note");
    require(writtenQueries == 36-skipQueries, "G10 diagnosis did not write the remaining frozen queries");
    std::cout << "g10_diagnose frozen_success_rounds=" << writtenQueries
              << " gates=" << gatesPath.string() << " candidates=" << candidatesPath.string()
              << " summary=" << summaryPath.string() << " parser_scope=" << parserNotePath.string() << '\n';
    return 0;
}

struct G10CandidateSnapshot {
    std::string source, key, channel, family, ending, structureReason, musicFingerprint;
    int structureQualified{}, structurePriority{}, historyFamilyPresent{}, historyMechanismPresent{};
    std::size_t nextPhraseSourceCount{};
    double historySimilarity{};
};

using G10CandidateTable = std::map<std::string, std::map<std::string, G10CandidateSnapshot>>;

G10CandidateTable g10ReadBaselineCandidates(const std::filesystem::path& path,
                                            const std::set<std::string>& wantedQueries) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot open baseline G10 candidates TSV: " + path.string());
    std::string line;
    require(static_cast<bool>(std::getline(input, line)), "baseline G10 candidates TSV is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto header = g8Split(line, '\t');
    std::unordered_map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < header.size(); ++i)
        require(columns.emplace(header[i], i).second, "duplicate column in baseline G10 candidates TSV");
    constexpr std::array<std::string_view, 17> required{
        "query", "source", "key", "channel", "family", "ending", "structure_qualified",
        "structure_priority", "structure_reason", "history_family_present",
        "history_mechanism_present", "history_similarity", "next_phrase_source_count",
        "candidate_music_fingerprint", "visible_rank", "page", "input_length"};
    for (const auto name : required)
        require(columns.contains(std::string(name)),
                "baseline G10 candidates TSV lacks column " + std::string(name));
    const auto cell = [&](const std::vector<std::string>& cells, std::string_view name) -> const std::string& {
        require(cells.size() == header.size(), "baseline G10 candidate has an invalid column count");
        return cells[columns.at(std::string(name))];
    };

    G10CandidateTable result;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto cells = g8Split(line, '\t');
        const auto& query = cell(cells, "query");
        if (!wantedQueries.contains(query)) continue;
        G10CandidateSnapshot row;
        row.source = cell(cells, "source");
        row.key = cell(cells, "key");
        row.channel = cell(cells, "channel");
        row.family = cell(cells, "family");
        row.ending = cell(cells, "ending");
        row.structureQualified = g8ParseInt(cell(cells, "structure_qualified"), "baseline structure flag");
        row.structurePriority = g8ParseInt(cell(cells, "structure_priority"), "baseline structure priority");
        row.structureReason = cell(cells, "structure_reason");
        row.historyFamilyPresent = g8ParseInt(cell(cells, "history_family_present"), "baseline family history");
        row.historyMechanismPresent = g8ParseInt(cell(cells, "history_mechanism_present"), "baseline mechanism history");
        row.historySimilarity = g8ParseDouble(cell(cells, "history_similarity"), "baseline history similarity");
        row.nextPhraseSourceCount = static_cast<std::size_t>(
            g8ParseInt(cell(cells, "next_phrase_source_count"), "baseline next-source count"));
        row.musicFingerprint = cell(cells, "candidate_music_fingerprint");
        require(!row.musicFingerprint.empty(), "baseline candidate has an empty music fingerprint");
        require(result[query].emplace(row.musicFingerprint, std::move(row)).second,
                "baseline G10 query repeats an actual candidate music fingerprint: " + query);
    }
    require(input.eof(), "cannot finish reading baseline G10 candidates TSV");
    return result;
}

G10CandidateSnapshot g10SnapshotCandidate(const ContinuationCandidate& candidate,
                                          const CandidateIndex& index) {
    return {candidate.primaryTemplate, g9KeyText(candidate.key), recommendationChannelName(candidate.channel),
        g8SourceFamily(candidate, index), phraseEndingName(candidate.ending), candidate.structureReason,
        g8CandidateMusicFingerprint(candidate), candidate.structureQualified ? 1 : 0,
        candidate.structurePriority, candidate.historyFamilyPresent ? 1 : 0,
        candidate.historyMechanismPresent ? 1 : 0, candidate.nextPhraseSources.size(),
        candidate.historySimilarity};
}

bool g10SameCandidateSnapshot(const G10CandidateSnapshot& left,
                              const G10CandidateSnapshot& right) {
    return left.source == right.source && left.key == right.key && left.channel == right.channel &&
        left.family == right.family && left.ending == right.ending &&
        left.structureReason == right.structureReason &&
        left.musicFingerprint == right.musicFingerprint &&
        left.structureQualified == right.structureQualified &&
        left.structurePriority == right.structurePriority &&
        left.historyFamilyPresent == right.historyFamilyPresent &&
        left.historyMechanismPresent == right.historyMechanismPresent &&
        left.nextPhraseSourceCount == right.nextPhraseSourceCount &&
        std::abs(left.historySimilarity - right.historySimilarity) <= 0.00051;
}

template<class T, std::size_t N>
std::string g10ArrayText(const std::array<T, N>& values) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) output << ',';
        output << values[i];
    }
    return output.str();
}

bool g10SameRouteProfile(const CreativeRouteProfile& left,
                         const CreativeRouteProfile& right) {
    constexpr float tolerance = 1e-6f;
    const auto sameFloats = [tolerance](const auto& a, const auto& b) {
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::abs(a[i] - b[i]) > tolerance) return false;
        return true;
    };
    return left.ready == right.ready && left.tones == right.tones &&
        sameFloats(left.bass, right.bass) && sameFloats(left.tension, right.tension) &&
        sameFloats(left.palette, right.palette) &&
        std::abs(left.contextFit - right.contextFit) <= tolerance &&
        std::abs(left.historySimilarity - right.historySimilarity) <= tolerance &&
        std::abs(left.recentSimilarity - right.recentSimilarity) <= tolerance;
}

std::map<std::string, const ContinuationCandidate*> g10CandidatesByFingerprint(
    const RecommendationSet& recommendations) {
    std::map<std::string, const ContinuationCandidate*> result;
    for (const auto position : mixedInspirationOrder(recommendations)) {
        const auto* candidate = g8CandidateAt(recommendations, position);
        require(candidate != nullptr, "G10 profile check encountered an invalid candidate position");
        require(result.emplace(g8CandidateMusicFingerprint(*candidate), candidate).second,
                "G10 mixed order repeats an actual candidate music identity");
    }
    return result;
}

Progression g10AliasActualSymbols(const Progression& progression, bool& changed) {
    constexpr const char* enharmonicNames[]{"B#", "C#", "D", "D#", "Fb", "E#",
                                            "F#", "G", "G#", "A", "A#", "B"};
    Progression result = progression;
    for (auto& event : result) {
        const auto actual = chordPitches(event);
        require(actual.root >= 0 && actual.bass >= 0,
                "cannot alias a frozen chord without explicit actual root and bass");
        std::string name = enharmonicNames[actual.root];
        if (actual.bass != actual.root) name += "/" + std::string(enharmonicNames[actual.bass]);
        changed |= name != event.name;
        event.name = std::move(name);
        event.root = static_cast<PitchClass>(actual.root);
        event.bass = static_cast<PitchClass>(actual.bass);
    }
    return result;
}

std::string g10CheckProfileInvariance(const MatchQuery& query,
                                      const CandidateIndex& index,
                                      const RecommendationSet& original,
                                      int meterNumerator, int meterDenominator) {
    bool symbolsChanged{};
    const auto aliasEvents = g10AliasActualSymbols(query.source, symbolsChanged);
    require(symbolsChanged, "G10 symbol-alias case did not change any chord spelling");
    require(g8EventListFingerprint(aliasEvents) == g8EventListFingerprint(query.source),
            "enharmonic symbol alias changed actual root, bass, pitch set, or timing");
    const auto aliasQuery = g8MakeQuery(aliasEvents, std::pair{meterNumerator, meterDenominator});
    const auto aliasResults = g10DiagnosticRecommendations(aliasQuery, index);
    const auto originalByMusic = g10CandidatesByFingerprint(original);
    const auto aliasByMusic = g10CandidatesByFingerprint(aliasResults);
    require(originalByMusic.size() == aliasByMusic.size(),
            "same-sound enharmonic query changed the eligible candidate count");
    for (const auto& [identity, originalCandidate] : originalByMusic) {
        const auto found = aliasByMusic.find(identity);
        require(found != aliasByMusic.end(),
                "same-sound enharmonic query removed an actual route candidate");
        require(originalCandidate->structureQualified == found->second->structureQualified &&
                originalCandidate->structurePriority == found->second->structurePriority &&
                originalCandidate->structureReason == found->second->structureReason,
                "same-sound enharmonic query changed structural qualification");
        require(g10SameRouteProfile(originalCandidate->routeProfile, found->second->routeProfile),
                "actual route profile changed under enharmonic MIDI symbol reinterpretation");
        require(creativeRouteSimilarity(*originalCandidate, *found->second) >= .999999f,
                "actual route similarity changed under enharmonic MIDI symbol reinterpretation");
    }

    auto renamed = original;
    for (auto& group : renamed.groups) for (auto& candidate : group) {
        candidate.primaryTemplate = "renamed-source:" + candidate.primaryTemplate;
        candidate.family = "renamed-family";
        candidate.developmentMechanism = "renamed-mechanism";
    }
    prepareContextualDiversity(query, renamed);
    const auto renamedByMusic = g10CandidatesByFingerprint(renamed);
    require(originalByMusic.size() == renamedByMusic.size(),
            "ID/family rename changed the legal actual candidate count");
    for (const auto& [identity, originalCandidate] : originalByMusic) {
        const auto found = renamedByMusic.find(identity);
        require(found != renamedByMusic.end() &&
                g10SameRouteProfile(originalCandidate->routeProfile, found->second->routeProfile),
                "ID/family rename changed a route profile");
        require(creativeRouteSimilarity(*originalCandidate, *found->second) >= .999999f,
                "ID/family rename changed actual route similarity");
    }
    return "same-sound alias and ID/family rename preserve every route profile and similarity";
}

std::string g10CheckHistoryIsSoft(const RecommendationSet& original) {
    auto lowHistory = original;
    auto highHistory = original;
    lowHistory.inspirationOrder.clear();
    highHistory.inspirationOrder.clear();
    for (auto* set : {&lowHistory, &highHistory})
        for (auto& group : set->groups) for (auto& candidate : group)
            candidate.routeProfile.historySimilarity = set == &lowHistory ? 0.f : 1.f;
    const auto baseline = g10CandidatesByFingerprint(original);
    const auto low = g10CandidatesByFingerprint(lowHistory);
    const auto high = g10CandidatesByFingerprint(highHistory);
    require(baseline.size() == low.size() && baseline.size() == high.size(),
            "history similarity changed the number of actual candidates");
    for (const auto& [identity, candidate] : baseline) {
        const auto lowFound = low.find(identity), highFound = high.find(identity);
        require(lowFound != low.end() && highFound != high.end(),
                "history similarity removed an actual candidate");
        for (const auto* changed : {lowFound->second, highFound->second})
            require(changed->baseQualified == candidate->baseQualified &&
                    changed->hardConstraintsPassed == candidate->hardConstraintsPassed &&
                    changed->structureQualified == candidate->structureQualified &&
                    changed->structurePriority == candidate->structurePriority &&
                    changed->structureReason == candidate->structureReason,
                    "history similarity changed route eligibility or structure gates");
    }
    return "history similarity changes ordering evidence only; candidate set and gates are unchanged";
}

std::string g10CheckTwoRoutePagination() {
    CreativeRouteProfile routeA, routeB;
    routeA.ready = routeB.ready = true;
    routeA.tones.fill(0x003);
    routeA.bass.fill(0.f);
    routeA.palette[0] = 1.f;
    routeA.contextFit = .7f;
    routeB.tones.fill(0x180);
    routeB.bass.fill(6.f);
    routeB.tension.fill(.8f);
    routeB.palette[6] = 1.f;
    routeB.contextFit = .6f;

    const auto makeCandidate = [](std::string source, std::vector<std::string> chords,
                                  const CreativeRouteProfile& profile) {
        ContinuationCandidate candidate;
        candidate.id = source;
        candidate.primaryTemplate = std::move(source);
        candidate.channel = RecommendationChannel::PhraseTransition;
        candidate.intent = PhraseIntent::Develop;
        candidate.baseQualified = true;
        candidate.hardConstraintsPassed = true;
        candidate.structureQualified = true;
        candidate.sourceEndPreserved = true;
        candidate.completeSourcePhrase = true;
        candidate.reviewedEnding = true;
        candidate.ending = PhraseEnding::OpenUnresolved;
        candidate.rankingScore = 80.f;
        candidate.creativeQuality.tier = 2;
        candidate.routeProfile = profile;
        for (auto& label : chords) {
            ConcreteChordEvent event;
            event.label = std::move(label);
            event.durationQN = 4.0;
            candidate.continuation.push_back(std::move(event));
        }
        return candidate;
    };
    RecommendationSet recommendations;
    recommendations.groups[0].push_back(makeCandidate("A1", {"C", "F", "G", "Am"}, routeA));
    recommendations.groups[0].push_back(makeCandidate("B1", {"Dm", "Bb", "F", "C"}, routeB));
    recommendations.groups[0].push_back(makeCandidate("A2", {"C", "Dm", "G", "Am"}, routeA));
    recommendations.groups[0].push_back(makeCandidate("B2", {"Dm", "Gm", "Bb", "A"}, routeB));
    recommendations.groups[0].push_back(makeCandidate("A3", {"C", "Em", "F", "G"}, routeA));
    recommendations.groups[0].push_back(makeCandidate("B3", {"Dm", "Bb", "Gm", "A"}, routeB));
    for (std::size_t i = 0; i < recommendations.groups[0].size(); ++i)
        recommendations.inspirationOrder.push_back({0, i});

    const auto firstPageSize = inspirationFirstPageSize(recommendations, ui::recommendationPageSize);
    const auto order = mixedInspirationOrder(recommendations);
    require(firstPageSize == 2, "two distinct actual routes must produce a dynamic first page of two");
    require(order.size() == 6, "dynamic first-page sizing removed legal variants from the full list");
    const auto first = ui::browsePage(order.size(), 0, firstPageSize);
    const auto rest = ui::browsePage(order.size(), 1, firstPageSize);
    require(first.pages == 2 && first.begin == 0 && first.end == 2 &&
            rest.begin == 2 && rest.end == order.size(),
            "two-route dynamic pagination does not expose every legal candidate");
    for (std::size_t flat = 0; flat < order.size(); ++flat)
        require(ui::browsePageIndex(flat, firstPageSize) == (flat < 2 ? 0u : 1u),
                "UI flat-rank to page conversion disagrees with dynamic two-route pagination");
    return "2 actual route profiles yield firstPageSize=2; all 6 legal variants remain pageable";
}

std::string g10CheckLongInput(const library::LoadResult& factory,
                              const CandidateIndex& index,
                              const std::filesystem::path& roundsPath,
                              std::ostream& output) {
    const auto firstChain = g8ReadRecordedRounds(roundsPath, "V3_CYCLE_027");
    require(firstChain.size() == 3 && firstChain[2].inputLength == 20,
            "G10 long-input case must start from the frozen V3_CYCLE_027 20-event query");
    const auto openBase = g10ParseFrozenFingerprint(firstChain[2].queryInputFingerprint);
    const auto completeBase = g10ParseCompleteFrozenFingerprint(firstChain[1].actualImportFingerprint);
    g8RequireRecordedCarry(completeBase, openBase);
    require(completeBase.size() == 20, "G10 long-input complete prefix is not 20 events");

    const auto& phraseSource = g9FindTemplate(factory, firstChain[0].primarySource);
    const auto phraseKey = g9KeyFromActualTail(phraseSource,
        g10ParseCompleteFrozenFingerprint(firstChain[0].actualImportFingerprint));
    const auto phraseRoute = preview::templateRoute(phraseSource, phraseKey);
    require(static_cast<bool>(phraseRoute), "G10 long-input append route failed: " + phraseRoute.error);
    require(phraseRoute.imported.events.size() == phraseSource.rawChords.size() &&
            !phraseRoute.imported.events.empty(), "G10 long-input append route is not a full source phrase");

    auto events = openBase;
    events.back().durationQN = completeBase.back().durationQN;
    events.back().openEnded = false;
    double nextStartQN = events.back().startQN + events.back().durationQN.value_or(0.0);
    while (events.size() < 84) {
        for (const auto& sourceEvent : phraseRoute.imported.events) {
            auto event = sourceEvent;
            event.startQN = nextStartQN;
            event.openEnded = false;
            require(event.durationQN && *event.durationQN > 0.0,
                    "G10 long-input appended source has unknown duration");
            nextStartQN += *event.durationQN;
            events.push_back(std::move(event));
        }
    }
    require(events.size() >= 84, "G10 long-input construction did not reach 84 events");
    events.back().durationQN.reset();
    events.back().openEnded = true;
    Progression prefix(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(completeBase.size()));
    g9RequireSameTimeline(completeBase, prefix, "G10 long-input frozen prefix");

    ImportedProgressionSession imported;
    require(imported.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
            "G10 long-input session creation failed");
    const auto before = imported.events;
    const auto queryStarted = G8Clock::now();
    const auto query = g8MakeQuery(imported.events,
        std::pair{phraseRoute.meterNumerator, phraseRoute.meterDenominator});
    const auto recommendations = g8NormalRecommendations(query, index);
    const auto normalFlowMs = std::chrono::duration<double, std::milli>(
        G8Clock::now() - queryStarted).count();
    g9RequireSameTimeline(before, imported.events, "G10 84-event recommendation query");
    const auto order = mixedInspirationOrder(recommendations);
    require(!order.empty(), "G10 84-event recommendation query returned no pageable candidates");
    const auto firstPageSize = inspirationFirstPageSize(recommendations, ui::recommendationPageSize);
    output << "input_events\tcomplete_prefix_events\tactual_candidate_count\tfirst_page_size"
              "\ttimed_scope\tquery_and_normal_recommendation_ms\tall_candidates_pageable\n"
           << imported.events.size() << '\t' << completeBase.size() << '\t' << order.size() << '\t'
           << firstPageSize << "\tg8MakeQuery+g8NormalRecommendations; existing CandidateIndex reused\t"
           << std::fixed << std::setprecision(3) << normalFlowMs << "\t1\n";
    require(static_cast<bool>(output), "cannot write G10 long-input result");
    return "one 84-event g8MakeQuery+g8NormalRecommendations flow preserved its timeline (" +
        std::to_string(normalFlowMs) + " ms; existing CandidateIndex reused)";
}

struct G10CheckState {
    ImportedProgressionSession input;
    MatchQuery query;
    RecommendationSet recommendations;
    int meterNumerator{}, meterDenominator{};
};

void g10WriteFailedMidiProbe(const std::filesystem::path& path,
                             std::string_view startId, std::size_t round,
                             const MatchQuery& query,
                             const RecommendationSet& normal,
                             const CandidateIndex& index) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "cannot create failed G10 MIDI probe: " + path.string());
    constexpr std::size_t columnCount = 27;
    using ProbeRow = std::array<std::string, columnCount>;
    output << "record_type\tstart_id\tround\tinput_length\tglobal_interpretation_count"
              "\trecent_interpretation_count\tmeter\tnormal_candidate_count\tposition"
              "\tsource\tkey\tchannel\tending\treviewed_ending\tcomplete_source_phrase"
              "\tbase_qualified\thard_constraints_passed\tstructure_qualified\tstructure_priority"
              "\tstructure_reason\tnext_phrase_ids\tcontinuation\tkey_confidence\tkey_score"
              "\tdisposition\tboundary\tdetail\n";
    std::size_t normalCount{};
    for (const auto& group : normal.groups) normalCount += group.size();
    const auto meterNumerator = query.timeSigNumerator.value_or(4);
    const auto meterDenominator = query.timeSigDenominator.value_or(4);
    const auto recentCount = std::min<std::size_t>(8, query.source.size());
    Progression recentEvents(query.source.end() - static_cast<std::ptrdiff_t>(recentCount),
                             query.source.end());
    const auto recentQuery = g8MakeQuery(recentEvents,
        std::pair{meterNumerator, meterDenominator});
    const auto baseRow = [&](std::string_view type) {
        ProbeRow row{};
        row[0] = type;
        row[1] = std::string(startId);
        row[2] = std::to_string(round);
        row[3] = std::to_string(query.source.size());
        row[4] = std::to_string(query.interpretations.size());
        row[5] = std::to_string(recentQuery.interpretations.size());
        row[6] = std::to_string(meterNumerator) + "/" + std::to_string(meterDenominator);
        row[7] = std::to_string(normalCount);
        return row;
    };
    const auto emit = [&](ProbeRow row) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i) output << '\t';
            output << g8Tsv(std::move(row[i]));
        }
        output << '\n';
    };
    auto summary = baseRow("summary");
    summary[26] = "normal recommendation objects; zero is explicit when empty";
    emit(std::move(summary));
    const auto writeKeys = [&](std::string_view scope, const MatchQuery& keyQuery) {
        if (keyQuery.interpretations.empty()) {
            auto row = baseRow(std::string(scope) + "_key");
            row[3] = std::to_string(keyQuery.source.size());
            row[8] = "0";
            row[26] = "no key interpretations";
            emit(std::move(row));
            return;
        }
        for (std::size_t i = 0; i < keyQuery.interpretations.size(); ++i) {
            const auto& interpretation = keyQuery.interpretations[i];
            auto row = baseRow(std::string(scope) + "_key");
            row[3] = std::to_string(keyQuery.source.size());
            row[8] = std::to_string(i + 1);
            row[10] = g9KeyText(interpretation.key.key);
            row[22] = std::to_string(interpretation.key.confidence);
            row[23] = std::to_string(interpretation.key.score);
            row[26] = std::string(scope) + " query interpretation";
            emit(std::move(row));
        }
    };
    writeKeys("global", query);
    writeKeys("recent_last_8", recentQuery);

    std::size_t position{};
    for (const auto& group : normal.groups) for (const auto& candidate : group) {
        auto row = baseRow("normal_candidate");
        row[8] = std::to_string(++position);
        row[9] = candidate.primaryTemplate;
        row[10] = g9KeyText(candidate.key);
        row[11] = recommendationChannelName(candidate.channel);
        row[12] = phraseEndingName(candidate.ending);
        row[13] = candidate.reviewedEnding ? "1" : "0";
        row[14] = candidate.completeSourcePhrase ? "1" : "0";
        row[15] = candidate.baseQualified ? "1" : "0";
        row[16] = candidate.hardConstraintsPassed ? "1" : "0";
        row[17] = candidate.structureQualified ? "1" : "0";
        row[18] = std::to_string(candidate.structurePriority);
        row[19] = candidate.structureReason;
        row[20] = g8NextIds(candidate);
        row[21] = g8ContinuationLabels(candidate);
        row[26] = "all normal recommendation objects";
        emit(std::move(row));
    }

    try {
        const auto diagnostic = g10DiagnosticRecommendations(query, index);
        auto diagnosticSummary = baseRow("diagnostic_summary");
        diagnosticSummary[26] = "diagnoseCreative=true; same Exact-to-Inspiration wrapper";
        emit(std::move(diagnosticSummary));
        std::set<std::tuple<std::string, int, int, std::string>> uniqueTrials;
        for (const auto& trial : diagnostic.creativeTrials) {
            const auto key = std::tuple(trial.source, static_cast<int>(trial.key.tonic),
                                        static_cast<int>(trial.key.mode), trial.disposition);
            if (!uniqueTrials.insert(key).second) continue;
            auto row = baseRow("creative_trial");
            row[8] = std::to_string(uniqueTrials.size());
            row[9] = trial.source;
            row[10] = g9KeyText(trial.key);
            row[24] = trial.disposition;
            row[25] = std::to_string(trial.boundary);
            row[26] = "deduplicated by source+key+disposition";
            emit(std::move(row));
        }
    } catch (const std::exception& error) {
        auto diagnosticError = baseRow("diagnostic_error");
        diagnosticError[26] = error.what();
        emit(std::move(diagnosticError));
    }
    output.flush();
    require(static_cast<bool>(output), "cannot finish failed G10 MIDI probe: " + path.string());
}

std::string g10RunMiniMidiChain(const library::LoadResult& factory,
                               const CandidateIndex& index,
                               const std::filesystem::path& roundsPath,
                               const std::filesystem::path& outputDirectory,
                               std::string_view startId,
                               std::ostream& report,
                               const std::filesystem::path& failedProbePath = {}) {
    const auto records = g8ReadRecordedRounds(roundsPath, startId);
    require(records.size() == 3 && records.front().round == 1,
            "G10 mini-chain requires the three frozen rounds for " + std::string(startId));
    const auto& start = g9FindTemplate(factory, startId);
    const auto startKey = start.rawChordKey.value_or(KeySignature{PitchClass::C, start.mode});
    const auto startRoute = preview::templateRoute(start, startKey);
    require(static_cast<bool>(startRoute), "G10 mini-chain start route failed: " + startRoute.error);
    const auto startPreview = midi::previewFromTemplate(start, startKey, 120.0);
    require(static_cast<bool>(startPreview), "G10 mini-chain start Preview failed: " + startPreview.error);
    const midi::Meter meter{startRoute.meterNumerator, startRoute.meterDenominator};

    auto initialEvents = g10ParseFrozenFingerprint(records.front().queryInputFingerprint);
    require(initialEvents.size() == records.front().inputLength,
            "G10 mini-chain frozen first-round input length changed");
    ImportedProgressionSession current;
    require(current.replace(std::move(initialEvents), TimelineCoordinateMode::RelativeToSelection),
            "G10 mini-chain cannot restore its first-round query");
    std::set<std::string> usedMusic{g8EventListFingerprint(startPreview.sequence)};
    std::set<TemplateID> usedSources{start.id};
    std::set<std::string> usedFamilies;
    const auto midiDirectory = outputDirectory / ".g10-check-midi-import";
    std::filesystem::create_directories(midiDirectory);
    std::size_t completed{};
    for (std::size_t round = 1; round <= 3; ++round) {
        std::size_t inputLength = current.events.size(), selectedFlat{}, page{}, firstPageSize{};
        const ContinuationCandidate* selected{};
        std::string selectedSource, keyText, family, ending, candidateFingerprint, completeFingerprint,
                    openInputFingerprint, mechanism;
        int structureQualified{}, structurePriority{}, profileReady{};
        float contextFit{}, historySimilarity{}, recentSimilarity{};
        std::size_t reinterpreted{};
        std::filesystem::path midiFile;
        try {
            const auto inputFingerprint = g8EventListFingerprint(current.events);
            const auto query = g8MakeQuery(current.events,
                std::pair{meter.numerator, meter.denominator});
            const auto recommendations = g8NormalRecommendations(query, index);
            require(g8EventListFingerprint(current.events) == inputFingerprint,
                    "G10 mini-chain recommendation query rewrote its imported input identity");
            const auto order = mixedInspirationOrder(recommendations);
            firstPageSize = inspirationFirstPageSize(recommendations, ui::recommendationPageSize);
            for (std::size_t visible = 0; visible < order.size(); ++visible) {
                const auto* candidate = g8CandidateAt(recommendations, order[visible]);
                if (candidate && g8Eligible(*candidate, index, usedMusic, usedSources,
                                            usedFamilies, true)) {
                    selected = candidate;
                    selectedFlat = visible;
                    break;
                }
            }
            if (!selected) {
                if (!failedProbePath.empty())
                    g10WriteFailedMidiProbe(failedProbePath, startId, round, query,
                                            recommendations, index);
                require(false, "no eligible structure-qualified open phrase in G10 mini-chain round " +
                               std::to_string(round));
            }
            page = ui::browsePageIndex(selectedFlat, firstPageSize) + 1;
            selectedSource = selected->primaryTemplate;
            keyText = g9KeyText(selected->key);
            family = g8SourceFamily(*selected, index);
            ending = phraseEndingName(selected->ending);
            mechanism = selected->developmentMechanism;
            structureQualified = selected->structureQualified ? 1 : 0;
            structurePriority = selected->structurePriority;
            profileReady = selected->routeProfile.ready ? 1 : 0;
            contextFit = selected->routeProfile.contextFit;
            historySimilarity = selected->routeProfile.historySimilarity;
            recentSimilarity = selected->routeProfile.recentSimilarity;
            candidateFingerprint = g8CandidateMusicFingerprint(*selected);

            const auto built = preview::buildSequence(current, selected, 120.0);
            require(static_cast<bool>(built), "G10 mini-chain Preview failed: " + built.error);
            const midi::CandidateExportContext exportContext{current, 120.0, meter, selected->key,
                midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
            const auto payload = midi::candidatePayload(exportContext, *selected);
            require(static_cast<bool>(payload) && !payload.payload.dawClipBytes.empty(),
                    "G10 mini-chain DAWClip payload failed: " + payload.error);
            const auto clip = midi::buildClip(built.sequence, midi::ArrangementMode::VoiceLed,
                midi::ExportScope::FullPhrase, meter, selected->key, selected->intent);
            require(static_cast<bool>(clip), "G10 mini-chain MIDI Clip failed: " + clip.error);
            midi::ExportConfig dawConfig;
            dawConfig.profile = midi::ExportProfile::DAWClip;
            const auto dawBytes = midi::writeToMemory(clip.sequence, dawConfig);
            require(static_cast<bool>(dawBytes) && dawBytes.bytes == payload.payload.dawClipBytes,
                    "G10 mini-chain DAWClip differs from Preview-derived production payload");

            midiFile = midiDirectory /
                (std::string(startId) + "-round-" + std::to_string(round) + ".mid");
            std::string writeError;
            require(midi::writeToFile(payload.payload.dawClipBytes, midiFile, writeError),
                    "G10 mini-chain DAWClip file write failed: " + writeError);
            const auto completeImport = midi::importFile(midiFile, false);
            require(static_cast<bool>(completeImport),
                    "G10 mini-chain complete DAWClip import failed: " + completeImport.extraction.error);
            ImportedProgressionSession complete;
            require(midi::applyImport(completeImport, complete),
                    "G10 mini-chain applyImport failed for complete DAWClip");
            const auto midiSummary = g8CompareMidiToPreview(built.sequence, clip.sequence,
                complete.events, completeImport.midi.file, false, "G10 mini-chain complete DAWClip");
            reinterpreted = midiSummary.reinterpretationCount;
            completeFingerprint = g8EventListFingerprint(complete.events);

            const auto openImport = midi::importFile(midiFile, true);
            require(static_cast<bool>(openImport),
                    "G10 mini-chain OPEN DAWClip import failed: " + openImport.extraction.error);
            ImportedProgressionSession next;
            require(midi::applyImport(openImport, next),
                    "G10 mini-chain applyImport failed for OPEN DAWClip");
            (void)g8CompareMidiToPreview(built.sequence, clip.sequence, next.events,
                openImport.midi.file, true, "G10 mini-chain OPEN DAWClip");
            auto reopenedTail = complete.events;
            reopenedTail.back().durationQN.reset();
            reopenedTail.back().openEnded = true;
            openInputFingerprint = g8EventListFingerprint(next.events);
            require(g8EventListFingerprint(reopenedTail) == openInputFingerprint,
                    "G10 mini-chain next input is not the completed DAWClip reopened at OPEN");
            std::error_code removeError;
            std::filesystem::remove(midiFile, removeError);

            usedMusic.insert(candidateFingerprint);
            usedSources.insert(selected->primaryTemplate);
            usedFamilies.insert(family);
            current = std::move(next);
            report << g8Tsv(std::string(startId)) << '\t' << round << "\tPASS\t" << inputLength << '\t'
                   << selectedFlat << '\t' << page << '\t' << firstPageSize << '\t'
                   << g8Tsv(selectedSource) << '\t' << keyText << '\t' << g8Tsv(family) << '\t'
                   << ending << '\t' << structureQualified << '\t' << structurePriority << '\t'
                   << g8Tsv(mechanism) << '\t' << profileReady << '\t' << contextFit << '\t'
                   << historySimilarity << '\t' << recentSimilarity << '\t'
                   << g8Tsv(candidateFingerprint) << '\t' << g8Tsv(completeFingerprint) << '\t'
                   << g8Tsv(openInputFingerprint) << '\t' << reinterpreted
                   << "\tactual DAWClip import and OPEN carry verified\n";
            require(static_cast<bool>(report), "cannot write G10 mini-chain MIDI result");
            ++completed;
        } catch (const std::exception& error) {
            std::error_code removeError;
            if (!midiFile.empty()) std::filesystem::remove(midiFile, removeError);
            report << g8Tsv(std::string(startId)) << '\t' << round << "\tFAIL\t" << inputLength << '\t'
                   << selectedFlat << '\t' << page << '\t' << firstPageSize << '\t'
                   << g8Tsv(selectedSource) << '\t' << keyText << '\t'
                   << g8Tsv(family) << '\t' << ending << '\t' << structureQualified << '\t'
                   << structurePriority << '\t' << g8Tsv(mechanism) << '\t' << profileReady << '\t'
                   << contextFit << '\t' << historySimilarity << '\t' << recentSimilarity << '\t'
                   << g8Tsv(candidateFingerprint) << '\t' << g8Tsv(completeFingerprint) << '\t'
                   << g8Tsv(openInputFingerprint) << '\t' << reinterpreted << '\t'
                   << g8Tsv(error.what()) << '\n';
            throw;
        }
    }
    require(completed == 3, "G10 mini-chain did not complete three actual MIDI imports");
    return "three normal mixed-order structure-qualified rounds exported and re-imported as DAWClip";
}

int runG10ProbeMidi(const std::filesystem::path& factoryPath,
                    const std::filesystem::path& roundsPath,
                    const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G10 MIDI probe Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G10 MIDI probe requires Factory version 8 or newer");
    require(std::filesystem::is_regular_file(roundsPath),
            "G10 MIDI probe requires the frozen G9 rounds TSV");
    std::filesystem::create_directories(outputDirectory);
    const CandidateIndex index(factory.templates);
    const auto failedProbePath = outputDirectory / "failed-probe.tsv";
    const auto roundsReportPath = outputDirectory / "g10-probe-midi-rounds.tsv";
    std::ofstream roundsReport(roundsReportPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(roundsReport), "cannot create G10 MIDI probe round report");
    roundsReport << "start_id\tround\tstatus\tinput_length\tvisible_rank\tpage\tfirst_page_size"
                    "\tsource\tkey\tfamily\tending\tstructure_qualified\tstructure_priority"
                    "\tmechanism\tprofile_ready\tcontext_fit\thistory_similarity\trecent_similarity"
                    "\tcandidate_music_fingerprint\tcomplete_import_fingerprint\topen_input_fingerprint"
                    "\tmidi_reinterpretation_count\tdetail\n";
    try {
        const auto result = g10RunMiniMidiChain(factory, index, roundsPath, outputDirectory,
            "V3_CYCLE_027", roundsReport, failedProbePath);
        roundsReport.flush();
        require(static_cast<bool>(roundsReport), "cannot finish G10 MIDI probe round report");
        std::cout << "g10_midi_probe start=V3_CYCLE_027 result=" << result
                  << " rounds=" << roundsReportPath.string() << '\n';
    } catch (...) {
        roundsReport.flush();
        std::cout << "g10_midi_probe start=V3_CYCLE_027 failed_probe="
                  << failedProbePath.string() << " rounds=" << roundsReportPath.string() << '\n';
        throw;
    }
    return 0;
}

int runG10Checks(const std::filesystem::path& factoryPath,
                 const std::filesystem::path& roundsPath,
                 const std::filesystem::path& baselinePath,
                 const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G10 check Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G10 checks require Factory version 8 or newer");
    require(std::filesystem::is_regular_file(roundsPath) && std::filesystem::is_regular_file(baselinePath),
            "G10 checks require the frozen G9 rounds and baseline candidate TSV files");
    std::filesystem::create_directories(outputDirectory);
    const std::array<std::string_view, 3> startIds{
        "V3_CYCLE_027", "V6_B3_02_054", "V8_01_006"};
    std::set<std::string> queryIds;
    for (const auto id : startIds) queryIds.emplace(std::string(id) + ":round-1");
    const auto baseline = g10ReadBaselineCandidates(baselinePath, queryIds);
    const auto recordedRounds = g10ReadSuccessfulFrozenRounds(roundsPath);
    const CandidateIndex index(factory.templates);
    const auto checksPath = outputDirectory / "g10-checks.tsv";
    const auto candidatesPath = outputDirectory / "g10-checks-candidates.tsv";
    const auto summaryPath = outputDirectory / "g10-checks-summary.tsv";
    const auto longInputPath = outputDirectory / "g10-checks-long-input.tsv";
    const auto midiPath = outputDirectory / "g10-checks-midi.tsv";
    std::ofstream checks(checksPath, std::ios::binary | std::ios::trunc);
    std::ofstream candidates(candidatesPath, std::ios::binary | std::ios::trunc);
    std::ofstream summary(summaryPath, std::ios::binary | std::ios::trunc);
    std::ofstream longInput(longInputPath, std::ios::binary | std::ios::trunc);
    std::ofstream midiReport(midiPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(checks) && static_cast<bool>(candidates) && static_cast<bool>(summary) &&
            static_cast<bool>(longInput) && static_cast<bool>(midiReport),
            "cannot create G10 focused-check output files");
    checks << "check\tstatus\tcase\tdetail\n";
    candidates << "query\tsample_order\tstart_id\tround\tinput_length\tvisible_rank\tpage"
                  "\ton_first_page\tsource\tkey\tchannel\tfamily\tending\tbase_qualified"
                  "\thard_constraints_passed\tstructure_qualified\tstructure_priority"
                  "\tstructure_reason\thistory_family_present\thistory_mechanism_present"
                  "\thistory_similarity\tranking_score\tnext_phrase_source_count"
                  "\tcontinuation\tcandidate_music_fingerprint\troute_profile_ready"
                  "\troute_tones\troute_bass\troute_tension\troute_palette\tcontext_fit"
                  "\troute_history_similarity\trecent_similarity\n";
    summary << "query\tsample_order\tstart_id\tbaseline_candidates\tcurrent_candidates"
               "\tfirst_page_size\tfirst_page_candidates\tall_profiles_ready\n";
    longInput << "";
    midiReport << "start_id\tround\tstatus\tinput_length\tvisible_rank\tpage\tfirst_page_size"
                  "\tsource\tkey\tfamily\tending\tstructure_qualified\tstructure_priority"
                  "\tmechanism\tprofile_ready\tcontext_fit\thistory_similarity\trecent_similarity"
                  "\tcandidate_music_fingerprint\tcomplete_import_fingerprint\topen_input_fingerprint"
                  "\tmidi_reinterpretation_count\tdetail\n";
    std::size_t failures{};
    const auto recordCheck = [&](std::string_view name, std::string_view caseId,
                                 const auto& action) {
        try {
            const auto detail = action();
            checks << name << "\tPASS\t" << g8Tsv(std::string(caseId)) << '\t'
                   << g8Tsv(detail) << '\n';
        } catch (const std::exception& error) {
            ++failures;
            checks << name << "\tFAIL\t" << g8Tsv(std::string(caseId)) << '\t'
                   << g8Tsv(error.what()) << '\n';
        }
    };

    for (const auto startId : startIds) {
        const auto queryId = std::string(startId) + ":round-1";
        const auto recorded = std::find_if(recordedRounds.begin(), recordedRounds.end(),
            [&](const auto& row) { return row.startId == startId && row.round == 1; });
        if (recorded == recordedRounds.end()) {
            ++failures;
            checks << "frozen-first-round\tFAIL\t" << queryId << "\tmissing G9 frozen round 1\n";
            continue;
        }

        std::optional<G10CheckState> state;
        try {
            const auto& source = g9FindTemplate(factory, startId);
            const auto key = source.rawChordKey.value_or(KeySignature{PitchClass::C, source.mode});
            const auto route = preview::templateRoute(source, key);
            require(static_cast<bool>(route), "G10 fixed source route failed: " + route.error);
            auto events = g10ParseFrozenFingerprint(recorded->queryInputFingerprint);
            require(events.size() == recorded->inputLength,
                    "G10 fixed query length differs from its frozen fingerprint");
            ImportedProgressionSession input;
            require(input.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
                    "cannot reconstruct G10 fixed first-round query");
            const auto before = input.events;
            const auto query = g8MakeQuery(input.events,
                std::pair{route.meterNumerator, route.meterDenominator});
            auto recommendations = g10DiagnosticRecommendations(query, index);
            g9RequireSameTimeline(before, input.events, "G10 check " + queryId);
            state.emplace(G10CheckState{std::move(input), query, std::move(recommendations),
                route.meterNumerator, route.meterDenominator});
        } catch (const std::exception& error) {
            ++failures;
            checks << "frozen-first-round\tFAIL\t" << queryId << '\t' << g8Tsv(error.what()) << '\n';
            continue;
        }

        const auto order = mixedInspirationOrder(state->recommendations);
        const auto currentByMusic = g10CandidatesByFingerprint(state->recommendations);
        const auto firstPageSize = inspirationFirstPageSize(state->recommendations,
                                                             ui::recommendationPageSize);
        bool allProfilesReady = true;
        std::size_t firstPageCandidates{};
        for (std::size_t visible = 0; visible < order.size(); ++visible) {
            const auto* candidate = g8CandidateAt(state->recommendations, order[visible]);
            require(candidate != nullptr, "G10 check mixed order contains an invalid candidate position");
            allProfilesReady &= candidate->routeProfile.ready;
            const auto page = ui::browsePageIndex(visible, firstPageSize);
            const bool onFirstPage = page == 0;
            firstPageCandidates += onFirstPage ? 1 : 0;
            candidates << g8Tsv(queryId) << '\t' << recorded->sampleOrder << '\t'
                       << g8Tsv(std::string(startId)) << "\t1\t" << recorded->inputLength << '\t'
                       << visible + 1 << '\t' << page + 1 << '\t' << (onFirstPage ? 1 : 0) << '\t'
                       << g8Tsv(candidate->primaryTemplate) << '\t' << g9KeyText(candidate->key) << '\t'
                       << recommendationChannelName(candidate->channel) << '\t'
                       << g8Tsv(g8SourceFamily(*candidate, index)) << '\t'
                       << phraseEndingName(candidate->ending) << '\t' << (candidate->baseQualified ? 1 : 0)
                       << '\t' << (candidate->hardConstraintsPassed ? 1 : 0) << '\t'
                       << (candidate->structureQualified ? 1 : 0) << '\t' << candidate->structurePriority << '\t'
                       << g8Tsv(candidate->structureReason) << '\t'
                       << (candidate->historyFamilyPresent ? 1 : 0) << '\t'
                       << (candidate->historyMechanismPresent ? 1 : 0) << '\t'
                       << std::fixed << std::setprecision(3) << candidate->historySimilarity << '\t'
                       << candidate->rankingScore << '\t' << candidate->nextPhraseSources.size() << '\t'
                       << g8Tsv(g8ContinuationLabels(*candidate)) << '\t'
                       << g8Tsv(g8CandidateMusicFingerprint(*candidate)) << '\t'
                       << (candidate->routeProfile.ready ? 1 : 0) << '\t'
                       << g10ArrayText(candidate->routeProfile.tones) << '\t'
                       << g10ArrayText(candidate->routeProfile.bass) << '\t'
                       << g10ArrayText(candidate->routeProfile.tension) << '\t'
                       << g10ArrayText(candidate->routeProfile.palette) << '\t'
                       << candidate->routeProfile.contextFit << '\t'
                       << candidate->routeProfile.historySimilarity << '\t'
                       << candidate->routeProfile.recentSimilarity << '\n';
        }
        const auto baselineFound = baseline.find(queryId);
        recordCheck("baseline-candidate-identity-and-gates", queryId, [&] {
            require(baselineFound != baseline.end(), "baseline candidates omit " + queryId);
            require(baselineFound->second.size() == currentByMusic.size(),
                    "actual candidate music count changed from the frozen diagnosis");
            for (const auto& [identity, candidate] : currentByMusic) {
                const auto old = baselineFound->second.find(identity);
                require(old != baselineFound->second.end(),
                        "actual candidate music identity is absent from baseline: " + identity);
                require(g10SameCandidateSnapshot(old->second, g10SnapshotCandidate(*candidate, index)),
                        "candidate source/key/structure qualification changed from baseline: " + identity);
                require(candidate->baseQualified && candidate->hardConstraintsPassed,
                        "mixed Inspiration order contains an unqualified candidate: " + identity);
            }
            return std::to_string(currentByMusic.size()) + " actual music identities and structure gates match baseline";
        });
        recordCheck("actual-profile-symbol-and-metadata-invariance", queryId, [&] {
            return g10CheckProfileInvariance(state->query, index, state->recommendations,
                                             state->meterNumerator, state->meterDenominator);
        });
        recordCheck("history-remains-soft", queryId, [&] {
            return g10CheckHistoryIsSoft(state->recommendations);
        });
        recordCheck("profile-readiness", queryId, [&] {
            require(allProfilesReady, "a returned actual candidate lacks its route profile");
            return std::to_string(firstPageSize) + " candidates on the dynamic first page";
        });
        summary << g8Tsv(queryId) << '\t' << recorded->sampleOrder << '\t' << g8Tsv(std::string(startId))
                << '\t' << (baselineFound == baseline.end() ? 0 : baselineFound->second.size()) << '\t'
                << currentByMusic.size() << '\t' << firstPageSize << '\t' << firstPageCandidates << '\t'
                << (allProfilesReady ? 1 : 0) << '\n';
    }

    recordCheck("dynamic-two-route-pagination", "legal-variants", [&] {
        return g10CheckTwoRoutePagination();
    });
    recordCheck("single-long-input", "V3_CYCLE_027-84-events", [&] {
        return g10CheckLongInput(factory, index, roundsPath, longInput);
    });
    for (const auto startId : startIds)
        recordCheck("three-round-DAWClip-import", startId, [&] {
            return g10RunMiniMidiChain(factory, index, roundsPath, outputDirectory,
                                       startId, midiReport);
        });
    checks.flush();
    candidates.flush();
    summary.flush();
    longInput.flush();
    midiReport.flush();
    require(static_cast<bool>(checks) && static_cast<bool>(candidates) && static_cast<bool>(summary) &&
            static_cast<bool>(longInput) && static_cast<bool>(midiReport),
            "cannot finish G10 focused-check reports");
    std::cout << "g10_checks failures=" << failures << " checks=" << checksPath.string()
              << " candidates=" << candidatesPath.string() << " summary=" << summaryPath.string()
              << " long_input=" << longInputPath.string() << " midi=" << midiPath.string() << '\n';
    require(failures == 0, "G10 focused checks failed; see " + checksPath.string());
    return 0;
}
} // namespace
