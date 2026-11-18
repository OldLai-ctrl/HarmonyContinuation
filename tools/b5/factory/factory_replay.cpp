#include "benchmark/BenchJson.h"
#include "core/ChordPitchSet.h"
#include "core/ContextualContinuation.h"
#include "library/ProgressionLibrary.h"
#include "plugin/RecommendationWorker.h"
#include "product/ProductVersion.h"
#include "snapshot/RecommendationSnapshot.h"
#include "ui/RecommendationBrowse.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
using namespace harmony;
using namespace harmony::benchmark;
using json::Value;
namespace fs = std::filesystem;

constexpr std::string_view factorySha =
    "c32257f878eead44e0df6febe14340ac08eab4a66d7c4db34bf6af911ed6ce9e";
struct FactoryIdentity {
    int version{8};
    std::size_t count{1288};
    std::string_view sha256{factorySha};
};

struct ReplayOptions {
    FactoryIdentity expected;
    bool recoverConsumedTail{};
    bool checkSnapshots{};
};

constexpr std::string_view usage =
    "usage: b5_factory_replay <frozen factory.db> <queries.jsonl> <new output directory>"
    " [--expect <version> <count> <lowercase sha256>] [--recover-consumed-tail] [--check-snapshots]";

int positiveInteger(std::string_view value) {
    int result{};
    const auto [position, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || position != value.data() + value.size() || result <= 0)
        throw std::runtime_error("expected a positive integer, got: " + std::string(value));
    return result;
}

ReplayOptions parseOptions(int argc, char** argv) {
    if (argc < 4) throw std::runtime_error(std::string(usage));
    ReplayOptions options;
    bool hasExplicitIdentity{};
    for (int i = 4; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--recover-consumed-tail" && !options.recoverConsumedTail) {
            options.recoverConsumedTail = true;
        } else if (argument == "--check-snapshots" && !options.checkSnapshots) {
            options.checkSnapshots = true;
        } else if (argument == "--expect" && !hasExplicitIdentity && argc - i > 3) {
            const int version = positiveInteger(argv[++i]);
            const auto count = static_cast<std::size_t>(positiveInteger(argv[++i]));
            const std::string_view sha256(argv[++i]);
            if (version < 3 || sha256.size() != 64 ||
                sha256.find_first_not_of("0123456789abcdef") != std::string_view::npos)
                throw std::runtime_error("expected a Schema 2 Factory version and lowercase SHA-256");
            options.expected = {version, count, sha256};
            hasExplicitIdentity = true;
        } else {
            throw std::runtime_error(std::string(usage));
        }
    }
    return options;
}
constexpr std::array<std::string_view, 12> notes{
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
constexpr std::array<const char*, 7> channels{
    "Exact", "ContextFull", "ContextThree", "ContextTwo", "PhraseTransition", "ContextFour", "ContextPhrase"};

struct ChordName {
    ChordQuality quality;
    std::string_view family;
    std::string_view detail;
    std::string_view suffix;
};
constexpr std::array<ChordName, 15> chordNames{{
    {ChordQuality::Major, "maj", "maj", ""},
    {ChordQuality::Major7, "maj", "maj7", "maj7"},
    {ChordQuality::Minor, "min", "min", "m"},
    {ChordQuality::Minor7, "min", "min7", "m7"},
    {ChordQuality::Dominant7, "dom", "7", "7"},
    {ChordQuality::Diminished, "dim", "dim", "dim"},
    {ChordQuality::Diminished7, "dim", "dim7", "dim7"},
    {ChordQuality::HalfDiminished7, "hdim", "hdim7", "m7b5"},
    {ChordQuality::Augmented, "aug", "aug", "aug"},
    {ChordQuality::Sus2, "sus", "sus2", "sus2"},
    {ChordQuality::Sus4, "sus", "sus4", "sus4"},
    {ChordQuality::Unknown, "5", "5", "5"},
    {ChordQuality::Unknown, "1", "1", "1"},
    {ChordQuality::Unknown, "other", "other", ""},
    {ChordQuality::Unknown, "other", "X", ""},
}};

const ChordName& nameForInput(std::string_view family, std::string_view detail) {
    for (const auto& name : chordNames)
        if (name.detail == detail) return name;
    for (const auto& name : chordNames)
        if (name.family == family) return name;
    return chordNames[13];
}

const Value* optional(const Value& row, std::string_view field) {
    const auto* value = row.find(field);
    return value && value->type != Value::Type::Null ? value : nullptr;
}

Progression makeInput(const Value& query) {
    const auto& prefix = query.at("prefix").items();
    if (prefix.empty() || prefix.size() > 128)
        throw std::runtime_error("prefix length outside Worker input bounds");
    Progression progression;
    for (const auto& row : prefix) {
        const auto* unit = optional(row, "unit");
        if (!unit || unit->text() != "qn")
            throw std::runtime_error("Factory replay requires verified quarter-note input");
        const auto root = row.at("root").integer(0, 11);
        const auto* detail = optional(row, "detail");
        const auto& name = nameForInput(row.at("quality").text(), detail ? detail->text() : "");
        ChordEvent event;
        event.root = static_cast<PitchClass>(root);
        event.name = std::string(notes[root]) + std::string(name.suffix);
        event.quality = name.quality;
        if (const auto* bass = optional(row, "bass")) {
            const int value = bass->integer(0, 11);
            event.bass = static_cast<PitchClass>(value);
            if (value != root) event.name += "/" + std::string(notes[value]);
        }
        if (const auto* intervals = optional(row, "intervals")) {
            unsigned mask{};
            for (const auto& interval : intervals->items())
                mask |= 1u << interval.integer(0, 11);
            if (mask) {
                std::ostringstream hexadecimal;
                hexadecimal << std::hex << mask;
                event.extensions.mask = hexadecimal.str();
            }
        }
        event.startQN = row.at("onset").finite();
        if (const auto* duration = optional(row, "duration")) {
            event.durationQN = duration->finite();
            if (*event.durationQN <= 0)
                throw std::runtime_error("nonpositive annotated duration");
        }
        event.openEnded = !event.durationQN;
        if (!progression.empty() && event.startQN <= progression.back().startQN)
            throw std::runtime_error("prefix onsets must be strictly increasing");
        progression.push_back(std::move(event));
    }
    return progression;
}

AnalysisContext makeContext(const Value& query) {
    AnalysisContext context;
    const auto& prefix = query.at("prefix").items();
    const auto* meter = optional(prefix.front(), "meter");
    if (!meter) return context;
    for (const auto& row : prefix) {
        const auto* current = optional(row, "meter");
        if (!current || current->text() != meter->text()) return context;
    }
    std::istringstream signature(meter->text());
    int numerator{}, denominator{};
    char separator{};
    if ((signature >> numerator >> separator >> denominator) &&
        signature.eof() && separator == '/' && numerator > 0 && numerator <= 32 &&
        denominator > 0 && denominator <= 32 && (denominator & (denominator - 1)) == 0) {
        context.timeSigNumerator = numerator;
        context.timeSigDenominator = denominator;
    }
    return context;
}

Value chordRecord(const ChordEvent& event, double onset, std::optional<double> duration,
                  bool catalog = false) {
    const auto normalized = normalizeChord(event);
    Value result = Value::map();
    if (normalized.root) result.object["root"] = static_cast<int>(*normalized.root);
    else result.object["root"] = {};
    // Match the annotated-bass convention of the song benchmark. Keep an
    // explicit source bass and leave a model/voicer fallback unlabelled.
    if (normalized.explicitBass && normalized.bass)
        result.object["bass"] = static_cast<int>(*normalized.bass);
    else result.object["bass"] = {};
    const auto* name = &chordNames[13];
    for (const auto& candidate : chordNames) {
        if (candidate.quality == normalized.quality && normalized.quality != ChordQuality::Unknown) {
            name = &candidate;
            break;
        }
    }
    std::string detail(name->detail);
    const auto& label = event.name;
    const auto start = normalized.rootSpelling.size();
    if (start && start < label.size()) {
        auto end = label.find('/', start);
        if (end != std::string::npos && end + 1 < label.size() && label[end + 1] == '9')
            end = label.find('/', end + 1);
        const auto suffix = label.substr(start, end == std::string::npos ? end : end - start);
        if (suffix == "maj9" || suffix == "maj11" || suffix == "maj13" ||
            suffix == "9" || suffix == "11" || suffix == "13")
            detail = suffix;
        else if (suffix == "m9" || suffix == "m11" || suffix == "m13")
            detail = "min" + suffix.substr(1);
        else if (suffix == "6") detail = "maj6";
        else if (suffix == "m6") detail = "min6";
        else if (suffix != name->suffix && !catalog && !suffix.empty())
            detail += ":" + suffix;
    }
    if (name->family == "other" && normalized.intervalMask == 0x81) name = &chordNames[11];
    if (name->family == "other" && normalized.intervalMask == 0x1) name = &chordNames[12];
    result.object["quality"] = std::string(name->family);
    result.object["detail"] = name->family == "5" || name->family == "1" ? std::string(name->detail) : detail;
    result.object["label"] = label;
    if (duration) result.object["duration"] = *duration;
    else result.object["duration"] = {};
    result.object["onset"] = onset;
    result.object["unit"] = "qn";
    Value intervals = Value::list();
    for (int n = 0; n < 12; ++n)
        if (normalized.intervalMask & (1u << n)) intervals.array.emplace_back(n);
    result.object["intervals"] = std::move(intervals);
    return result;
}

ChordEvent asEvent(const ConcreteChordEvent& chord) {
    if (chord.harmonicData) return *chord.harmonicData;
    ChordEvent event;
    event.name = chord.label;
    event.quality = chord.quality;
    return event;
}

plugin::WorkerResult waitFor(plugin::RecommendationWorker& worker, std::uint64_t generation,
                             const FactoryIdentity& expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = worker.takeLatest(); result && result->generation == generation) {
            if (!result->error.empty()) throw std::runtime_error(result->error);
            if (result->factoryVersion != expected.version || result->factoryCount != expected.count ||
                result->userCount != 0 || result->factorySha256 != expected.sha256)
                throw std::runtime_error("unexpected Factory/user identity: V" + std::to_string(result->factoryVersion) +
                    " / " + std::to_string(result->factoryCount) + " Factory, " + std::to_string(result->userCount) +
                    " User / " + result->factorySha256);
            return std::move(*result);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Worker timed out");
}

void writeCatalog(const library::LoadResult& factory, std::ostream& out) {
    for (const auto& item : factory.templates) {
        Value row = Value::map();
        row.object["source"] = item.id;
        row.object["tonic"] = static_cast<int>(item.rawChordKey.value_or(KeySignature{PitchClass::C, item.mode}).tonic);
        row.object["mode"] = static_cast<int>(item.mode);
        row.object["meter"] = std::to_string(item.meterNumerator) + "/" + std::to_string(item.meterDenominator);
        row.object["template_version"] = item.version;
        row.object["intent"] = static_cast<int>(item.intent);
        row.object["loopable"] = item.loopable;
        Value events = Value::list();
        double onset{};
        for (std::size_t i = 0; i < item.full.size(); ++i) {
            const auto duration = item.full[i].durationQN;
            ChordEvent event;
            if (item.rawChords.size() == item.full.size() && item.rawChordKey) event = item.rawChords[i];
            else event = asEvent(realizeContinuation(item.full[i], {PitchClass::C, item.mode},
                                                     duration.value_or(0)));
            events.array.push_back(chordRecord(event, onset, duration, true));
            onset += duration.value_or(0);
        }
        row.object["events"] = std::move(events);
        out << json::dump(row) << '\n';
    }
}

Value replay(const Value& query, plugin::RecommendationWorker& worker,
             const std::set<TemplateID>& factoryIds, const ReplayOptions& options) {
    const auto progression = makeInput(query);
    RecommendationRequest request;
    request.prepareBothViews = true;
    request.recoverConsumedFactoryTail = options.recoverConsumedTail;
    if (const auto* sources = optional(query, "trace_sources")) {
        for (const auto& source : sources->items()) request.traceSources.push_back(source.text());
        std::sort(request.traceSources.begin(), request.traceSources.end());
    }
    if (const auto* melody = optional(query, "hard_melody")) {
        MelodyConstraint note;
        note.pitchClass = melody->at("pitch_class").integer(0, 11);
        note.startQN = melody->at("onset").finite();
        note.durationQN = melody->at("duration").finite();
        note.strictness = ConstraintStrictness::Hard;
        request.constraints.melody.push_back(note);
        if (!validConstraints(request.constraints)) throw std::runtime_error("invalid hard melody fixture");
    }
    const auto context = makeContext(query);
    const auto result = waitFor(worker, worker.submit(progression, context, request), options.expected);
    ImportedProgressionSession imported;
    if (options.checkSnapshots &&
        !imported.replace(progression, TimelineCoordinateMode::RelativeToSelection))
        throw std::runtime_error("snapshot input has invalid timeline coordinates");
    Value row = Value::map();
    row.object["id"] = query.at("id");
    row.object["worker_ms"] = result.computationMs;
    const auto key = [](KeySignature key) {
        return static_cast<int>(key.tonic) + 12 * static_cast<int>(key.mode);
    };
    if (!request.traceSources.empty()) {
        Value exact = Value::list();
        for (const auto& match : result.recommendations.matches) {
            if (!std::binary_search(request.traceSources.begin(), request.traceSources.end(), match.templateId))
                continue;
            Value trace = Value::map();
            trace.object["source"] = match.templateId;
            trace.object["key"] = key(match.key);
            trace.object["start"] = static_cast<int>(match.templateMatchStart);
            trace.object["end"] = static_cast<int>(match.templateMatchEnd);
            trace.object["next"] = static_cast<int>(match.continuationStartIndex);
            trace.object["similarity"] = match.similarity;
            trace.object["eligibility"] = match.candidateEligibilityScore;
            trace.object["disposition"] = match.disposition;
            exact.array.push_back(std::move(trace));
        }
        Value contextual = Value::list();
        for (const auto& trial : result.recommendations.creativeTrials) {
            Value trace = Value::map();
            trace.object["source"] = trial.source;
            trace.object["key"] = key(trial.key);
            trace.object["next"] = static_cast<int>(trial.continuationStart);
            trace.object["disposition"] = trial.disposition;
            contextual.array.push_back(std::move(trace));
        }
        row.object["exact_trace"] = std::move(exact);
        row.object["context_trace"] = std::move(contextual);
    }
    row.object["aligned"] = static_cast<int>(result.recommendations.stats.aligned);
    row.object["exact_qualified"] = static_cast<int>(result.recommendations.stats.scorePassed);
    row.object["context_qualified"] = static_cast<int>(result.recommendations.contextualQualified +
                                                     result.recommendations.transitionQualified);
    Value candidates = Value::list();
    std::set<std::string> identities;
    std::vector<std::string> generatedIdentities;
    int generated{}, productRank{};
    std::set<TemplateID> snapshotSources;
    const auto& order = result.recommendations.inspirationOrder;
    const auto total = order.size();
    const auto generatedTotal = std::count_if(order.begin(), order.end(), [&](const auto ref) {
        return result.recommendations.groups.at(ref.group).at(ref.index).generated.has_value();
    });
    const auto generatedStart = std::min<std::size_t>(3, total - generatedTotal);
    for (std::size_t i = 0; i < total; ++i) {
        const auto page = ui::browsePage(total, ui::browsePageIndex(i));
        if (i < page.begin || i >= page.end || page.end - page.begin > ui::recommendationPageSize)
            throw std::runtime_error("invalid inspiration pagination");
        const auto position = order[i];
        const auto& candidate = result.recommendations.groups.at(position.group).at(position.index);
        ++productRank;
        const auto fingerprint = continuationMusicFingerprint(candidate);
        if (!identities.insert(fingerprint).second)
            throw std::runtime_error("duplicate music in product order");
        if (candidate.generated) {
            if (i != generatedStart + generated) throw std::runtime_error("unexpected G13 insertion position");
            if (!candidate.baseQualified || !candidate.hardConstraintsPassed || !candidate.primaryTemplate.empty())
                throw std::runtime_error("unexpected qualification or Factory identity for G13 candidate");
            ++generated;
            generatedIdentities.push_back(fingerprint);
            continue;
        }
        if (!candidate.baseQualified || !candidate.hardConstraintsPassed ||
            !factoryIds.contains(candidate.primaryTemplate))
            throw std::runtime_error("unexpected nonqualified or non-Factory candidate");
        if (options.checkSnapshots &&
            std::binary_search(request.traceSources.begin(), request.traceSources.end(), candidate.primaryTemplate) &&
            snapshotSources.insert(candidate.primaryTemplate).second) {
            const auto frozen = snapshot::capture(imported, candidate, {}, 120,
                context.timeSigNumerator.value_or(4), context.timeSigDenominator.value_or(4));
            const auto decoded = snapshot::deserialize(snapshot::serialize(frozen));
            if (!decoded || !decoded.value.candidateAvailable ||
                continuationMusicFingerprint(decoded.value.candidate) != fingerprint ||
                decoded.value.candidate.primaryTemplate != candidate.primaryTemplate)
                throw std::runtime_error("snapshot changed Factory source or music identity");
        }
        Value record = Value::map();
        record.object["source"] = candidate.primaryTemplate;
        record.object["id"] = candidate.id;
        record.object["fingerprint"] = fingerprint;
        record.object["score"] = static_cast<double>(candidate.rankingScore);
        record.object["eligibility"] = static_cast<double>(candidate.eligibilityScore);
        record.object["group"] = static_cast<int>(position.group);
        record.object["product_rank"] = productRank;
        record.object["channel"] = channels.at(static_cast<std::size_t>(candidate.channel));
        record.object["key"] = key(candidate.key);
        record.object["match_start"] = static_cast<int>(candidate.matchStart);
        record.object["match_end"] = static_cast<int>(candidate.matchEnd);
        record.object["next"] = static_cast<int>(candidate.continuationStart);
        record.object["structure_qualified"] = candidate.structureQualified;
        record.object["complete_source_phrase"] = candidate.completeSourcePhrase;
        record.object["source_end_preserved"] = candidate.sourceEndPreserved;
        record.object["family"] = candidate.family;
        record.object["ending"] = static_cast<int>(candidate.ending);
        record.object["reviewed_ending"] = candidate.reviewedEnding;
        record.object["provenance"] = static_cast<int>(candidate.provenance);
        record.object["rhythm_scale"] = static_cast<double>(candidate.rhythmScale);
        record.object["exploratory"] = candidate.exploratory;
        record.object["seam_qn"] = candidate.seamQN;
        Value supports = Value::list();
        for (const auto& source : candidate.supportingTemplates) supports.array.emplace_back(source);
        record.object["support_sources"] = std::move(supports);
        if (!request.traceSources.empty()) {
            Value explanations = Value::list();
            for (const auto& evidence : candidate.evidence) {
                if (!std::binary_search(request.traceSources.begin(), request.traceSources.end(), evidence.templateId))
                    continue;
                Value explanation = Value::map();
                explanation.object["source"] = evidence.templateId;
                explanation.object["key"] = key(evidence.key);
                explanation.object["start"] = static_cast<int>(evidence.matchStart);
                explanation.object["end"] = static_cast<int>(evidence.matchEnd);
                explanation.object["next"] = static_cast<int>(evidence.continuationStart);
                explanations.array.push_back(std::move(explanation));
            }
            record.object["evidence"] = std::move(explanations);
        }
        Value events = Value::list();
        double onset = progression.back().startQN + candidate.suggestedCurrentChordDurationQN.value_or(
            progression.back().durationQN.value_or(0));
        for (const auto& chord : candidate.continuation) {
            events.array.push_back(chordRecord(asEvent(chord), onset, chord.durationQN));
            onset += chord.durationQN;
        }
        record.object["events"] = std::move(events);
        candidates.array.push_back(std::move(record));
    }
    row.object["generated_count"] = generated;
    if (options.checkSnapshots) row.object["snapshot_sources"] = static_cast<int>(snapshotSources.size());
    Value generatedRecords = Value::list();
    for (const auto& identity : generatedIdentities) generatedRecords.array.emplace_back(identity);
    row.object["generated_identities"] = std::move(generatedRecords);
    row.object["candidates"] = std::move(candidates);
    return row;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (product::printVersionIfRequested(argc, argv)) return 0;
        const auto options = parseOptions(argc, argv);
        const auto database = fs::absolute(argv[1]);
        const auto output = fs::absolute(argv[3]);
        std::ifstream queries(argv[2], std::ios::binary);
        if (!queries) throw std::runtime_error("cannot open input JSONL");
        if (!fs::create_directory(output)) throw std::runtime_error("output directory must be new");
        plugin::RecommendationWorker worker(database, output / "empty-user.db", output / "empty-store");
        const auto warm = waitFor(worker, worker.submit({}, {}), options.expected);
        const auto factory = library::loadFactory(database);
        if (!factory || factory.storageSchemaVersion != library::factorySchemaVersion ||
            factory.libraryVersion != options.expected.version || factory.templates.size() != options.expected.count)
            throw std::runtime_error("Factory schema/catalog check failed: " + factory.error);
        std::ofstream replayFile(output / "factory_replay.jsonl", std::ios::binary);
        std::ofstream catalogFile(output / "factory_catalog.jsonl", std::ios::binary);
        if (!replayFile || !catalogFile) throw std::runtime_error("cannot open output JSONL");
        writeCatalog(factory, catalogFile);
        std::set<TemplateID> factoryIds;
        for (const auto& item : factory.templates) factoryIds.insert(item.id);

        std::string line;
        std::set<std::string> seen;
        while (std::getline(queries, line)) {
            const auto query = json::parse(line);
            const auto& id = query.at("id").text();
            if (!seen.insert(id).second) throw std::runtime_error("duplicate input query: " + id);
            try {
                replayFile << json::dump(replay(query, worker, factoryIds, options)) << '\n';
            } catch (const std::exception& error) {
                throw std::runtime_error(id + ": " + error.what());
            }
        }
        if (!queries.eof() || seen.empty()) throw std::runtime_error("failed or empty input JSONL");
        replayFile.close();
        catalogFile.close();
        if (!replayFile || !catalogFile) throw std::runtime_error("failed to write complete result");
        Value identity = Value::map();
        identity.object["factory_version"] = warm.factoryVersion;
        identity.object["factory_schema"] = factory.storageSchemaVersion;
        identity.object["factory_count"] = static_cast<int>(warm.factoryCount);
        identity.object["factory_sha256"] = warm.factorySha256;
        identity.object["user_count"] = static_cast<int>(warm.userCount);
        identity.object["queries"] = static_cast<int>(seen.size());
        identity.object["recover_consumed_tail"] = options.recoverConsumedTail;
        std::ofstream identityFile(output / "factory_identity.json", std::ios::binary);
        identityFile << json::dump(identity) << '\n';
        identityFile.close();
        if (!identityFile) throw std::runtime_error("failed to write Factory identity");
        std::cout << "Replay engine base: " << product::banner() << "\nFactory V" << warm.factoryVersion
                  << " / Schema " << factory.storageSchemaVersion << " / " << warm.factoryCount << " / " << warm.factorySha256
                  << "\nWorker queries: " << seen.size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
