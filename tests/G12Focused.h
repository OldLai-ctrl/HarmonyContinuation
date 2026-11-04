#pragma once

#include "G8Focused.h"
#include "G9Focused.h"
#include "G10Focused.h"
#include "G11Focused.h"
#include "core/ContextualContinuation.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <string_view>

namespace {
using namespace harmony;

struct G12Start {
    std::string_view id;
    std::size_t sampleOrder, representativeOrder;
    std::array<std::string_view, 3> sources;
};
constexpr std::array<G12Start, 3> g12Starts{{
    {"V3_CYCLE_027", 1, 1, {"V8_01_009", "V8_01_002", "V8_01_010"}},
    {"V6_B3_02_054", 3, 2, {"V8_01_013", "V8_01_010", "V8_01_009"}},
    {"V8_01_006", 10, 3, {"V8_01_008", "V8_01_011", "V8_01_009"}}
}};

struct G12Paths { std::filesystem::path snapshot, midi; };
G12Paths g12OriginalPaths(const std::filesystem::path& roundsPath, const G12Start& start) {
    const auto directory = roundsPath.parent_path() / "representative-paths" /
        (std::to_string(start.representativeOrder) + "-" + std::string(start.id));
    const auto stem = directory / (std::string(start.id) + "-round-3");
    return {stem.string() + ".snapshot3.json", stem.string() + ".mid"};
}

bool g12HasTag(const ProgressionTemplate& item, std::string_view tag) {
    return std::find(item.tags.begin(), item.tags.end(), std::string(tag)) != item.tags.end();
}

const ProgressionTemplate& g12IndexedSource(const CandidateIndex& index, std::string_view id) {
    const auto found = std::find_if(index.templates().begin(), index.templates().end(),
        [&](const auto& item) { return item.id == id; });
    require(found != index.templates().end(), "G12 indexed Factory source missing: " + std::string(id));
    return *found;
}

std::vector<std::uint8_t> g12ReadBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot read G12 original media bytes: " + path.string());
    std::vector<std::uint8_t> bytes;
    for (char value; input.get(value);) bytes.push_back(static_cast<std::uint8_t>(value));
    require(input.eof(), "cannot finish G12 original media read: " + path.string());
    return bytes;
}

double g12Duration(const std::vector<ConcreteChordEvent>& events) {
    double result{};
    for (const auto& event : events) result += event.durationQN;
    return result;
}

int runG12Boundaries(const std::filesystem::path& factoryPath,
                     const std::filesystem::path& roundsPath,
                     const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G12 Factory: " + factory.error);
    const CandidateIndex index(factory.templates);
    std::filesystem::create_directories(outputDirectory);
    const auto sourcesPath = outputDirectory / "g12-boundaries-sources.tsv";
    const auto candidatesPath = outputDirectory / "g12-boundaries-candidates.tsv";
    require(!std::filesystem::exists(sourcesPath) && !std::filesystem::exists(candidatesPath),
            "G12 boundary output already exists; refusing to overwrite it");
    std::ofstream sources(sourcesPath, std::ios::binary | std::ios::trunc);
    std::ofstream candidates(candidatesPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(sources) && static_cast<bool>(candidates),
            "cannot create G12 boundary TSV outputs");
    sources << "start_id\tr3_source\toriginal_midi\tinput_events\topen_tail_start_qn"
               "\tsource_end_qn\tmeter\tactual_import_fingerprint\topen_input_fingerprint\n";
    candidates << "start_id\tr3_source\tsource_end_qn\tnormal_rank\tpage"
                  "\tcandidate_source\tkey\tending\tfirst_event\tlabels"
                  "\tcandidate_music_fingerprint\n";

    const std::set<std::string> noUsedMusic;
    const std::set<TemplateID> noUsedSources;
    const std::set<std::string> noUsedFamilies;
    std::size_t totalCandidates{};
    for (const auto& start : g12Starts) {
        const auto records = g8ReadRecordedRounds(roundsPath, start.id);
        require(records.size() == start.sources.size() && records.back().round == 3,
                "G12 boundary chain must contain three G11 rounds: " + std::string(start.id));
        for (std::size_t i = 0; i < records.size(); ++i)
            require(records[i].sampleOrder == start.sampleOrder &&
                    records[i].primarySource == start.sources[i],
                    "G12 boundary input differs from the frozen G11 chain: " +
                        std::string(start.id));
        const auto& finalRecord = records.back();
        require(!finalRecord.actualImportFingerprint.empty(),
                "G11 round 3 lacks its full actual-import fingerprint: " + std::string(start.id));
        const auto completeRecorded = g10ParseCompleteFrozenFingerprint(
            finalRecord.actualImportFingerprint);
        const auto& closedTail = completeRecorded.back();
        require(closedTail.durationQN && !closedTail.openEnded,
                "G11 round 3 actual import lacks a closed final duration: " + std::string(start.id));
        const auto sourceEndQN = closedTail.startQN + *closedTail.durationQN;

        const auto midiPath = g12OriginalPaths(roundsPath, start).midi;
        require(std::filesystem::exists(midiPath),
                "cannot find original G11 round-3 MIDI: " + midiPath.string());
        const auto imported = midi::importFile(midiPath, true);
        require(static_cast<bool>(imported),
                "G12 original round-3 MIDI OPEN import failed: " + imported.extraction.error);
        ImportedProgressionSession input;
        require(midi::applyImport(imported, input),
                "G12 original round-3 MIDI applyImport failed: " + std::string(start.id));
        g8RequireRecordedCarry(completeRecorded, input.events);

        const auto& originalSource = g9FindTemplate(factory, start.id);
        const auto originalKey = originalSource.rawChordKey.value_or(
            KeySignature{PitchClass::C, originalSource.mode});
        const auto route = preview::templateRoute(originalSource, originalKey);
        require(static_cast<bool>(route),
                "G12 original source route failed: " + route.error);
        const auto meter = midi::uniformImportedMeter(imported);
        if (meter)
            require(*meter == std::pair{route.meterNumerator, route.meterDenominator},
                    "G12 round-3 MIDI meter differs from its original source route");
        const auto queryMeter = std::pair{route.meterNumerator, route.meterDenominator};
        const auto openFingerprint = g8EventListFingerprint(input.events);
        const auto query = g8MakeQuery(input.events, queryMeter);
        require(!query.interpretations.empty(),
                "G12 OPEN round-3 MIDI query has no key interpretation: " + std::string(start.id));
        const auto recommendations = g8NormalRecommendations(query, index);
        require(g8EventListFingerprint(input.events) == openFingerprint,
                "G12 normal recommendation query rewrote the original MIDI input");

        sources << g8Tsv(std::string(start.id)) << '\t' << g8Tsv(finalRecord.primarySource)
                << '\t' << g8Tsv(midiPath.string()) << '\t' << input.events.size() << '\t'
                << input.events.back().startQN << '\t' << sourceEndQN << '\t'
                << queryMeter.first << '/' << queryMeter.second << '\t'
                << g8Tsv(finalRecord.actualImportFingerprint) << '\t'
                << g8Tsv(openFingerprint) << '\n';

        const auto order = mixedInspirationOrder(recommendations);
        const auto firstPageSize = inspirationFirstPageSize(
            recommendations, ui::recommendationPageSize);
        std::size_t emitted{};
        for (std::size_t visible = 0; visible < order.size() && emitted < 10; ++visible) {
            const auto* candidate = g8CandidateAt(recommendations, order[visible]);
            require(candidate != nullptr, "G12 mixed order contains an invalid candidate position");
            if (!g8Eligible(*candidate, index, noUsedMusic, noUsedSources,
                            noUsedFamilies, true)) continue;
            require(!candidate->continuation.empty(),
                    "G12 qualified candidate has no continuation events");
            candidates << g8Tsv(std::string(start.id)) << '\t'
                       << g8Tsv(finalRecord.primarySource) << '\t' << sourceEndQN << '\t'
                       << visible + 1 << '\t'
                       << ui::browsePageIndex(visible, firstPageSize) + 1 << '\t'
                       << g8Tsv(candidate->primaryTemplate) << '\t'
                       << g9KeyText(candidate->key) << '\t'
                       << phraseEndingName(candidate->ending) << '\t'
                       << g8Tsv(candidate->continuation.front().label) << '\t'
                       << g8ContinuationLabels(*candidate) << '\t'
                       << g8Tsv(g8CandidateMusicFingerprint(*candidate)) << '\n';
            ++emitted;
        }
        totalCandidates += emitted;
        std::cout << "g12_boundaries start=" << start.id << " input_events="
                  << input.events.size() << " source_end_qn=" << sourceEndQN
                  << " g9_qualified_candidates=" << emitted << " selection=none\n";
    }
    require(static_cast<bool>(sources) && static_cast<bool>(candidates),
            "cannot finish G12 boundary TSV outputs");
    std::cout << "g12_boundaries queries=" << g12Starts.size()
              << " candidate_rows=" << totalCandidates << " selection=none\n";
    return 0;
}

struct G12Selection {
    std::string startId, source, key, fingerprint, rationale;
    std::size_t rank{}, page{};
};

std::vector<G12Selection> g12ReadSelections(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "cannot open G12 boundary selections: " + path.string());
    std::string line;
    require(static_cast<bool>(std::getline(input, line)), "G12 selections TSV is empty");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto header = g8Split(line, '\t');
    std::unordered_map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < header.size(); ++i)
        require(columns.emplace(header[i], i).second, "duplicate G12 selection column");
    constexpr std::array<std::string_view, 7> required{
        "start_id", "E_source", "key", "normal_rank", "page",
        "candidate_music_fingerprint", "editorial_rationale"};
    for (const auto name : required)
        require(columns.contains(std::string(name)), "G12 selections TSV lacks " + std::string(name));
    const auto cell = [&](const std::vector<std::string>& row, std::string_view name) -> const std::string& {
        require(row.size() == header.size(), "invalid G12 selection row width");
        return row[columns.at(std::string(name))];
    };
    std::vector<G12Selection> result;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto row = g8Split(line, '\t');
        result.push_back({cell(row, "start_id"), cell(row, "E_source"), cell(row, "key"),
            cell(row, "candidate_music_fingerprint"), cell(row, "editorial_rationale"),
            static_cast<std::size_t>(g8ParseInt(cell(row, "normal_rank"), "G12 normal rank")),
            static_cast<std::size_t>(g8ParseInt(cell(row, "page"), "G12 page"))});
    }
    require(input.eof() && result.size() == g12Starts.size(),
            "G12 selections must contain exactly three readable rows");
    for (std::size_t i = 0; i < result.size(); ++i)
        require(result[i].startId == g12Starts[i].id && result[i].rank > 0 && result[i].page > 0,
                "G12 selections changed the frozen chain order or contain an invalid page/rank");
    return result;
}

ContinuationCandidate g12WholeSourceCandidate(const ProgressionTemplate& source,
                                               const CandidateIndex& index,
                                               KeySignature key) {
    const auto route = preview::templateRoute(source, key);
    require(static_cast<bool>(route), "G12 review source route failed: " + route.error);
    ContinuationCandidate candidate;
    candidate.id = "g12-review-check:" + source.id;
    candidate.primaryTemplate = source.id;
    candidate.key = key;
    candidate.intent = source.intent;
    candidate.channel = RecommendationChannel::PhraseTransition;
    candidate.continuation = g8Concrete(route.imported.events);
    annotatePhraseDevelopment(candidate, source, index, key);
    require(g8CandidateMusicFingerprint(candidate) == g8EventListFingerprint(route.imported.events),
            "G12 source review altered complete source notes/timing: " + source.id);
    return candidate;
}

int runG12Checks(const std::filesystem::path& factoryPath,
                 const std::filesystem::path& roundsPath,
                 const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G12 check Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G12 checks require Factory version 8 or newer");
    std::filesystem::create_directories(outputDirectory);
    const auto reportPath = outputDirectory / "g12-checks.tsv";
    const auto longPath = outputDirectory / "g12-checks-84-event.tsv";
    require(!std::filesystem::exists(reportPath) && !std::filesystem::exists(longPath),
            "G12 check output already exists; refusing to overwrite it");
    std::ofstream report(reportPath, std::ios::binary | std::ios::trunc);
    std::ofstream longInput(longPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report) && static_cast<bool>(longInput),
            "cannot create G12 check outputs");
    report << "check\tstatus\tcase\tdetail\n";
    std::size_t failures{};
    const auto check = [&](std::string_view name, std::string_view caseId, const auto& action) {
        try {
            const auto detail=action();
            report << name << "\tPASS\t" << g8Tsv(std::string(caseId)) << '\t'
                   << g8Tsv(detail) << '\n';
        } catch (const std::exception& error) {
            ++failures;
            report << name << "\tFAIL\t" << g8Tsv(std::string(caseId)) << '\t'
                   << g8Tsv(error.what()) << '\n';
        }
    };

    const CandidateIndex index(factory.templates);
    for (const auto id : {"V8_01_009", "V8_01_010", "V7_04_044", "V7_04_052"}) {
        check("review-identity-and-phrase-state", id, [&] {
            const auto& source = g12IndexedSource(index, id);
            require(g12HasTag(source, "G12-review-identity-bound"),
                    "expected G12 review signature did not bind to source identity");
            const auto key = source.rawChordKey.value_or(KeySignature{PitchClass::C, source.mode});
            const auto candidate = g12WholeSourceCandidate(source, index, key);
            require(candidate.sourceEndPreserved && candidate.completeSourcePhrase,
                    "complete source phrase was not retained");
            require(std::abs(g12Duration(candidate.continuation) - 32.0) < 1e-9,
                    "reviewed phrase source no longer spans 32 QN");
            if (source.id == "V8_01_009" || source.id == "V8_01_010") {
                require(g12HasTag(source, "G12-continuing-tail") && !candidate.reviewedEnding &&
                        candidate.exploratory,
                        "continuing source tail was not retained as exploratory");
                require(!g12HasTag(source, "G12-reviewed-phrase-boundary"),
                        "continuing tail was mislabeled as reviewed phrase end");
            } else {
                const bool modal = source.id == "V7_04_044";
                const auto ending = modal ? PhraseEnding::ModalDevelopment : PhraseEnding::LoopReturn;
                const auto endingTag = modal ? "G12-ending-modal_development" : "G12-ending-loop_return";
                require(g12HasTag(source, "G12-reviewed-phrase-boundary") &&
                        g12HasTag(source, "G12-grouping-4+4") && g12HasTag(source, endingTag) &&
                        candidate.reviewedEnding && !candidate.exploratory && candidate.ending == ending,
                        "reviewed 4+4 phrase ending metadata failed to bind");
            }
            return "identity-bound runtime review; notes/timing preserved; reviewedEnding=" +
                std::to_string(candidate.reviewedEnding) + "; exploratory=" +
                std::to_string(candidate.exploratory);
        });
    }

    check("review-signature-rejects-edit", "all-four-reviewed-sources", [&] {
        for (const auto id : {"V8_01_009", "V8_01_010", "V7_04_044", "V7_04_052"}) {
            auto edited = g9FindTemplate(factory, id);
            require(!edited.full.empty() && edited.full.back().durationQN,
                    "cannot construct G12 signature mutation");
            *edited.full.back().durationQN += 0.25;
            // V8 raw performance is authoritative over its matching projection.
            if(!edited.rawChords.empty())*edited.rawChords.back().durationQN += 0.25;
            const CandidateIndex altered({std::move(edited)});
            const auto& changed = altered.templates().front();
            require(!g12HasTag(changed, "G12-review-identity-bound") &&
                    !g12HasTag(changed, "G12-reviewed-phrase-boundary") &&
                    !g12HasTag(changed, "G12-grouping-4+4"),
                    "edited source unexpectedly retained a review signature");
        }
        return "signature edits rejected for 009/010/044/052";
    });

    check("curated-modal-OPEN-hold", "V7_04_044", [&] {
        const auto& source=g12IndexedSource(index,"V7_04_044");
        const auto key=source.rawChordKey.value_or(KeySignature{PitchClass::C,source.mode});
        const auto route=preview::templateRoute(source,key);
        const auto built=midi::previewFromTemplate(source,key,120.0);
        require(bool(route)&&bool(built),"curated modal source could not be realized");
        const midi::Meter meter{route.meterNumerator,route.meterDenominator};
        const auto clip=midi::buildClip(built.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,meter);
        require(bool(clip),"curated modal source MIDI failed");
        midi::ExportConfig config;config.profile=midi::ExportProfile::DAWClip;
        const auto encoded=midi::writeToMemory(clip.sequence,config);
        require(bool(encoded),"curated modal DAWClip encode failed");
        const auto path=outputDirectory/"V7_04_044-OPEN-input.mid";std::string error;
        require(midi::writeToFile(encoded.bytes,path,error),"curated modal MIDI write failed: "+error);
        const auto imported=midi::importFile(path,true);ImportedProgressionSession input;
        require(bool(imported)&&midi::applyImport(imported,input),"curated modal OPEN import failed");
        const auto query=g8MakeQuery(input.events,std::pair{meter.numerator,meter.denominator});
        const auto recs=g8NormalRecommendations(query,index);
        const std::set<std::string> none;std::size_t qualified{};
        for(const auto& group:recs.groups)for(const auto& c:group)
            if(g8Eligible(c,index,none,none,none,true)) {
                require(c.suggestedCurrentChordDurationQN&&std::abs(*c.suggestedCurrentChordDurationQN-8.)<1e-9&&
                    std::abs(c.seamQN-32.)<1e-9,"curated 8-QN terminal hold changed on real OPEN import");
                ++qualified;
            }
        require(qualified>0,"curated modal source has no normally qualified continuation");
        return "real DAWClip OPEN import preserves source's last 8 QN and 32-QN phrase boundary";
    });
    check("unreviewed-asymmetry-retained", "V7_03_008", [&] {
        const auto& source=g12IndexedSource(index,"V7_03_008");
        require(!g12HasTag(source,"G12-reviewed-phrase-boundary"),"unreviewed source gained a phrase certificate");
        const auto key=source.rawChordKey.value_or(KeySignature{PitchClass::C,source.mode});
        auto c=g12WholeSourceCandidate(source,index,key);
        require(std::abs(g12Duration(c.continuation)-42.)<1e-9,"original 42 QN was changed");
        const auto records=g8ReadRecordedRounds(roundsPath,"V3_CYCLE_027");
        const auto query=g8MakeQuery(g10ParseFrozenFingerprint(records.front().queryInputFingerprint),std::pair{4,4});
        RecommendationSet set;set.groups[1].push_back(c);assessInspirationStructure(query,index,set);
        require(!set.groups[1][0].structureQualified,"an unreviewed off-bar source end passed G9 structure");
        return "original 42-QN music retained; no padding, clipping, or unsupported phrase certificate";
    });

    check("84-event-shared-index-timing", "V3_CYCLE_027", [&] {
        const auto detail = g10CheckLongInput(factory, index, roundsPath, longInput);
        longInput.flush();
        require(static_cast<bool>(longInput), "cannot finish G12 84-event result");
        return detail;
    });
    report.flush();
    require(static_cast<bool>(report) && static_cast<bool>(longInput),
            "cannot finish G12 checks outputs");
    std::cout << "g12_checks cases=8 failures=" << failures << " report=" << reportPath.string()
              << " long_input=" << longPath.string() << '\n';
    require(failures == 0, "one or more G12 focused checks failed; see " + reportPath.string());
    return 0;
}

bool g12SamePrefixNotes(const midi::ExportSequence& oldD,
                        const midi::ExportSequence& dPlusE,
                        double dDurationQN) {
    std::vector<midi::Note> oldNotes = oldD.notes, prefix;
    for (const auto& note : dPlusE.notes)
        if (note.startQN < dDurationQN - 1e-9) prefix.push_back(note);
    const auto order = [](const auto& a, const auto& b) {
        return std::tie(a.startQN, a.midiNote, a.durationQN, a.channel, a.bass) <
               std::tie(b.startQN, b.midiNote, b.durationQN, b.channel, b.bass);
    };
    std::sort(oldNotes.begin(), oldNotes.end(), order);
    std::sort(prefix.begin(), prefix.end(), order);
    if (oldNotes.size() != prefix.size()) return false;
    for (std::size_t i = 0; i < oldNotes.size(); ++i)
        if (oldNotes[i].midiNote != prefix[i].midiNote || oldNotes[i].channel != prefix[i].channel ||
            oldNotes[i].velocity != prefix[i].velocity || oldNotes[i].bass != prefix[i].bass ||
            std::abs(oldNotes[i].startQN - prefix[i].startQN) > 1e-9 ||
            std::abs(oldNotes[i].durationQN - prefix[i].durationQN) > 1e-9)
            return false;
    return true;
}

int runG12Auditions(const std::filesystem::path& factoryPath,
                    const std::filesystem::path& roundsPath,
                    const std::filesystem::path& selectionsPath,
                    const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G12 audition Factory: " + factory.error);
    const auto selections = g12ReadSelections(selectionsPath);
    std::filesystem::create_directories(outputDirectory);
    const auto manifestPath = outputDirectory / "g12-auditions.tsv";
    require(!std::filesystem::exists(manifestPath), "G12 audition manifest already exists");
    std::ofstream manifest(manifestPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(manifest), "cannot create G12 audition manifest");
    manifest << "start_id\tA_B_C_D\tE_source\tE_key\tnormal_rank\tpage\tE_music_fingerprint"
                "\tD_QN\tE_QN\tD_to_E_qn\tD_to_E_seconds\told_snapshot\told_midi_byte_identity"
                "\tfocus_midi\tfocus_wav\tE_snapshot_roundtrip\teditorial_rationale\tassembly\n";

    const CandidateIndex index(factory.templates);
    const std::set<std::string> noUsedMusic, noUsedFamilies;
    const std::set<TemplateID> noUsedSources;
    std::set<std::string> selectedSources, selectedMusic;
    for (std::size_t n = 0; n < g12Starts.size(); ++n) {
        const auto& start = g12Starts[n];
        const auto& selection = selections[n];
        const auto records = g8ReadRecordedRounds(roundsPath, start.id);
        require(records.size() == 3 && records.back().round == 3,
                "G12 audition source chain is not three recorded G11 rounds");
        for (std::size_t i = 0; i < records.size(); ++i)
            require(records[i].sampleOrder == start.sampleOrder && records[i].primarySource == start.sources[i],
                    "G12 audition chain differs from the frozen G11 sequence");
        require(std::none_of(records.begin(), records.end(), [&](const auto& row) {
                    return row.primarySource == selection.source;
                }) && selection.source != start.id,
                "selected E source already appears in A/B/C/D history");
        require(selectedSources.insert(selection.source).second &&
                selectedMusic.insert(selection.fingerprint).second,
                "selected G12 E phrases repeat a source or complete music identity");

        const auto paths = g12OriginalPaths(roundsPath, start);
        const auto reopened = snapshot::loadFile(paths.snapshot, &factory);
        require(static_cast<bool>(reopened) && reopened.value.candidateAvailable &&
                reopened.value.candidate.primaryTemplate == records.back().primarySource,
                "G12 original round-3 Snapshot3 failed to reopen with its D source");
        const auto& old = reopened.value;
        const midi::Meter meter{old.meterNumerator, old.meterDenominator};
        const midi::CandidateExportContext oldContext{old.imported, old.tempoBPM, meter,
            old.candidate.key, midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
        const auto oldPayload = midi::candidatePayload(oldContext, old.candidate);
        require(static_cast<bool>(oldPayload) && oldPayload.payload.dawClipBytes == g12ReadBytes(paths.midi),
                "old G11 Snapshot3 no longer reproduces original round-3 DAWClip bytes");
        const auto oldPreview = preview::buildSequence(old.imported, &old.candidate, old.tempoBPM);
        require(static_cast<bool>(oldPreview), "G12 original D Preview failed: " + oldPreview.error);
        const auto oldClip = midi::buildClip(oldPreview.sequence, midi::ArrangementMode::VoiceLed,
            midi::ExportScope::ContinuationOnly, meter, old.candidate.key, old.candidate.intent);
        require(static_cast<bool>(oldClip), "G12 original D-only MIDI Clip failed: " + oldClip.error);

        const auto actualImport = midi::importFile(paths.midi, true);
        require(static_cast<bool>(actualImport), "G12 reimport of original round-3 MIDI failed: " +
                actualImport.extraction.error);
        ImportedProgressionSession openInput;
        require(midi::applyImport(actualImport, openInput), "G12 round-3 OPEN MIDI applyImport failed");
        const auto completeRecorded = g10ParseCompleteFrozenFingerprint(records.back().actualImportFingerprint);
        g8RequireRecordedCarry(completeRecorded, openInput.events);
        const auto beforeQuery = g8EventListFingerprint(openInput.events);
        const auto query = g8MakeQuery(openInput.events,
            std::pair{meter.numerator, meter.denominator});
        const auto recommendations = g8NormalRecommendations(query, index);
        require(g8EventListFingerprint(openInput.events) == beforeQuery,
                "normal G12 OPEN query rewrote actual MIDI events");
        const auto order = mixedInspirationOrder(recommendations);
        const auto firstPageSize = inspirationFirstPageSize(recommendations, ui::recommendationPageSize);
        const ContinuationCandidate* selected{};
        std::size_t rank{};
        for (std::size_t visible = 0; visible < order.size(); ++visible) {
            const auto* candidate = g8CandidateAt(recommendations, order[visible]);
            require(candidate != nullptr, "G12 audition mixed order contains an invalid position");
            if (candidate->primaryTemplate == selection.source && g9KeyText(candidate->key) == selection.key &&
                g8CandidateMusicFingerprint(*candidate) == selection.fingerprint) {
                require(g8Eligible(*candidate, index, noUsedMusic, noUsedSources, noUsedFamilies, true),
                        "selected G12 E candidate no longer passes normal G9 eligibility");
                selected = candidate;
                rank = visible + 1;
                break;
            }
        }
        require(selected && rank == selection.rank &&
                ui::browsePageIndex(rank - 1, firstPageSize) + 1 == selection.page,
                "selected G12 E source/key/fingerprint moved from its recorded rank/page");
        const double dQN = g12Duration(old.candidate.continuation);
        const double eQN = g12Duration(selected->continuation);
        require(std::abs(dQN - 32.0) < 1e-9 && std::abs(eQN - 32.0) < 1e-9,
                "G12 audition must preserve full 32-QN D and E phrases");

        auto assembly = old.candidate;
        assembly.id = "G12 transient D+E audit assembly";
        assembly.primaryTemplate = "G12 transient D+E audit assembly";
        assembly.continuation.insert(assembly.continuation.end(), selected->continuation.begin(),
                                     selected->continuation.end());
        const auto combined = preview::buildSequence(old.imported, &assembly, old.tempoBPM);
        require(static_cast<bool>(combined), "G12 transient D+E Preview failed: " + combined.error);
        require(combined.sequence.recommendationBoundary == oldPreview.sequence.recommendationBoundary &&
                std::abs(combined.sequence.totalQN - oldPreview.sequence.totalQN - 32.0) < 1e-9 &&
                combined.sequence.events.size() == oldPreview.sequence.events.size() + selected->continuation.size(),
                "G12 two-phrase assembly changed the source boundary or event count");
        const auto eEventIndex = oldPreview.sequence.events.size();
        require(eEventIndex < combined.sequence.events.size() &&
                std::abs(combined.sequence.events[eEventIndex].startQN - oldPreview.sequence.totalQN) < 1e-9,
                "G12 E does not start immediately after complete D");
        const auto focusClip = midi::buildClip(combined.sequence, midi::ArrangementMode::VoiceLed,
            midi::ExportScope::ContinuationOnly, meter, {}, old.candidate.intent);
        require(static_cast<bool>(focusClip) && std::abs(focusClip.sequence.totalQN - 64.0) < 1e-9 &&
                g12SamePrefixNotes(oldClip.sequence, focusClip.sequence, dQN),
                "D+E MIDI did not preserve D's first 32 QN note performance");
        const auto audio = preview::renderOffline(combined.sequence, 48000.0, preview::PreviewScope::Suffix);
        const auto expectedFrames = preview::qnToSamples(64.0, old.tempoBPM, 48000.0) + 7200;
        require(!audio.left.empty() && audio.left.size() == audio.right.size() && audio.rms > 0.0 &&
                audio.left.size() == static_cast<std::size_t>(expectedFrames),
                "G12 D+E suffix Preview did not render both 32-QN phrases and release tail");
        const auto midiPath = outputDirectory / (std::string(start.id) + "-D-plus-E-focus.mid");
        const auto wavPath = outputDirectory / (std::string(start.id) + "-D-plus-E-focus.wav");
        require(!std::filesystem::exists(midiPath) && !std::filesystem::exists(wavPath),
                "G12 D+E audit media already exists; refusing to overwrite it");
        std::string error;
        midi::ExportConfig dawConfig;
        dawConfig.profile = midi::ExportProfile::DAWClip;
        const auto focusBytes = midi::writeToMemory(focusClip.sequence, dawConfig);
        require(static_cast<bool>(focusBytes), "cannot encode G12 D+E focus MIDI: " + focusBytes.error);
        require(midi::writeToFile(focusBytes.bytes, midiPath, error),
                "cannot write G12 D+E focus MIDI: " + error);
        require(preview::writeWav16(audio, wavPath, error),
                "cannot write G12 D+E focus WAV: " + error);

        std::string eSnapshotPathText;
        if (selection.source == "V7_04_044") {
            const auto snapshotPath = outputDirectory / "V7_04_044-selected-E.snapshot3.json";
            require(!std::filesystem::exists(snapshotPath), "G12 new E Snapshot3 already exists");
            eSnapshotPathText = snapshotPath.string();
            auto eSnapshot = snapshot::capture(openInput, *selected, {}, old.tempoBPM,
                meter.numerator, meter.denominator, selected->key, {}, selected->intent);
            eSnapshot.factoryLibraryVersion = factory.libraryVersion;
            require(snapshot::saveFile(eSnapshot, snapshotPath, error),
                    "cannot save selected E Snapshot3: " + error);
            const auto eReopened = snapshot::loadFile(snapshotPath, &factory);
            require(static_cast<bool>(eReopened) && eReopened.value.candidateAvailable &&
                    eReopened.value.candidate.primaryTemplate == selection.source &&
                    g8CandidateMusicFingerprint(eReopened.value.candidate) == selection.fingerprint,
                    "selected source044 Snapshot3 changed its E candidate identity");
            const auto before = preview::buildSequence(openInput, selected, old.tempoBPM);
            const auto after = preview::buildSequence(eReopened.value.imported,
                &eReopened.value.candidate, eReopened.value.tempoBPM);
            require(static_cast<bool>(before) && static_cast<bool>(after) &&
                    g8SamePreview(before.sequence, after.sequence),
                    "selected source044 Snapshot3 changed Preview output");
            const midi::CandidateExportContext eContext{openInput, old.tempoBPM, meter, selected->key,
                midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
            const midi::CandidateExportContext restoredContext{eReopened.value.imported,
                eReopened.value.tempoBPM, meter, eReopened.value.candidate.key,
                midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
            const auto beforePayload = midi::candidatePayload(eContext, *selected);
            const auto afterPayload = midi::candidatePayload(restoredContext, eReopened.value.candidate);
            require(static_cast<bool>(beforePayload) && static_cast<bool>(afterPayload) &&
                    beforePayload.payload.smfBytes == afterPayload.payload.smfBytes &&
                    beforePayload.payload.dawClipBytes == afterPayload.payload.dawClipBytes,
                    "selected source044 Snapshot3 changed production MIDI bytes");
        }

        manifest << g8Tsv(std::string(start.id)) << '\t'
                 << g8Tsv(std::string(start.id) + " -> " + records[0].primarySource + " -> " +
                         records[1].primarySource + " -> " + records[2].primarySource) << '\t'
                 << selection.source << '\t' << selection.key << '\t' << rank << '\t'
                 << selection.page << '\t' << g8Tsv(selection.fingerprint) << '\t'
                 << dQN << '\t' << eQN << '\t' << dQN << '\t'
                 << dQN * 60.0 / old.tempoBPM << '\t' << g8Tsv(paths.snapshot.string())
                 << "\tbyte-identical\t" << g8Tsv(midiPath.string()) << '\t'
                 << g8Tsv(wavPath.string()) << '\t' << g8Tsv(eSnapshotPathText) << '\t'
                 << g8Tsv(selection.rationale)
                 << "\ttemporary two-phrase audit assembly; D source identity not extended\n";
        require(static_cast<bool>(manifest), "cannot finish G12 audition manifest row");
        std::cout << "g12_audition start=" << start.id << " E=" << selection.source
                  << " rank=" << rank << " page=" << selection.page
                  << " D+E_QN=" << dQN + eQN << " focus=" << midiPath.filename().string() << '\n';
    }
    manifest.flush();
    require(static_cast<bool>(manifest), "cannot finish G12 audition manifest");
    std::cout << "g12_auditions chains=" << g12Starts.size()
              << " E044_snapshot_roundtrip=PASS output=" << outputDirectory.string() << '\n';
    return 0;
}

} // namespace
