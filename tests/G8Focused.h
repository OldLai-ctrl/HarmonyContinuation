#pragma once

#include "core/RecommendationPolicy.h"
#include "library/LegacyFactoryIdResolver.h"
#include "midi/MidiImportWorkflow.h"
#include "midi/MidiWorkflow.h"
#include "preview/OfflinePreviewRenderer.h"
#include "session/PluginSessionState.h"
#include "snapshot/RecommendationSnapshot.h"
#include "ui/RecommendationBrowse.h"
#include "wish/MusicIdentity.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
using namespace harmony;
using G8Clock = std::chrono::steady_clock;
inline constexpr std::uint32_t g8SampleSeed = 20261027;
inline constexpr std::size_t g8SampleCount = 50;

struct G8Start {
    const ProgressionTemplate* item{};
    KeySignature key;
    std::string identity;
};

struct G8RoundRecord {
    ImportedProgressionSession input;
    ContinuationCandidate candidate;
    preview::Sequence preview;
    midi::MidiClipPayload payload;
    std::string inputFingerprint;
    std::string realizedFingerprint;
    std::size_t flatIndex{}, page{};
    double recommendationMs{}, elapsedMs{};
};

std::string g8Tsv(std::string value) {
    for (auto& ch : value) if (ch == '\t' || ch == '\r' || ch == '\n') ch = ' ';
    return value;
}

std::uint16_t g8AbsolutePitchMask(const ChordPitchSet& chord);

void g8AppendMusicEvent(std::ostream& output, const ChordPitchSet& chord, double duration) {
    output << '|' << chord.root << ':' << chord.bass << ':' << static_cast<int>(chord.quality)
           << ':' << g8AbsolutePitchMask(chord) << '@' << duration;
}

std::string g8EventListFingerprint(const Progression& progression) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17);
    for (const auto& chord : progression)
        g8AppendMusicEvent(output, chordPitches(chord), chord.durationQN.value_or(0.0));
    return output.str();
}

std::string g8CandidateMusicFingerprint(const ContinuationCandidate& candidate) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17);
    for (const auto& event : candidate.continuation) {
        const auto chord = event.harmonicData ? chordPitches(*event.harmonicData)
                                               : chordPitches(event.label, event.quality);
        g8AppendMusicEvent(output, chord, event.durationQN);
    }
    return output.str();
}

std::string g8EventListFingerprint(const preview::Sequence& sequence) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(17);
    for (const auto& chord : sequence.events)
        g8AppendMusicEvent(output, chord.chord, chord.durationQN);
    return output.str();
}

std::size_t g8RootCount(const ContinuationCandidate& candidate) {
    std::set<int> roots;
    for (const auto& event : candidate.continuation) {
        const auto chord = event.harmonicData ? chordPitches(*event.harmonicData)
                                               : chordPitches(event.label, event.quality);
        if (chord.root >= 0) roots.insert(chord.root);
    }
    return roots.size();
}

std::uint16_t g8AbsolutePitchMask(const ChordPitchSet& chord) {
    if (chord.root < 0) return 0;
    std::uint16_t result{};
    const auto relative = static_cast<std::uint16_t>(chord.intervals | chord.colorMask);
    for (int interval = 0; interval < 12; ++interval)
        if (relative & (1u << interval)) result |= 1u << ((chord.root + interval) % 12);
    if (chord.bass >= 0) result |= 1u << chord.bass;
    return result;
}

bool g8SameHarmony(const ChordPitchSet& left, const ChordPitchSet& right) {
    return left.root >= 0 && right.root >= 0 && left.bass >= 0 && right.bass >= 0 &&
        left.root == right.root && left.bass == right.bass && left.quality == right.quality &&
        g8AbsolutePitchMask(left) == g8AbsolutePitchMask(right);
}

bool g8SameMidiNotes(const midi::MidiFile& left, const midi::MidiFile& right) {
    if (left.ppq != right.ppq || left.notes.size() != right.notes.size()) return false;
    auto a = left.notes;
    auto b = right.notes;
    const auto order = [](const auto& x, const auto& y) {
        return std::tie(x.startQN, x.pitch, x.durationQN, x.channel) <
               std::tie(y.startQN, y.pitch, y.durationQN, y.channel);
    };
    std::sort(a.begin(), a.end(), order);
    std::sort(b.begin(), b.end(), order);
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].pitch != b[i].pitch || a[i].channel != b[i].channel ||
            std::abs(a[i].startQN - b[i].startQN) > 0.5 / left.ppq ||
            std::abs(a[i].durationQN - b[i].durationQN) > 0.5 / left.ppq)
            return false;
    return true;
}

bool g8SameExportNotes(const midi::ExportSequence& expected, const midi::MidiFile& actual) {
    if (expected.notes.size() != actual.notes.size()) return false;
    using NoteKey = std::tuple<int, int, double, double>;
    std::vector<NoteKey> left, right;
    left.reserve(expected.notes.size());
    right.reserve(actual.notes.size());
    for (const auto& note : expected.notes)
        left.emplace_back(note.midiNote, note.channel, note.startQN, note.durationQN);
    for (const auto& note : actual.notes)
        right.emplace_back(note.pitch, note.channel, note.startQN, note.durationQN);
    const auto order = [](const auto& a, const auto& b) { return a < b; };
    std::sort(left.begin(), left.end(), order);
    std::sort(right.begin(), right.end(), order);
    for (std::size_t i = 0; i < left.size(); ++i)
        if (std::get<0>(left[i]) != std::get<0>(right[i]) ||
            std::get<1>(left[i]) != std::get<1>(right[i]) ||
            std::abs(std::get<2>(left[i]) - std::get<2>(right[i])) > 0.5 / 480.0 ||
            std::abs(std::get<3>(left[i]) - std::get<3>(right[i])) > 0.5 / 480.0)
            return false;
    return true;
}

bool g8SamePreview(const preview::Sequence& left, const preview::Sequence& right) {
    if (left.events.size() != right.events.size() || left.recommendationBoundary != right.recommendationBoundary ||
        std::abs(left.tempoBPM - right.tempoBPM) > 1e-9 || std::abs(left.totalQN - right.totalQN) > 1e-8)
        return false;
    for (std::size_t i = 0; i < left.events.size(); ++i) {
        const auto& a = left.events[i];
        const auto& b = right.events[i];
        if (!g8SameHarmony(a.chord, b.chord) || a.segment != b.segment ||
            std::abs(a.startQN - b.startQN) > 0.5 / 480.0 ||
            std::abs(a.durationQN - b.durationQN) > 0.5 / 480.0)
            return false;
    }
    return true;
}

struct G8MidiImportSummary {
    std::size_t reinterpretationCount{};
    std::string reinterpretations;
};

G8MidiImportSummary g8CompareMidiToPreview(const preview::Sequence& expected,
                            const midi::ExportSequence& expectedClip, const Progression& actual,
                            const midi::MidiFile& actualMidi, bool openEnded,
                            std::string_view where) {
    require(expected.events.size() == actual.size(),
            std::string(where) + ": MIDI import changed chord count");
    require(expectedClip.chordCount == expected.events.size(),
            std::string(where) + ": Preview-derived Clip changed chord count");
    require(g8SameExportNotes(expectedClip, actualMidi),
            std::string(where) + ": MIDI import changed physical notes, onset, or note duration");
    G8MidiImportSummary summary;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto got = chordPitches(actual[i]);
        std::uint16_t expectedMask{};
        int minimumPitch = 128;
        std::size_t eventNoteCount{};
        for (const auto& note : expectedClip.notes) {
            if (std::abs(note.startQN - expected.events[i].startQN) > 0.5 / 480.0) continue;
            expectedMask |= static_cast<std::uint16_t>(1u << (note.midiNote % 12));
            minimumPitch = (std::min)(minimumPitch, note.midiNote);
            ++eventNoteCount;
            require(std::abs(note.durationQN - expected.events[i].durationQN) <= 0.5 / 480.0,
                    std::string(where) + ": Preview-derived Clip changed note duration at chord " +
                        std::to_string(i));
        }
        require(eventNoteCount > 0 && expectedMask != 0,
                std::string(where) + ": Preview-derived Clip has no physical notes at chord " +
                    std::to_string(i));
        require(g8AbsolutePitchMask(got) == expectedMask && got.bass == minimumPitch % 12,
                std::string(where) + ": MIDI import changed physical pitch classes or minimum bass at chord " +
                    std::to_string(i));
        require(std::abs(actual[i].startQN - expected.events[i].startQN) <= 0.5 / 480.0,
                std::string(where) + ": MIDI import changed event onset at chord " + std::to_string(i));
        const bool isOpenTail = openEnded && i + 1 == actual.size();
        if (isOpenTail) {
            require(actual[i].openEnded && !actual[i].durationQN,
                    std::string(where) + ": MIDI open import did not preserve final OPEN");
        } else {
            require(actual[i].durationQN && !actual[i].openEnded &&
                    std::abs(*actual[i].durationQN - expected.events[i].durationQN) <= 0.5 / 480.0,
                    std::string(where) + ": MIDI import changed duration at chord " +
                        std::to_string(i));
        }
        if (got.root != expected.events[i].chord.root ||
            got.quality != expected.events[i].chord.quality) {
            ++summary.reinterpretationCount;
            if (!summary.reinterpretations.empty()) summary.reinterpretations += ";";
            summary.reinterpretations += g8Tsv(expected.events[i].chord.label) + "->" +
                g8Tsv(actual[i].name);
        }
    }
    return summary;
}

MatchQuery g8MakeQuery(const Progression& input,
                       std::optional<std::pair<int, int>> meter = std::nullopt) {
    auto query = makeMatchQuery(input, recommendationContext(AnalysisContext{}));
    if (meter) {
        query.timeSigNumerator = meter->first;
        query.timeSigDenominator = meter->second;
    }
    return query;
}

RecommendationSet g8NormalRecommendations(const MatchQuery& query, const CandidateIndex& index) {
    const auto weights = recommendationWeights();
    RecommendationRequest exactRequest;
    exactRequest.inspirationPriority = false;
    const auto exact = recommendContinuations(query, index, exactRequest, weights);

    RecommendationRequest inspirationRequest;
    inspirationRequest.inspirationPriority = true;
    inspirationRequest.preferredIntent.reset();
    auto inspiration = recommendContinuations(query, index, inspirationRequest, weights, &exact);
    const auto analysis = analyzeHarmony(query.source, recommendationContext(AnalysisContext{}));
    const auto inputCenter = analysis.selectedKey ? std::optional(analysis.selectedKey->key) : std::nullopt;
    for (auto& group : inspiration.groups) for (auto& candidate : group) {
        if (candidate.channel != RecommendationChannel::Exact && !candidate.continuation.empty())
            require(candidate.sourceEndPreserved,
                    "Inspiration candidate does not preserve its complete source ending: " +
                        candidate.primaryTemplate);
        if (candidate.ending == PhraseEnding::CenterTransition)
            require(!candidate.targetCenter && candidate.centerRelation == CenterRelation::Undetermined,
                    "center-transition ending claims an established destination: " + candidate.primaryTemplate);
        candidate.direction = inspirationDirection(candidate, inputCenter);
        if (candidate.channel == RecommendationChannel::Exact) candidate.originCenter = inputCenter;
    }
    inspiration.inspirationOrder = mixedInspirationOrder(inspiration);
    return inspiration;
}

std::string g8SourceFamily(const ContinuationCandidate& candidate, const CandidateIndex& index) {
    const auto source = std::find_if(index.templates().begin(), index.templates().end(),
        [&](const auto& item) { return item.id == candidate.primaryTemplate; });
    if (source == index.templates().end()) return candidate.family;
    std::string family;
    for (const auto& tag : source->tags) if (tag != "V7-editorial") { family = tag; break; }
    if (family.empty()) for (const auto& event : source->full) if (event.degree)
        family += std::to_string(event.degree->degree) + ":" +
            std::to_string(event.degree->alteration) + ",";
    return family;
}

bool g8Eligible(const ContinuationCandidate& candidate, const CandidateIndex& index,
                const std::set<std::string>& usedMusic,
                const std::set<TemplateID>& usedSources,
                const std::set<std::string>& usedFamilies) {
    if (!candidate.baseQualified || !candidate.hardConstraintsPassed || candidate.exploratory ||
        !candidate.reviewedEnding || !candidate.sourceEndPreserved ||
        !unterminatedEnding(candidate.ending) ||
        !candidate.completeSourcePhrase || candidate.continuation.size() < 4 ||
        g8RootCount(candidate) < 3)
        return false;
    if (usedMusic.contains(g8CandidateMusicFingerprint(candidate)) ||
        usedSources.contains(candidate.primaryTemplate) ||
        usedFamilies.contains(g8SourceFamily(candidate, index)))
        return false;
    return true;
}

const ContinuationCandidate* g8CandidateAt(const RecommendationSet& recommendations,
                                           RecommendationPosition position) {
    if (position.group >= recommendations.groups.size() ||
        position.index >= recommendations.groups[position.group].size()) return nullptr;
    return &recommendations.groups[position.group][position.index];
}

std::string g8NextIds(const ContinuationCandidate& candidate) {
    return g6Join(candidate.nextPhraseSources, ";", [](const auto& id) { return g8Tsv(id); });
}

std::string g8ContinuationLabels(const ContinuationCandidate& candidate) {
    return g6Join(candidate.continuation, " | ", [](const auto& event) { return g8Tsv(event.label); });
}

std::vector<ConcreteChordEvent> g8Concrete(const Progression& progression) {
    std::vector<ConcreteChordEvent> result;
    result.reserve(progression.size());
    for (const auto& chord : progression) {
        ConcreteChordEvent event;
        event.label = chord.name;
        event.durationQN = chord.durationQN.value_or(4.0);
        event.quality = chord.quality;
        event.harmonicData = chord;
        result.push_back(std::move(event));
    }
    return result;
}

void g8WriteRepresentative(const std::filesystem::path& outputDirectory,
                           const std::string& startId, std::size_t representativeIndex,
                           const std::vector<G8RoundRecord>& rounds,
                           const library::LoadResult& factory, int meterNumerator,
                           int meterDenominator) {
    const auto pathDirectory = outputDirectory / "representative-paths" /
        (std::to_string(representativeIndex + 1) + "-" + startId);
    std::filesystem::create_directories(pathDirectory);
    const midi::Meter meter{meterNumerator, meterDenominator};
    for (std::size_t i = 0; i < rounds.size(); ++i) {
        const auto& round = rounds[i];
        const auto stem = startId + "-round-" + std::to_string(i + 1);
        auto snapshotValue = snapshot::capture(round.input, round.candidate, {}, 120.0,
            meterNumerator, meterDenominator, round.candidate.key, {}, round.candidate.intent);
        snapshotValue.factoryLibraryVersion = factory.libraryVersion;
        const auto snapshotPath = pathDirectory / (stem + ".snapshot3.json");
        std::string error;
        require(snapshot::saveFile(snapshotValue, snapshotPath, error),
                "G8 representative Snapshot3 write: " + error);
        const auto reopened = snapshot::loadFile(snapshotPath, &factory);
        require(static_cast<bool>(reopened) && reopened.value.candidateAvailable,
                "G8 representative Snapshot3 read: " + reopened.error);
        require(reopened.value.schemaVersion == snapshot::RecommendationSnapshot::currentSchemaVersion &&
                continuationMusicFingerprint(reopened.value.candidate) ==
                    continuationMusicFingerprint(round.candidate),
                "G8 Snapshot3 changed the selected continuation identity");
        const auto restoredPreview = preview::buildSequence(
            reopened.value.imported, &reopened.value.candidate, 120.0);
        require(static_cast<bool>(restoredPreview) && g8SamePreview(round.preview, restoredPreview.sequence),
                "G8 Snapshot3 changed the frozen Preview");
        const midi::CandidateExportContext context{reopened.value.imported, 120.0, meter,
            reopened.value.candidate.key, midi::ArrangementMode::VoiceLed,
            midi::ExportScope::FullPhrase};
        const auto restoredPayload = midi::candidatePayload(context, reopened.value.candidate);
        require(static_cast<bool>(restoredPayload) &&
                restoredPayload.payload.smfBytes == round.payload.smfBytes &&
                restoredPayload.payload.dawClipBytes == round.payload.dawClipBytes,
                "G8 Snapshot3 changed the production MIDI payload");
        const auto audio = preview::renderOffline(round.preview, 48000.0);
        require(!audio.left.empty() && audio.left.size() == audio.right.size() && audio.rms > 0.0,
                "G8 representative Preview is silent");
        const auto midiPath = pathDirectory / (stem + ".mid");
        const auto wavPath = pathDirectory / (stem + ".wav");
        require(midi::writeToFile(round.payload.dawClipBytes, midiPath, error),
                "G8 representative MIDI write: " + error);
        require(preview::writeWav16(audio, wavPath, error),
                "G8 representative WAV write: " + error);
    }
}

int runG8Focused(const std::filesystem::path& factoryPath,
                 const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G8 Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G8 focused acceptance requires Factory version 8 or newer");
    std::filesystem::create_directories(outputDirectory);

    std::vector<G8Start> validStarts;
    std::set<std::string> startIdentities;
    validStarts.reserve(factory.templates.size());
    for (const auto& item : factory.templates) {
        if (item.full.empty() || item.full.size() > 64) continue;
        const auto key = item.rawChordKey.value_or(KeySignature{PitchClass::C, item.mode});
        const auto route = preview::templateRoute(item, key);
        if (!route) continue;
        const auto audible = midi::previewFromTemplate(item, key, 120.0);
        if (!audible || audible.sequence.events.empty()) continue;
        const auto identity = library::canonicalMusicalFingerprint(item);
        if (!startIdentities.insert(identity).second) continue;
        validStarts.push_back({&item, key, identity});
    }
    require(validStarts.size() >= g8SampleCount,
            "G8 valid full-Factory start pool has fewer than 50 distinct music identities");
    const auto populationSize = validStarts.size();
    std::mt19937 generator(g8SampleSeed);
    std::shuffle(validStarts.begin(), validStarts.end(), generator);
    validStarts.resize(g8SampleCount);

    const auto sampledPath = outputDirectory / "sampled-starts.tsv";
    {
        std::ofstream sampled(sampledPath, std::ios::binary | std::ios::trunc);
        require(static_cast<bool>(sampled), "cannot create sampled-starts.tsv");
        sampled << "order\tstart_source_id\tcomplete_factory_music_identity\n";
        for (std::size_t i = 0; i < validStarts.size(); ++i)
            sampled << i + 1 << '\t' << g8Tsv(validStarts[i].item->id) << '\t'
                    << g8Tsv(validStarts[i].identity) << '\n';
        sampled.flush();
        require(static_cast<bool>(sampled), "cannot finish sampled-starts.tsv");
    }

    CandidateIndex index(factory.templates);
    const auto roundsPath = outputDirectory / "g8-rounds.tsv";
    const auto startsPath = outputDirectory / "g8-start-results.tsv";
    std::ofstream roundsReport(roundsPath, std::ios::binary | std::ios::trunc);
    std::ofstream startsReport(startsPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(roundsReport) && static_cast<bool>(startsReport),
            "cannot create G8 RCR reports");
    roundsReport << "sample_order\tstart_id\tround\tvisible_flat_index_0based\tpage_1based"
                    "\tprimary_source\tfamily\tchannel\tending\tnext_phrase_ids\tcontinuation"
                    "\tinput_length\tquery_input_fingerprint\tactual_import_fingerprint"
                    "\trecommendation_ms\tmidi_reinterpretation_count\tmidi_reinterpretations"
                    "\telapsed_ms\n";
    startsReport << "sample_order\tstart_id\tstart_identity\tstatus\tcompleted_rounds"
                    "\tfirst_page_rounds\treason\n";

    std::size_t passed{}, representatives{}, allThreeFirstPage{};
    const auto tempDirectory = outputDirectory / ".g8-midi-import";
    std::filesystem::create_directories(tempDirectory);
    for (std::size_t sample = 0; sample < validStarts.size(); ++sample) {
        const auto& start = validStarts[sample];
        std::vector<G8RoundRecord> chain;
        std::string failure;
        std::size_t firstPageRounds{};
        try {
            const auto route = preview::templateRoute(*start.item, start.key);
            require(static_cast<bool>(route), "A template route failed: " + route.error);
            const auto aPreview = midi::previewFromTemplate(*start.item, start.key, 120.0);
            require(static_cast<bool>(aPreview), "A normal template playback failed: " + aPreview.error);
            const auto routePreview = preview::buildSequence(route.imported, nullptr, 120.0);
            require(static_cast<bool>(routePreview) && g8SamePreview(aPreview.sequence, routePreview.sequence),
                    "A template playback and imported route differ");

            ImportedProgressionSession current = route.imported;
            require(!current.events.empty(), "A template route has no imported chords");
            current.events.back().durationQN.reset();
            current.events.back().openEnded = true;
            std::set<std::string> usedMusic{g8EventListFingerprint(aPreview.sequence)};
            std::set<TemplateID> usedSources{start.item->id};
            std::set<std::string> usedFamilies;
            const midi::Meter meter{route.meterNumerator, route.meterDenominator};

            for (std::size_t round = 1; round <= 3; ++round) {
                const auto roundStarted = G8Clock::now();
                const auto inputFingerprint = g8EventListFingerprint(current.events);
                const auto queryStarted = G8Clock::now();
                const auto query = g8MakeQuery(current.events,
                    std::pair{meter.numerator, meter.denominator});
                require(!query.interpretations.empty(), "RCR input has no key interpretation");
                const auto recommendations = g8NormalRecommendations(query, index);
                const auto recommendationMs = std::chrono::duration<double, std::milli>(
                    G8Clock::now() - queryStarted).count();
                require(g8EventListFingerprint(current.events) == inputFingerprint,
                        "recommendation query rewrote imported input chord identity");
                const auto order = mixedInspirationOrder(recommendations);
                const ContinuationCandidate* selected{};
                std::size_t selectedFlat{};
                for (std::size_t visible = 0; visible < order.size(); ++visible) {
                    const auto* candidate = g8CandidateAt(recommendations, order[visible]);
                    if (candidate && g8Eligible(*candidate, index, usedMusic, usedSources, usedFamilies)) {
                        selected = candidate;
                        selectedFlat = visible;
                        break;
                    }
                }
                require(selected != nullptr, "no eligible complete open phrase in mixed Inspiration order");

                const auto built = preview::buildSequence(current, selected, 120.0);
                require(static_cast<bool>(built), "RCR Preview failed: " + built.error);
                midi::CandidateExportContext exportContext{current, 120.0, meter, selected->key,
                    midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
                const auto payload = midi::candidatePayload(exportContext, *selected);
                require(static_cast<bool>(payload) && !payload.payload.dawClipBytes.empty(),
                        "production candidate MIDI/DAWClip export failed: " + payload.error);

                const auto clip = midi::buildClip(built.sequence, midi::ArrangementMode::VoiceLed,
                    midi::ExportScope::FullPhrase, meter, selected->key, selected->intent);
                require(static_cast<bool>(clip), "production MIDI Clip construction failed: " + clip.error);
                const auto annotated = midi::writeToMemory(clip.sequence);
                require(static_cast<bool>(annotated) && annotated.bytes == payload.payload.smfBytes,
                        "candidate MIDI payload differs from Preview-derived Clip");
                midi::ExportConfig dawConfig;
                dawConfig.profile = midi::ExportProfile::DAWClip;
                const auto dawClip = midi::writeToMemory(clip.sequence, dawConfig);
                require(static_cast<bool>(dawClip) && dawClip.bytes == payload.payload.dawClipBytes,
                        "candidate DAWClip payload differs from Preview-derived Clip");

                const auto stem = std::to_string(sample + 1) + "-" + std::to_string(round);
                const auto annotatedPath = tempDirectory / (stem + "-annotated.mid");
                const auto dawPath = tempDirectory / (stem + "-daw.mid");
                std::string writeError;
                require(midi::writeToFile(payload.payload.smfBytes, annotatedPath, writeError),
                        "production annotated MIDI file write failed: " + writeError);
                require(midi::writeToFile(payload.payload.dawClipBytes, dawPath, writeError),
                        "production DAWClip file write failed: " + writeError);
                const auto annotatedImport = midi::importFile(annotatedPath, false);
                require(static_cast<bool>(annotatedImport),
                        "annotated MIDI import failed: " + annotatedImport.extraction.error);
                const auto completeImport = midi::importFile(dawPath, false);
                require(static_cast<bool>(completeImport), "DAWClip MIDI import failed: " + completeImport.extraction.error);
                require(g8SameMidiNotes(annotatedImport.midi.file, completeImport.midi.file),
                        "Annotated and DAWClip MIDI note performances differ");
                ImportedProgressionSession completeSession;
                require(midi::applyImport(completeImport, completeSession),
                        "normal MIDI applyImport failed for complete phrase");
                const auto dawImportSummary = g8CompareMidiToPreview(built.sequence, clip.sequence,
                    completeSession.events, completeImport.midi.file, false,
                    "RCR DAWClip complete import");
                ImportedProgressionSession annotatedSession;
                require(midi::applyImport(annotatedImport, annotatedSession),
                        "annotated MIDI applyImport failed for complete phrase");
                (void)g8CompareMidiToPreview(built.sequence, clip.sequence,
                    annotatedSession.events, annotatedImport.midi.file, false,
                    "RCR annotated complete import");
                require(g8EventListFingerprint(annotatedSession.events) ==
                            g8EventListFingerprint(completeSession.events),
                        "Annotated and DAWClip import semantics differ");
                const auto openImport = midi::importFile(dawPath, true);
                require(static_cast<bool>(openImport), "normal OPEN MIDI import failed: " + openImport.extraction.error);
                ImportedProgressionSession next;
                require(midi::applyImport(openImport, next), "normal MIDI applyImport failed for OPEN phrase");
                (void)g8CompareMidiToPreview(built.sequence, clip.sequence, next.events,
                    openImport.midi.file, true, "RCR OPEN import");
                std::error_code removeError;
                std::filesystem::remove(annotatedPath, removeError);
                std::filesystem::remove(dawPath, removeError);

                const auto realizedFingerprint = g8EventListFingerprint(completeSession.events);
                const auto elapsedMs = std::chrono::duration<double, std::milli>(
                    G8Clock::now() - roundStarted).count();
                if (selectedFlat < ui::recommendationPageSize) ++firstPageRounds;
                chain.push_back({current, *selected, built.sequence, payload.payload,
                    inputFingerprint, realizedFingerprint, selectedFlat,
                    selectedFlat / ui::recommendationPageSize + 1, recommendationMs, elapsedMs});
                roundsReport << sample + 1 << '\t' << g8Tsv(start.item->id) << '\t' << round << '\t'
                    << selectedFlat << '\t' << selectedFlat / ui::recommendationPageSize + 1 << '\t'
                    << g8Tsv(selected->primaryTemplate) << '\t' << g8SourceFamily(*selected, index)
                    << '\t' << recommendationChannelName(selected->channel)
                    << '\t' << phraseEndingName(selected->ending) << '\t' << g8NextIds(*selected) << '\t'
                    << g8ContinuationLabels(*selected) << '\t' << current.events.size() << '\t'
                    << g8Tsv(inputFingerprint) << '\t' << g8Tsv(realizedFingerprint) << '\t'
                    << std::fixed << std::setprecision(3)
                    << recommendationMs << '\t' << dawImportSummary.reinterpretationCount << '\t'
                    << g8Tsv(dawImportSummary.reinterpretations) << '\t' << elapsedMs << '\n';
                require(static_cast<bool>(roundsReport), "cannot finish G8 round report");

                usedMusic.insert(g8CandidateMusicFingerprint(*selected));
                usedSources.insert(selected->primaryTemplate);
                usedFamilies.insert(g8SourceFamily(*selected, index));
                current = std::move(next);
            }

            require(chain.size() == 3, "RCR chain did not complete A to B to C to D");
            if (representatives < 3) {
                g8WriteRepresentative(outputDirectory, start.item->id, representatives,
                    chain, factory, meter.numerator, meter.denominator);
                ++representatives;
            }
            ++passed;
            allThreeFirstPage += firstPageRounds == 3 ? 1 : 0;
            startsReport << sample + 1 << '\t' << g8Tsv(start.item->id) << '\t'
                << g8Tsv(start.identity) << "\tPASS\t3\t" << firstPageRounds << "\t\n";
        } catch (const std::exception& error) {
            failure = error.what();
            startsReport << sample + 1 << '\t' << g8Tsv(start.item->id) << '\t'
                << g8Tsv(start.identity) << "\tFAIL\t" << chain.size() << '\t'
                << firstPageRounds << '\t' << g8Tsv(failure) << '\n';
        }
        require(static_cast<bool>(startsReport), "cannot finish G8 start results");
        std::cout << "g8_start order=" << sample + 1 << " id=" << start.item->id
                  << " status=" << (failure.empty() ? "PASS" : "FAIL")
                  << " rounds=" << chain.size();
        if (!failure.empty()) std::cout << " reason=" << g8Tsv(failure);
        std::cout << '\n';
    }
    require(static_cast<bool>(roundsReport) && static_cast<bool>(startsReport),
            "cannot finish G8 RCR reports");
    std::cout << "g8_rcr seed=" << g8SampleSeed << " population_size=" << populationSize
              << " sampled=" << g8SampleCount
              << " passed=" << passed << " required=10 representatives=" << representatives
              << " all_three_rounds_page1=" << allThreeFirstPage
              << " sampled_starts=" << sampledPath.string()
              << " rounds=" << roundsPath.string() << " results=" << startsPath.string() << '\n';
    require(passed >= 10, "G8 RCR acceptance below required 10 of 50 completed three-round chains");
    return 0;
}

std::vector<const ProgressionTemplate*> g8CompleteTemplates(const library::LoadResult& factory) {
    std::vector<const ProgressionTemplate*> result;
    for (const auto& item : factory.templates)
        if (std::find(item.tags.begin(), item.tags.end(), "G8-complete") != item.tags.end())
            result.push_back(&item);
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    return result;
}

void g8CheckTemplateData(const library::LoadResult& factory, const CandidateIndex& index,
                         std::ostream& report) {
    const auto complete = g8CompleteTemplates(factory);
    require(complete.size() == 16, "Factory must contain exactly 16 G8 complete phrases");
    std::set<std::string> familyNames;
    std::set<std::string> openFamilies;
    std::size_t openPhraseCount{};
    const std::array<std::string_view, 9> endings{
        "closed", "modulated_closure", "interim_pause", "open_unresolved", "suspended",
        "loop_return", "modal_development", "center_transition", "unknown"};
    for (std::size_t caseIndex = 0; caseIndex < complete.size(); ++caseIndex) {
        const auto& item = *complete[caseIndex];
        const auto expectedId = "V8_01_" + std::string(caseIndex + 1 < 10 ? "00" : "0") +
            std::to_string(caseIndex + 1);
        require(item.id == expectedId, "unexpected G8 source ID ordering: " + item.id);
        require(item.full.size() == 8 && item.rawChords.size() == 8 && item.rawChordKey,
                item.id + ": full/raw source must be eight events with an authored key");
        require(std::find(item.tags.begin(), item.tags.end(), "G8-editorial") != item.tags.end(),
                item.id + ": missing G8-editorial tag");
        std::size_t endingTagCount{};
        std::string endingValue;
        for (const auto& tag : item.tags) if (tag.starts_with("G8-ending-")) {
            ++endingTagCount;
            endingValue = tag.substr(std::string("G8-ending-").size());
            require(std::find(endings.begin(), endings.end(), endingValue) != endings.end(),
                    item.id + ": unrecognized G8 ending tag");
        }
        require(endingTagCount == 1, item.id + ": expected exactly one editorial ending tag");
        if (item.id == "V8_01_015")
            require(std::find(item.tags.begin(), item.tags.end(),
                              "G8-ending-interim_pause") != item.tags.end(),
                    "V8_01_015 must retain its reviewed InterimPause ending");
        double totalQN{};
        for (std::size_t i = 0; i < item.full.size(); ++i) {
            const auto& source = item.full[i];
            require(source.durationQN && *source.durationQN > 0.0 && source.sourceIndex < item.rawChords.size(),
                    item.id + ": source event lacks a valid source index or duration");
            require(source.sourceIndex == i, item.id + ": source index does not map to authored event order");
            totalQN += *source.durationQN;
            const auto realized = realizeContinuation(source, *item.rawChordKey, *source.durationQN);
            const auto projected = realized.harmonicData ? chordPitches(*realized.harmonicData)
                                                          : chordPitches(realized.label, realized.quality);
            const auto raw = chordPitches(item.rawChords[source.sourceIndex]);
            require(g8SameHarmony(projected, raw),
                    item.id + ": realized source bass or pitch mask differs from authored MIDI chord");
            require(item.rawChords[source.sourceIndex].durationQN &&
                    std::abs(*item.rawChords[source.sourceIndex].durationQN - *source.durationQN) < 1e-9,
                    item.id + ": typed/full duration differs from authored source duration");
        }
        require(std::abs(totalQN - 32.0) < 1e-9,
                item.id + ": complete eight-event phrase is not 32 QN");
        require(!item.tags.empty(), item.id + ": missing source family tag");
        familyNames.insert(item.tags.front());
        if (endingValue == "open_unresolved" || endingValue == "suspended" ||
            endingValue == "loop_return" || endingValue == "modal_development" ||
            endingValue == "center_transition") {
            ++openPhraseCount;
            openFamilies.insert(item.tags.front());
        }
        report << "template\tPASS\t" << item.id << "\tevents=8\tqn=" << totalQN
               << "\tending=" << endingValue;
        report << '\n';
    }
    require(familyNames.size() == 13, "G8 complete phrases must cover 13 reviewed source families");
    require(openPhraseCount == 15 && openFamilies.size() == 12,
            "G8 complete source must contain 15 open phrases from 12 families");
    report << "content\tPASS\tG8-complete\tphrases=" << complete.size()
           << "\tfamilies=" << familyNames.size() << "\topen=" << openPhraseCount
           << "\topen_families=" << openFamilies.size() << '\n';
    const auto interimSource = std::find_if(complete.begin(), complete.end(),
        [](const auto* item) { return item->id == "V8_01_015"; });
    require(interimSource != complete.end(), "V8_01_015 InterimPause source is missing");
    ContinuationCandidate interim;
    interim.primaryTemplate = (*interimSource)->id;
    interim.baseQualified = true;
    interim.hardConstraintsPassed = true;
    interim.reviewedEnding = true;
    interim.sourceEndPreserved = true;
    interim.completeSourcePhrase = true;
    interim.ending = PhraseEnding::InterimPause;
    interim.continuation = g8Concrete((*interimSource)->rawChords);
    const std::set<std::string> noMusic{};
    const std::set<TemplateID> noSources{};
    const std::set<std::string> noFamilies{};
    require(!g8Eligible(interim, index, noMusic, noSources, noFamilies),
            "V8_01_015 InterimPause was eligible as an open RCR continuation");
    report << "ending-exclusion\tPASS\tV8_01_015\tInterimPause-not-unterminated\n";
}

void g8CheckLongTail(const std::vector<const ProgressionTemplate*>& complete,
                     const CandidateIndex& index, std::ostream& report) {
    const std::array<std::size_t, 3> lengths{4, 3, 2};
    for (std::size_t test = 0; test < lengths.size(); ++test) {
        const auto length = lengths[test];
        const auto& target = *complete[(test * 5 + 2) % complete.size()];
        const auto key = target.rawChordKey.value_or(KeySignature{PitchClass::C, target.mode});
        const auto targetRoute = preview::templateRoute(target, key);
        require(static_cast<bool>(targetRoute), "G8 long-tail target route: " + targetRoute.error);
        const auto contextEnd = std::size_t(5);
        const auto contextBegin = contextEnd - length;

        const ProgressionTemplate* unrelated{};
        Progression history;
        for (const auto* source : complete) {
            if (source->id == target.id) continue;
            const auto route = preview::templateRoute(*source, key);
            if (!route || route.imported.events.size() < 4) continue;
            bool avoidsLongerWindow = true;
            for (std::size_t extra = 1; extra <= 4 - length; ++extra) {
                if (contextBegin < extra) continue;
                bool same = true;
                for (std::size_t j = 0; j < extra; ++j) {
                    const auto a = chordPitches(route.imported.events[4 - extra + j]);
                    const auto& sourceEvent = target.full[contextBegin - extra + j];
                    const auto b = realizeContinuation(sourceEvent, key,
                        sourceEvent.durationQN.value_or(4.0));
                    const auto bp = b.harmonicData ? chordPitches(*b.harmonicData)
                                                   : chordPitches(b.label, b.quality);
                    same &= g8SameHarmony(a, bp);
                }
                if (same) { avoidsLongerWindow = false; break; }
            }
            if (avoidsLongerWindow) {
                unrelated = source;
                history.assign(route.imported.events.begin(), route.imported.events.begin() + 4);
                break;
            }
        }
        require(unrelated != nullptr, "could not find unrelated G8 history for long-tail check");

        Progression queryEvents;
        for (std::size_t i = 0; i < history.size(); ++i) {
            auto event = history[i];
            event.startQN = static_cast<double>(i) * 4.0;
            event.durationQN = 4.0;
            event.openEnded = false;
            queryEvents.push_back(std::move(event));
        }
        for (std::size_t i = contextBegin; i < contextEnd; ++i) {
            auto event = targetRoute.imported.events[i];
            event.startQN = static_cast<double>(queryEvents.size()) * 4.0;
            event.durationQN = 4.0;
            event.openEnded = false;
            queryEvents.push_back(std::move(event));
        }
        queryEvents.back().durationQN.reset();
        queryEvents.back().openEnded = true;
        AnalysisContext context;
        context.forcedKey = key;
        const auto query = makeMatchQuery(queryEvents, recommendationContext(context));
        const auto results = g8NormalRecommendations(query, index);

        ContinuationCandidate expectedSuffix;
        expectedSuffix.continuation = g8Concrete(Progression(
            targetRoute.imported.events.begin() + static_cast<std::ptrdiff_t>(contextEnd),
            targetRoute.imported.events.end()));
        const auto expectedSuffixIdentity = g8CandidateMusicFingerprint(expectedSuffix);
        std::vector<std::string> exactAnchors;
        for (const auto& anchor : index.contextAnchors()) {
            if (anchor.terminalPrototype || anchor.continuationStart < length ||
                anchor.templateIndex >= index.templates().size()) continue;
            const auto& source = index.templates()[anchor.templateIndex];
            if (source.mode != key.mode || anchor.continuationStart > source.full.size()) continue;
            bool matches = true;
            for (std::size_t j = 0; j < length && matches; ++j) {
                const auto& sourceEvent = source.full[anchor.continuationStart - length + j];
                const auto realized = realizeContinuation(sourceEvent, key,
                    sourceEvent.durationQN.value_or(4.0));
                const auto sourceChord = realized.harmonicData ? chordPitches(*realized.harmonicData)
                    : chordPitches(realized.label, realized.quality);
                matches = g8SameHarmony(chordPitches(queryEvents[queryEvents.size() - length + j]),
                                        sourceChord);
            }
            if (matches) exactAnchors.push_back(source.id + "@" +
                std::to_string(anchor.continuationStart));
        }
        std::vector<std::string> returned;
        for (const auto& group : results.groups) for (const auto& candidate : group) {
            const bool targetSource = candidate.primaryTemplate == target.id ||
                std::find(candidate.supportingTemplates.begin(), candidate.supportingTemplates.end(),
                          target.id) != candidate.supportingTemplates.end();
            const bool sameSuffix = g8CandidateMusicFingerprint(candidate) == expectedSuffixIdentity;
            if (!targetSource && !sameSuffix) continue;
            returned.push_back(std::string(recommendationChannelName(candidate.channel)) + ":" +
                candidate.primaryTemplate + ":context=" + std::to_string(candidate.contextLength) +
                ":start=" + std::to_string(candidate.continuationStart) +
                ":ending=" + phraseEndingName(candidate.ending) + ":sourceEnd=" +
                (candidate.sourceEndPreserved ? "1" : "0") + ":suffix=" +
                g8ContinuationLabels(candidate));
        }
        report << "long-tail-diagnostic\tINFO\t" << length << "\ttarget=" << target.id
               << ";unrelated=" << unrelated->id << ";query="
               << g8Tsv(g8EventListFingerprint(queryEvents)) << ";exact_matching_anchors="
               << (exactAnchors.empty() ? "none" : g6Join(exactAnchors, ",", [](const auto& value) { return value; }))
               << ";target_or_same_suffix_candidates="
               << (returned.empty() ? "none" : g6Join(returned, ",", [](const auto& value) { return value; }))
               << '\n';

        bool found{};
        RecommendationChannel expected = length == 4 ? RecommendationChannel::ContextFour
            : length == 3 ? RecommendationChannel::ContextThree : RecommendationChannel::ContextTwo;
        for (const auto& group : results.groups) for (const auto& candidate : group)
            found |= candidate.primaryTemplate == target.id && candidate.channel == expected &&
                candidate.contextLength == length && candidate.baseQualified && candidate.hardConstraintsPassed;
        require(found, "long-tail " + std::to_string(length) + " search did not retain target after unrelated history");
        report << "long-tail\tPASS\t" << length << "\t" << target.id << "\t"
               << unrelated->id << "\tinput_events=" << queryEvents.size() << '\n';
    }
}

void g8CheckG7AndHardMelody(const library::LoadResult& factory,
                            const std::vector<const ProgressionTemplate*>& complete,
                            const CandidateIndex& index, std::ostream& report) {
    const auto g7 = std::find_if(factory.templates.begin(), factory.templates.end(),
        [](const auto& item) { return item.id == "V7_01_081"; });
    require(g7 != factory.templates.end(), "G7 reviewed ending sample V7_01_081 is missing");
    Progression g7Input;
    for (const auto& [label, start] : std::array<std::pair<const char*, double>, 4>{
            std::pair{"F", 0.0}, {"G", 4.0}, {"Em", 8.0}, {"Am", 12.0}}) {
        ChordEvent event;
        event.name = label;
        event.startQN = start;
        event.durationQN = 4.0;
        event.openEnded = false;
        g7Input.push_back(std::move(event));
    }
    g7Input.back().durationQN.reset();
    g7Input.back().openEnded = true;
    const auto g7Results = g8NormalRecommendations(g8MakeQuery(g7Input), index);
    bool sawG7{};
    for (const auto& group : g7Results.groups) for (const auto& candidate : group)
        if (candidate.primaryTemplate == "V7_01_081") {
            sawG7 = true;
            require(!unterminatedEnding(candidate.ending),
                    "G7 reviewed fourth sample V7_01_081 was incorrectly marked unterminated");
        }
    require(sawG7, "G7 fourth sample V7_01_081 was not retained for ending check");
    report << "g7-ending\tPASS\tV7_01_081\tending-not-open\n";

    bool hardMelodyChecked{};
    for (const auto* source : complete) {
        const auto key = source->rawChordKey.value_or(KeySignature{PitchClass::C, source->mode});
        const auto route = preview::templateRoute(*source, key);
        if (!route) continue;
        auto open = route.imported;
        open.events.back().durationQN.reset();
        open.events.back().openEnded = true;
        const auto query = g8MakeQuery(open.events);
        const auto recommendations = g8NormalRecommendations(query, index);
        RecommendationRequest inspirationRequest;
        inspirationRequest.inspirationPriority = true;
        inspirationRequest.preferredIntent.reset();
        std::ostringstream evidence;
        if (g6RejectHardMelody(source->id, query, index, inspirationRequest,
                               recommendationWeights(), recommendations, evidence)) {
            hardMelodyChecked = true;
            report << "open-hard-melody\tPASS\t" << source->id << "\t"
                   << g8Tsv(evidence.str()) << '\n';
            break;
        }
    }
    require(hardMelodyChecked, "OPEN plus hard-melody rejection was not exercised on G8 input");
}

void g8CheckLongPersistenceAndMidi(const library::LoadResult& factory,
                                  const std::vector<const ProgressionTemplate*>& complete,
                                  const CandidateIndex& index,
                                  std::ostream& report,
                                  const std::filesystem::path& outputDirectory) {
    require(complete.size() >= 3, "need three G8 phrases for long-input compatibility check");
    const auto& historySource = *complete[0];
    const auto& targetSource = *complete[1];
    const auto key = targetSource.rawChordKey.value_or(KeySignature{PitchClass::C, targetSource.mode});
    const auto historyRoute = preview::templateRoute(historySource, key);
    const auto targetRoute = preview::templateRoute(targetSource, key);
    require(historyRoute && targetRoute, "G8 >64 source route failed");

    Progression events;
    while (events.size() < 80) {
        for (const auto& source : historyRoute.imported.events) {
            if (events.size() == 80) break;
            auto event = source;
            event.startQN = static_cast<double>(events.size()) * 4.0;
            event.durationQN = 4.0;
            event.openEnded = false;
            if (!events.empty() && g8SameHarmony(chordPitches(events.back()), chordPitches(event))) continue;
            events.push_back(std::move(event));
        }
    }
    for (std::size_t i = targetRoute.imported.events.size() - 4;
         i < targetRoute.imported.events.size(); ++i) {
        auto event = targetRoute.imported.events[i];
        event.startQN = static_cast<double>(events.size()) * 4.0;
        event.durationQN = 4.0;
        event.openEnded = false;
        events.push_back(std::move(event));
    }
    require(events.size() == 84, "G8 >64 query was not 80 history plus four tail events");
    events.back().durationQN.reset();
    events.back().openEnded = true;
    ImportedProgressionSession imported;
    require(imported.replace(events, TimelineCoordinateMode::RelativeToSelection),
            "G8 >64 imported session creation failed");

    const auto longQuery = g8MakeQuery(imported.events);
    require(!longQuery.interpretations.empty(), "G8 >64 query analysis returned no key interpretations");
    const auto longRecommendations = g8NormalRecommendations(longQuery, index);
    std::size_t longCandidateCount{};
    for (const auto& group : longRecommendations.groups) longCandidateCount += group.size();
    report << "recommendation-capacity\tPASS\tinput=84\tqualified_candidates="
           << longCandidateCount << "\tExactBase=reused\n";

    session::PluginSessionState state;
    state.imported = imported;
    const auto sessionText = session::serialize(state);
    const auto sessionAgain = session::deserialize(sessionText);
    require(static_cast<bool>(sessionAgain) &&
            sessionAgain.state.schemaVersion == session::PluginSessionState::currentSchemaVersion &&
            sessionAgain.state.imported.events.size() == 84 &&
            g8EventListFingerprint(sessionAgain.state.imported.events) == g8EventListFingerprint(imported.events),
            "Session5 did not preserve an 84-event imported progression");
    report << "session5-capacity\tPASS\tevents=84\tschema=5\n";

    const ProgressionTemplate* suffixSourcePointer{};
    ImportedProgressionSession suffixRoute;
    for (const auto* possible : complete) {
        if (possible->id == targetSource.id || possible->id == historySource.id) continue;
        const auto route = preview::templateRoute(*possible, key);
        if (!route || route.imported.events.empty() ||
            g8SameHarmony(chordPitches(events.back()), chordPitches(route.imported.events.front()))) continue;
        suffixSourcePointer = possible;
        suffixRoute = route.imported;
        break;
    }
    require(suffixSourcePointer != nullptr, "no non-merging G8 suffix source for >64 MIDI check");
    const auto& suffixSource = *suffixSourcePointer;

    ContinuationCandidate candidate;
    candidate.id = "g8-capacity:" + suffixSource.id;
    candidate.primaryTemplate = suffixSource.id;
    candidate.key = key;
    candidate.intent = PhraseIntent::Develop;
    candidate.rankingScore = 80.0f;
    candidate.matchSimilarity = 1.0f;
    candidate.continuation = g8Concrete(suffixRoute.events);
    candidate.baseQualified = true;
    candidate.hardConstraintsPassed = true;
    candidate.channel = RecommendationChannel::PhraseTransition;
    candidate.reviewedEnding = true;
    candidate.sourceEndPreserved = true;
    candidate.completeSourcePhrase = true;

    const midi::Meter meter{4, 4};
    const midi::CandidateExportContext context{imported, 120.0, meter, key,
        midi::ArrangementMode::VoiceLed, midi::ExportScope::FullPhrase};
    const auto payload = midi::candidatePayload(context, candidate);
    require(static_cast<bool>(payload), "G8 >64 production MIDI payload failed: " + payload.error);
    const auto built = preview::buildSequence(imported, &candidate, 120.0);
    require(static_cast<bool>(built), "G8 >64 Preview failed: " + built.error);
    const auto clip = midi::buildClip(built.sequence, midi::ArrangementMode::VoiceLed,
        midi::ExportScope::FullPhrase, meter, key, candidate.intent);
    require(static_cast<bool>(clip), "G8 >64 MIDI Clip failed: " + clip.error);
    const auto encoded = midi::writeToMemory(clip.sequence);
    require(encoded && encoded.bytes == payload.payload.smfBytes,
            "G8 >64 candidate payload differs from Preview-derived Clip");
    const auto tempDirectory = outputDirectory / ".g8-check-midi";
    std::filesystem::create_directories(tempDirectory);
    const auto annotatedPath = tempDirectory / "84-event-cumulative-annotated.mid";
    const auto dawPath = tempDirectory / "84-event-cumulative-daw.mid";
    std::string error;
    require(midi::writeToFile(payload.payload.smfBytes, annotatedPath, error),
            "G8 >64 annotated MIDI write failed: " + error);
    require(midi::writeToFile(payload.payload.dawClipBytes, dawPath, error),
            "G8 >64 DAWClip MIDI write failed: " + error);
    const auto annotatedImport = midi::importFile(annotatedPath, false);
    require(static_cast<bool>(annotatedImport), "G8 >64 annotated import failed: " + annotatedImport.extraction.error);
    const auto completeImport = midi::importFile(dawPath, false);
    require(static_cast<bool>(completeImport), "G8 >64 DAWClip import failed: " + completeImport.extraction.error);
    require(g8SameMidiNotes(annotatedImport.midi.file, completeImport.midi.file),
            "G8 >64 Annotated and DAWClip note performances differ");
    ImportedProgressionSession completeSession;
    require(midi::applyImport(completeImport, completeSession), "G8 >64 applyImport failed");
    const auto dawImportSummary = g8CompareMidiToPreview(built.sequence, clip.sequence,
        completeSession.events, completeImport.midi.file, false, "G8 >64 complete import");
    ImportedProgressionSession annotatedSession;
    require(midi::applyImport(annotatedImport, annotatedSession), "G8 >64 annotated applyImport failed");
    (void)g8CompareMidiToPreview(built.sequence, clip.sequence, annotatedSession.events,
        annotatedImport.midi.file, false, "G8 >64 annotated import");
    require(g8EventListFingerprint(annotatedSession.events) ==
                g8EventListFingerprint(completeSession.events),
            "G8 >64 Annotated and DAWClip import semantics differ");
    const auto openImport = midi::importFile(dawPath, true);
    require(static_cast<bool>(openImport), "G8 >64 OPEN MIDI import failed: " + openImport.extraction.error);
    ImportedProgressionSession openSession;
    require(midi::applyImport(openImport, openSession), "G8 >64 OPEN applyImport failed");
    (void)g8CompareMidiToPreview(built.sequence, clip.sequence, openSession.events,
        openImport.midi.file, true, "G8 >64 OPEN import");
    std::error_code removeError;
    std::filesystem::remove(annotatedPath, removeError);
    std::filesystem::remove(dawPath, removeError);
    report << "midi-import-capacity\tPASS\tinput=84\toutput=" << built.sequence.events.size()
           << "\tactual=" << g8Tsv(g8EventListFingerprint(completeSession.events))
           << "\treinterpretations=" << dawImportSummary.reinterpretationCount << ':'
           << g8Tsv(dawImportSummary.reinterpretations) << '\n';

    auto snapshotValue = snapshot::capture(imported, candidate, {}, 120.0, 4, 4, key, {}, candidate.intent);
    snapshotValue.factoryLibraryVersion = factory.libraryVersion;
    const auto text = snapshot::serialize(snapshotValue);
    const auto reopened = snapshot::deserialize(text, &factory);
    require(static_cast<bool>(reopened) && reopened.value.candidateAvailable &&
            reopened.value.imported.events.size() == 84 &&
            g8EventListFingerprint(reopened.value.imported.events) == g8EventListFingerprint(imported.events),
            "Snapshot3 did not preserve the 84-event imported progression");
    const auto restoredPreview = preview::buildSequence(reopened.value.imported,
        &reopened.value.candidate, 120.0);
    require(static_cast<bool>(restoredPreview) && g8SamePreview(built.sequence, restoredPreview.sequence),
            "Snapshot3 changed the >64 event Preview");
    report << "snapshot3-capacity\tPASS\tevents=84\tschema=3\n";
}

int runG8Checks(const std::filesystem::path& factoryPath,
                const std::filesystem::path& outputDirectory) {
    const auto factory = library::loadFactory(factoryPath);
    require(static_cast<bool>(factory), "cannot load G8 check Factory: " + factory.error);
    require(factory.libraryVersion >= 8, "G8 checks require Factory version 8 or newer");
    std::filesystem::create_directories(outputDirectory);
    const auto reportPath = outputDirectory / "g8-checks.tsv";
    std::ofstream report(reportPath, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(report), "cannot create G8 checks report");
    report << "check\tstatus\tcase\tdetail\n";
    const auto complete = g8CompleteTemplates(factory);
    const CandidateIndex index(factory.templates);
    std::size_t failed{};
    const auto run = [&](std::string_view name, const auto& check) {
        try {
            check();
        } catch (const std::exception& error) {
            ++failed;
            report << name << "\tFAIL\t-\t" << g8Tsv(error.what()) << '\n';
        }
    };
    run("template-data", [&] { g8CheckTemplateData(factory, index, report); });
    run("long-tail", [&] { g8CheckLongTail(complete, index, report); });
    run("g7-and-hard-melody", [&] { g8CheckG7AndHardMelody(factory, complete, index, report); });
    run("long-capacity-midi-session-snapshot", [&] {
        g8CheckLongPersistenceAndMidi(factory, complete, index, report, outputDirectory);
    });
    report.flush();
    require(static_cast<bool>(report), "cannot finish G8 check report");
    std::cout << "g8_checks failed=" << failed << " report=" << reportPath.string() << '\n';
    require(failed == 0, "G8 focused checks failed; see " + reportPath.string());
    return 0;
}
} // namespace
