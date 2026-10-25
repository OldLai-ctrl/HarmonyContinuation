#include "TemplateJson.h"
#include "benchmark/BenchJson.h"
#include "core/ContinuationEngine.h"
#include "core/RecommendationPolicy.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "library/LegacyFactoryIdResolver.h"
#include "library/LibraryStore.h"
#include "library/ProgressionLibrary.h"
#include "midi/MidiClip.h"
#include "midi/StandardMidiFileReader.h"
#include "midi/StandardMidiFileWriter.h"
#include "midi/MidiWorkflow.h"
#include "preview/PreviewSequence.h"
#include "preview/PreviewSynth.h"
#include "preview/OfflinePreviewRenderer.h"
#include "session/PluginSessionState.h"
#include "snapshot/RecommendationSnapshot.h"
#include "wish/ModeAnnotations.h"
#include "wish/MusicIdentity.h"
#include "wish/WishSampler.h"
#include "wish/WishCatalog.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string_view>

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

std::string rankedSourceDifference(const RecommendationSet& expected,
                                   const RecommendationSet& actual,
                                   float similarityTolerance,
                                   float scoreTolerance) {
    for (std::size_t group = 0; group < expected.groups.size(); ++group) {
        const auto& expectedEntries = expected.groups[group];
        const auto& actualEntries = actual.groups[group];
        if (expectedEntries.size() != actualEntries.size()) {
            return std::string{"group="} + groupName(group) + " entry_count=" +
                   std::to_string(expectedEntries.size()) + " vs " +
                   std::to_string(actualEntries.size());
        }
        for (std::size_t rank = 0; rank < expectedEntries.size(); ++rank) {
            const auto& left = expectedEntries[rank];
            const auto& right = actualEntries[rank];
            if (left.primaryTemplate != right.primaryTemplate ||
                left.supportingTemplates != right.supportingTemplates) {
                std::ostringstream difference;
                difference << "group=" << groupName(group) << " rank=" << (rank + 1)
                           << " sources=" << left.primaryTemplate << "/";
                for (std::size_t source = 0; source < left.supportingTemplates.size(); ++source) {
                    if (source) difference << ',';
                    difference << left.supportingTemplates[source];
                }
                difference << " vs " << right.primaryTemplate << "/";
                for (std::size_t source = 0; source < right.supportingTemplates.size(); ++source) {
                    if (source) difference << ',';
                    difference << right.supportingTemplates[source];
                }
                return difference.str();
            }
            const auto scoreDifference = [&](const char* name, float leftScore, float rightScore,
                                             float tolerance) -> std::string {
                if (std::abs(leftScore - rightScore) <= tolerance) return {};
                std::ostringstream difference;
                difference << "group=" << groupName(group) << " rank=" << (rank + 1)
                           << " source=" << left.primaryTemplate << " score=" << name
                           << " values=" << leftScore << " vs " << rightScore
                           << " tolerance=" << tolerance;
                return difference.str();
            };
            if (const auto difference = scoreDifference(
                    "matchSimilarity", left.matchSimilarity, right.matchSimilarity,
                    similarityTolerance); !difference.empty()) return difference;
            if (const auto difference = scoreDifference(
                    "eligibilityScore", left.eligibilityScore, right.eligibilityScore,
                    scoreTolerance); !difference.empty()) return difference;
            if (const auto difference = scoreDifference(
                    "rankingScore", left.rankingScore, right.rankingScore,
                    scoreTolerance); !difference.empty()) return difference;
        }
    }
    return {};
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
    const std::set<std::string> originalPendingIds{
        "V6_B1_046", "V6_B1_057", "V6_B1_059", "V6_B1_060"};
    const std::set<std::string> originalNoMembershipIds{
        "V6_B1_024", "V6_B1_026", "V6_B1_042", "V6_B1_047", "V6_B1_048",
        "V6_B1_049", "V6_B1_050", "V6_B1_053", "V6_B1_054"};
    const std::set<std::string> representativePassIds{
        "V6_B1_001", "V6_B1_011", "V6_B1_016", "V6_B1_017", "V6_B1_025",
        "V6_B1_032", "V6_B1_045", "V6_B1_050", "V6_B1_059", "V6_B1_060"};
    std::set<std::string> metadataIds, editorialIds, pendingIds, approvedByMetadata, noMembershipIds;
    std::array<std::size_t, 3> gradeCounts{};
    std::size_t membershipRows{};
    std::size_t candidatesWithoutMembership{};
    for (const auto& item : metadata.at("candidates").items()) {
        const auto& id = item.at("id").text();
        require(metadataIds.insert(id).second, "duplicate metadata ID: " + id);
        const auto templateIt = candidateById.find(id);
        require(templateIt != candidateById.end(), "metadata ID absent from candidate JSON: " + id);
        const auto& status = item.at("status").text();
        if (status == "editorial-reviewed") editorialIds.insert(id);
        else if (status == "pending") pendingIds.insert(id);
        else throw std::runtime_error("unknown V6 admission status: " + status);
        const auto& grade = item.at("grade").text();
        const auto gradeIndex = grade == "A" ? 0 : grade == "B" ? 1 : grade == "C" ? 2 : 3;
        require(gradeIndex < gradeCounts.size(), "unknown candidate grade: " + grade);
        ++gradeCounts[gradeIndex];
        const auto* initialGradeValue = item.find("initialGrade");
        const auto* labelActionsValue = item.find("labelActions");
        if (id.starts_with("V6_B1_"))
            require(initialGradeValue != nullptr,
                    "original B1 candidate needs an initialGrade review record: " + id);
        if (initialGradeValue) {
            const auto& initialGrade = initialGradeValue->text();
            require(initialGrade == "A" || initialGrade == "B" || initialGrade == "C",
                    "unknown initial candidate grade: " + initialGrade);
            if (grade != initialGrade)
                require(labelActionsValue && !labelActionsValue->items().empty(),
                        "changed candidate grade requires a labelActions audit record: " + id);
        }
        if (labelActionsValue) (void)labelActionsValue->items();
        const auto& adoptionStatus = item.at("adoptionStatus").text();
        const bool qualifiesForApproval = grade == "A" && status == "editorial-reviewed";
        const std::string_view expectedAdoption = qualifiesForApproval ? "approved" : "pending";
        require(adoptionStatus == expectedAdoption,
                "adoptionStatus disagrees with grade/editorial status: " + id);
        if (qualifiesForApproval) approvedByMetadata.insert(id);

        const auto& auditionStatus = item.at("auditionStatus").text();
        const bool representativePass = auditionStatus == "user-representative-pass";
        require(representativePass || auditionStatus == "pending-human",
                "unknown candidate audition status: " + auditionStatus);
        require(representativePass == representativePassIds.contains(id),
                "representative audition status must match the ten explicitly sampled IDs: " + id);
        if (originalPendingIds.contains(id))
            require(status == "pending" && adoptionStatus == "pending",
                    "original pending candidate was promoted: " + id);
        const auto& memberships = item.at("memberships").items();
        membershipRows += memberships.size();
        if (memberships.empty()) {
            ++candidatesWithoutMembership;
            noMembershipIds.insert(id);
        }
        const auto* annotation = findAnnotation(merged.catalog, id);
        if (memberships.empty()) {
            require(annotation == nullptr, "HCA has an invented mode membership: " + id);
            if (originalNoMembershipIds.contains(id))
                require(grade == "C" && adoptionStatus == "pending",
                        "original no-membership candidate must remain grade C and unadopted: " + id);
            continue;
        }
        require(annotation != nullptr, "HCA missing metadata memberships: " + id);
        require(annotation->sourceFingerprint == wish::sourceFingerprint(*templateIt->second),
                "V6 HCA fingerprint mismatch: " + id);
        require(annotation->memberships.size() == memberships.size(),
                "V6 HCA membership count mismatch: " + id);
        if (originalPendingIds.contains(id))
            require(!memberships.empty(), "original pending candidate lost its membership: " + id);
        for (const auto& membership : memberships) {
            require(wish::parseMode(membership.at("mode").text()).has_value(),
                    "unsupported metadata mode token: " + membership.at("mode").text());
            if (originalPendingIds.contains(id))
                require(membership.at("review").text() == "pending",
                        "original pending candidate has a reviewed membership: " + id);
        }
        const auto resolution = wish::resolveAnnotations(*templateIt->second, merged.catalog);
        require(resolution.state != wish::AnnotationState::Invalid &&
                resolution.state != wish::AnnotationState::SourceMismatch,
                "V6 HCA did not resolve to its exact source: " + id);
    }
    require(metadataIds.size() == candidateById.size(), "metadata and candidates have different counts");
    require(approvedByMetadata == approved,
            "approved IDs differ from grade-A editorial-reviewed metadata IDs");
    for (std::size_t number = 1; number <= 60; ++number) {
        const auto suffix = std::to_string(number);
        const auto id = "V6_B1_" + std::string(3 - suffix.size(), '0') + suffix;
        require(candidateById.contains(id) && metadataIds.contains(id),
                "an original B1 source ID is missing: " + id);
    }
    for (const auto& id : originalPendingIds)
        require(metadataIds.contains(id) && pendingIds.contains(id) && !approved.contains(id),
                "an original pending source is missing or promoted: " + id);
    for (const auto& id : originalNoMembershipIds) {
        require(metadataIds.contains(id) && noMembershipIds.contains(id) && !approved.contains(id),
                "an original no-membership source was omitted or adopted: " + id);
    }
    for (const auto& id : pendingIds)
        require(!approved.contains(id), "pending candidate entered the experiment allowlist: " + id);
    for (const auto& [id, item] : candidateById) {
        (void)item;
        require(metadataIds.contains(id), "candidate absent from metadata: " + id);
    }
    std::cout << "annotations v5_sources=" << base.catalog.entries.size()
              << " v6_sources=" << merged.catalog.entries.size() - base.catalog.entries.size()
              << " v6_memberships=" << membershipRows
              << " v6_candidates_without_membership=" << candidatesWithoutMembership
              << " v6_original_without_membership=" << originalNoMembershipIds.size()
              << " grades_A_B_C=" << gradeCounts[0] << '/' << gradeCounts[1] << '/' << gradeCounts[2]
              << " approved=" << approved.size() << " pending_adoption=" << candidateById.size() - approved.size()
              << " editorial_reviewed=" << editorialIds.size() << " editorial_pending=" << pendingIds.size()
              << " representative_audition_pass=" << representativePassIds.size()
              << " byte_prefix_preserved=yes pending_experiment_ids=0 PASS\n";
}

void verifyWishModePools(const library::LoadResult& baseline,
                         const library::LoadResult& experimental,
                         const std::filesystem::path& baseHcaPath,
                         const std::filesystem::path& mergedHcaPath,
                         bool includeBaseline = true) {
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
    const auto mergedCatalog = wish::parseAnnotations(readText(mergedHcaPath));
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
    if (includeBaseline) {
        const auto baseCatalog = wish::parseAnnotations(readText(baseHcaPath));
        require(static_cast<bool>(baseCatalog), "WishPool V5 annotations: " + baseCatalog.error);
        (void)collect(baseline, baseCatalog.catalog, "FactoryV5", true);
    }
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

struct RankedPrefixSelection {
    std::size_t prefix{};
    Progression input;
    RecommendationSet results;
};

RankedPrefixSelection selectRankedPrefix(const ProgressionTemplate& item, KeySignature key,
                                         const CandidateIndex& index,
                                         const RecommendationRequest& request,
                                         const RecommendationWeights& weights,
                                         bool preferLongest) {
    std::optional<RankedPrefixSelection> selected;
    for (std::size_t prefix = 1; prefix < item.full.size(); ++prefix) {
        auto input = makePrefix(item, key, prefix);
        AnalysisContext context;
        context.forcedKey = key;
        auto results = recommendContinuations(
            makeMatchQuery(input, recommendationContext(context)), index, request, weights);
        if (!findCandidate(results, item.id)) continue;
        RankedPrefixSelection match{prefix, std::move(input), std::move(results)};
        if (!preferLongest) return match;
        selected = std::move(match);
    }
    if (selected) return std::move(*selected);
    throw std::runtime_error("approved candidate is absent from every legal ranked prefix: " + item.id);
}

void verifySnapshotAndPreview(const library::LoadResult& experimental,
                              const ProgressionTemplate& item,
                              const CandidateIndex& index) {
    const KeySignature key{PitchClass(0), item.mode};
    RecommendationRequest request;
    request.preferredIntent = item.intent;
    auto weights = recommendationWeights();
    weights.perGroup = 0;
    auto selected = selectRankedPrefix(item, key, index, request, weights, false);
    const auto* chosen = findCandidate(selected.results, item.id);
    require(chosen != nullptr, "selected legal prefix lost its ranked candidate: " + item.id);
    const auto chosenContainsSource = chosen->primaryTemplate == item.id ||
        std::find(chosen->supportingTemplates.begin(), chosen->supportingTemplates.end(), item.id) !=
            chosen->supportingTemplates.end();
    require(chosenContainsSource, "ranked candidate lost its approved source: " + item.id);
    const auto previewResult = preview::buildSequence(
        ImportedProgressionSession{selected.input, TimelineCoordinateMode::RelativeToSelection}, chosen, 120.0);
    require(static_cast<bool>(previewResult), "V6 preview build");
    exactOutput(previewResult.sequence);

    ImportedProgressionSession imported;
    require(imported.replace(selected.input, TimelineCoordinateMode::RelativeToSelection), "V6 imported session");
    auto captured = snapshot::capture(imported, *chosen, selected.results.matches, 120.0, 4, 4, key,
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
              << " default_shortlist_limit=0 median_ms=" << timing.medianMs
              << " p95_ms=" << timing.p95Ms << " max_ms=" << timing.maxMs
              << " full_scan=yes\n";
}

TimingSummary summarize(std::vector<double> elapsedMs) {
    require(!elapsedMs.empty(), "empty shortlist timing suite");
    std::sort(elapsedMs.begin(), elapsedMs.end());
    const auto quantile = [&](double fraction) {
        const auto at = static_cast<std::size_t>(std::ceil(fraction * elapsedMs.size())) - 1;
        return elapsedMs[std::min(at, elapsedMs.size() - 1)];
    };
    return {quantile(0.50), quantile(0.95), elapsedMs.back()};
}

std::vector<std::string> matchIds(const std::vector<MatchResult>& matches) {
    std::vector<std::string> result;
    result.reserve(matches.size());
    for (const auto& match : matches) result.push_back(match.templateId);
    return result;
}

std::size_t sharedIdCount(const std::vector<std::string>& left,
                          const std::vector<std::string>& right) {
    const std::set<std::string> rightIds(right.begin(), right.end());
    return static_cast<std::size_t>(std::count_if(left.begin(), left.end(),
        [&](const auto& id) { return rightIds.contains(id); }));
}

void verifyActualShortlistGate(const CandidateIndex& shadowIndex,
                               const CandidateIndex& experimentalIndex,
                               const std::vector<QueryCase>& cases,
                               const std::filesystem::path& outputDirectory) {
    require(!cases.empty(), "real shortlist comparison needs query cases");
    std::filesystem::create_directories(outputDirectory);
    const auto reportPath = outputDirectory / "real-shortlist-gate.tsv";
    std::ofstream report(reportPath, std::ios::binary);
    require(static_cast<bool>(report), "cannot write real shortlist gate report");
    report << "record\tmode\tpool\tentries\tcap_inputs\tquery_source\tkey\tprefix\tfull_matches\tcap_matches"
              "\tfull_top20\ttop20_retained\tfull_source_rank\tcap_source_rank\tfull_only_ids"
              "\tdefault_qualified\tdefault_visible\tdefault_group\tdefault_rank\tdefault_top_id"
              "\tcap_ms\tfull_ms\n";

    const auto evaluate = [&](const char* poolLabel, const CandidateIndex& index, bool checkDefault) {
        MatchWeights cappedWeights;
        cappedWeights.shortlistLimit = 800;
        MatchWeights fullWeights;
        std::vector<double> cappedTimes, fullTimes;
        std::size_t fullMatchCount{}, cappedMatchCount{}, top20Count{}, top20Retained{};
        std::size_t fullOnlyCount{}, sourceMisses{}, qualified{}, defaultVisible{};
        std::size_t truncatedQueries{};
        std::array<std::size_t, 4> defaultGroupEntries{};
        bool top20Loss{};
        for (const auto& test : cases) {
            const KeySignature key{PitchClass(test.tonic), test.item->mode};
            const auto query = queryFor(*test.item, key, test.prefix);
            MatchRunStats cappedStats, fullStats;
            auto started = Clock::now();
            const auto cappedMatches = matchProgression(
                query, index, cappedWeights, index.templates().size(), &cappedStats);
            cappedTimes.push_back(std::chrono::duration<double, std::milli>(Clock::now() - started).count());
            started = Clock::now();
            const auto fullMatches = matchProgression(
                query, index, fullWeights, index.templates().size(), &fullStats);
            fullTimes.push_back(std::chrono::duration<double, std::milli>(Clock::now() - started).count());

            const auto cappedIds = matchIds(cappedMatches);
            const auto fullIds = matchIds(fullMatches);
            const auto fullTopCount = std::min<std::size_t>(20, fullIds.size());
            const std::vector<std::string> fullTop(fullIds.begin(), fullIds.begin() + fullTopCount);
            const auto retainedTop = sharedIdCount(fullTop, cappedIds);
            fullMatchCount += fullIds.size();
            cappedMatchCount += cappedIds.size();
            top20Count += fullTopCount;
            top20Retained += retainedTop;
            if (retainedTop != fullTopCount) top20Loss = true;
            if (cappedStats.shortlistSize < fullStats.shortlistSize) ++truncatedQueries;

            const auto rankOf = [](const std::vector<std::string>& ids, const std::string& id) {
                const auto found = std::find(ids.begin(), ids.end(), id);
                return found == ids.end() ? std::size_t{} : static_cast<std::size_t>(found - ids.begin()) + 1;
            };
            const auto fullSourceRank = rankOf(fullIds, test.item->id);
            const auto capSourceRank = rankOf(cappedIds, test.item->id);
            if (fullSourceRank && !capSourceRank) ++sourceMisses;
            std::vector<std::string> omitted;
            for (const auto& id : fullIds)
                if (std::find(cappedIds.begin(), cappedIds.end(), id) == cappedIds.end())
                    omitted.push_back(id);
            fullOnlyCount += omitted.size();

            bool isQualified{};
            bool isVisible{};
            std::string defaultGroup, defaultTopId;
            std::size_t defaultRank{};
            if (checkDefault) {
                RecommendationRequest request;
                auto qualifiedWeights = RecommendationWeights{};
                qualifiedWeights.perGroup = 0;
                const auto eligible = recommendContinuations(query, index, request, qualifiedWeights);
                isQualified = findCandidate(eligible, test.item->id) != nullptr;
                if (isQualified) ++qualified;
                auto weights = RecommendationWeights{};
                const auto recommended = recommendContinuations(query, index, request, weights);
                const auto* visible = findCandidate(recommended, test.item->id);
                isVisible = visible != nullptr;
                if (isVisible) {
                    const auto position = groupRank(recommended, visible->id);
                    require(position.has_value(), "default-qualified source has no group rank: " + test.item->id);
                    defaultGroup = groupName(position->first);
                    defaultRank = position->second;
                    ++defaultVisible;
                }
                for (std::size_t group = 0; group < recommended.groups.size(); ++group) {
                    const auto& entries = recommended.groups[group];
                    defaultGroupEntries[group] += entries.size();
                    if (!entries.empty() && defaultTopId.empty()) defaultTopId = entries.front().id;
                    for (std::size_t position = 1; position < entries.size(); ++position)
                        require(entries[position - 1].rankingScore >= entries[position].rankingScore,
                                "default recommendation group is not sorted by ranking score");
                }
            }
            std::string omittedText;
            for (std::size_t i = 0; i < omitted.size(); ++i) {
                if (i) omittedText += ';';
                omittedText += omitted[i];
            }
            report << "query\texplicit-legacy-cap-diagnostic\t" << poolLabel << '\t'
                   << index.templates().size() << '\t'
                   << cappedStats.shortlistSize << '\t' << test.item->id << '\t'
                   << static_cast<int>(test.tonic) << '\t' << test.prefix << '\t'
                   << fullIds.size() << '\t' << cappedIds.size() << '\t' << fullTopCount << '\t'
                   << retainedTop << '\t' << fullSourceRank << '\t' << capSourceRank << '\t'
                   << omittedText << '\t' << (isQualified ? "yes" : "no") << '\t'
                   << (isVisible ? "yes" : "no") << '\t'
                   << defaultGroup << '\t' << defaultRank << '\t' << defaultTopId << '\t'
                   << cappedTimes.back() << '\t' << fullTimes.back() << '\n';
        }
        const auto cappedTiming = summarize(std::move(cappedTimes));
        const auto fullTiming = summarize(std::move(fullTimes));
        const bool inputTruncated = index.templates().size() > 800;
        const bool acquisitionStop = inputTruncated || fullOnlyCount > 0 || sourceMisses > 0 || top20Loss;
        report << "summary\texplicit-legacy-cap-diagnostic\t" << poolLabel << '\t'
               << index.templates().size() << '\t'
               << std::min<std::size_t>(800, index.templates().size()) << "\t-\t-\t-\t"
               << fullMatchCount << '\t' << cappedMatchCount << '\t' << top20Count << '\t'
               << top20Retained << "\t-\t-\tfull_only_raw_sources=" << fullOnlyCount
               << ";query_source_misses=" << sourceMisses << ";truncated_queries=" << truncatedQueries
               << ";explicit_cap_diagnostic_gate=" << (acquisitionStop ? "STOP" : "OPEN") << "\t"
               << "qualified=" << qualified << '/' << (checkDefault ? cases.size() : 0)
               << "\tvisible=" << defaultVisible << '/' << (checkDefault ? cases.size() : 0)
               << "\tresolve=" << defaultGroupEntries[0] << ",develop=" << defaultGroupEntries[1]
               << ",loop=" << defaultGroupEntries[2] << ",color=" << defaultGroupEntries[3]
               << "\t-\t-\t" << cappedTiming.medianMs << '\t' << fullTiming.medianMs << '\n';
        require(static_cast<bool>(report), "cannot finish real shortlist gate report");
        std::cout << "shortlist_gate mode=explicit-legacy-cap-diagnostic"
                  << " default_shortlist_limit=0 cap_limit=800 pool=" << poolLabel
                  << " entries=" << index.templates().size()
                  << " cap_inputs=" << std::min<std::size_t>(800, index.templates().size())
                  << " full_only_raw_sources=" << fullOnlyCount
                  << " query_source_misses=" << sourceMisses
                  << " full_top20_retained=" << top20Retained << '/' << top20Count
                  << " default_qualified=" << qualified << '/' << (checkDefault ? cases.size() : 0)
                  << " default_visible=" << defaultVisible << '/' << (checkDefault ? cases.size() : 0)
                  << " continue_groups_resolve_develop_loop_color=" << defaultGroupEntries[0] << '/'
                  << defaultGroupEntries[1] << '/' << defaultGroupEntries[2] << '/'
                  << defaultGroupEntries[3]
                  << " cap_median_ms=" << cappedTiming.medianMs
                  << " cap_p95_ms=" << cappedTiming.p95Ms
                  << " full_median_ms=" << fullTiming.medianMs
                  << " full_p95_ms=" << fullTiming.p95Ms
                  << " explicit_cap_diagnostic_gate=" << (acquisitionStop ? "STOP" : "OPEN")
                  << " evidence=" << reportPath.string() << '\n';
        if (checkDefault && !inputTruncated)
            require(fullOnlyCount == 0 && sourceMisses == 0 && !top20Loss,
                    "real experiment pool below 800 diverged between cap and full scan");
        if (acquisitionStop)
            std::cout << "shortlist_gate_action mode=explicit-legacy-cap-diagnostic"
                      << " default_shortlist_limit=0 cap_limit=800 pool=" << poolLabel
                      << " minimum_correction=hold_affected_additions_pending_until_real_shortlist_recall_is_resolved"
                      << " scoring_gates_weights_and_algorithm=unchanged\n";
    };
    evaluate("baseline-plus-all-grade-A-shadow", shadowIndex, true);
    evaluate("experiment-active", experimentalIndex, true);
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
    const auto difference = rankedSourceDifference(first, second, 0.0001f, 0.0001f);
    require(difference.empty(), "ranking changed for identical input: " + difference);
}

void verifyTranspositionRecommendationStability(const CandidateIndex& index,
                                                const ProgressionTemplate& item) {
    constexpr float similarityTolerance = 0.001f;
    constexpr float scoreTolerance = 0.05f;
    RecommendationRequest request;
    request.preferredIntent = item.intent;
    const auto weights = recommendationWeights();
    const KeySignature referenceKey{PitchClass::C, item.mode};
    const auto selected = selectRankedPrefix(item, referenceKey, index, request, weights, true);
    const auto prefix = selected.prefix;
    std::optional<RecommendationSet> reference;
    std::size_t stableInputs{};
    std::size_t matchingReferenceKeys{};
    std::size_t targetPresentKeys{};
    std::size_t nonemptyKeys{};
    std::size_t referenceEntryCount{};
    std::size_t minimumEntryCount = std::numeric_limits<std::size_t>::max();
    std::size_t maximumEntryCount{};
    std::vector<std::string> differences;
    for (int shift = 0; shift < 12; ++shift) {
        const KeySignature key{PitchClass(shift), item.mode};
        const auto query = queryFor(item, key, prefix);
        const auto first = recommendContinuations(query, index, request, weights);
        const auto repeated = recommendContinuations(query, index, request, weights);
        const auto repeatedDifference = rankedSourceDifference(
            first, repeated, 0.0001f, 0.0001f);
        if (!repeatedDifference.empty())
            differences.push_back("shift=" + std::to_string(shift) +
                                   " repeated-input " + repeatedDifference);
        else ++stableInputs;
        std::size_t entryCount{};
        for (const auto& group : first.groups) entryCount += group.size();
        if (entryCount) ++nonemptyKeys;
        minimumEntryCount = std::min(minimumEntryCount, entryCount);
        maximumEntryCount = std::max(maximumEntryCount, entryCount);
        if (containsTemplate(first, item.id)) ++targetPresentKeys;
        if (!reference) {
            require(containsTemplate(first, item.id),
                    "selected C-key prefix did not retain its ranked source: " + item.id);
            reference = first;
            referenceEntryCount = entryCount;
            ++matchingReferenceKeys;
            continue;
        }
        const auto transpositionDifference = rankedSourceDifference(
            *reference, first, similarityTolerance, scoreTolerance);
        if (!transpositionDifference.empty())
            differences.push_back("shift=" + std::to_string(shift) + " vs C " +
                                  transpositionDifference);
        else ++matchingReferenceKeys;
    }
    const bool hasRankedCandidates = maximumEntryCount > 0;
    std::cout << "transposition_recommendation id=" << item.id
              << " keys=12 invariant_source_sequences=";
    if (hasRankedCandidates) std::cout << matchingReferenceKeys << "/12";
    else std::cout << "not-applicable-no-ranked-results";
    std::cout
              << " prefix=" << prefix
              << " repeated_input_stable=" << stableInputs << "/12"
              << " target_source_present=" << targetPresentKeys << "/12"
              << " ranked_candidates_at_C=" << referenceEntryCount
              << " nonempty_ranked_keys=" << nonemptyKeys << "/12"
              << " ranked_candidates_min_max=" << minimumEntryCount << '/' << maximumEntryCount
              << " score_tolerance=" << scoreTolerance
              << " similarity_tolerance=" << similarityTolerance
              << " candidate_ids_compared=no"
              << " status=" << (!differences.empty() ? "MISMATCH" :
                                  hasRankedCandidates ? "PASS" : "NO_RANKED_RESULTS") << '\n';
    if (!differences.empty())
        std::cout << "transposition_recommendation_first_difference id=" << item.id
                  << " detail=" << differences.front()
                  << " total_differences=" << differences.size() << '\n';
    require(differences.empty(), "transposed recommendation changed for " + item.id +
                                  ": " + (differences.empty() ? std::string{} : differences.front()));
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
        "V6_B1_022", "V6_B1_023", "V6_B1_027", "V6_B1_029", "V6_B1_030", "V6_B1_039",
        "V6_B3_01_048", "V6_B3_01_056", "V6_B3_01_065", "V6_B3_01_071"
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
    std::cout << "qualification_coverage scope=all-approved-last-nonfull-plus-targeted-all-prefixes"
              << " approved_sources=" << approved.size()
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
        verifyPreviewAudio(route.sequence);
        chords += route.sequence.events.size();
    }
    std::cout << "midi approved_full_routes=" << approved.size() << " chord_events=" << chords
              << " full_pitch_masks_and_smf_readback=PASS full_preview_audio=PASS\n";
}

void verifyApprovedEnrichment(const std::vector<ProgressionTemplate>& candidates,
                              const std::set<std::string>& approved) {
    std::array<std::size_t, 3> groupCounts{};
    std::size_t sourcesWithCandidates{}, noOpSources{}, candidateCount{};
    enrichment::EnrichmentResult::Diagnostics totalDiagnostics{};
    for (const auto& id : approved) {
        const auto& item = find(candidates, id);
        const KeySignature key{PitchClass::C, item.mode};
        const auto route = preview::templateRoute(item, key);
        require(static_cast<bool>(route), "cannot build full source route for Enrich: " + id);
        AnalysisContext context;
        context.forcedKey = key;
        const auto analysis = analyzeHarmony(route.imported.events, context);
        const auto result = enrichment::enrichProgression(route.imported.events, analysis);
        require(result.error.empty(), "Enrich API failed for approved source " + id + ": " + result.error);
        totalDiagnostics.protectedChords += result.diagnostics.protectedChords;
        totalDiagnostics.insertionAttempts += result.diagnostics.insertionAttempts;
        totalDiagnostics.insertionFailed += result.diagnostics.insertionFailed;
        totalDiagnostics.duplicates += result.diagnostics.duplicates;
        totalDiagnostics.hardFiltered += result.diagnostics.hardFiltered;
        const auto sourceCandidateCount = std::accumulate(result.groups.begin(), result.groups.end(),
            std::size_t{}, [](std::size_t sum, const auto& group) { return sum + group.size(); });
        if (sourceCandidateCount) ++sourcesWithCandidates;
        else {
            ++noOpSources;
            if (noOpSources <= 5)
                std::cout << "enrich_noop id=" << id << " opportunities=" << result.opportunities.size()
                          << " protected_chords=" << result.diagnostics.protectedChords
                          << " insertion_attempts=" << result.diagnostics.insertionAttempts
                          << " insertion_failed=" << result.diagnostics.insertionFailed
                          << " duplicates=" << result.diagnostics.duplicates
                          << " hard_filtered=" << result.diagnostics.hardFiltered << '\n';
        }
        for (std::size_t groupIndex = 0; groupIndex < result.groups.size(); ++groupIndex) {
            groupCounts[groupIndex] += result.groups[groupIndex].size();
            for (const auto& candidate : result.groups[groupIndex]) {
                require(!candidate.progression.empty() && !candidate.operations.empty() &&
                        !candidate.fingerprint.empty() && std::isfinite(candidate.score) &&
                        candidate.skeletonPreservation >= enrichment::EnrichmentConfig{}.minimumSkeletonPreservation,
                        "Enrich produced an incomplete or unsafe candidate for " + id);
                require(static_cast<std::size_t>(candidate.group) == groupIndex,
                        "Enrich candidate was returned in the wrong output group: " + id);
                for (std::size_t eventIndex = 0; eventIndex < candidate.progression.size(); ++eventIndex) {
                    const auto& event = candidate.progression[eventIndex];
                    require(std::isfinite(event.startQN) &&
                            (!eventIndex || event.startQN > candidate.progression[eventIndex - 1].startQN) &&
                            normalizeChord(event).quality != ChordQuality::Unknown,
                            "Enrich returned an invalid event timeline or chord for " + id);
                }
                ++candidateCount;
            }
        }
    }
    std::cout << "enrich approved_sources=" << approved.size()
              << " api_calls=" << approved.size()
              << " sources_with_candidates=" << sourcesWithCandidates
              << " no_op_sources=" << noOpSources
              << " candidates=" << candidateCount
              << " polish_rich_advanced=" << groupCounts[0] << '/' << groupCounts[1] << '/' << groupCounts[2]
              << " protected_chords=" << totalDiagnostics.protectedChords
              << " insertion_attempts=" << totalDiagnostics.insertionAttempts
              << " insertion_failed=" << totalDiagnostics.insertionFailed
              << " duplicates=" << totalDiagnostics.duplicates
              << " hard_filtered=" << totalDiagnostics.hardFiltered
              << " no_op_explanation=default_rules_found_no_safe_unique_edit_not_api_error\n";
}

void writeRepresentativeMedia(const std::vector<ProgressionTemplate>& candidates,
                              const std::set<std::string>& approved,
                              const std::filesystem::path& outputDirectory) {
    struct Sample { const char* id; const char* role; bool expectedApproved; };
    const std::array samples{
        Sample{"V6_B3_01_048", "mixed-minor", true},
        Sample{"V6_B3_01_056", "F-sharp-extension", true},
        Sample{"V6_B3_01_065", "chromatic-root-neighbor", true},
        Sample{"V6_B3_01_071", "secondary-dominant-chain", true},
        Sample{"V6_B3_01_070", "pending-suspension-independent-B-bass", false}
    };
    std::filesystem::create_directories(outputDirectory);
    std::ofstream index(outputDirectory / "media-index.tsv", std::ios::binary);
    require(static_cast<bool>(index), "cannot write representative media index");
    index << "id\trole\tpool_status\tfull_midi\tfull_preview_wav\n";
    for (const auto& sample : samples) {
        const auto& item = find(candidates, sample.id);
        const bool isApproved = approved.contains(item.id);
        require(sample.expectedApproved == isApproved,
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
              << (isApproved ? "approved-experiment" : "pending-not-in-experiment") << '\t'
              << midiPath.filename().string() << '\t' << wavPath.filename().string() << '\n';
        std::cout << "media id=" << item.id << " role=" << sample.role
                  << " status=" << (isApproved ? "approved" : "pending")
                  << " midi_bytes=" << midiBytes.bytes.size() << " wav_frames=" << audio.left.size()
                  << " rms=" << audio.rms << '\n';
    }
    require(static_cast<bool>(index), "cannot finish representative media index");
}

int runR1DefaultScan(const std::filesystem::path& baselinePath,
                    const std::filesystem::path& candidatesPath,
                    const std::filesystem::path& metadataPath,
                    const std::filesystem::path& outputDirectory) {
    const auto baseline = library::loadFactory(baselinePath);
    require(static_cast<bool>(baseline), "cannot load V5 baseline: " + baseline.error);
    require(baseline.libraryVersion == 5 && baseline.storageSchemaVersion == 2 &&
            baseline.templates.size() == 658, "Factory V5 baseline contract");

    const auto parsed = dev::parseTemplateJson(readText(candidatesPath), true);
    require(static_cast<bool>(parsed), "cannot parse candidates.json: " + parsed.error);
    const auto metadata = benchmark::json::parse(readText(metadataPath));
    std::set<std::string> gradeAIds;
    for (const auto& row : metadata.at("candidates").items())
        if (row.at("grade").text() == "A") gradeAIds.insert(row.at("id").text());
    require(gradeAIds.size() == 152, "R1 frozen grade-A count changed");

    std::vector<ProgressionTemplate> shadowEntries = baseline.templates;
    std::set<std::string> candidateIds;
    for (const auto& item : parsed.templates) {
        require(candidateIds.insert(item.id).second, "duplicate candidate ID: " + item.id);
        if (gradeAIds.contains(item.id)) shadowEntries.push_back(item);
    }
    for (const auto& id : gradeAIds)
        require(candidateIds.contains(id), "grade-A metadata ID missing from candidates.json: " + id);
    require(shadowEntries.size() == 810,
            "R1 real shadow must contain V5 658 plus all 152 grade-A candidates");
    const CandidateIndex shadowIndex(shadowEntries);
    std::filesystem::create_directories(outputDirectory);

    const std::array comparisonIds{
        "V6_B3_01_048", "V6_B3_01_056", "V6_B3_01_065"
    };
    std::ofstream comparison(outputDirectory / "r1-cap-vs-default.tsv", std::ios::binary);
    require(static_cast<bool>(comparison), "cannot write R1 cap comparison report");
    comparison << "query\tfull_sources\tcap_sources\tfull_only_sources\tquery_rank_full"
                  "\tquery_rank_cap\ttop20_retained\tshared_score_max_delta\tfull_ms\tcap_ms\n";

    for (std::size_t queryIndex = 0; queryIndex < comparisonIds.size(); ++queryIndex) {
        const auto& item = find(parsed.templates, comparisonIds[queryIndex]);
        require(gradeAIds.contains(item.id), "R1 representative is not grade A: " + item.id);
        const auto prefix = item.full.size() > 2 ? item.full.size() - 1 : std::size_t{1};
        const auto query = queryFor(item, {PitchClass::C, item.mode}, prefix);
        const auto defaultIndices = shadowIndex.shortlist(query);
        require(defaultIndices.size() == 810,
                "default shortlist did not include all 810 real shadow sources: " + item.id);
        for (std::size_t i = 0; i < defaultIndices.size(); ++i)
            require(defaultIndices[i] == i,
                    "default shortlist order is not the complete source index: " + item.id);

        MatchWeights defaultWeights;
        MatchWeights cappedWeights = defaultWeights;
        cappedWeights.shortlistLimit = 800;
        MatchRunStats fullStats, capStats;
        const auto fullResults = matchProgression(
            query, shadowIndex, defaultWeights, shadowIndex.templates().size(), &fullStats);
        const auto capResults = matchProgression(
            query, shadowIndex, cappedWeights, shadowIndex.templates().size(), &capStats);
        require(fullStats.shortlistSize == 810 && capStats.shortlistSize == 800,
                "R1 full/capped matcher shortlist sizes changed: " + item.id);

        const auto fullIds = matchIds(fullResults);
        const auto capIds = matchIds(capResults);
        std::map<std::string, const MatchResult*> cappedById;
        for (const auto& result : capResults)
            cappedById.emplace(result.templateId, &result);
        float maxSharedScoreDelta{};
        for (const auto& result : fullResults) {
            const auto found = cappedById.find(result.templateId);
            if (found == cappedById.end()) continue;
            require(result.key.tonic == found->second->key.tonic &&
                    result.key.mode == found->second->key.mode,
                    "shared cap/full source changed selected key: " + result.templateId);
            const auto delta = std::abs(result.similarity - found->second->similarity);
            maxSharedScoreDelta = std::max(maxSharedScoreDelta, delta);
            require(delta <= 1e-6f,
                    "shared cap/full source changed similarity: " + result.templateId);
        }
        std::vector<std::string> fullOnly;
        for (const auto& id : fullIds)
            if (std::find(capIds.begin(), capIds.end(), id) == capIds.end()) fullOnly.push_back(id);
        const auto top20 = std::vector<std::string>(fullIds.begin(),
            fullIds.begin() + std::min<std::size_t>(20, fullIds.size()));
        const auto top20Retained = sharedIdCount(top20, capIds);
        const auto rankOf = [](const std::vector<std::string>& ids, const std::string& target) {
            const auto found = std::find(ids.begin(), ids.end(), target);
            return found == ids.end() ? std::size_t{} : static_cast<std::size_t>(found - ids.begin()) + 1;
        };
        const auto fullMs = queryIndex == 0 ? fullStats.totalMs : 0.0;
        const auto capMs = queryIndex == 0 ? capStats.totalMs : 0.0;
        comparison << item.id << '\t' << fullIds.size() << '\t' << capIds.size() << '\t'
                   << fullOnly.size() << '\t' << rankOf(fullIds, item.id) << '\t'
                   << rankOf(capIds, item.id) << '\t' << top20Retained << '/' << top20.size()
                   << '\t' << maxSharedScoreDelta << '\t';
        if (queryIndex == 0) comparison << fullMs << '\t' << capMs;
        else comparison << "-\t-";
        comparison << '\n';

        std::cout << "r1_cap_compare query=" << item.id << " prefix=" << prefix
                  << " index_default=810 cap=800 full_matches=" << fullIds.size()
                  << " cap_matches=" << capIds.size() << " full_only=" << fullOnly.size()
                  << " query_rank_full=" << rankOf(fullIds, item.id)
                  << " query_rank_cap=" << rankOf(capIds, item.id)
                  << " top20_retained=" << top20Retained << '/' << top20.size()
                  << " shared_score_max_delta=" << maxSharedScoreDelta;
        if (queryIndex == 0)
            std::cout << " timing_ms_full=" << fullMs << " timing_ms_cap=" << capMs;
        if (!fullOnly.empty()) {
            std::cout << " full_only_ids=";
            for (std::size_t i = 0; i < fullOnly.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << fullOnly[i];
            }
        }
        std::cout << '\n';
    }
    require(static_cast<bool>(comparison), "cannot finish R1 cap comparison report");

    struct GroupQueryCase { const ProgressionTemplate* item; std::size_t prefix; };
    const auto colorSource = std::find_if(baseline.templates.begin(), baseline.templates.end(),
        [](const auto& item) { return item.intent == PhraseIntent::Color && item.full.size() >= 3; });
    require(colorSource != baseline.templates.end(), "V5 baseline has no Color recommendation representative");
    const std::array<GroupQueryCase, 4> groupQueries{{
        {&find(parsed.templates, "V6_B3_01_048"), 6},
        {&find(parsed.templates, "V6_B3_01_056"), 7},
        {&find(parsed.templates, "V6_B3_01_065"), 5},
        {&*colorSource, colorSource->full.size() - 2}
    }};
    std::array<std::size_t, 4> observedGroupEntries{};
    for (std::size_t queryIndex = 0; queryIndex < groupQueries.size(); ++queryIndex) {
        const auto& item = *groupQueries[queryIndex].item;
        const auto query = queryFor(item, {PitchClass::C, item.mode}, groupQueries[queryIndex].prefix);
        RecommendationRequest request;
        request.preferredIntent = item.intent;
        const auto recommendations = recommendContinuations(query, shadowIndex, request);
        require(recommendations.stats.shortlistSize == 810,
                "default recommendation did not use the full real index: " + item.id);
        std::size_t total{};
        for (std::size_t group = 0; group < recommendations.groups.size(); ++group) {
            for (std::size_t rank = 0; rank < recommendations.groups[group].size(); ++rank) {
                const auto& candidate = recommendations.groups[group][rank];
                require(std::isfinite(candidate.rankingScore),
                        "recommendation group returned non-finite ranking score: " + item.id);
                if (rank)
                    require(recommendations.groups[group][rank - 1].rankingScore >= candidate.rankingScore,
                            "recommendation group is not sorted by ranking score: " + item.id);
                ++observedGroupEntries[group];
                ++total;
            }
        }
        if (queryIndex == 3)
            require(!recommendations.groups[3].empty(),
                    "V5 Color representative did not reach the Color recommendation group");
        std::cout << "r1_recommend query=" << item.id << " prefix=" << groupQueries[queryIndex].prefix
                  << " shortlist=810"
                  << " resolve=" << recommendations.groups[0].size()
                  << " develop=" << recommendations.groups[1].size()
                  << " loop=" << recommendations.groups[2].size()
                  << " color=" << recommendations.groups[3].size()
                  << " total=" << total << '\n';
    }
    std::cout << "r1_recommend_group_coverage resolve=" << observedGroupEntries[0]
              << " develop=" << observedGroupEntries[1]
              << " loop=" << observedGroupEntries[2]
              << " color=" << observedGroupEntries[3] << '\n';

    const auto& enrichSource = find(parsed.templates, "V6_B3_01_065");
    const KeySignature enrichKey{PitchClass::C, enrichSource.mode};
    const auto enrichRoute = preview::templateRoute(enrichSource, enrichKey);
    require(static_cast<bool>(enrichRoute), "cannot build R1 Enrich source route");
    AnalysisContext enrichContext;
    enrichContext.forcedKey = enrichKey;
    const auto enrichAnalysis = analyzeHarmony(enrichRoute.imported.events, enrichContext);
    const auto enrichment = enrichment::enrichProgression(enrichRoute.imported.events, enrichAnalysis);
    require(enrichment.error.empty(), "R1 Enrich API failed: " + enrichment.error);
    const auto enrichmentCandidates = std::accumulate(enrichment.groups.begin(), enrichment.groups.end(),
        std::size_t{}, [](std::size_t sum, const auto& group) { return sum + group.size(); });
    std::cout << "r1_enrich source=" << enrichSource.id << " api=PASS candidates="
              << enrichmentCandidates << " no_op=" << (enrichmentCandidates == 0 ? "yes" : "no") << '\n';

    const auto& wishSource = baseline.templates.front();
    const KeySignature wishKey{PitchClass::C, wishSource.mode};
    const auto wishPrefixLength = wishSource.full.size() - 1;
    wish::WishInput wishInput;
    require(wishInput.imported.replace(makePrefix(wishSource, wishKey, wishPrefixLength),
                                       TimelineCoordinateMode::RelativeToSelection),
            "cannot create R1 input-constrained Wish prefix");
    wishInput.analysis.forcedKey = wishKey;
    wishInput.request.preferredIntent = wishSource.intent;
    wish::AnnotationCatalog emptyCatalog;
    const std::array<ProgressionTemplate, 1> wishSources{wishSource};
    const auto wishPool = wish::makeInputPool(wishSources, emptyCatalog, wishInput);
    require(static_cast<bool>(wishPool), "R1 input-constrained Wish API failed: " + wishPool.error);
    require(!wishPool.items.empty(), "R1 input-constrained Wish returned no route for an exact source prefix");
    require(wishPool.unifiedStats.shortlistSize == 1,
            "R1 input-constrained Wish did not shortlist its isolated source");
    require(std::any_of(wishPool.items.begin(), wishPool.items.end(), [&](const auto& wishItem) {
        return std::any_of(wishItem.sources.begin(), wishItem.sources.end(), [&](const auto& source) {
            return source.id == wishSource.id;
        });
    }), "R1 input-constrained Wish lost its source identity");
    std::cout << "r1_wish source=" << wishSource.id << " constrained_prefix=" << wishPrefixLength
              << " shortlist=" << wishPool.unifiedStats.shortlistSize
              << " output_routes=" << wishPool.items.size() << " api=PASS\n";

    auto isolatedUser = baseline.templates.front();
    isolatedUser.id = "USER_B3R_ISOLATED_SOURCE";
    isolatedUser.name = isolatedUser.nameEn = isolatedUser.nameZh = isolatedUser.id;
    isolatedUser.aliases.clear();
    isolatedUser.sourceType = "user";
    const auto suffix = std::to_string(Clock::now().time_since_epoch().count());
    const auto userDatabasePath = outputDirectory / ("r1-isolated-user-" + suffix + ".db");
    std::string userError;
    require(library::UserLibrary(userDatabasePath).addProgression(isolatedUser, userError),
            "cannot persist isolated R1 user source: " + userError);
    const auto loadedUser = library::UserLibrary(userDatabasePath).loadAll();
    require(static_cast<bool>(loadedUser) && loadedUser.templates.size() == 1 &&
            loadedUser.templates.front().id == isolatedUser.id &&
            loadedUser.templates.front().sourceType == "user",
            "isolated R1 user source did not reload from UserLibrary");
    auto workerMergedEntries = shadowEntries;
    for (const auto& userItem : loadedUser.templates) {
        require(std::none_of(workerMergedEntries.begin(), workerMergedEntries.end(), [&](const auto& item) {
            return item.id == userItem.id;
        }), "isolated R1 user source ID collides with the factory index");
        workerMergedEntries.push_back(userItem);
    }
    const CandidateIndex workerMergedIndex(std::move(workerMergedEntries));
    const auto userQuery = queryFor(isolatedUser, {PitchClass::C, isolatedUser.mode},
                                    isolatedUser.full.size() > 2 ? isolatedUser.full.size() - 1 : 1);
    const auto mergedIndices = workerMergedIndex.shortlist(userQuery);
    require(mergedIndices.size() == 811 && mergedIndices.back() == 810,
            "Worker-style factory plus user merge is not fully visible to the default index");
    const auto mergedMatches = matchProgression(
        userQuery, workerMergedIndex, MatchWeights{}, workerMergedIndex.templates().size());
    require(std::any_of(mergedMatches.begin(), mergedMatches.end(), [&](const auto& match) {
        return match.templateId == isolatedUser.id;
    }), "isolated user source was not matched after Worker-style merge");
    std::cout << "r1_user_merge factory_grade_A=810 isolated_user=1 merged_index=811"
              << " user_source=" << isolatedUser.id << " matched=yes db=" << userDatabasePath.string() << '\n';

    std::cout << "r1_default_scan PASS baseline=658 grade_A=152 full_default=810 cap_comparison_queries=3"
              << " group_queries=4 enrich_calls=1 wish_input_sources=1 user_sources=1"
              << " report=" << (outputDirectory / "r1-cap-vs-default.tsv").string() << '\n';
    return 0;
}

int runIncremental(const std::filesystem::path& baselinePath,
                   const std::filesystem::path& candidatesPath,
                   const std::filesystem::path& metadataPath,
                   const std::filesystem::path& approvedPath,
                   const std::filesystem::path& proposalPath,
                   const std::filesystem::path& experimentPath,
                   const std::filesystem::path& baseHcaPath,
                   const std::filesystem::path& mergedHcaPath,
                   const std::filesystem::path& mediaDirectory,
                   const std::filesystem::path& selectedIdsPath,
                   const std::filesystem::path& sampleIdsPath,
                   bool finalInventory) {
    const auto baseline = library::loadFactory(baselinePath);
    const auto proposal = library::loadFactory(proposalPath);
    const auto experimental = library::loadFactory(experimentPath);
    require(static_cast<bool>(baseline), "cannot load incremental V5 baseline: " + baseline.error);
    require(static_cast<bool>(proposal), "cannot load incremental proposal DB: " + proposal.error);
    require(static_cast<bool>(experimental), "cannot load incremental experiment DB: " + experimental.error);
    require(baseline.libraryVersion == 5 && baseline.storageSchemaVersion == 2 &&
            baseline.templates.size() == 658, "incremental V5 baseline contract");

    const auto parsedCandidates = dev::parseTemplateJson(readText(candidatesPath), true);
    require(static_cast<bool>(parsedCandidates),
            "cannot parse incremental candidates.json: " + parsedCandidates.error);
    const auto& candidates = parsedCandidates.templates;
    std::set<std::string> candidateIds;
    for (const auto& item : candidates)
        require(candidateIds.insert(item.id).second, "duplicate incremental candidate ID: " + item.id);
    const auto approved = readApprovedIds(approvedPath);
    const auto selectedIds = readApprovedIds(selectedIdsPath);
    const auto sampleIds = readApprovedIds(sampleIdsPath);
    require(!selectedIds.empty(), "incremental selected ID list is empty");
    require(proposal.libraryVersion == 6 && proposal.storageSchemaVersion == 2 &&
            proposal.templates.size() == baseline.templates.size() + candidates.size(),
            "incremental proposal DB identity/count");
    require(experimental.libraryVersion == 6 && experimental.storageSchemaVersion == 2 &&
            experimental.templates.size() == baseline.templates.size() + approved.size(),
            "incremental experiment DB identity/count");

    const auto metadataRoot = benchmark::json::parse(readText(metadataPath));
    require(metadataRoot.at("version").integer(6, 6) == 6 &&
            metadataRoot.at("baseline").text() == "2421c6f",
            "incremental metadata version/baseline");
    const auto baseHcaText = readText(baseHcaPath);
    const auto mergedHcaText = readText(mergedHcaPath);
    require(mergedHcaText.starts_with(baseHcaText), "incremental HCA changed the exact V5 prefix");
    const auto baseHca = wish::parseAnnotations(baseHcaText);
    const auto mergedHca = wish::parseAnnotations(mergedHcaText);
    require(static_cast<bool>(baseHca), "incremental baseline HCA parse: " + baseHca.error);
    require(static_cast<bool>(mergedHca), "incremental merged HCA parse: " + mergedHca.error);
    require(mergedHca.catalog.entries.size() >=
                baseHca.catalog.entries.size() + selectedIds.size(),
            "incremental merged HCA is missing selected source annotations");

    const auto& metadataRows = metadataRoot.at("candidates").items();
    const auto parseUsage = [](std::string_view value) {
        if (value == "high") return wish::EditorialUsage::High;
        if (value == "medium") return wish::EditorialUsage::Medium;
        if (value == "low") return wish::EditorialUsage::Low;
        if (value == "unrated") return wish::EditorialUsage::Unrated;
        throw std::runtime_error("unknown selected membership usage: " + std::string(value));
    };
    const auto parseReview = [](std::string_view value) {
        if (value == "pending") return wish::ReviewStatus::Pending;
        if (value == "reviewed") return wish::ReviewStatus::Reviewed;
        if (value == "unconfirmed") return wish::ReviewStatus::Unconfirmed;
        throw std::runtime_error("unknown selected membership review: " + std::string(value));
    };
    for (const auto& id : selectedIds) {
        require(approved.contains(id), "selected source is not in approved-ids.txt: " + id);
        const auto metadataCount = std::count_if(metadataRows.begin(), metadataRows.end(), [&](const auto& row) {
            return row.at("id").text() == id;
        });
        require(metadataCount == 1, "selected source metadata ID count is not one: " + id);
        const auto metadata = std::find_if(metadataRows.begin(), metadataRows.end(), [&](const auto& row) {
            return row.at("id").text() == id;
        });
        require(metadata != metadataRows.end(), "selected source is absent from metadata.json: " + id);
        require(metadata->at("grade").text() == "A" &&
                metadata->at("status").text() == "editorial-reviewed" &&
                metadata->at("adoptionStatus").text() == "approved",
                "selected source metadata is not approved A/reviewed: " + id);

        const auto& source = find(candidates, id);
        const auto& proposalSource = find(proposal, id);
        const auto& experimentSource = find(experimental, id);
        const auto sourcePayload = dev::serializeTemplateJson(source);
        require(sourcePayload == dev::serializeTemplateJson(proposalSource) &&
                sourcePayload == dev::serializeTemplateJson(experimentSource),
                "selected source complete payload differs across candidates/proposal/experiment: " + id);
        require(library::canonicalMusicalFingerprint(source) ==
                    library::canonicalMusicalFingerprint(experimentSource) &&
                wish::sourceFingerprint(source) == wish::sourceFingerprint(experimentSource),
                "selected source musical identity differs in experiment DB: " + id);

        wish::ModeAnnotation expectedEntry;
        expectedEntry.templateId = id;
        expectedEntry.sourceFingerprint = wish::sourceFingerprint(source);
        for (const auto& rawMembership : metadata->at("memberships").items()) {
            wish::ModeMembership membership;
            membership.tonicOffset = rawMembership.at("tonicOffset").integer(0, 11);
            const auto mode = wish::parseMode(rawMembership.at("mode").text());
            require(mode.has_value(), "unknown selected membership mode: " + id);
            membership.mode = *mode;
            membership.usage = parseUsage(rawMembership.at("usage").text());
            membership.usageBasis = rawMembership.at("usageBasis").text();
            membership.musicalEvidence = rawMembership.at("musicalEvidence").text();
            membership.review = parseReview(rawMembership.at("review").text());
            membership.reviewer = rawMembership.at("reviewer").text();
            membership.melodicContext = rawMembership.at("melodicContext").text();
            std::string validationError;
            require(wish::validMembership(membership, validationError),
                    "invalid selected membership " + id + ": " + validationError);
            expectedEntry.memberships.push_back(std::move(membership));
        }
        require(!expectedEntry.memberships.empty(), "selected source metadata has no membership: " + id);
        const auto* actualEntry = findAnnotation(mergedHca.catalog, id);
        require(actualEntry, "selected source HCA membership is missing from compiled HCA: " + id);
        require(expectedEntry.sourceFingerprint == wish::sourceFingerprint(source) &&
                wish::serializeAnnotations({1, {expectedEntry}}) ==
                    wish::serializeAnnotations({1, {*actualEntry}}),
                "selected source metadata/HCA membership differs: " + id);
        const auto resolution = wish::resolveAnnotations(experimentSource, mergedHca.catalog);
        require(resolution.state != wish::AnnotationState::SourceMismatch &&
                resolution.state != wish::AnnotationState::Invalid,
                "selected source does not resolve against compiled HCA: " + id);
        std::cout << "incremental_identity id=" << id
                  << " candidate_proposal_experiment=exact"
                  << " musical_fingerprint=exact metadata_grade=A status=editorial-reviewed"
                  << " adoption=approved hca_memberships=" << actualEntry->memberships.size()
                  << " hca_binding=PASS\n";
    }

    const CandidateIndex index(experimental.templates);
    RecommendationRequest request;
    auto weights = recommendationWeights();
    weights.perGroup = 0;
    std::vector<std::string> qualificationFailures;
    std::size_t qualifiedSources{};
    for (const auto& id : selectedIds) {
        try {
            const auto& item = find(experimental, id);
            request.preferredIntent = item.intent;
            const auto selected = selectRankedPrefix(
                item, {PitchClass::C, item.mode}, index, request, weights, false);
            const auto* candidate = findCandidate(selected.results, id);
            require(candidate && candidate->baseQualified && candidate->hardConstraintsPassed,
                    "selected source has no base-qualified candidate at the chosen prefix");
            const auto match = std::find_if(selected.results.matches.begin(), selected.results.matches.end(),
                [&](const auto& result) { return result.templateId == id; });
            require(match != selected.results.matches.end(),
                    "selected source has no corresponding matcher result at the chosen prefix");
            const auto position = groupRank(selected.results, candidate->id);
            require(position.has_value(), "selected source candidate has no recommendation group rank");
            const bool primary = candidate->primaryTemplate == id;
            ++qualifiedSources;
            std::cout << "incremental_prefix id=" << id << " key=C mode="
                      << (item.mode == Mode::Major ? "major" : "minor")
                      << " prefix=" << selected.prefix << " qualified=yes group="
                      << groupName(position->first) << " rank=" << position->second
                      << " source_role=" << (primary ? "primary" : "supporting")
                      << " similarity=" << match->similarity
                      << " eligibility_score=" << candidate->eligibilityScore
                      << " ranking_score=" << candidate->rankingScore << '\n';
        } catch (const std::exception& error) {
            qualificationFailures.push_back(id);
            std::cout << "incremental_prefix id=" << id << " qualified=no detail="
                      << error.what() << '\n';
        }
    }
    require(qualificationFailures.empty(),
            "selected sources failed legal-prefix qualification: " +
            (qualificationFailures.empty() ? std::string{} : qualificationFailures.front()));

    for (const auto& id : sampleIds) {
        require(selectedIds.contains(id), "sample source is not in selected-ids.txt: " + id);
        const auto& item = find(experimental, id);
        const auto route = preview::buildTemplateSequence(
            item, {PitchClass::C, item.mode}, preview::defaultTempoBPM);
        require(static_cast<bool>(route), "cannot build complete sample route: " + id);
        verifyMidiRoundTrip(route.sequence);
        verifyPreviewAudio(route.sequence);

        const auto clip = midi::buildClip(route.sequence, midi::ArrangementMode::VoiceLed,
                                          midi::ExportScope::FullPhrase, {});
        require(static_cast<bool>(clip), "cannot build complete sample MIDI: " + id);
        const auto midiBytes = midi::writeToMemory(clip.sequence);
        require(midiBytes && midiBytes.bytes.size() > 14, "cannot serialize complete sample MIDI: " + id);
        std::filesystem::create_directories(mediaDirectory);
        const auto midiPath = mediaDirectory / (id + "-complete.mid");
        std::ofstream midiOut(midiPath, std::ios::binary);
        require(static_cast<bool>(midiOut), "cannot write complete sample MIDI: " + id);
        midiOut.write(reinterpret_cast<const char*>(midiBytes.bytes.data()),
                      static_cast<std::streamsize>(midiBytes.bytes.size()));
        require(static_cast<bool>(midiOut), "cannot finish complete sample MIDI: " + id);

        const auto audio = preview::renderOffline(route.sequence, 48000.0);
        require(!audio.left.empty() && audio.left.size() == audio.right.size() && audio.rms > 0,
                "complete sample Preview is silent: " + id);
        std::string wavError;
        const auto wavPath = mediaDirectory / (id + "-preview.wav");
        require(preview::writeWav16(audio, wavPath, wavError),
                "cannot write complete sample Preview: " + wavError);
        std::cout << "incremental_sample id=" << id << " full_midi=PASS full_preview=PASS"
                  << " midi_bytes=" << midiBytes.bytes.size() << " wav_frames=" << audio.left.size()
                  << " rms=" << audio.rms << '\n';
    }

    if (finalInventory) {
        verifyWishModePools(baseline, experimental, baseHcaPath, mergedHcaPath, false);
        const auto& representative = find(experimental, "V6_B3_01_048");
        const auto prefix = representative.full.size() > 2
            ? representative.full.size() - 1 : std::size_t{1};
        const auto query = queryFor(representative, {PitchClass::C, representative.mode}, prefix);
        RecommendationRequest request;
        request.preferredIntent = representative.intent;
        const auto started = Clock::now();
        const auto recommendation = recommendContinuations(query, index, request);
        const auto elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        require(recommendation.stats.shortlistSize == experimental.templates.size(),
                "final representative recommendation did not scan the complete experiment index");
        std::size_t recommendedEntries{};
        for (const auto& group : recommendation.groups) recommendedEntries += group.size();
        std::cout << "r3_representative_timing id=" << representative.id
                  << " prefix=" << prefix << " pool=" << experimental.templates.size()
                  << " default_shortlist_limit=0 full_scan=yes elapsed_ms=" << elapsedMs
                  << " group_entries=" << recommendedEntries << '\n';
    }

    std::cout << "incremental PASS baseline=658 candidates=" << candidates.size()
              << " approved=" << approved.size() << " proposal=" << proposal.templates.size()
              << " experiment=" << experimental.templates.size()
              << " selected=" << selectedIds.size() << " qualified=" << qualifiedSources
              << " sample_ids=" << sampleIds.size()
              << " final_inventory=" << (finalInventory ? "yes" : "no") << '\n';
    return 0;
}
} // namespace

namespace {
void verifyWishItemRoundTrip(const wish::WishItem& item, const library::LoadResult& factory) {
    const auto previewResult = preview::buildSequence(item.route.imported, &item.route.candidate, 120.0);
    require(static_cast<bool>(previewResult), "installed Wish preview: " + previewResult.error);
    require(wish::performanceFingerprint(previewResult.sequence, item.route.meterNumerator,
        item.route.meterDenominator) == item.musicIdentity, "installed Wish full music identity");

    const std::vector<MatchResult> matches = item.match ? std::vector<MatchResult>{*item.match}
                                                         : std::vector<MatchResult>{};
    auto captured = snapshot::capture(item.route.imported, item.route.candidate, matches, 120.0,
        item.route.meterNumerator, item.route.meterDenominator);
    captured.factoryLibraryVersion = 6;
    const auto restored = snapshot::deserialize(snapshot::serialize(captured), &factory);
    require(static_cast<bool>(restored), "installed Wish Snapshot3 readback: " + restored.error);
    require(restored.value.candidateAvailable &&
        restored.value.candidate.primaryTemplate == item.route.candidate.primaryTemplate,
        "installed Wish Snapshot lost its Factory source reference");
    const auto restoredPreview = preview::buildSequence(
        restored.value.imported, &restored.value.candidate, 120.0);
    require(static_cast<bool>(restoredPreview), "installed Wish restored preview");
    require(wish::performanceFingerprint(restoredPreview.sequence, item.route.meterNumerator,
        item.route.meterDenominator) == item.musicIdentity, "installed Wish Snapshot music identity");

    midi::CandidateExportContext context{item.route.imported, 120.0,
        {item.route.meterNumerator, item.route.meterDenominator}};
    const auto before = midi::candidatePayload(context, item.route.candidate);
    const midi::CandidateExportContext restoredContext{restored.value.imported, 120.0,
        {item.route.meterNumerator, item.route.meterDenominator}};
    const auto after = midi::candidatePayload(restoredContext, restored.value.candidate);
    require(before && after && before.payload.smfBytes == after.payload.smfBytes &&
        before.payload.dawClipBytes == after.payload.dawClipBytes,
        "installed Wish Snapshot/MIDI byte identity");
}

int runInstalledSmoke(const std::filesystem::path& factoryPath,
                      const std::filesystem::path& fixtureDirectory) {
    std::filesystem::create_directories(fixtureDirectory);
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "installed Factory load: " + factory.error);
    require(factory.libraryVersion == 6 && factory.storageSchemaVersion == 2 && factory.templates.size() == 1014,
            "installed Factory V6/Schema2/1014 identity");
    const bool activeSelection = factoryPath.parent_path().filename() == "6";
    if (activeSelection) {
        const auto selected = library::loadAvailableFactory({}, factoryPath.parent_path().parent_path());
        require(selected.library && selected.path == factoryPath && selected.library.libraryVersion == 6 &&
            selected.library.templates.size() == 1014,
            "active Factory store did not select the installed V6 package");
    }
    const auto annotations = wish::annotationsForFactory(factoryPath, factory.libraryVersion);
    require(static_cast<bool>(annotations), "installed external HCA: " + annotations.error);
    const auto hcaBytes = std::filesystem::file_size(factoryPath.parent_path() / "factory.hca");
    require(annotations.catalog.entries.size() == factory.templates.size(), "installed HCA bound source count");

    require(hcaBytes > 1024u * 1024u && hcaBytes <= 2u * 1024u * 1024u,
            "installed HCA size is outside the supported 2 MiB contract");

    constexpr std::array modes{wish::MusicalMode::Dorian, wish::MusicalMode::Mixolydian,
        wish::MusicalMode::Lydian, wish::MusicalMode::Phrygian,
        wish::MusicalMode::MelodicMinorAscending};
    std::size_t modeIndex{};
    for (const auto mode : modes) {
        wish::WishFilter filter;
        filter.fixedTonic = PitchClass::C;
        filter.modes = {mode};
        filter.strict = true;
        auto pool = wish::makeFreePool(factory.templates, annotations.catalog, filter);
        require(static_cast<bool>(pool), "installed strict fixed-C Wish pool: " + pool.error);
        require(!pool.items.empty(), std::string("installed strict fixed-C pool is empty: ") + wish::modeName(mode));
        wish::WishRound round(std::move(pool), 6800 + modeIndex++);
        require(round.draw(1).size() == 1,
                std::string("installed strict fixed-C Wish draw: ") + wish::modeName(mode));
    }

    const auto colorSource = std::find_if(factory.templates.begin(), factory.templates.end(),
        [](const auto& item) { return item.intent == PhraseIntent::Color && item.full.size() >= 3; });
    require(colorSource != factory.templates.end(), "installed Factory has no Color representative");
    struct GroupQueryCase { const ProgressionTemplate* item; std::size_t prefix; };
    const std::array<GroupQueryCase, 4> groupQueries{{
        {&find(factory, "V6_B3_01_048"), 6}, {&find(factory, "V6_B3_01_056"), 7},
        {&find(factory, "V6_B3_01_065"), 5}, {&*colorSource, colorSource->full.size() - 2}
    }};
    const CandidateIndex recommendationIndex(factory.templates);
    std::array<std::size_t, 4> groupCoverage{};
    for (const auto& test : groupQueries) {
        const auto query = queryFor(*test.item, {PitchClass::C, test.item->mode}, test.prefix);
        RecommendationRequest request;
        request.preferredIntent = test.item->intent;
        const auto recommendations = recommendContinuations(query, recommendationIndex, request);
        require(recommendations.stats.shortlistSize == 1014,
                "installed recommendation did not scan the complete Factory");
        for (std::size_t group = 0; group < recommendations.groups.size(); ++group)
            groupCoverage[group] += recommendations.groups[group].size();
    }
    require(std::all_of(groupCoverage.begin(), groupCoverage.end(),
        [](const auto count) { return count > 0; }), "installed representative recommendation group is empty");

    const auto& enrichSource = find(factory, "V6_B3_01_065");
    const KeySignature enrichKey{PitchClass::C, enrichSource.mode};
    const auto enrichRoute = preview::templateRoute(enrichSource, enrichKey);
    require(static_cast<bool>(enrichRoute), "installed Enrich source route");
    AnalysisContext enrichContext;
    enrichContext.forcedKey = enrichKey;
    const auto enrichAnalysis = analyzeHarmony(enrichRoute.imported.events, enrichContext);
    const auto enrichment = enrichment::enrichProgression(enrichRoute.imported.events, enrichAnalysis);
    require(enrichment.error.empty(), "installed Enrich API: " + enrichment.error);
    const auto enrichmentCandidates = std::accumulate(enrichment.groups.begin(), enrichment.groups.end(),
        std::size_t{}, [](std::size_t sum, const auto& group) { return sum + group.size(); });
    require(enrichmentCandidates > 0, "installed Enrich representative returned no candidates");
    const auto& freeSource = find(factory, "V6_B3_01_048");
    wish::WishFilter freeFilter;
    freeFilter.fixedTonic = PitchClass::C;
    auto freePool = wish::makeFreePool(factory.templates, annotations.catalog, freeFilter);
    require(static_cast<bool>(freePool), "installed free Wish pool: " + freePool.error);
    std::erase_if(freePool.items, [&](const auto& item) {
        return std::none_of(item.sources.begin(), item.sources.end(),
            [&](const auto& source) { return source.id == freeSource.id; });
    });
    require(!freePool.items.empty(), "installed free Wish lost the selected V6 source");
    wish::WishRound freeRound(std::move(freePool), 6848);
    auto freeItems = freeRound.draw(1);
    require(freeItems.size() == 1 && !freeItems.front().match &&
        std::any_of(freeItems.front().sources.begin(), freeItems.front().sources.end(),
            [&](const auto& source) { return source.id == freeSource.id; }), "installed free Wish source identity");

    const auto& inputSource = find(factory, "V6_B3_01_056");
    const KeySignature inputKey{PitchClass::C, inputSource.mode};
    wish::WishInput input;
    require(inputSource.full.size() > 7, "installed input Wish source is too short for reviewed prefix");
    require(input.imported.replace(makePrefix(inputSource, inputKey, 7), TimelineCoordinateMode::RelativeToSelection),
        "installed input Wish prefix");
    input.analysis.forcedKey = inputKey;
    auto inputPool = wish::makeInputPool(factory.templates, annotations.catalog, input);
    require(static_cast<bool>(inputPool), "installed input Wish pool: " + inputPool.error);
    std::erase_if(inputPool.items, [&](const auto& item) {
        return std::none_of(item.sources.begin(), item.sources.end(),
            [&](const auto& source) { return source.id == inputSource.id; });
    });
    require(!inputPool.items.empty(), "installed input Wish lost the exact V6 source in full Factory search");
    wish::WishRound inputRound(std::move(inputPool), 6856);
    auto inputItems = inputRound.draw(1);
    require(inputItems.size() == 1 && inputItems.front().match.has_value(),
            "installed input Wish draw/match identity");

    verifyWishItemRoundTrip(freeItems.front(), factory);
    verifyWishItemRoundTrip(inputItems.front(), factory);

    auto isolatedUser = factory.templates.front();
    isolatedUser.id = "USER_INSTALLED_SMOKE_SOURCE";
    isolatedUser.name = isolatedUser.nameEn = isolatedUser.nameZh = isolatedUser.id;
    isolatedUser.aliases.clear();
    isolatedUser.sourceType = "user";
    const auto userPath = fixtureDirectory / "installed-smoke-user.db";
    std::error_code ignored;
    std::filesystem::remove(userPath, ignored);
    std::string userError;
    require(library::UserLibrary(userPath).addProgression(isolatedUser, userError),
            "installed User fixture save: " + userError);
    const auto loadedUser = library::UserLibrary(userPath).loadAll();
    require(static_cast<bool>(loadedUser) && loadedUser.templates.size() == 1 &&
        loadedUser.templates.front().id == isolatedUser.id,
        "installed User fixture complete retrieval");
    auto mergedSources = factory.templates;
    mergedSources.insert(mergedSources.end(), loadedUser.templates.begin(), loadedUser.templates.end());
    const CandidateIndex mergedIndex(std::move(mergedSources));
    const auto userQuery = queryFor(isolatedUser, {PitchClass::C, isolatedUser.mode},
        isolatedUser.full.size() > 2 ? isolatedUser.full.size() - 1 : 1);
    const auto userMatches = matchProgression(userQuery, mergedIndex, MatchWeights{},
        mergedIndex.templates().size());
    require(std::any_of(userMatches.begin(), userMatches.end(), [&](const auto& match) {
        return match.templateId == isolatedUser.id;
    }), "full Factory plus User fixture retrieval lost the User source");

    std::cout << "Factory V6 installed smoke PASS factory=" << factory.templates.size()
              << " active_selection=" << (activeSelection ? "6" : "build")
              << " hca_bytes=" << hcaBytes << " bound_sources=" << annotations.catalog.entries.size()
              << " fixed_c_strict_modes=5 free_wish=1 input_wish=1 user_full_retrieval=1"
              << " recommendation_queries=4 enrich_candidates=" << enrichmentCandidates
              << " preview_midi_snapshot_cards=2\n";
    return 0;
}
} // namespace
int main(int argc, char** argv) {
    static_assert(library::factorySchemaVersion == 2);
    static_assert(library::userSchemaVersion == 2);
    static_assert(snapshot::RecommendationSnapshot::currentSchemaVersion == 3);
    static_assert(session::PluginSessionState::currentSchemaVersion == 5);
    if (argc > 1 && std::string_view(argv[1]) == "--installed-smoke") {
        try {
            require(argc == 4, "usage: FactoryV6Tests --installed-smoke factory.db fixture-dir");
            return runInstalledSmoke(argv[2], argv[3]);
        } catch (const std::exception& error) {
            std::cerr << "Factory V6 installed smoke FAIL: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc > 1 && std::string_view(argv[1]) == "--incremental") {
        try {
            const bool finalInventory = argc == 14 && std::string_view(argv[13]) == "--inventory";
            require(argc == 13 || finalInventory,
                    "usage: FactoryV6Tests --incremental v5.db candidates.json metadata.json "
                                "approved-ids.txt proposal.db experiment.db factory-v5.hca merged-v6.hca "
                                "media-dir selected-ids.txt sample-ids.txt [--inventory]");
            return runIncremental(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7],
                                  argv[8], argv[9], argv[10], argv[11], argv[12], finalInventory);
        } catch (const std::exception& error) {
            std::cerr << "Factory V6 incremental verification FAIL: " << error.what() << '\n';
            return 1;
        }
    }
    if (argc > 1 && std::string_view(argv[1]) == "--r1-default-scan") {
        try {
            require(argc == 6, "usage: FactoryV6Tests --r1-default-scan v5.db candidates.json metadata.json output-dir");
            return runR1DefaultScan(argv[2], argv[3], argv[4], argv[5]);
        } catch (const std::exception& error) {
            std::cerr << "Factory V6 R1 default-scan FAIL: " << error.what() << '\n';
            return 1;
        }
    }
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
        const auto admissionMetadata = benchmark::json::parse(readText(argv[3]));
        std::set<std::string> gradeAIds;
        for (const auto& row : admissionMetadata.at("candidates").items())
            if (row.at("grade").text() == "A")
                gradeAIds.insert(row.at("id").text());
        require(!gradeAIds.empty(), "grade-A shortlist shadow is empty");
        std::vector<ProgressionTemplate> shadowEntries = baseline.templates;
        for (const auto& item : parsed.templates)
            if (gradeAIds.contains(item.id)) shadowEntries.push_back(item);
        require(shadowEntries.size() == baseline.templates.size() + gradeAIds.size(),
                "grade-A shadow is not exactly baseline plus all grade-A sources");
        const CandidateIndex shadowIndex(std::move(shadowEntries));
        const auto budgetHoldGradeA = std::count_if(gradeAIds.begin(), gradeAIds.end(),
            [&](const auto& id) { return !approved.contains(id); });
        std::cout << "shortlist_shadow baseline=" << baseline.templates.size()
                  << " grade_A_sources=" << gradeAIds.size()
                  << " budget_hold_grade_A=" << budgetHoldGradeA
                  << " entries=" << shadowIndex.templates().size()
                  << " includes_all_grade_A_even_if_pending=yes\n";
        verifyWishModePools(baseline, experimental, argv[7], argv[8]);
        verifyApprovedMidi(parsed.templates, approved);
        verifyApprovedEnrichment(parsed.templates, approved);
        writeRepresentativeMedia(parsed.templates, approved, argv[9]);
        auto anyPrefixQualified = verifyRecall(experimental, approved, experimentalIndex);
        anyPrefixQualified = verifyTargetPrefixQualification(
            experimental, experimentalIndex, approved, std::move(anyPrefixQualified));
        verifyStableRanking(experimentalIndex, find(experimental, *approved.begin()));
        for (const auto* id : {"V6_B3_01_048", "V6_B3_01_056", "V6_B3_01_065", "V6_B3_01_071"})
            verifyTranspositionRecommendationStability(experimentalIndex, find(experimental, id));
        for (const auto& id : approved)
            verifySnapshotAndPreview(experimental, find(experimental, id), experimentalIndex);
        std::cout << "snapshot_preview approved_sources=" << approved.size()
                  << " shortest_legal_prefix_search=PASS restored_preview=PASS\n";

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

        std::vector<QueryCase> gradeAShadowCases;
        for (const auto& id : gradeAIds) {
            const auto& item = find(parsed.templates, id);
            gradeAShadowCases.push_back(
                {&item, 0, item.full.size() > 2 ? item.full.size() - 1 : 1});
        }
        verifyActualShortlistGate(shadowIndex, experimentalIndex, gradeAShadowCases, argv[9]);
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
