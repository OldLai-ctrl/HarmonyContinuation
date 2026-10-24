#include "TemplateJson.h"
#include "benchmark/BenchJson.h"
#include "core/ContinuationEngine.h"
#include "core/RecommendationPolicy.h"
#include "library/LegacyFactoryIdResolver.h"
#include "library/ProgressionLibrary.h"
#include "midi/MidiClip.h"
#include "midi/StandardMidiFileReader.h"
#include "midi/StandardMidiFileWriter.h"
#include "preview/PreviewSequence.h"
#include "preview/PreviewSynth.h"
#include "preview/OfflinePreviewRenderer.h"
#include "session/PluginSessionState.h"
#include "snapshot/RecommendationSnapshot.h"
#include "wish/ModeAnnotations.h"
#include "wish/MusicIdentity.h"
#include "wish/WishSampler.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

using namespace harmony;
namespace {
using Clock = std::chrono::steady_clock;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "cannot read " + path.string());
    return {std::istreambuf_iterator<char>{file}, {}};
}

std::set<std::string> readApprovedIds(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "cannot read " + path.string());
    std::set<std::string> result;
    for (std::string line; std::getline(file, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto begin = line.find_first_not_of(" \t");
        if (begin == std::string::npos || line[begin] == '#') continue;
        const auto end = line.find_last_not_of(" \t");
        const auto id = line.substr(begin, end - begin + 1);
        require(id.find_first_of(" \t") == std::string::npos, "invalid approved ID: " + id);
        require(result.insert(id).second, "duplicate approved ID: " + id);
    }
    require(file.eof(), "cannot finish reading " + path.string());
    return result;
}

const ProgressionTemplate& find(const library::LoadResult& library, const std::string& id) {
    const auto found = std::find_if(library.templates.begin(), library.templates.end(),
                                    [&](const auto& item) { return item.id == id; });
    require(found != library.templates.end(), "missing template: " + id);
    return *found;
}

ChordEvent makeChord(const MatchEvent& event, KeySignature key, double startQN) {
    auto realized = realizeContinuation(event, key, event.durationQN.value_or(4.0));
    auto chord = realized.harmonicData.value_or(ChordEvent{});
    chord.name = realized.label;
    chord.quality = realized.quality;
    chord.startQN = startQN;
    chord.durationQN = realized.durationQN;
    chord.openEnded = false;
    return chord;
}

Progression makePrefix(const ProgressionTemplate& item, KeySignature key, std::size_t count) {
    require(count > 0 && count < item.full.size(), "prefix must leave a candidate continuation");
    Progression result;
    double at{};
    for (std::size_t i = 0; i < count; ++i) {
        auto event = makeChord(item.full[i], key, at);
        at += event.durationQN.value_or(4.0);
        result.push_back(std::move(event));
    }
    result.back().durationQN.reset();
    result.back().openEnded = true;
    return result;
}

MatchQuery queryFor(const ProgressionTemplate& item, KeySignature key, std::size_t prefixLength) {
    AnalysisContext context;
    context.forcedKey = key;
    return makeMatchQuery(makePrefix(item, key, prefixLength), recommendationContext(context));
}

bool containsTemplate(const RecommendationSet& results, const std::string& id) {
    for (const auto& group : results.groups)
        for (const auto& candidate : group)
            if (candidate.primaryTemplate == id ||
                std::find(candidate.supportingTemplates.begin(), candidate.supportingTemplates.end(), id) !=
                    candidate.supportingTemplates.end())
                return true;
    return false;
}

const ContinuationCandidate* findCandidate(const RecommendationSet& results, const std::string& id) {
    for (const auto& group : results.groups)
        for (const auto& candidate : group)
            if (candidate.primaryTemplate == id ||
                std::find(candidate.supportingTemplates.begin(), candidate.supportingTemplates.end(), id) !=
                    candidate.supportingTemplates.end())
                return &candidate;
    return nullptr;
}

const char* groupName(std::size_t index) {
    constexpr std::array names{"resolve", "develop", "loop", "color"};
    return index < names.size() ? names[index] : "unknown";
}

std::optional<std::pair<std::size_t, std::size_t>> groupRank(
    const RecommendationSet& results, const std::string& candidateId) {
    for (std::size_t group = 0; group < results.groups.size(); ++group)
        for (std::size_t position = 0; position < results.groups[group].size(); ++position)
            if (results.groups[group][position].id == candidateId)
                return std::pair{group, position + 1};
    return std::nullopt;
}

std::vector<std::string> rankedIds(const RecommendationSet& results) {
    std::vector<std::string> ids;
    for (const auto& group : results.groups)
        for (const auto& candidate : group)
            ids.push_back(candidate.id + "|" + candidate.primaryTemplate + "|" +
                          std::to_string(candidate.rankingScore));
    return ids;
}

void requirePreserved(const library::LoadResult& baseline, const library::LoadResult& combined,
                      const std::set<std::string>& expectedCandidateIds) {
    require(combined.libraryVersion == 6 && combined.storageSchemaVersion == 2,
            "V6 Library 6 / Schema 2");
    require(combined.templates.size() == baseline.templates.size() + expectedCandidateIds.size(),
            "combined entry count");
    std::set<std::string> actualIds;
    for (const auto& item : combined.templates) actualIds.insert(item.id);
    std::set<std::string> expectedIds;
    for (const auto& item : baseline.templates) expectedIds.insert(item.id);
    expectedIds.insert(expectedCandidateIds.begin(), expectedCandidateIds.end());
    require(actualIds == expectedIds, "combined ID set differs from explicit contents");
    for (const auto& old : baseline.templates) {
        const auto& restored = find(combined, old.id);
        require(library::canonicalMusicalFingerprint(old) ==
                library::canonicalMusicalFingerprint(restored),
                "inherited V5 musical fingerprint changed: " + old.id);
        require(dev::serializeTemplateJson(old) == dev::serializeTemplateJson(restored),
                "inherited V5 payload changed: " + old.id);
    }
}

const ProgressionTemplate& find(const std::vector<ProgressionTemplate>& items, const std::string& id) {
    const auto found = std::find_if(items.begin(), items.end(),
                                    [&](const auto& item) { return item.id == id; });
    require(found != items.end(), "missing candidate template: " + id);
    return *found;
}

const wish::ModeAnnotation* findAnnotation(const wish::AnnotationCatalog& catalog,
                                           const std::string& id) {
    const auto found = std::find_if(catalog.entries.begin(), catalog.entries.end(),
                                    [&](const auto& item) { return item.templateId == id; });
    return found == catalog.entries.end() ? nullptr : &*found;
}

void verifyAnnotations(const library::LoadResult& baseline,
                       const std::vector<ProgressionTemplate>& candidates,
                       const std::set<std::string>& approved,
                       const std::filesystem::path& metadataPath,
                       const std::filesystem::path& baseHcaPath,
                       const std::filesystem::path& mergedHcaPath) {
    const auto baseText = readText(baseHcaPath);
    const auto mergedText = readText(mergedHcaPath);
    require(mergedText.starts_with(baseText), "V5 HCA bytes are not preserved as the exact prefix");
    const auto base = wish::parseAnnotations(baseText);
    const auto merged = wish::parseAnnotations(mergedText);
    require(static_cast<bool>(base), "V5 HCA parse: " + base.error);
    require(static_cast<bool>(merged), "merged V6 HCA parse: " + merged.error);
    for (const auto& entry : base.catalog.entries) {
        const auto* restored = findAnnotation(merged.catalog, entry.templateId);
        require(restored != nullptr, "merged HCA lost V5 ID: " + entry.templateId);
        require(wish::serializeAnnotations({1, {entry}}) == wish::serializeAnnotations({1, {*restored}}),
                "merged HCA changed a V5 annotation record: " + entry.templateId);
        const auto& item = find(baseline, entry.templateId);
        require(entry.sourceFingerprint == wish::sourceFingerprint(item),
                "V5 HCA binding changed: " + entry.templateId);
    }

    const auto metadata = benchmark::json::parse(readText(metadataPath));
    require(metadata.at("version").integer(6, 6) == 6 &&
            metadata.at("baseline").text() == "2421c6f", "V6 metadata version/baseline");
    std::map<std::string, const ProgressionTemplate*> candidateById;
    for (const auto& item : candidates)
        require(candidateById.emplace(item.id, &item).second, "duplicate candidate ID: " + item.id);
    std::set<std::string> metadataIds, editorialIds, pendingIds;
    std::size_t membershipRows{};
    for (const auto& item : metadata.at("candidates").items()) {
        const auto& id = item.at("id").text();
        require(metadataIds.insert(id).second, "duplicate metadata ID: " + id);
        const auto templateIt = candidateById.find(id);
        require(templateIt != candidateById.end(), "metadata ID absent from candidate JSON: " + id);
        const auto& status = item.at("status").text();
        if (status == "editorial-reviewed") editorialIds.insert(id);
        else if (status == "pending") pendingIds.insert(id);
        else throw std::runtime_error("unknown V6 admission status: " + status);
        require(item.at("auditionStatus").text() == "pending-human",
                "candidate must retain pending-human audition status: " + id);
        const auto& memberships = item.at("memberships").items();
        membershipRows += memberships.size();
        const auto* annotation = findAnnotation(merged.catalog, id);
        if (memberships.empty()) {
            require(annotation == nullptr, "HCA has an invented mode membership: " + id);
            continue;
        }
        require(annotation != nullptr, "HCA missing metadata memberships: " + id);
        require(annotation->sourceFingerprint == wish::sourceFingerprint(*templateIt->second),
                "V6 HCA fingerprint mismatch: " + id);
        require(annotation->memberships.size() == memberships.size(),
                "V6 HCA membership count mismatch: " + id);
        for (const auto& membership : memberships) {
            require(wish::parseMode(membership.at("mode").text()).has_value(),
                    "unsupported metadata mode token: " + membership.at("mode").text());
            if (status == "pending")
                require(membership.at("review").text() == "pending",
                        "pending candidate has a non-pending membership: " + id);
        }
        const auto resolution = wish::resolveAnnotations(*templateIt->second, merged.catalog);
        require(resolution.state != wish::AnnotationState::Invalid &&
                resolution.state != wish::AnnotationState::SourceMismatch,
                "V6 HCA did not resolve to its exact source: " + id);
    }
    require(metadataIds.size() == candidateById.size(), "metadata and candidates have different counts");
    require(editorialIds == approved, "approved IDs differ from editorial-reviewed metadata IDs");
    require(pendingIds.size() == 4 && editorialIds.size() == 56 &&
            membershipRows == 51 && metadataIds.size() == 60,
            "V6 editorial/membership content counts changed");
    for (const auto& id : pendingIds)
        require(!approved.contains(id), "pending candidate entered the experiment allowlist: " + id);
    for (const auto& [id, item] : candidateById) {
        (void)item;
        require(metadataIds.contains(id), "candidate absent from metadata: " + id);
    }
    std::cout << "annotations v5_sources=" << base.catalog.entries.size()
              << " v6_sources=" << merged.catalog.entries.size() - base.catalog.entries.size()
              << " v6_memberships=" << membershipRows << " candidates_without_membership=9"
              << " byte_prefix_preserved=yes pending_experiment_ids=0 PASS\n";
}

void verifyWishModePools(const library::LoadResult& baseline,
                         const library::LoadResult& experimental,
                         const std::filesystem::path& baseHcaPath,
                         const std::filesystem::path& mergedHcaPath) {
    constexpr std::array modes{
        wish::MusicalMode::Ionian, wish::MusicalMode::Dorian, wish::MusicalMode::Phrygian,
        wish::MusicalMode::Lydian, wish::MusicalMode::Mixolydian, wish::MusicalMode::Aeolian,
        wish::MusicalMode::Locrian, wish::MusicalMode::HarmonicMinor,
        wish::MusicalMode::MelodicMinorAscending, wish::MusicalMode::ClassicalMelodicMinor
    };
    const std::map<wish::MusicalMode, std::size_t> expectedV5Strict{
        {wish::MusicalMode::Ionian, 238}, {wish::MusicalMode::Dorian, 3},
        {wish::MusicalMode::Phrygian, 1}, {wish::MusicalMode::Lydian, 3},
        {wish::MusicalMode::Mixolydian, 8}, {wish::MusicalMode::Aeolian, 64},
        {wish::MusicalMode::Locrian, 0}, {wish::MusicalMode::HarmonicMinor, 29},
        {wish::MusicalMode::MelodicMinorAscending, 0},
        {wish::MusicalMode::ClassicalMelodicMinor, 0}
    };
    const auto baseCatalog = wish::parseAnnotations(readText(baseHcaPath));
    const auto mergedCatalog = wish::parseAnnotations(readText(mergedHcaPath));
    require(static_cast<bool>(baseCatalog), "WishPool V5 annotations: " + baseCatalog.error);
    require(static_cast<bool>(mergedCatalog), "WishPool V6 annotations: " + mergedCatalog.error);
    const auto collect = [&](const library::LoadResult& factory,
                             const wish::AnnotationCatalog& annotations,
                             const char* label,
                             bool checkV5) {
        std::cout << "wish_pool label=" << label << " library=" << factory.libraryVersion
                  << " entries=" << factory.templates.size() << " fixed_tonic=C\n";
        for (const auto mode : modes) {
            std::set<std::string> eligibleIds;
            for (const auto& entry : annotations.entries)
                if (std::any_of(entry.memberships.begin(), entry.memberships.end(), [&](const auto& membership) {
                    return membership.mode == mode && membership.review == wish::ReviewStatus::Reviewed;
                })) eligibleIds.insert(entry.templateId);
            std::vector<ProgressionTemplate> eligibleSources;
            for (const auto& item : factory.templates)
                if (eligibleIds.contains(item.id)) eligibleSources.push_back(item);
            const auto count = [&](bool strict) {
                wish::WishFilter filter;
                filter.fixedTonic = PitchClass::C;
                filter.modes = {mode};
                filter.strict = strict;
                const auto pool = wish::makeFreePool(eligibleSources, annotations, filter);
                require(static_cast<bool>(pool), std::string("WishPool ") + label + ": " + pool.error);
                require(pool.sourceMismatches == 0 && pool.invalidSources == 0 &&
                        pool.invalidAnnotations == 0,
                        std::string("WishPool source integrity failed for ") + label + "/" + wish::modeName(mode));
                return pool.items.size();
            };
            const auto loose = count(false);
            const auto exact = count(true);
            if (checkV5) require(exact == expectedV5Strict.at(mode),
                std::string("V5 strict fixed-C count changed for ") + wish::modeName(mode) +
                ": expected " + std::to_string(expectedV5Strict.at(mode)) + " got " + std::to_string(exact));
            std::cout << "wish_mode corpus=" << label << " mode=" << wish::modeName(mode)
                      << " nonstrict=" << loose << " strict=" << exact
                      << " reviewed_sources=" << eligibleSources.size();
            std::cout << '\n';
        }
    };
    (void)collect(baseline, baseCatalog.catalog, "FactoryV5", true);
    (void)collect(experimental, mergedCatalog.catalog, "FactoryV6Experiment", false);
}

void verifyMidiRoundTrip(const preview::Sequence& sequence) {
    const auto clip = midi::buildClip(sequence, midi::ArrangementMode::VoiceLed,
                                      midi::ExportScope::FullPhrase, {});
    require(static_cast<bool>(clip), "MIDI clip build");
    for (const auto& event : sequence.events) {
        unsigned intervals{};
        int bass = -1;
        for (const auto& note : clip.sequence.notes) {
            if (std::abs(note.startQN - event.startQN) >= 1e-7) continue;
            require(note.midiNote >= 0 && note.midiNote <= 127 && note.durationQN > 0,
                    "MIDI note bounds");
            if (note.bass) bass = note.midiNote % 12;
            else intervals |= 1u << ((note.midiNote - event.chord.root + 120) % 12);
        }
        require(bass == event.chord.bass, "MIDI bass changed: " + event.chord.label);
        if (event.chord.exactIntervals)
            require(intervals == static_cast<unsigned>(event.chord.intervals | event.chord.colorMask),
                    "MIDI pitch set changed: " + event.chord.label);
    }
    const auto written = midi::writeToMemory(clip.sequence);
    require(written && written.bytes.size() > 14, "SMF write");
    const auto reread = midi::readFromMemory(written.bytes);
    require(reread && reread.file.notes.size() == clip.sequence.notes.size(), "SMF readback");
    for (const auto& note : clip.sequence.notes)
        require(std::any_of(reread.file.notes.begin(), reread.file.notes.end(), [&](const auto& actual) {
            return actual.pitch == note.midiNote &&
                   std::abs(actual.startQN - note.startQN) < 1.0 / reread.file.ppq &&
                   std::abs(actual.durationQN - note.durationQN) < 2.0 / reread.file.ppq;
        }), "SMF changed note pitch or timing");
}

void verifyPreviewAudio(const preview::Sequence& sequence) {
    preview::PreviewSynth synth;
    synth.prepare(48000);
    require(synth.start(sequence), "preview synth start");
    std::array<float, 4096> left{}, right{};
    synth.process(left.data(), right.data(), static_cast<std::uint32_t>(left.size()));
    require(std::any_of(left.begin(), left.end(), [](float value) { return std::abs(value) > 1e-5f; }),
            "preview was silent");
}

void exactOutput(const preview::Sequence& sequence) {
    verifyMidiRoundTrip(sequence);
    verifyPreviewAudio(sequence);
}

void verifySnapshotAndPreview(const library::LoadResult& experimental,
                              const ProgressionTemplate& item,
                              const CandidateIndex& index) {
    const KeySignature key{PitchClass(0), item.mode};
    const auto prefixLength = item.full.size() > 2 ? item.full.size() - 1 : 1;
    const auto input = makePrefix(item, key, prefixLength);
    AnalysisContext context;
    context.forcedKey = key;
    RecommendationRequest request;
    request.preferredIntent = item.intent;
    auto weights = recommendationWeights();
    weights.perGroup = 0;
    const auto results = recommendContinuations(makeMatchQuery(input, recommendationContext(context)),
                                                index, request, weights);
    const auto* chosen = findCandidate(results, item.id);
    require(chosen != nullptr, "approved candidate is absent from ranked experiment results: " + item.id);
    const auto chosenContainsSource = chosen->primaryTemplate == item.id ||
        std::find(chosen->supportingTemplates.begin(), chosen->supportingTemplates.end(), item.id) !=
            chosen->supportingTemplates.end();
    require(chosenContainsSource, "ranked candidate lost its approved source: " + item.id);
    const auto previewResult = preview::buildSequence(
        ImportedProgressionSession{input, TimelineCoordinateMode::RelativeToSelection}, chosen, 120.0);
    require(static_cast<bool>(previewResult), "V6 preview build");
    exactOutput(previewResult.sequence);

    ImportedProgressionSession imported;
    require(imported.replace(input, TimelineCoordinateMode::RelativeToSelection), "V6 imported session");
    auto captured = snapshot::capture(imported, *chosen, results.matches, 120.0, 4, 4, key,
                                      {}, item.intent);
    captured.factoryLibraryVersion = 6;
    const auto encoded = snapshot::serialize(captured);
    const auto restored = snapshot::deserialize(encoded, &experimental);
    require(static_cast<bool>(restored), "V6 snapshot readback: " + restored.error);
    require(restored.value.factoryLibraryVersion == 6 && restored.value.candidateAvailable &&
            restored.value.candidate.primaryTemplate == chosen->primaryTemplate &&
            restored.value.candidate.supportingTemplates == chosen->supportingTemplates &&
            (restored.value.candidate.primaryTemplate == item.id ||
             std::find(restored.value.candidate.supportingTemplates.begin(),
                       restored.value.candidate.supportingTemplates.end(), item.id) !=
                 restored.value.candidate.supportingTemplates.end()),
            "V6 snapshot candidate reference");
    const auto restoredPreview = preview::buildSequence(restored.value.imported,
                                                        &restored.value.candidate, 120.0);
    require(static_cast<bool>(restoredPreview), "restored V6 preview build");
    exactOutput(restoredPreview.sequence);
}

struct QueryCase {
    const ProgressionTemplate* item{};
    int tonic{};
    std::size_t prefix{};
};

struct TimingSummary {
    double medianMs{}, p95Ms{}, maxMs{};
};

TimingSummary measure(const CandidateIndex& index, const std::vector<QueryCase>& cases,
                      int repetitions) {
    std::vector<double> elapsedMs;
    RecommendationRequest request;
    auto weights = recommendationWeights();
    weights.perGroup = 0;
    for (int repeat = 0; repeat < repetitions; ++repeat) {
        for (const auto& test : cases) {
            const KeySignature key{PitchClass(test.tonic), test.item->mode};
            const auto query = queryFor(*test.item, key, test.prefix);
            const auto start = Clock::now();
            const auto result = recommendContinuations(query, index, request, weights);
            const auto finish = Clock::now();
            require(result.retrievalTimingMode == RetrievalTimingMode::Unified,
                    "benchmark changed retrieval policy");
            elapsedMs.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
        }
    }
    require(!elapsedMs.empty(), "empty benchmark suite");
    std::sort(elapsedMs.begin(), elapsedMs.end());
    const auto quantile = [&](double fraction) {
        const auto at = static_cast<std::size_t>(std::ceil(fraction * elapsedMs.size())) - 1;
        return elapsedMs[std::min(at, elapsedMs.size() - 1)];
    };
    return {quantile(0.50), quantile(0.95), elapsedMs.back()};
}

void reportTiming(const char* label, std::size_t poolSize, const TimingSummary& timing) {
    std::cout << "timing label=" << label << " candidates=" << poolSize
              << " shortlist_limit=800 median_ms=" << timing.medianMs
              << " p95_ms=" << timing.p95Ms << " max_ms=" << timing.maxMs
              << (poolSize <= 800 ? " full_scan=yes" : " full_scan=no") << '\n';
}

std::vector<ProgressionTemplate> syntheticPool(const std::vector<ProgressionTemplate>& source,
                                               std::size_t targetSize) {
    require(source.size() <= targetSize, "synthetic target below source size");
    auto result = source;
    for (std::size_t i = result.size(); i < targetSize; ++i) {
        auto item = source[(i - source.size()) % source.size()];
        item.id = "SYNTHETIC_STRESS_" + std::to_string(i);
        item.name = item.id;
        item.nameEn = item.id;
        item.nameZh = item.id;
        result.push_back(std::move(item));
    }
    return result;
}

void verifySyntheticShortlistFairness(const QueryCase& test,
                                      const std::vector<ProgressionTemplate>& source,
                                      std::size_t targetSize) {
    auto ordered = syntheticPool(source, targetSize);
    auto shuffled = ordered;
    std::mt19937 generator(static_cast<std::mt19937::result_type>(20261024u + targetSize));
    std::shuffle(shuffled.begin(), shuffled.end(), generator);
    const CandidateIndex cappedIndex(ordered);
    const CandidateIndex shuffledCappedIndex(shuffled);
    const CandidateIndex fullIndex(std::move(ordered));
    const auto query = queryFor(*test.item, {PitchClass(test.tonic), test.item->mode}, test.prefix);
    MatchWeights cappedWeights;
    cappedWeights.shortlistLimit = 800;
    MatchWeights fullWeights = cappedWeights;
    fullWeights.shortlistLimit = targetSize;
    const auto capped = matchProgression(query, cappedIndex, cappedWeights, targetSize);
    const auto shuffledCapped = matchProgression(query, shuffledCappedIndex, cappedWeights, targetSize);
    const auto full = matchProgression(query, fullIndex, fullWeights, targetSize);
    const auto ids = [](const std::vector<MatchResult>& results) {
        std::vector<std::string> result;
        result.reserve(results.size());
        for (const auto& item : results) result.push_back(item.templateId);
        return result;
    };
    const auto cappedIds = ids(capped);
    const auto shuffledIds = ids(shuffledCapped);
    const auto fullIds = ids(full);
    const std::set<std::string> cappedSet(cappedIds.begin(), cappedIds.end());
    const std::set<std::string> shuffledSet(shuffledIds.begin(), shuffledIds.end());
    const std::set<std::string> fullSet(fullIds.begin(), fullIds.end());
    const auto intersectionSize = [](const auto& left, const auto& right) {
        std::size_t count{};
        for (const auto& value : left) if (right.contains(value)) ++count;
        return count;
    };
    const auto top20End = std::min<std::size_t>(20, fullIds.size());
    const std::set<std::string> fullTop20(fullIds.begin(), fullIds.begin() + top20End);
    const auto targetRank = [](const auto& values, const std::string& id) -> std::size_t {
        const auto found = std::find(values.begin(), values.end(), id);
        return found == values.end() ? 0 : static_cast<std::size_t>(found - values.begin()) + 1;
    };
    const auto sharedSet = intersectionSize(cappedSet, shuffledSet);
    const auto reorderedCapped = std::vector<std::string>(cappedIds.begin(), cappedIds.end());
    const auto reorderedShuffled = std::vector<std::string>(shuffledIds.begin(), shuffledIds.end());
    std::cout << "SYNTHETIC_STRESS_ONLY shortlist_fairness pool=" << targetSize
              << " query_source=" << test.item->id << " prefix=" << test.prefix
              << " full_matches=" << fullIds.size() << " capped_matches=" << cappedIds.size()
              << " cap_source_recall=" << intersectionSize(cappedSet, fullSet) << '/' << fullSet.size()
              << " cap_full_top20_retained=" << intersectionSize(cappedSet, fullTop20) << "/" << fullTop20.size()
              << " query_source_rank_full=" << targetRank(fullIds, test.item->id)
              << " query_source_rank_cap=" << targetRank(cappedIds, test.item->id)
              << " shuffled_cap_matches=" << shuffledIds.size()
              << " order_shared_sources=" << sharedSet
              << " order_union_sources=" << (cappedSet.size() + shuffledSet.size() - sharedSet)
              << " exact_order_equal=" << (reorderedCapped == reorderedShuffled ? "yes" : "no")
              << " cap_vs_shuffle_changed_sources="
              << (cappedSet.size() + shuffledSet.size() - 2 * sharedSet) << '\n';
}

void verifyStableRanking(const CandidateIndex& index, const ProgressionTemplate& item) {
    const KeySignature key{PitchClass(0), item.mode};
    const auto query = queryFor(item, key, item.full.size() > 2 ? item.full.size() - 1 : 1);
    RecommendationRequest request;
    auto weights = recommendationWeights();
    weights.perGroup = 0;
    const auto first = recommendContinuations(query, index, request, weights);
    const auto second = recommendContinuations(query, index, request, weights);
    require(rankedIds(first) == rankedIds(second), "ranking order changed for identical input");
}

std::set<std::string> verifyRecall(const library::LoadResult& experimental,
                                   const std::set<std::string>& approved,
                                   const CandidateIndex& index) {
    require(!approved.empty(), "approved-ids.txt contains no experiment candidates");
    std::size_t prefixCases{};
    std::size_t transpositionCases{};
    std::size_t fixedCMinorCases{};
    std::size_t approvedMinorCandidates{};
    std::size_t qualifiedAtLastPrefix{};
    std::set<std::string> lastPrefixQualified;
    for (const auto& id : approved) {
        const auto& item = find(experimental, id);
        for (std::size_t prefix = 1; prefix < item.full.size(); ++prefix) {
            if (prefix == 1 && item.full.size() > 2) continue;
            const KeySignature key{PitchClass(0), item.mode};
            const auto query = queryFor(item, key, prefix);
            const auto matches = matchProgression(query, index, {}, index.templates().size());
            require(std::any_of(matches.begin(), matches.end(), [&](const auto& match) {
                return match.templateId == id && match.continuationLength > 0;
            }), "approved candidate missed legal prefix " + std::to_string(prefix) + ": " + id);
            ++prefixCases;
        }
        const auto prefix = item.full.size() > 2 ? item.full.size() - 1 : 1;
        if (item.mode == Mode::Minor) {
            ++approvedMinorCandidates;
            const KeySignature fixedCMinor{PitchClass::C, Mode::Minor};
            const auto query = queryFor(item, fixedCMinor, prefix);
            require(query.interpretations.size() == 1 &&
                    query.interpretations.front().key.key.tonic == PitchClass::C &&
                    query.interpretations.front().key.key.mode == Mode::Minor,
                    "fixed-C-minor query did not preserve the requested coordinate: " + id);
            const auto matches = matchProgression(query, index, {}, index.templates().size());
            require(std::any_of(matches.begin(), matches.end(), [&](const auto& match) {
                return match.templateId == id && match.continuationLength > 0;
            }), "approved minor candidate missed explicit fixed-C-minor recall: " + id);
            ++fixedCMinorCases;
        }
        for (int shift = 0; shift < 12; ++shift) {
            const KeySignature key{PitchClass(shift), item.mode};
            const auto query = queryFor(item, key, prefix);
            const auto matches = matchProgression(query, index, {}, index.templates().size());
            require(std::any_of(matches.begin(), matches.end(), [&](const auto& match) {
                return match.templateId == id && match.continuationLength > 0;
            }), "approved candidate missed transposition " + std::to_string(shift) + ": " + id);
            ++transpositionCases;
            if (shift == 0) {
                RecommendationRequest request;
                auto weights = recommendationWeights();
                weights.perGroup = 0;
                const auto ranked = recommendContinuations(query, index, request, weights);
                const auto* visible = findCandidate(ranked, id);
                if (visible) {
                    ++qualifiedAtLastPrefix;
                    lastPrefixQualified.insert(id);
                    const auto position = groupRank(ranked, visible->id);
                    require(position.has_value(), "qualified candidate has no group position: " + id);
                    const auto defaultResults = recommendContinuations(
                        query, index, request, RecommendationWeights{});
                    const auto* defaultVisible = findCandidate(defaultResults, id);
                    const auto defaultPosition = defaultVisible
                        ? groupRank(defaultResults, defaultVisible->id) : std::nullopt;
                    require((defaultVisible != nullptr) == (position->second <= 3),
                            "perGroup=3 visibility disagrees with unbounded group rank: " + id);
                    std::cout << "recommendation_last_prefix id=" << id << " prefix=" << prefix
                              << " qualified=yes group=" << groupName(position->first)
                              << " unbounded_rank=" << position->second
                              << " perGroup3_visible=" << (defaultVisible ? "yes" : "no");
                    if (defaultPosition)
                        std::cout << " perGroup3_rank=" << defaultPosition->second;
                    std::cout
                              << " primary=" << visible->primaryTemplate
                              << " source_role=" << (visible->primaryTemplate == id ? "primary" : "supporting")
                              << " supports=";
                    for (std::size_t source = 0; source < visible->supportingTemplates.size(); ++source) {
                        if (source) std::cout << ',';
                        std::cout << visible->supportingTemplates[source];
                    }
                    std::cout << '\n';
                } else {
                    bool reported{};
                    for (const auto& match : ranked.matches) if (match.templateId == id) {
                        std::cout << "recommendation_ineligible id=" << id
                                  << " disposition=" << match.disposition
                                  << " similarity=" << match.similarity
                                  << " eligibility_score=" << match.candidateEligibilityScore
                                  << " ranking_score=" << match.candidateRankingScore << '\n';
                        reported = true;
                    }
                    if (!reported)
                        std::cout << "recommendation_ineligible id=" << id << " match_result=missing\n";
                }
            }
        }
    }
    require(approvedMinorCandidates > 0 && fixedCMinorCases == approvedMinorCandidates,
            "fixed-C-minor qualification did not cover every approved minor candidate");
    std::cout << "recall approved=" << approved.size() << " legal_prefix_cases=" << prefixCases
              << " transposition_cases=" << transpositionCases
              << " fixed_C_minor_cases=" << fixedCMinorCases
              << " qualified_at_last_nonfull_prefix=" << qualifiedAtLastPrefix
              << '/' << approved.size() << " PASS\n";
    return lastPrefixQualified;
}

std::set<std::string> verifyTargetPrefixQualification(
    const library::LoadResult& experimental, const CandidateIndex& index,
    const std::set<std::string>& approved, std::set<std::string> anyPrefixQualified) {
    constexpr std::array targets{
        "V6_B1_022", "V6_B1_023", "V6_B1_027", "V6_B1_029", "V6_B1_030", "V6_B1_039"
    };
    std::size_t testedPrefixes{};
    std::size_t qualifiedPrefixes{};
    for (const auto* id : targets) {
        require(approved.contains(id), "targeted qualification ID is not approved: " + std::string(id));
        const auto& item = find(experimental, id);
        std::optional<std::size_t> shortestQualified;
        std::optional<std::size_t> shortestTop3Visible;
        for (std::size_t prefix = 2; prefix < item.full.size(); ++prefix) {
            const auto query = queryFor(item, {PitchClass::C, item.mode}, prefix);
            RecommendationRequest request;
            auto weights = recommendationWeights();
            weights.perGroup = 0;
            const auto qualifiedResults = recommendContinuations(query, index, request, weights);
            const auto* candidate = findCandidate(qualifiedResults, id);
            const auto match = std::find_if(qualifiedResults.matches.begin(), qualifiedResults.matches.end(),
                [&](const auto& result) { return result.templateId == id; });
            ++testedPrefixes;
            if (!candidate) {
                std::cout << "qualification_prefix id=" << id << " prefix=" << prefix
                          << " qualified=no";
                if (match != qualifiedResults.matches.end())
                    std::cout << " disposition=" << match->disposition
                              << " similarity=" << match->similarity
                              << " eligibility_score=" << match->candidateEligibilityScore
                              << " ranking_score=" << match->candidateRankingScore;
                else std::cout << " match_result=missing";
                std::cout << '\n';
                continue;
            }
            ++qualifiedPrefixes;
            anyPrefixQualified.insert(id);
            if (!shortestQualified) shortestQualified = prefix;
            const auto position = groupRank(qualifiedResults, candidate->id);
            require(position.has_value(), "qualified candidate has no group position: " + std::string(id));

            auto top3Weights = RecommendationWeights{};
            const auto top3Results = recommendContinuations(query, index, request, top3Weights);
            const auto* top3Candidate = findCandidate(top3Results, id);
            const auto top3Position = top3Candidate ? groupRank(top3Results, top3Candidate->id) : std::nullopt;
            if (top3Candidate && !shortestTop3Visible) shortestTop3Visible = prefix;
            const bool isPrimary = candidate->primaryTemplate == id;
            std::cout << "qualification_prefix id=" << id << " prefix=" << prefix
                      << " qualified=yes group=" << groupName(position->first)
                      << " unbounded_rank=" << position->second
                      << " top3_visible=" << (top3Candidate ? "yes" : "no");
            if (top3Position)
                std::cout << " top3_rank=" << top3Position->second;
            std::cout << " primary=" << candidate->primaryTemplate
                      << " source_role=" << (isPrimary ? "primary" : "supporting")
                      << " supports=";
            for (std::size_t source = 0; source < candidate->supportingTemplates.size(); ++source) {
                if (source) std::cout << ',';
                std::cout << candidate->supportingTemplates[source];
            }
            std::cout << " eligibility_score=" << candidate->eligibilityScore
                      << " ranking_score=" << candidate->rankingScore << '\n';
        }
        std::cout << "qualification_candidate id=" << id << " tested_legal_prefixes="
                  << (item.full.size() > 2 ? item.full.size() - 2 : 0)
                  << " shortest_qualified_prefix=";
        if (shortestQualified) std::cout << *shortestQualified;
        else std::cout << "none";
        std::cout << " shortest_top3_visible_prefix=";
        if (shortestTop3Visible) std::cout << *shortestTop3Visible;
        else std::cout << "none";
        std::cout << " any_legal_prefix_qualified="
                  << (shortestQualified || anyPrefixQualified.contains(id) ? "yes" : "no") << '\n';
    }
    std::cout << "qualification_coverage scope=50-observed-last-nonfull-plus-six-all-prefixes"
              << " candidates=" << anyPrefixQualified.size() << '/' << approved.size()
              << " qualified_at_any_tested_legal_prefix=" << anyPrefixQualified.size() << '/' << approved.size()
              << " targeted_prefixes_tested=" << testedPrefixes
              << " targeted_qualified_prefixes=" << qualifiedPrefixes << '\n';
    return anyPrefixQualified;
}

void verifyApprovedMidi(const std::vector<ProgressionTemplate>& candidates,
                        const std::set<std::string>& approved) {
    std::size_t chords{};
    for (const auto& id : approved) {
        const auto& item = find(candidates, id);
        const auto route = preview::buildTemplateSequence(
            item, {PitchClass::C, item.mode}, preview::defaultTempoBPM);
        require(static_cast<bool>(route), "cannot build full V6 route for MIDI verification: " + id);
        verifyMidiRoundTrip(route.sequence);
        chords += route.sequence.events.size();
    }
    std::cout << "midi approved_full_routes=" << approved.size() << " chord_events=" << chords
              << " full_pitch_masks_and_smf_readback=PASS\n";
}

void writeRepresentativeMedia(const std::vector<ProgressionTemplate>& candidates,
                              const std::set<std::string>& approved,
                              const std::filesystem::path& outputDirectory) {
    struct Sample { const char* id; const char* role; bool pending; };
    const std::array samples{
        Sample{"V6_B1_001", "Dorian", false},
        Sample{"V6_B1_011", "Lydian", false},
        Sample{"V6_B1_017", "Phrygian", false},
        Sample{"V6_B1_016", "Phrygian-pedal", false},
        Sample{"V6_B1_025", "chromatic-slash-bass", false},
        Sample{"V6_B1_032", "suspended-altered", false},
        Sample{"V6_B1_050", "suspension", false},
        Sample{"V6_B1_045", "jazz-melodic-minor", false},
        Sample{"V6_B1_059", "Locrian-pending", true},
        Sample{"V6_B1_060", "Locrian-pending", true}
    };
    std::filesystem::create_directories(outputDirectory);
    std::ofstream index(outputDirectory / "media-index.tsv", std::ios::binary);
    require(static_cast<bool>(index), "cannot write representative media index");
    index << "id\trole\tpool_status\tfull_midi\tfull_preview_wav\n";
    for (const auto& sample : samples) {
        const auto& item = find(candidates, sample.id);
        require(sample.pending ? !approved.contains(item.id) : approved.contains(item.id),
                "representative approval state changed: " + item.id);
        const auto route = preview::buildTemplateSequence(
            item, {PitchClass::C, item.mode}, preview::defaultTempoBPM);
        require(static_cast<bool>(route), "representative V6 route build failed: " + item.id);
        if (item.id == "V6_B1_025") {
            constexpr std::array<int, 7> expectedBass{0, 11, 10, 9, 8, 7, 0};
            require(route.sequence.events.size() == expectedBass.size(),
                    "chromatic slash-bass phrase event count changed");
            for (std::size_t event = 0; event < expectedBass.size(); ++event)
                require(route.sequence.events[event].chord.bass == expectedBass[event],
                        "chromatic slash-bass route changed at event " + std::to_string(event));
        }
        verifyMidiRoundTrip(route.sequence);
        const auto midiFile = midi::buildClip(route.sequence, midi::ArrangementMode::VoiceLed,
                                              midi::ExportScope::FullPhrase, {});
        require(static_cast<bool>(midiFile), "representative MIDI clip failed: " + item.id);
        const auto midiBytes = midi::writeToMemory(midiFile.sequence);
        require(midiBytes && midiBytes.bytes.size() > 14, "representative MIDI serialization failed: " + item.id);
        const auto midiPath = outputDirectory / (item.id + "-complete.mid");
        std::ofstream midiOut(midiPath, std::ios::binary);
        require(static_cast<bool>(midiOut), "cannot write representative MIDI: " + item.id);
        midiOut.write(reinterpret_cast<const char*>(midiBytes.bytes.data()),
                      static_cast<std::streamsize>(midiBytes.bytes.size()));
        require(static_cast<bool>(midiOut), "cannot finish representative MIDI: " + item.id);

        const auto audio = preview::renderOffline(route.sequence, 48000.0);
        require(!audio.left.empty() && audio.left.size() == audio.right.size() && audio.rms > 0,
                "representative preview render is silent: " + item.id);
        std::string error;
        const auto wavPath = outputDirectory / (item.id + "-preview.wav");
        require(preview::writeWav16(audio, wavPath, error),
                "cannot write representative preview WAV: " + error);
        index << item.id << '\t' << sample.role << '\t'
              << (sample.pending ? "pending-not-in-experiment" : "approved-experiment") << '\t'
              << midiPath.filename().string() << '\t' << wavPath.filename().string() << '\n';
        std::cout << "media id=" << item.id << " role=" << sample.role
                  << " status=" << (sample.pending ? "pending" : "approved")
                  << " midi_bytes=" << midiBytes.bytes.size() << " wav_frames=" << audio.left.size()
                  << " rms=" << audio.rms << '\n';
    }
    require(static_cast<bool>(index), "cannot finish representative media index");
}
} // namespace

int main(int argc, char** argv) {
    static_assert(library::factorySchemaVersion == 2);
    static_assert(library::userSchemaVersion == 2);
    static_assert(snapshot::RecommendationSnapshot::currentSchemaVersion == 3);
    static_assert(session::PluginSessionState::currentSchemaVersion == 5);
    require(argc == 10, "usage: FactoryV6Tests v5.db candidates.json metadata.json approved-ids.txt "
                        "proposal.db experiment.db factory-v5.hca merged-v6.hca media-dir");
    try {
        const auto baseline = library::loadFactory(argv[1]);
        const auto proposal = library::loadFactory(argv[5]);
        const auto experimental = library::loadFactory(argv[6]);
        require(static_cast<bool>(baseline), "cannot load V5 baseline: " + baseline.error);
        require(static_cast<bool>(proposal), "cannot load proposal DB: " + proposal.error);
        require(static_cast<bool>(experimental), "cannot load experiment DB: " + experimental.error);
        require(baseline.libraryVersion == 5 && baseline.storageSchemaVersion == 2 &&
                baseline.templates.size() == 658, "Factory V5 baseline contract");
        require(proposal.libraryVersion == 6 && proposal.storageSchemaVersion == 2,
                "proposal DB version/schema");
        require(experimental.libraryVersion == 6 && experimental.storageSchemaVersion == 2,
                "experiment DB version/schema");

        const auto parsed = dev::parseTemplateJson(readText(argv[2]), true);
        require(static_cast<bool>(parsed), "cannot parse candidates.json: " + parsed.error);
        std::set<std::string> candidateIds;
        for (const auto& item : parsed.templates)
            require(candidateIds.insert(item.id).second, "duplicate candidate ID: " + item.id);
        const auto approved = readApprovedIds(argv[4]);
        for (const auto& id : approved)
            require(candidateIds.contains(id), "approved ID is absent from candidates.json: " + id);
        verifyAnnotations(baseline, parsed.templates, approved, argv[3], argv[7], argv[8]);
        requirePreserved(baseline, proposal, candidateIds);
        requirePreserved(baseline, experimental, approved);
        const auto minorCandidates = std::count_if(parsed.templates.begin(), parsed.templates.end(),
            [](const auto& item) { return item.mode == Mode::Minor; });
        const auto approvedMinorCandidates = std::count_if(parsed.templates.begin(), parsed.templates.end(),
            [&](const auto& item) { return item.mode == Mode::Minor && approved.contains(item.id); });
        require(approvedMinorCandidates <= minorCandidates,
                "approved minor coordinate count exceeds all candidate minor coordinate count");
        std::cout << "coordinate_modes candidates_minor=" << minorCandidates
                  << " approved_minor=" << approvedMinorCandidates
                  << " pending_or_unapproved_minor=" << (minorCandidates - approvedMinorCandidates) << '\n';
        std::cout << "integrity v5=658 proposal=" << proposal.templates.size()
                  << " experiment=" << experimental.templates.size()
                  << " candidates=" << candidateIds.size() << " approved=" << approved.size()
                  << " schema=Factory2/User2/Session5/Snapshot3 PASS\n";

        const CandidateIndex proposalIndex(proposal.templates);
        const CandidateIndex experimentalIndex(experimental.templates);
        verifyWishModePools(baseline, experimental, argv[7], argv[8]);
        verifyApprovedMidi(parsed.templates, approved);
        writeRepresentativeMedia(parsed.templates, approved, argv[9]);
        auto anyPrefixQualified = verifyRecall(experimental, approved, experimentalIndex);
        anyPrefixQualified = verifyTargetPrefixQualification(
            experimental, experimentalIndex, approved, std::move(anyPrefixQualified));
        verifyStableRanking(experimentalIndex, find(experimental, *approved.begin()));
        verifySnapshotAndPreview(experimental, find(experimental, *approved.begin()), experimentalIndex);

        std::vector<QueryCase> cases;
        for (const auto& id : approved) {
            if (cases.size() == 12) break;
            const auto& item = find(experimental, id);
            cases.push_back({&item, 0, item.full.size() > 2 ? item.full.size() - 1 : 1});
        }
        if (cases.empty()) {
            const auto& item = parsed.templates.front();
            cases.push_back({&item, 0, item.full.size() > 2 ? item.full.size() - 1 : 1});
        }
        std::vector<QueryCase> transposed;
        for (const auto& item : cases)
            for (int shift = 0; shift < 12; ++shift)
                transposed.push_back({item.item, shift, item.prefix});

        reportTiming("FactoryV5", baseline.templates.size(),
                     measure(CandidateIndex(baseline.templates), transposed, 2));
        reportTiming("FactoryV6Proposal", proposal.templates.size(), measure(proposalIndex, transposed, 2));
        reportTiming("FactoryV6Experiment", experimental.templates.size(),
                     measure(experimentalIndex, transposed, 2));
        verifyStableRanking(proposalIndex, *transposed.front().item);

        for (const auto stressSize : {std::size_t{1000}, std::size_t{1200}}) {
            auto synthetic = syntheticPool(proposal.templates, stressSize);
            const CandidateIndex stressIndex(std::move(synthetic));
            reportTiming("SYNTHETIC_STRESS_ONLY", stressIndex.templates().size(),
                         measure(stressIndex, transposed, 1));
            verifyStableRanking(stressIndex, *transposed.front().item);
            verifySyntheticShortlistFairness(transposed.front(), proposal.templates, stressSize);
        }

        std::cout << "Factory V6 integration PASS; pending/unapproved IDs are absent from experiment DB.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Factory V6 integration FAIL: " << error.what() << '\n';
        return 1;
    }
}
