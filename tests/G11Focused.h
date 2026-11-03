#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
using namespace harmony;

std::size_t g11IndependentContourCount(
    const std::vector<const ContinuationCandidate*>& candidates) {
    std::vector<const ContinuationCandidate*> representatives;
    for (const auto* candidate : candidates) {
        const bool represented = std::any_of(representatives.begin(), representatives.end(),
            [&](const auto* prior) {
                if (candidate->routeProfile.ready && prior->routeProfile.ready)
                    return creativeRouteSimilarity(*candidate, *prior) >= 0.9f;
                return g8CandidateMusicFingerprint(*candidate) ==
                    g8CandidateMusicFingerprint(*prior);
            });
        if (!represented) representatives.push_back(candidate);
    }
    return representatives.size();
}

void g11WriteBranchingFields(std::ostream& output,
                             const ContinuationBranching& branching) {
    output << (branching.assessed ? 1 : 0) << '\t' << branching.legalRoutes << '\t'
           << branching.independentIdeas << '\t' << branching.families << '\t'
           << branching.centerDirections << '\t' << branching.freshIdeas << '\t'
           << std::fixed << std::setprecision(3) << branching.opportunity;
}

int runG11Diagnose(const std::filesystem::path& factoryPath,
                   const std::filesystem::path& roundsPath,
                   const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G11 diagnostic Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G11 diagnostics require Factory version 8 or newer");
    require(std::filesystem::is_regular_file(roundsPath),
            "cannot find frozen G10 rounds TSV: " + roundsPath.string());
    std::filesystem::create_directories(outputDirectory);

    const auto rounds = g10ReadSuccessfulFrozenRounds(roundsPath);
    const CandidateIndex index(factory.templates);
    const auto candidatesPath = outputDirectory / "g11-candidates.tsv";
    const auto gatesPath = outputDirectory / "g11-gates.tsv";
    const auto summaryPath = outputDirectory / "g11-summary.tsv";
    const auto parserNotePath = outputDirectory / "g11-parser-scope.txt";
    std::ofstream candidates(candidatesPath, std::ios::binary | std::ios::trunc);
    std::ofstream gates(gatesPath, std::ios::binary | std::ios::trunc);
    std::ofstream summary(summaryPath, std::ios::binary | std::ios::trunc);
    std::ofstream parserNote(parserNotePath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(candidates) && static_cast<bool>(gates) &&
            static_cast<bool>(summary) && static_cast<bool>(parserNote),
            "cannot create G11 diagnostic output files");

    parserNote << "This diagnostic reconstructs only frozen rounds 2 and 3 from the recorded chord-level "
                  "fingerprints. It recovers root, explicit bass, quality, actual pitch-class set, and timing; "
                  "it does not recover hidden original note assignments, MIDI note voicings, or source metadata. "
                  "Candidate profiles are recalculated by the current recommendation path from those reconstructed "
                  "inputs. Independent contour count greedily groups route profiles at similarity >= 0.90, matching "
                  "the existing Inspiration first-page distinction rule. These re-queries and creative trials are "
                  "diagnostics, not newly completed RCR rounds or audible/DAW acceptance evidence.\n";
    candidates << "query\tsample_order\tstart_id\tround\tinput_length\tquery_actualfp"
                  "\tvisible_rank\tpage\ton_first_page\tsource\tkey\tchannel\tfamily\tending"
                  "\tmechanism\tbase_qualified\thard_constraints_passed\tstructure_qualified"
                  "\tstructure_priority\tstructure_reason\texploratory\tcomplete_source_phrase"
                  "\treviewed_ending\tsource_end_preserved"
                  "\tprofile_ready\troute_tones\troute_bass\troute_tension\troute_palette"
                  "\tcontext_fit\thistory_family_present\thistory_mechanism_present"
                  "\thistory_similarity\troute_history_similarity\tdevelopment_history_similarity"
                  "\trecent_similarity\tactualfp\n";
    gates << "query\tsample_order\tstart_id\tround\tsource\tkey\tdisposition\tboundary\n";
    summary << "query\tsample_order\tstart_id\tround\tinput_length\tqualified_candidate_count"
               "\tunique_family_count\tunique_actualfp_count\tindependent_contour_count_0.90\n";

    std::size_t contexts{};
    std::size_t totalCandidates{};
    std::set<std::string> allFamilies, allActualFingerprints;
    std::vector<ContinuationCandidate> allContourCandidates;
    for (std::size_t chainStart = 0; chainStart < rounds.size(); chainStart += 3) {
        const auto& first = rounds[chainStart];
        require(first.round == 1 && rounds[chainStart + 1].round == 2 &&
                rounds[chainStart + 2].round == 3 &&
                rounds[chainStart + 1].startId == first.startId &&
                rounds[chainStart + 2].startId == first.startId,
                "frozen G10 rounds are not grouped as a three-round successful chain: " + first.startId);

        const auto& startSource = g9FindTemplate(factory, first.startId);
        const auto startKey = startSource.rawChordKey.value_or(
            KeySignature{PitchClass::C, startSource.mode});
        const auto startRoute = preview::templateRoute(startSource, startKey);
        require(static_cast<bool>(startRoute),
                "G11 frozen start route failed: " + first.startId + ": " + startRoute.error);

        for (std::size_t roundIndex = 1; roundIndex < 3; ++roundIndex) {
            const auto& record = rounds[chainStart + roundIndex];
            const auto& previous = rounds[chainStart + roundIndex - 1];
            auto events = g10ParseFrozenFingerprint(record.queryInputFingerprint);
            require(events.size() == record.inputLength,
                    "G11 query length differs from frozen round fingerprint: " + record.startId);

            const auto previousComplete =
                g10ParseCompleteFrozenFingerprint(previous.actualImportFingerprint);
            g8RequireRecordedCarry(previousComplete, events);
            auto reopenedTail = previousComplete;
            reopenedTail.back().durationQN.reset();
            reopenedTail.back().openEnded = true;
            require(g8EventListFingerprint(reopenedTail) == record.queryInputFingerprint,
                    "G11 frozen next query changed previous-round MIDI music or duration identity: " +
                        record.startId);

            ImportedProgressionSession input;
            require(input.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
                    "cannot reconstruct G11 frozen query input: " + record.startId);
            const auto inputBefore = input.events;
            const auto query = g8MakeQuery(input.events,
                std::pair{startRoute.meterNumerator, startRoute.meterDenominator});
            const auto recommendations = g10DiagnosticRecommendations(query, index);
            const auto queryId = record.startId + ":round-" + std::to_string(record.round);
            g9RequireSameTimeline(inputBefore, input.events, "G11 " + queryId);

            std::set<std::tuple<std::string, int, int, std::string>> uniqueTrials;
            for (const auto& trial : recommendations.creativeTrials) {
                const auto identity = std::tuple(trial.source, static_cast<int>(trial.key.tonic),
                    static_cast<int>(trial.key.mode), trial.disposition);
                if (!uniqueTrials.insert(identity).second) continue;
                gates << g8Tsv(queryId) << '\t' << record.sampleOrder << '\t'
                      << g8Tsv(record.startId) << '\t' << record.round << '\t'
                      << g8Tsv(trial.source) << '\t' << g9KeyText(trial.key) << '\t'
                      << g8Tsv(trial.disposition) << '\t' << std::fixed << std::setprecision(3)
                      << trial.boundary << '\n';
            }

            const auto order = mixedInspirationOrder(recommendations);
            const auto firstPageSize = inspirationFirstPageSize(
                recommendations, ui::recommendationPageSize);
            std::vector<const ContinuationCandidate*> eligible;
            std::set<std::string> families, actualFingerprints;
            for (std::size_t visible = 0; visible < order.size(); ++visible) {
                const auto* candidate = g8CandidateAt(recommendations, order[visible]);
                require(candidate != nullptr,
                        "G11 mixed order contains an invalid candidate position: " + queryId);
                const std::set<std::string> noUsedMusic, noUsedFamilies;
                const std::set<TemplateID> noUsedSources;
                if (!g8Eligible(*candidate, index, noUsedMusic, noUsedSources,
                                noUsedFamilies, true))
                    continue;

                const auto family = g8SourceFamily(*candidate, index);
                const auto actualFingerprint = g8CandidateMusicFingerprint(*candidate);
                const auto page = ui::browsePageIndex(visible, firstPageSize) + 1;
                candidates << g8Tsv(queryId) << '\t' << record.sampleOrder << '\t'
                           << g8Tsv(record.startId) << '\t' << record.round << '\t'
                           << record.inputLength << '\t' << g8Tsv(record.queryInputFingerprint) << '\t'
                           << visible + 1 << '\t' << page << '\t'
                           << (page == 1 ? 1 : 0) << '\t' << g8Tsv(candidate->primaryTemplate) << '\t'
                           << g9KeyText(candidate->key) << '\t'
                           << recommendationChannelName(candidate->channel) << '\t'
                           << g8Tsv(family) << '\t'
                           << phraseEndingName(candidate->ending) << '\t'
                           << g8Tsv(candidate->developmentMechanism) << '\t'
                           << (candidate->baseQualified ? 1 : 0) << '\t'
                           << (candidate->hardConstraintsPassed ? 1 : 0) << '\t'
                           << (candidate->structureQualified ? 1 : 0) << '\t'
                           << candidate->structurePriority << '\t'
                           << g8Tsv(candidate->structureReason) << '\t'
                           << (candidate->exploratory ? 1 : 0) << '\t'
                           << (candidate->completeSourcePhrase ? 1 : 0) << '\t'
                           << (candidate->reviewedEnding ? 1 : 0) << '\t'
                           << (candidate->sourceEndPreserved ? 1 : 0) << '\t'
                           << (candidate->routeProfile.ready ? 1 : 0) << '\t'
                           << g10ArrayText(candidate->routeProfile.tones) << '\t'
                           << g10ArrayText(candidate->routeProfile.bass) << '\t'
                           << g10ArrayText(candidate->routeProfile.tension) << '\t'
                           << g10ArrayText(candidate->routeProfile.palette) << '\t'
                           << std::fixed << std::setprecision(3)
                           << candidate->routeProfile.contextFit << '\t'
                           << (candidate->historyFamilyPresent ? 1 : 0) << '\t'
                           << (candidate->historyMechanismPresent ? 1 : 0) << '\t'
                           << candidate->historySimilarity << '\t'
                           << candidate->routeProfile.historySimilarity << '\t'
                           << candidate->routeProfile.developmentHistorySimilarity << '\t'
                           << candidate->routeProfile.recentSimilarity << '\t'
                           << g8Tsv(actualFingerprint) << '\n';
                eligible.push_back(candidate);
                families.insert(family);
                actualFingerprints.insert(actualFingerprint);
                allFamilies.insert(family);
                allActualFingerprints.insert(actualFingerprint);
                allContourCandidates.push_back(*candidate);
            }

            totalCandidates += eligible.size();
            summary << g8Tsv(queryId) << '\t' << record.sampleOrder << '\t'
                    << g8Tsv(record.startId) << '\t' << record.round << '\t'
                    << record.inputLength << '\t' << eligible.size() << '\t'
                    << families.size() << '\t' << actualFingerprints.size() << '\t'
                    << g11IndependentContourCount(eligible) << '\n';
            ++contexts;
        }
    }

    std::vector<const ContinuationCandidate*> allContourPointers;
    allContourPointers.reserve(allContourCandidates.size());
    for (const auto& candidate : allContourCandidates)
        allContourPointers.push_back(&candidate);
    summary << "TOTAL\t-\t-\t-\t-\t" << totalCandidates << '\t' << allFamilies.size()
            << '\t' << allActualFingerprints.size() << '\t'
            << g11IndependentContourCount(allContourPointers) << '\n';
    candidates.flush();
    gates.flush();
    summary.flush();
    parserNote.flush();
    require(static_cast<bool>(candidates) && static_cast<bool>(gates) &&
            static_cast<bool>(summary) && static_cast<bool>(parserNote),
            "cannot finish G11 diagnostic outputs");
    require(contexts == 24, "G11 diagnosis did not process exactly the 24 frozen round-2/round-3 contexts");
    std::cout << "g11_diagnose frozen_success_chains=12 contexts=" << contexts
              << " qualified_candidates=" << totalCandidates
              << " candidates=" << candidatesPath.string()
              << " gates=" << gatesPath.string() << " summary=" << summaryPath.string()
              << " parser_scope=" << parserNotePath.string() << '\n';
    return 0;
}

int runG11Branches(const std::filesystem::path& factoryPath,
                   const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G11 branch-probe Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G11 branch probes require Factory version 8 or newer");
    std::filesystem::create_directories(outputDirectory);

    const CandidateIndex index(factory.templates);
    const std::array<std::string_view, 3> startIds{
        "V3_CYCLE_027", "V6_B3_02_054", "V8_01_006"};
    const auto branchesPath = outputDirectory / "g11-branches.tsv";
    const auto candidatesPath = outputDirectory / "g11-branches-candidates.tsv";
    const auto midiDirectory = outputDirectory / ".g11-branches-midi";
    std::filesystem::create_directories(midiDirectory);
    std::ofstream branches(branchesPath, std::ios::binary | std::ios::trunc);
    std::ofstream candidateReport(candidatesPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(branches) && static_cast<bool>(candidateReport),
            "cannot create G11 branch-probe outputs");
    branches << "start_id\tbranch\tA_event_count\tA_open_actualfp\tA_first_page_size"
                "\tB_rank\tB_page\tB_source\tB_key\tB_family\tB_channel\tB_ending"
                "\tB_profile_similarity_to_branch1\tB_actualfp\tB_route_tones\tB_route_bass"
                "\tB_route_tension\tB_route_palette\tB_context_fit\tB_boundary\tB_direction"
                "\tB_history_similarity"
                "\tB_route_history_similarity\tB_development_history_similarity"
                "\tB_recent_similarity\tDAWClip_bytes"
                "\tB_branching_assessed\tB_branching_legal_routes\tB_branching_independent_ideas"
                "\tB_branching_families\tB_branching_center_directions\tB_branching_fresh_ideas"
                "\tB_branching_opportunity"
                "\tcomplete_import_actualfp\tOPEN_import_actualfp\tMIDI_reinterpretation_count"
                "\tC_event_count\tC_qualified_total\tC_unique_source_count\tC_unique_family_count"
                "\tC_unique_actualfp_count\tC_independent_contour_count_0.90"
                "\tC_first_page_qualified_count\tC_sources\tC_families\n";
    candidateReport << "start_id\tbranch\tB_rank\tB_source\tB_actualfp\tC_rank\tC_page"
                       "\tC_on_first_page\tC_source\tC_key\tC_channel\tC_family\tC_ending"
                       "\tC_mechanism\tC_structure_priority\tC_structure_reason\tC_profile_ready"
                       "\tC_route_tones\tC_route_bass\tC_route_tension\tC_route_palette"
                       "\tC_context_fit\tC_boundary\tC_direction"
                       "\tC_history_family_present\tC_history_mechanism_present"
                       "\tC_history_similarity\tC_route_history_similarity"
                       "\tC_development_history_similarity\tC_recent_similarity"
                       "\tC_branching_assessed\tC_branching_legal_routes\tC_branching_independent_ideas"
                       "\tC_branching_families\tC_branching_center_directions\tC_branching_fresh_ideas"
                       "\tC_branching_opportunity"
                       "\tC_actualfp\n";

    const auto networkPath = outputDirectory / "network.tsv";
    std::ofstream network(networkPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(network), "cannot create G11 continuation network output");
    network << "source_id\tsource_key\ttarget_id\ttarget_key\tboundary_score\n";
    for (const auto& link : index.continuationLinks()) {
        require(link.sourceTemplate < index.templates().size() &&
                link.nextTemplate < index.templates().size(),
                "G11 continuation network contains an invalid template index");
        network << g8Tsv(index.templates()[link.sourceTemplate].id) << '\t'
                << g9KeyText(link.sourceKey) << '\t'
                << g8Tsv(index.templates()[link.nextTemplate].id) << '\t'
                << g9KeyText(link.nextKey) << '\t' << std::fixed << std::setprecision(3)
                << link.boundaryScore << '\n';
    }

    std::size_t branchCount{};
    for (const auto startId : startIds) {
        const auto& start = g9FindTemplate(factory, startId);
        const auto startKey = start.rawChordKey.value_or(KeySignature{PitchClass::C, start.mode});
        const auto startRoute = preview::templateRoute(start, startKey);
        require(static_cast<bool>(startRoute),
                "G11 branch A route failed: " + std::string(startId) + ": " + startRoute.error);
        require(!startRoute.imported.events.empty(),
                "G11 branch A has no imported Factory events: " + std::string(startId));
        const auto startPreview = midi::previewFromTemplate(start, startKey, 120.0);
        require(static_cast<bool>(startPreview),
                "G11 branch A Preview failed: " + std::string(startId) + ": " + startPreview.error);
        const auto routePreview = preview::buildSequence(startRoute.imported, nullptr, 120.0);
        require(static_cast<bool>(routePreview) &&
                g8SamePreview(startPreview.sequence, routePreview.sequence),
                "G11 Factory A playback and imported route differ: " + std::string(startId));

        ImportedProgressionSession aInput;
        require(aInput.replace(startRoute.imported.events, TimelineCoordinateMode::RelativeToSelection),
                "cannot create G11 Factory A input: " + std::string(startId));
        aInput.events.back().durationQN.reset();
        aInput.events.back().openEnded = true;
        const auto aFingerprint = g8EventListFingerprint(aInput.events);
        const auto meter = midi::Meter{startRoute.meterNumerator, startRoute.meterDenominator};
        const auto aQuery = g8MakeQuery(aInput.events,
            std::pair{meter.numerator, meter.denominator});
        const auto aRecommendations = g8NormalRecommendations(aQuery, index);
        require(g8EventListFingerprint(aInput.events) == aFingerprint,
                "G11 A recommendation rewrote Factory input identity: " + std::string(startId));
        const auto aOrder = mixedInspirationOrder(aRecommendations);
        const auto aFirstPageSize = inspirationFirstPageSize(
            aRecommendations, ui::recommendationPageSize);
        const std::set<std::string> aUsedMusic{g8EventListFingerprint(startPreview.sequence)};
        const std::set<TemplateID> aUsedSources{start.id};
        const std::set<std::string> noUsedFamilies;
        struct G11BranchChoice {
            const ContinuationCandidate* candidate{};
            std::size_t flatRank{};
            std::string actualFingerprint;
        };
        std::vector<G11BranchChoice> choices;
        for (std::size_t visible = 0; visible < aOrder.size() && visible < aFirstPageSize; ++visible) {
            const auto* candidate = g8CandidateAt(aRecommendations, aOrder[visible]);
            require(candidate != nullptr,
                    "G11 A mixed order contains an invalid candidate: " + std::string(startId));
            if (!g8Eligible(*candidate, index, aUsedMusic, aUsedSources, noUsedFamilies, true) ||
                !candidate->routeProfile.ready)
                continue;
            const auto actualFingerprint = g8CandidateMusicFingerprint(*candidate);
            if (std::any_of(choices.begin(), choices.end(), [&](const auto& prior) {
                    return prior.actualFingerprint == actualFingerprint ||
                        creativeRouteSimilarity(*candidate, *prior.candidate) >= 0.90f;
                }))
                continue;
            choices.push_back({candidate, visible, actualFingerprint});
            if (choices.size() == 2) break;
        }
        require(choices.size() == 2,
                "G11 A first page lacks two eligible, distinct actual profiles below 0.90 similarity: " +
                    std::string(startId));

        for (std::size_t branch = 0; branch < choices.size(); ++branch) {
            const auto& choice = choices[branch];
            const auto& selected = *choice.candidate;
            const auto selectedFamily = g8SourceFamily(selected, index);
            const auto selectedKey = g9KeyText(selected.key);
            const auto profileSimilarity = branch == 0 ? 1.0f :
                creativeRouteSimilarity(selected, *choices.front().candidate);
            const auto built = preview::buildSequence(aInput, &selected, 120.0);
            require(static_cast<bool>(built),
                    "G11 branch B Preview failed: " + built.error);
            const midi::CandidateExportContext exportContext{aInput, 120.0, meter, selected.key,
                midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
            const auto payload = midi::candidatePayload(exportContext, selected);
            require(static_cast<bool>(payload) && !payload.payload.dawClipBytes.empty(),
                    "G11 branch DAWClip payload failed: " + payload.error);
            const auto clip = midi::buildClip(built.sequence, midi::ArrangementMode::VoiceLed,
                midi::ExportScope::FullPhrase, meter, selected.key, selected.intent);
            require(static_cast<bool>(clip), "G11 branch MIDI Clip failed: " + clip.error);
            midi::ExportConfig dawConfig;
            dawConfig.profile = midi::ExportProfile::DAWClip;
            const auto dawBytes = midi::writeToMemory(clip.sequence, dawConfig);
            require(static_cast<bool>(dawBytes) && dawBytes.bytes == payload.payload.dawClipBytes,
                    "G11 branch DAWClip differs from Preview-derived production payload");

            const auto midiFile = midiDirectory /
                (std::string(startId) + "-branch-" + std::to_string(branch + 1) + ".mid");
            std::string writeError;
            require(midi::writeToFile(payload.payload.dawClipBytes, midiFile, writeError),
                    "G11 branch DAWClip file write failed: " + writeError);
            try {
                const auto completeImport = midi::importFile(midiFile, false);
                require(static_cast<bool>(completeImport),
                        "G11 complete DAWClip import failed: " + completeImport.extraction.error);
                ImportedProgressionSession complete;
                require(midi::applyImport(completeImport, complete),
                        "G11 applyImport failed for complete DAWClip");
                const auto midiSummary = g8CompareMidiToPreview(built.sequence, clip.sequence,
                    complete.events, completeImport.midi.file, false, "G11 complete DAWClip");
                const auto completeFingerprint = g8EventListFingerprint(complete.events);

                const auto openImport = midi::importFile(midiFile, true);
                require(static_cast<bool>(openImport),
                        "G11 OPEN DAWClip import failed: " + openImport.extraction.error);
                ImportedProgressionSession next;
                require(midi::applyImport(openImport, next),
                        "G11 applyImport failed for OPEN DAWClip");
                (void)g8CompareMidiToPreview(built.sequence, clip.sequence, next.events,
                    openImport.midi.file, true, "G11 OPEN DAWClip");
                auto reopenedTail = complete.events;
                reopenedTail.back().durationQN.reset();
                reopenedTail.back().openEnded = true;
                const auto openFingerprint = g8EventListFingerprint(next.events);
                require(g8EventListFingerprint(reopenedTail) == openFingerprint,
                        "G11 C input is not the complete DAWClip re-imported at OPEN");

                const auto cInputBefore = next.events;
                const auto cQuery = g8MakeQuery(next.events,
                    std::pair{meter.numerator, meter.denominator});
                const auto cRecommendations = g8NormalRecommendations(cQuery, index);
                g9RequireSameTimeline(cInputBefore, next.events, "G11 C query");
                const auto cOrder = mixedInspirationOrder(cRecommendations);
                const auto cFirstPageSize = inspirationFirstPageSize(
                    cRecommendations, ui::recommendationPageSize);
                std::set<std::string> usedMusic = aUsedMusic;
                usedMusic.insert(choice.actualFingerprint);
                std::set<TemplateID> usedSources = aUsedSources;
                usedSources.insert(selected.primaryTemplate);
                const std::set<std::string> usedFamilies{selectedFamily};
                std::vector<const ContinuationCandidate*> cEligible;
                std::set<std::string> cActualFingerprints, cFamilies;
                std::set<TemplateID> cSources;
                std::size_t cFirstPageEligible{};
                for (std::size_t visible = 0; visible < cOrder.size(); ++visible) {
                    const auto* candidate = g8CandidateAt(cRecommendations, cOrder[visible]);
                    require(candidate != nullptr,
                            "G11 C mixed order contains an invalid candidate: " + std::string(startId));
                    if (!g8Eligible(*candidate, index, usedMusic, usedSources, usedFamilies, true))
                        continue;
                    const auto family = g8SourceFamily(*candidate, index);
                    const auto actualFingerprint = g8CandidateMusicFingerprint(*candidate);
                    const auto page = ui::browsePageIndex(visible, cFirstPageSize) + 1;
                    if (page == 1) ++cFirstPageEligible;
                    cEligible.push_back(candidate);
                    cActualFingerprints.insert(actualFingerprint);
                    cFamilies.insert(family);
                    cSources.insert(candidate->primaryTemplate);
                    candidateReport << g8Tsv(std::string(startId)) << '\t' << branch + 1 << '\t'
                                    << choice.flatRank + 1 << '\t'
                                    << g8Tsv(selected.primaryTemplate) << '\t'
                                    << g8Tsv(choice.actualFingerprint) << '\t' << visible + 1 << '\t'
                                    << page << '\t' << (page == 1 ? 1 : 0) << '\t'
                                    << g8Tsv(candidate->primaryTemplate) << '\t'
                                    << g9KeyText(candidate->key) << '\t'
                                    << recommendationChannelName(candidate->channel) << '\t'
                                    << g8Tsv(family) << '\t' << phraseEndingName(candidate->ending) << '\t'
                                    << g8Tsv(candidate->developmentMechanism) << '\t'
                                    << candidate->structurePriority << '\t'
                                    << g8Tsv(candidate->structureReason) << '\t'
                                    << (candidate->routeProfile.ready ? 1 : 0) << '\t'
                                    << g10ArrayText(candidate->routeProfile.tones) << '\t'
                                    << g10ArrayText(candidate->routeProfile.bass) << '\t'
                                    << g10ArrayText(candidate->routeProfile.tension) << '\t'
                                    << g10ArrayText(candidate->routeProfile.palette) << '\t'
                                    << std::fixed << std::setprecision(3)
                                    << candidate->routeProfile.contextFit << '\t'
                                    << candidate->creativeQuality.boundary << '\t'
                                    << g6DirectionName(candidate->direction) << '\t'
                                    << (candidate->historyFamilyPresent ? 1 : 0) << '\t'
                                    << (candidate->historyMechanismPresent ? 1 : 0) << '\t'
                                    << candidate->historySimilarity << '\t'
                                    << candidate->routeProfile.historySimilarity << '\t'
                                    << candidate->routeProfile.developmentHistorySimilarity << '\t'
                                    << candidate->routeProfile.recentSimilarity << '\t';
                    g11WriteBranchingFields(candidateReport, candidate->branching);
                    candidateReport << '\t' << g8Tsv(actualFingerprint) << '\n';
                }
                require(static_cast<bool>(candidateReport), "cannot write G11 C candidates report");
                const auto joinSet = [](const auto& values) {
                    std::string joined;
                    for (const auto& value : values) {
                        if (!joined.empty()) joined += ';';
                        joined += value;
                    }
                    return joined;
                };
                const auto aPage = ui::browsePageIndex(choice.flatRank, aFirstPageSize) + 1;
                branches << g8Tsv(std::string(startId)) << '\t' << branch + 1 << '\t'
                         << aInput.events.size() << '\t' << g8Tsv(aFingerprint) << '\t'
                         << aFirstPageSize << '\t' << choice.flatRank + 1 << '\t' << aPage << '\t'
                         << g8Tsv(selected.primaryTemplate) << '\t' << selectedKey << '\t'
                         << g8Tsv(selectedFamily) << '\t'
                         << recommendationChannelName(selected.channel) << '\t'
                         << phraseEndingName(selected.ending) << '\t'
                         << std::fixed << std::setprecision(3) << profileSimilarity << '\t'
                         << g8Tsv(choice.actualFingerprint) << '\t'
                         << g10ArrayText(selected.routeProfile.tones) << '\t'
                         << g10ArrayText(selected.routeProfile.bass) << '\t'
                         << g10ArrayText(selected.routeProfile.tension) << '\t'
                         << g10ArrayText(selected.routeProfile.palette) << '\t'
                         << selected.routeProfile.contextFit << '\t'
                         << selected.creativeQuality.boundary << '\t'
                         << g6DirectionName(selected.direction) << '\t'
                         << selected.historySimilarity << '\t'
                         << selected.routeProfile.historySimilarity << '\t'
                         << selected.routeProfile.developmentHistorySimilarity << '\t'
                         << selected.routeProfile.recentSimilarity << '\t'
                         << dawBytes.bytes.size() << '\t';
                g11WriteBranchingFields(branches, selected.branching);
                branches << '\t'
                         << g8Tsv(completeFingerprint) << '\t'
                         << g8Tsv(openFingerprint) << '\t' << midiSummary.reinterpretationCount << '\t'
                         << next.events.size() << '\t' << cEligible.size() << '\t' << cSources.size()
                         << '\t' << cFamilies.size() << '\t' << cActualFingerprints.size() << '\t'
                         << g11IndependentContourCount(cEligible) << '\t' << cFirstPageEligible << '\t'
                         << g8Tsv(joinSet(cSources)) << '\t' << g8Tsv(joinSet(cFamilies)) << '\n';
                require(static_cast<bool>(branches), "cannot write G11 branch summary");
                std::error_code removeError;
                std::filesystem::remove(midiFile, removeError);
            } catch (...) {
                std::error_code removeError;
                std::filesystem::remove(midiFile, removeError);
                throw;
            }
            ++branchCount;
        }
    }
    branches.flush();
    candidateReport.flush();
    network.flush();
    require(static_cast<bool>(branches) && static_cast<bool>(candidateReport) &&
            static_cast<bool>(network),
            "cannot finish G11 branch-probe outputs");
    require(branchCount == 6, "G11 branch probes did not complete exactly six one-hop branches");
    std::cout << "g11_branches representative_starts=3 branches=" << branchCount
              << " summary=" << branchesPath.string()
              << " candidates=" << candidatesPath.string()
              << " network=" << networkPath.string() << '\n';
    return 0;
}

using G11BaselineRanks = std::map<std::string, std::map<std::string, std::size_t>>;

G11BaselineRanks g11ReadBaselineRanks(const std::filesystem::path& path,
                                      const std::set<std::string>& wantedQueries) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot open frozen G11 candidate baseline: " + path.string());
    std::string line;
    require(static_cast<bool>(std::getline(input, line)), "frozen G11 candidate baseline is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto header = g8Split(line, '\t');
    std::unordered_map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < header.size(); ++i)
        require(columns.emplace(header[i], i).second,
                "duplicate column in frozen G11 candidate baseline");
    for (const auto name : {"query", "actualfp", "visible_rank"})
        require(columns.contains(name),
                "frozen G11 candidate baseline lacks column " + std::string(name));
    const auto cell = [&](const std::vector<std::string>& cells, std::string_view name)
        -> const std::string& {
        require(cells.size() == header.size(), "frozen G11 candidate row has an invalid column count");
        return cells[columns.at(std::string(name))];
    };

    G11BaselineRanks result;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto cells = g8Split(line, '\t');
        const auto& query = cell(cells, "query");
        if (!wantedQueries.contains(query)) continue;
        const auto& fingerprint = cell(cells, "actualfp");
        const auto rank = g8ParseInt(cell(cells, "visible_rank"), "G11 baseline visible rank");
        require(!fingerprint.empty() && rank > 0,
                "frozen G11 baseline candidate has an empty identity or invalid rank");
        require(result[query].emplace(fingerprint, static_cast<std::size_t>(rank)).second,
                "frozen G11 baseline repeats an actual candidate identity: " + query);
    }
    require(input.eof(), "cannot finish reading frozen G11 candidate baseline");
    for (const auto& query : wantedQueries)
        require(result.contains(query), "frozen G11 baseline has no qualified candidates for " + query);
    return result;
}

ContinuationCandidate g11SyntheticCandidate(std::string id,
                                            std::vector<std::string> labels,
                                            float boundary,
                                            float contextFit,
                                            int profileKind,
                                            float opportunity) {
    ContinuationCandidate candidate;
    candidate.id = id;
    candidate.primaryTemplate = id;
    candidate.channel = RecommendationChannel::PhraseTransition;
    candidate.direction = CreativeDirection::Rich;
    candidate.baseQualified = true;
    candidate.hardConstraintsPassed = true;
    candidate.structureQualified = true;
    candidate.completeSourcePhrase = true;
    candidate.sourceEndPreserved = true;
    candidate.reviewedEnding = true;
    candidate.ending = PhraseEnding::OpenUnresolved;
    candidate.rankingScore = 80.f;
    candidate.creativeQuality.boundary = boundary;
    candidate.creativeQuality.tier = 2;
    candidate.creativeQuality.qualified = true;
    candidate.routeProfile.ready = true;
    candidate.routeProfile.tones.fill(static_cast<std::uint16_t>(profileKind == 0 ? 0x003 : 0x180));
    candidate.routeProfile.bass.fill(profileKind == 0 ? 0.f : 6.f);
    candidate.routeProfile.tension.fill(profileKind == 0 ? .1f : .9f);
    candidate.routeProfile.palette[profileKind == 0 ? 0 : 6] = 1.f;
    candidate.routeProfile.contextFit = contextFit;
    candidate.routeProfile.historySimilarity = .2f;
    candidate.routeProfile.developmentHistorySimilarity = .2f;
    candidate.routeProfile.recentSimilarity = .2f;
    candidate.branching.assessed = true;
    candidate.branching.legalRoutes = opportunity > 0.f ? 4 : 1;
    candidate.branching.independentIdeas = opportunity > 0.f ? 3 : 1;
    candidate.branching.families = opportunity > 0.f ? 3 : 1;
    candidate.branching.centerDirections = opportunity > 0.f ? 2 : 1;
    candidate.branching.freshIdeas = opportunity > 0.f ? 3 : 0;
    candidate.branching.opportunity = opportunity;
    for (auto& label : labels) {
        ConcreteChordEvent event;
        event.label = std::move(label);
        event.durationQN = 4.0;
        candidate.continuation.push_back(std::move(event));
    }
    return candidate;
}

const ContinuationCandidate* g11FirstMixedCandidate(
    const RecommendationSet& set, std::string_view source) {
    const auto order = mixedInspirationOrder(set);
    if (order.empty()) return nullptr;
    const auto* candidate = g8CandidateAt(set, order.front());
    return candidate && candidate->primaryTemplate == std::string(source) ? candidate : nullptr;
}

std::string g11CheckFutureOpportunityRanking() {
    const auto orderStartsWith = [](RecommendationSet& set, std::string_view expected) {
        return g11FirstMixedCandidate(set, expected) != nullptr;
    };

    auto strong = g11SyntheticCandidate("strong", {"C", "F", "G", "Am"}, .90f, .90f, 0, 0.f);
    auto structurallyUnqualified = g11SyntheticCandidate(
        "unqualified", {"D", "G", "A", "Bm"}, .90f, .90f, 1, 1.f);
    structurallyUnqualified.structureQualified = false;
    RecommendationSet structureCase;
    structureCase.groups[1] = {structurallyUnqualified, strong};
    require(orderStartsWith(structureCase, "strong"),
            "branching opportunity moved a structurally unqualified candidate ahead of a qualified strong route");

    auto weaker = g11SyntheticCandidate("weaker", {"D", "G", "A", "Bm"}, .75f, .75f, 1, 1.f);
    RecommendationSet currentFitCase;
    currentFitCase.groups[1] = {weaker, strong};
    require(orderStartsWith(currentFitCase, "strong"),
            "branching opportunity moved a clearly weaker current connection ahead of a strong route");

    auto richFuture = g11SyntheticCandidate("rich-future", {"C", "F", "G", "Am"}, .80f, .80f, 0, 1.f);
    auto plainFuture = g11SyntheticCandidate("plain-future", {"D", "G", "A", "Bm"}, .80f, .80f, 1, 0.f);
    RecommendationSet comparableCase;
    comparableCase.groups[1] = {plainFuture, richFuture};
    require(orderStartsWith(comparableCase, "rich-future"),
            "future opportunity did not influence order between equal-quality, comparable-current routes");

    auto freshHistory = g11SyntheticCandidate("fresh-history", {"C", "F", "G", "Am"},
        .80f, .80f, 0, 0.f);
    auto repeatedHistory = g11SyntheticCandidate("repeated-history", {"D", "G", "A", "Bm"},
        .80f, .80f, 1, 0.f);
    repeatedHistory.routeProfile.developmentHistorySimilarity = 1.f;
    RecommendationSet historyCase;
    historyCase.groups[1] = {repeatedHistory, freshHistory};
    const auto historyOrder = mixedInspirationOrder(historyCase);
    require(historyOrder.size() == 2,
            "development-history ranking removed an actual route from full pagination");
    const auto* firstHistoryRoute = g8CandidateAt(historyCase, historyOrder.front());
    require(firstHistoryRoute && firstHistoryRoute->primaryTemplate == "fresh-history",
            "development history did not softly prefer a fresh route while preserving the high-similarity route");

    auto repeatedA = g11SyntheticCandidate("repeated-a", {"C", "F", "G", "Am"}, .80f, .80f, 0, 1.f);
    auto repeatedB = g11SyntheticCandidate("repeated-b", {"D", "G", "A", "Bm"}, .80f, .80f, 0, 1.f);
    repeatedA.routeProfile.historySimilarity = repeatedB.routeProfile.historySimilarity = 1.f;
    repeatedA.routeProfile.developmentHistorySimilarity =
        repeatedB.routeProfile.developmentHistorySimilarity = 1.f;
    require(g8CandidateMusicFingerprint(repeatedA) != g8CandidateMusicFingerprint(repeatedB) &&
            creativeRouteSimilarity(repeatedA, repeatedB) >= .90f,
            "synthetic repeated-contour case does not contain distinct high-similarity routes");
    RecommendationSet repeatedCase;
    repeatedCase.groups[1] = {repeatedA, repeatedB};
    const auto repeatedOrder = mixedInspirationOrder(repeatedCase);
    require(repeatedOrder.size() == 2,
            "high development-history similarity deleted an actual route from full pagination");
    return "unqualified and weaker routes stay behind; opportunity reorders comparable routes; repeated routes remain pageable";
}

std::string g11CheckDevelopmentHistoryAliases(const MatchQuery& query,
                                              int meterNumerator,
                                              int meterDenominator,
                                              const RecommendationSet& original,
                                              const CandidateIndex& index) {
    const auto compareDevelopment = [](const auto& left, const auto& right,
                                       std::string_view context) {
        require(left.size() == right.size(),
                "development-history alias changed candidate count: " + std::string(context));
        for (const auto& [fingerprint, candidate] : left) {
            const auto found = right.find(fingerprint);
            require(found != right.end(),
                    "development-history alias changed actual candidate identity: " + std::string(context));
            require(std::abs(candidate->routeProfile.developmentHistorySimilarity -
                             found->second->routeProfile.developmentHistorySimilarity) <= 1e-6f,
                    "development-history similarity changed under sound/metadata alias: " +
                        std::string(context));
        }
    };

    bool symbolsChanged{};
    const auto aliasEvents = g10AliasActualSymbols(query.source, symbolsChanged);
    require(symbolsChanged && g8EventListFingerprint(aliasEvents) ==
            g8EventListFingerprint(query.source),
            "development-history symbol alias changed actual input identity");
    const auto aliasQuery = g8MakeQuery(aliasEvents,
        std::pair{meterNumerator, meterDenominator});
    const auto aliasRecommendations = g8NormalRecommendations(aliasQuery, index);
    compareDevelopment(g10CandidatesByFingerprint(original),
        g10CandidatesByFingerprint(aliasRecommendations), "enharmonic input spelling");

    auto metadataAlias = original;
    for (auto& group : metadataAlias.groups) for (auto& candidate : group) {
        candidate.id = "metadata-alias:" + candidate.id;
        candidate.primaryTemplate = "metadata-alias:" + candidate.primaryTemplate;
        candidate.family = "metadata-alias-family";
        candidate.developmentMechanism = "metadata-alias-mechanism";
    }
    prepareContextualDiversity(query, metadataAlias);
    compareDevelopment(g10CandidatesByFingerprint(original),
        g10CandidatesByFingerprint(metadataAlias), "candidate metadata alias");
    return "development-history similarity is invariant to same-sound symbols and candidate metadata";
}

struct G11Round2State {
    G8RecordedRcrRound record;
    ImportedProgressionSession input;
    MatchQuery query;
    RecommendationSet recommendations;
    midi::Meter meter;
};

G11Round2State g11BuildRound2State(const library::LoadResult& factory,
                                   const CandidateIndex& index,
                                   const G8RecordedRcrRound& record) {
    const auto& start = g9FindTemplate(factory, record.startId);
    const auto key = start.rawChordKey.value_or(KeySignature{PitchClass::C, start.mode});
    const auto route = preview::templateRoute(start, key);
    require(static_cast<bool>(route), "G11 round-2 source route failed: " + route.error);
    auto events = g10ParseFrozenFingerprint(record.queryInputFingerprint);
    require(events.size() == record.inputLength,
            "G11 round-2 query length differs from frozen fingerprint: " + record.startId);
    ImportedProgressionSession input;
    require(input.replace(std::move(events), TimelineCoordinateMode::RelativeToSelection),
            "cannot reconstruct G11 frozen round-2 input: " + record.startId);
    const auto before = input.events;
    const midi::Meter meter{route.meterNumerator, route.meterDenominator};
    const auto query = g8MakeQuery(input.events,
        std::pair{meter.numerator, meter.denominator});
    auto recommendations = g8NormalRecommendations(query, index);
    g9RequireSameTimeline(before, input.events, "G11 round 2 " + record.startId);
    return {record, std::move(input), query, std::move(recommendations), meter};
}

std::string g11CheckFutureCannotChangeEligibility(const RecommendationSet& recommendations,
                                                  const CandidateIndex& index) {
    const std::set<std::string> noUsedMusic, noUsedFamilies;
    const std::set<TemplateID> noUsedSources;
    std::size_t checked{}, qualified{};
    for (const auto& group : recommendations.groups) for (const auto& candidate : group) {
        const bool original = g8Eligible(candidate, index, noUsedMusic, noUsedSources,
                                         noUsedFamilies, true);
        auto noFuture = candidate;
        noFuture.branching = {};
        auto maximalFuture = candidate;
        maximalFuture.branching = {true, 100, 100, 100, 100, 100, 1.f};
        require(g8Eligible(noFuture, index, noUsedMusic, noUsedSources,
                           noUsedFamilies, true) == original &&
                g8Eligible(maximalFuture, index, noUsedMusic, noUsedSources,
                           noUsedFamilies, true) == original,
                "future opportunity changed current complete-open G9 eligibility");
        ++checked;
        if (original) ++qualified;
    }
    require(checked > 0 && qualified > 0,
            "G11 future-opportunity check has no current candidates or qualified routes");
    return "qualification unchanged with branching cleared or maximal (candidates=" +
        std::to_string(checked) + ", qualified=" + std::to_string(qualified) + ")";
}

std::string g11CheckIncrementalProfileEquivalence(const G11Round2State& state,
                                                  const CandidateIndex& index) {
    const auto order = mixedInspirationOrder(state.recommendations);
    const std::set<TemplateID> noUsedSources;
    const std::set<std::string> noUsedFamilies;
    std::set<std::string> usedMusic;
    RecommendationSet nextCandidates;
    const ContinuationCandidate* appended{};
    for (const auto position : order) {
        const auto* candidate = g8CandidateAt(state.recommendations, position);
        require(candidate != nullptr,
                "incremental-profile check encountered an invalid candidate position");
        if (!g8Eligible(*candidate, index, usedMusic, noUsedSources,
                        noUsedFamilies, true))
            continue;
        const auto fingerprint = g8CandidateMusicFingerprint(*candidate);
        nextCandidates.groups[position.group].push_back(*candidate);
        usedMusic.insert(fingerprint);
        if (!appended) appended = candidate;
        if (usedMusic.size() == 2) break;
    }
    require(appended != nullptr && usedMusic.size() == 2,
            "incremental-profile check needs two distinct qualified real routes");

    const auto originalInputSize = state.query.source.size();
    MatchQuery futureQuery = state.query;
    futureQuery.source = continuationProgression(state.query.source, *appended);
    require(futureQuery.source.size() > originalInputSize,
            "incremental-profile check did not append a real continuation");
    MatchQuery baseQuery = futureQuery;
    baseQuery.source.resize(originalInputSize);

    auto baseCandidates = nextCandidates;
    auto incrementalCandidates = nextCandidates;
    auto fullCandidates = nextCandidates;
    prepareContextualDiversity(baseQuery, baseCandidates);
    prepareContextualDiversity(futureQuery, incrementalCandidates, originalInputSize);
    prepareContextualDiversity(futureQuery, fullCandidates);

    const auto seededByMusic = g10CandidatesByFingerprint(nextCandidates);
    const auto baseByMusic = g10CandidatesByFingerprint(baseCandidates);
    const auto incrementalByMusic = g10CandidatesByFingerprint(incrementalCandidates);
    const auto fullByMusic = g10CandidatesByFingerprint(fullCandidates);
    require(seededByMusic.size() == 2 && baseByMusic.size() == seededByMusic.size() &&
            incrementalByMusic.size() == seededByMusic.size() &&
            fullByMusic.size() == seededByMusic.size(),
            "profile preparation changed the two-candidate actual music identity set");

    constexpr float tolerance = 1e-6f;
    const auto compareProfile = [&](std::string_view actualFingerprint,
                                     const CreativeRouteProfile& incremental,
                                     const CreativeRouteProfile& full) {
        const auto context = " actualfp=" + std::string(actualFingerprint);
        const auto same = [&](float left, float right) {
            return std::isfinite(left) && std::isfinite(right) &&
                std::abs(left - right) <= tolerance;
        };
        require(incremental.ready == full.ready,
                "incremental profile readiness differs from full recompute:" + context);
        require(incremental.tones == full.tones,
                "incremental actual-tone profile differs from full recompute:" + context);
        const auto compareArray = [&](const auto& left, const auto& right,
                                      std::string_view field) {
            require(left.size() == right.size(),
                    "incremental profile array size differs from full recompute: " +
                        std::string(field) + context);
            for (std::size_t i = 0; i < left.size(); ++i)
                require(std::isfinite(static_cast<double>(left[i])) &&
                        std::isfinite(static_cast<double>(right[i])) &&
                        std::abs(static_cast<double>(left[i]) -
                                 static_cast<double>(right[i])) <= tolerance,
                        "incremental profile array differs from full recompute: " +
                            std::string(field) + "[" + std::to_string(i) + "]" + context);
        };
        compareArray(incremental.bass, full.bass, "bass");
        compareArray(incremental.tension, full.tension, "tension");
        compareArray(incremental.palette, full.palette, "palette");
        require(same(incremental.historySimilarity, full.historySimilarity),
                "incremental historySimilarity differs from full recompute:" + context);
        require(same(incremental.developmentHistorySimilarity,
                     full.developmentHistorySimilarity),
                "incremental developmentHistorySimilarity differs from full recompute:" + context);
        require(same(incremental.recentSimilarity, full.recentSimilarity),
                "incremental recentSimilarity differs from full recompute:" + context);
        require(same(incremental.contextFit, full.contextFit),
                "incremental contextFit differs from full recompute:" + context);
    };

    for (const auto& [fingerprint, seeded] : seededByMusic) {
        (void)seeded;
        const auto base = baseByMusic.find(fingerprint);
        const auto incremental = incrementalByMusic.find(fingerprint);
        const auto full = fullByMusic.find(fingerprint);
        require(base != baseByMusic.end() && incremental != incrementalByMusic.end() &&
                full != fullByMusic.end(),
                "profile preparation changed a real candidate music fingerprint");
        require(g8CandidateMusicFingerprint(*incremental->second) == fingerprint &&
                g8CandidateMusicFingerprint(*full->second) == fingerprint,
                "incremental/full profile comparison changed actual candidate music");
        compareProfile(fingerprint, incremental->second->routeProfile,
                       full->second->routeProfile);
    }
    return "two real qualified routes: incremental appended-window profiles match full recompute "
           "for history, development history, recent/context fit, actual arrays, and music identity";
}

std::string g11CheckOneMidiPath(const G11Round2State& state,
                                const CandidateIndex& index,
                                const std::filesystem::path& outputDirectory,
                                std::ostream& report) {
    const auto order = mixedInspirationOrder(state.recommendations);
    const ContinuationCandidate* selected{};
    std::size_t selectedRank{};
    const std::set<std::string> noUsedMusic, noUsedFamilies;
    const std::set<TemplateID> noUsedSources;
    for (std::size_t visible = 0; visible < order.size(); ++visible) {
        const auto* candidate = g8CandidateAt(state.recommendations, order[visible]);
        require(candidate != nullptr, "G11 MIDI check has an invalid mixed-order position");
        if (g8Eligible(*candidate, index, noUsedMusic, noUsedSources, noUsedFamilies, true)) {
            selected = candidate;
            selectedRank = visible;
            break;
        }
    }
    require(selected != nullptr, "G11 MIDI check found no current G9-qualified complete open route");
    const auto family = g8SourceFamily(*selected, index);
    const auto fingerprint = g8CandidateMusicFingerprint(*selected);
    const auto firstPageSize = inspirationFirstPageSize(
        state.recommendations, ui::recommendationPageSize);
    const auto page = ui::browsePageIndex(selectedRank, firstPageSize) + 1;
    const auto built = preview::buildSequence(state.input, selected, 120.0);
    require(static_cast<bool>(built), "G11 MIDI check Preview failed: " + built.error);
    const midi::CandidateExportContext exportContext{state.input, 120.0, state.meter,
        selected->key, midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
    const auto payload = midi::candidatePayload(exportContext, *selected);
    require(static_cast<bool>(payload) && !payload.payload.dawClipBytes.empty(),
            "G11 MIDI check DAWClip payload failed: " + payload.error);
    const auto clip = midi::buildClip(built.sequence, midi::ArrangementMode::VoiceLed,
        midi::ExportScope::FullPhrase, state.meter, selected->key, selected->intent);
    require(static_cast<bool>(clip), "G11 MIDI check Clip construction failed: " + clip.error);
    midi::ExportConfig config;
    config.profile = midi::ExportProfile::DAWClip;
    const auto dawBytes = midi::writeToMemory(clip.sequence, config);
    require(static_cast<bool>(dawBytes) && dawBytes.bytes == payload.payload.dawClipBytes,
            "G11 MIDI check DAWClip differs from production payload");

    const auto midiDirectory = outputDirectory / ".g11-check-midi";
    std::filesystem::create_directories(midiDirectory);
    const auto midiFile = midiDirectory / (state.record.startId + "-round-2.mid");
    std::string writeError;
    require(midi::writeToFile(payload.payload.dawClipBytes, midiFile, writeError),
            "G11 MIDI check DAWClip write failed: " + writeError);
    try {
        const auto completeImport = midi::importFile(midiFile, false);
        require(static_cast<bool>(completeImport),
                "G11 MIDI check complete import failed: " + completeImport.extraction.error);
        ImportedProgressionSession complete;
        require(midi::applyImport(completeImport, complete),
                "G11 MIDI check applyImport failed for complete DAWClip");
        const auto midiSummary = g8CompareMidiToPreview(built.sequence, clip.sequence,
            complete.events, completeImport.midi.file, false, "G11 focused DAWClip");
        const auto completeFingerprint = g8EventListFingerprint(complete.events);

        const auto openImport = midi::importFile(midiFile, true);
        require(static_cast<bool>(openImport),
                "G11 MIDI check OPEN import failed: " + openImport.extraction.error);
        ImportedProgressionSession next;
        require(midi::applyImport(openImport, next),
                "G11 MIDI check applyImport failed for OPEN DAWClip");
        (void)g8CompareMidiToPreview(built.sequence, clip.sequence, next.events,
            openImport.midi.file, true, "G11 focused OPEN DAWClip");
        auto reopenedTail = complete.events;
        reopenedTail.back().durationQN.reset();
        reopenedTail.back().openEnded = true;
        const auto openFingerprint = g8EventListFingerprint(next.events);
        require(g8EventListFingerprint(reopenedTail) == openFingerprint,
                "G11 MIDI check did not carry the complete import back at OPEN");

        const auto nextBefore = next.events;
        const auto nextQuery = g8MakeQuery(next.events,
            std::pair{state.meter.numerator, state.meter.denominator});
        const auto nextRecommendations = g8NormalRecommendations(nextQuery, index);
        g9RequireSameTimeline(nextBefore, next.events, "G11 focused MIDI continuation query");
        std::size_t nextQualified{};
        const std::set<std::string> usedMusic{fingerprint};
        const std::set<TemplateID> usedSources{selected->primaryTemplate};
        const std::set<std::string> usedFamilies{family};
        for (const auto& group : nextRecommendations.groups) for (const auto& candidate : group)
            if (g8Eligible(candidate, index, usedMusic, usedSources, usedFamilies, true))
                ++nextQualified;

        report << g8Tsv(state.record.startId) << "\t2\tPASS\t" << state.input.events.size()
               << '\t' << selectedRank + 1 << '\t' << page << '\t'
               << g8Tsv(selected->primaryTemplate) << '\t' << g9KeyText(selected->key) << '\t'
               << g8Tsv(family) << '\t' << g8Tsv(fingerprint) << '\t' << dawBytes.bytes.size()
               << '\t' << g8Tsv(completeFingerprint) << '\t' << g8Tsv(openFingerprint) << '\t'
               << midiSummary.reinterpretationCount << '\t' << nextQualified
               << "\tcomplete import, OPEN carry, and one next recommendation query passed\n";
        require(static_cast<bool>(report), "cannot write G11 MIDI check output");
        std::error_code removeError;
        std::filesystem::remove(midiFile, removeError);
    } catch (...) {
        std::error_code removeError;
        std::filesystem::remove(midiFile, removeError);
        throw;
    }
    return "one round-2 G9-qualified DAWClip path imported complete and OPEN; C was queried once";
}

int runG11Checks(const std::filesystem::path& factoryPath,
                 const std::filesystem::path& roundsPath,
                 const std::filesystem::path& baselinePath,
                 const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G11 check Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G11 checks require Factory version 8 or newer");
    require(std::filesystem::is_regular_file(roundsPath) &&
            std::filesystem::is_regular_file(baselinePath),
            "G11 checks require the frozen G10 rounds and sealed G11 candidate baseline");
    std::filesystem::create_directories(outputDirectory);

    const std::array<std::string_view, 3> startIds{
        "V3_CYCLE_027", "V6_B3_02_054", "V8_01_006"};
    std::set<std::string> queryIds;
    for (const auto id : startIds) queryIds.emplace(std::string(id) + ":round-2");
    const auto baseline = g11ReadBaselineRanks(baselinePath, queryIds);
    const auto recordedRounds = g10ReadSuccessfulFrozenRounds(roundsPath);
    const CandidateIndex index(factory.templates);

    const auto checksPath = outputDirectory / "g11-checks.tsv";
    const auto candidatesPath = outputDirectory / "g11-checks-candidates.tsv";
    const auto longInputPath = outputDirectory / "g11-checks-84-event.tsv";
    const auto midiPath = outputDirectory / "g11-checks-midi.tsv";
    std::ofstream checks(checksPath, std::ios::binary | std::ios::trunc);
    std::ofstream candidates(candidatesPath, std::ios::binary | std::ios::trunc);
    std::ofstream longInput(longInputPath, std::ios::binary | std::ios::trunc);
    std::ofstream midiReport(midiPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(checks) && static_cast<bool>(candidates) &&
            static_cast<bool>(longInput) && static_cast<bool>(midiReport),
            "cannot create G11 focused-check outputs");
    checks << "check\tstatus\tcase\tdetail\n";
    candidates << "query\tsample_order\tstart_id\tround\tbaseline_rank\tcurrent_rank\tpage"
                  "\ton_first_page\tsource\tkey\tfamily\tchannel\tstructure_priority"
                  "\tstructure_reason\tboundary\tcontext_fit\tdirection\thistory_similarity"
                  "\troute_history_similarity\tdevelopment_history_similarity\trecent_similarity"
                  "\tbranching_assessed\tbranching_legal_routes\tbranching_independent_ideas"
                  "\tbranching_families\tbranching_center_directions\tbranching_fresh_ideas"
                  "\tbranching_opportunity\tactualfp\n";
    longInput << "";
    midiReport << "start_id\tround\tstatus\tinput_events\tvisible_rank\tpage\tsource\tkey"
                  "\tfamily\tactualfp\tDAWClip_bytes\tcomplete_import_actualfp\tOPEN_import_actualfp"
                  "\tMIDI_reinterpretation_count\tC_g9_qualified_count\tdetail\n";

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

    recordCheck("future-opportunity-ranking", "synthetic-phrase-transition", [&] {
        return g11CheckFutureOpportunityRanking();
    });

    std::optional<G11Round2State> midiState;
    for (const auto startId : startIds) {
        const auto queryId = std::string(startId) + ":round-2";
        const auto recorded = std::find_if(recordedRounds.begin(), recordedRounds.end(),
            [&](const auto& row) { return row.startId == startId && row.round == 2; });
        if (recorded == recordedRounds.end()) {
            ++failures;
            checks << "frozen-round-2\tFAIL\t" << queryId << "\tmissing frozen round 2\n";
            continue;
        }

        std::optional<G11Round2State> state;
        try {
            state.emplace(g11BuildRound2State(factory, index, *recorded));
        } catch (const std::exception& error) {
            ++failures;
            checks << "round-2-query\tFAIL\t" << queryId << '\t'
                   << g8Tsv(error.what()) << '\n';
            continue;
        }
        auto& current = *state;
        recordCheck("future-opportunity-does-not-change-eligibility", queryId, [&] {
            return g11CheckFutureCannotChangeEligibility(current.recommendations, index);
        });
        recordCheck("development-history-alias-invariance", queryId, [&] {
            return g11CheckDevelopmentHistoryAliases(current.query,
                current.meter.numerator, current.meter.denominator,
                current.recommendations, index);
        });
        recordCheck("frozen-round-2-identity-set", queryId, [&] {
            const auto baselineForQuery = baseline.find(queryId);
            require(baselineForQuery != baseline.end(),
                    "no sealed candidate identities for " + queryId);
            const auto order = mixedInspirationOrder(current.recommendations);
            const auto firstPageSize = inspirationFirstPageSize(
                current.recommendations, ui::recommendationPageSize);
            const std::set<std::string> noUsedMusic, noUsedFamilies;
            const std::set<TemplateID> noUsedSources;
            std::set<std::string> currentFingerprints;
            for (std::size_t visible = 0; visible < order.size(); ++visible) {
                const auto* candidate = g8CandidateAt(current.recommendations, order[visible]);
                require(candidate != nullptr,
                        "G11 round-2 order contains an invalid candidate: " + queryId);
                if (!g8Eligible(*candidate, index, noUsedMusic, noUsedSources,
                                noUsedFamilies, true))
                    continue;
                const auto actualFingerprint = g8CandidateMusicFingerprint(*candidate);
                const auto parsedActual = g10ParseCompleteFrozenFingerprint(actualFingerprint);
                require(g8EventListFingerprint(parsedActual) == actualFingerprint,
                        "G11 candidate identity failed root/bass/pitch-set/timing round-trip: " + queryId);
                require(currentFingerprints.insert(actualFingerprint).second,
                        "G11 round-2 eligible list repeats an actual music identity: " + queryId);
                const auto baselineRank = baselineForQuery->second.find(actualFingerprint);
                const auto family = g8SourceFamily(*candidate, index);
                const auto page = ui::browsePageIndex(visible, firstPageSize) + 1;
                candidates << g8Tsv(queryId) << '\t' << current.record.sampleOrder << '\t'
                           << g8Tsv(std::string(startId)) << "\t2\t"
                           << (baselineRank == baselineForQuery->second.end() ? 0 : baselineRank->second)
                           << '\t' << visible + 1 << '\t' << page << '\t' << (page == 1 ? 1 : 0)
                           << '\t' << g8Tsv(candidate->primaryTemplate) << '\t'
                           << g9KeyText(candidate->key) << '\t' << g8Tsv(family) << '\t'
                           << recommendationChannelName(candidate->channel) << '\t'
                           << candidate->structurePriority << '\t' << g8Tsv(candidate->structureReason)
                           << '\t' << std::fixed << std::setprecision(3)
                           << candidate->creativeQuality.boundary << '\t'
                           << candidate->routeProfile.contextFit << '\t'
                           << g6DirectionName(candidate->direction) << '\t'
                           << candidate->historySimilarity << '\t'
                           << candidate->routeProfile.historySimilarity << '\t'
                           << candidate->routeProfile.developmentHistorySimilarity << '\t'
                           << candidate->routeProfile.recentSimilarity << '\t';
                g11WriteBranchingFields(candidates, candidate->branching);
                candidates << '\t' << g8Tsv(actualFingerprint) << '\n';
            }
            require(currentFingerprints == [&] {
                std::set<std::string> frozen;
                for (const auto& [fingerprint, rank] : baselineForQuery->second) {
                    (void)rank;
                    const auto parsed = g10ParseCompleteFrozenFingerprint(fingerprint);
                    require(g8EventListFingerprint(parsed) == fingerprint,
                            "sealed G11 baseline identity does not round-trip all actual fields");
                    frozen.insert(fingerprint);
                }
                return frozen;
            }(), "G11 round-2 qualified actualfp set changed; only order is allowed to change: " + queryId);
            require(static_cast<bool>(candidates), "cannot write G11 round-2 candidate detail");
            return "sealed/current full-open actualfp sets match; exact root, bass, pitch set, and timing preserved; " +
                std::to_string(currentFingerprints.size()) + " identities; only visible order may change";
        });
        if (startId == "V3_CYCLE_027") midiState = std::move(current);
    }

    recordCheck("84-event-shared-index-timing", "V3_CYCLE_027", [&] {
        const auto detail = g10CheckLongInput(factory, index, roundsPath, longInput);
        longInput.flush();
        require(static_cast<bool>(longInput), "cannot finish G11 84-event timing report");
        return detail;
    });
    if (midiState) {
        recordCheck("incremental-profile-equivalence", "V3_CYCLE_027:round-2", [&] {
            return g11CheckIncrementalProfileEquivalence(*midiState, index);
        });
        recordCheck("single-round-2-DAWClip-branch", "V3_CYCLE_027", [&] {
            return g11CheckOneMidiPath(*midiState, index, outputDirectory, midiReport);
        });
    } else {
        ++failures;
        checks << "incremental-profile-equivalence\tFAIL\tV3_CYCLE_027:round-2\t"
                  "no round-2 state available\n";
        ++failures;
        checks << "single-round-2-DAWClip-branch\tFAIL\tV3_CYCLE_027\tno round-2 state available\n";
    }

    checks.flush();
    candidates.flush();
    longInput.flush();
    midiReport.flush();
    require(static_cast<bool>(checks) && static_cast<bool>(candidates) &&
            static_cast<bool>(longInput) && static_cast<bool>(midiReport),
            "cannot finish G11 focused-check outputs");
    std::cout << "g11_checks representative_round2=3 failures=" << failures
              << " checks=" << checksPath.string()
              << " candidates=" << candidatesPath.string()
              << " long_input=" << longInputPath.string()
              << " midi=" << midiPath.string() << '\n';
    return failures == 0 ? 0 : 1;
}
} // namespace
