#include "benchmark/BenchJson.h"
#include "core/ChordPitchSet.h"
#include "core/ContextualContinuation.h"
#include "library/ProgressionLibrary.h"
#include "plugin/RecommendationWorker.h"
#include "product/ProductVersion.h"
#include "ui/RecommendationBrowse.h"

#include <algorithm>
#include <array>
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

plugin::WorkerResult waitFor(plugin::RecommendationWorker& worker, std::uint64_t generation) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = worker.takeLatest(); result && result->generation == generation) {
            if (!result->error.empty()) throw std::runtime_error(result->error);
            if (result->factoryVersion != 8 || result->factoryCount != 1288 ||
                result->userCount != 0 || result->factorySha256 != factorySha)
                throw std::runtime_error("unexpected Factory/user identity: " + result->factorySha256);
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
             const std::set<TemplateID>& factoryIds, bool recoverConsumedFactoryTail) {
    const auto progression = makeInput(query);
    RecommendationRequest request;
    request.prepareBothViews = true;
    request.recoverConsumedFactoryTail = recoverConsumedFactoryTail;
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
    const auto result = waitFor(worker, worker.submit(progression, makeContext(query), request));
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
        if (candidate.generated) {
            if (i != generatedStart + generated) throw std::runtime_error("unexpected G13 insertion position");
            ++generated;
            generatedIdentities.push_back(continuationMusicFingerprint(candidate));
            continue;
        }
        if (!candidate.baseQualified || !candidate.hardConstraintsPassed ||
            !factoryIds.contains(candidate.primaryTemplate))
            throw std::runtime_error("unexpected nonqualified or non-Factory candidate");
        Value record = Value::map();
        record.object["source"] = candidate.primaryTemplate;
        record.object["id"] = candidate.id;
        const auto fingerprint = continuationMusicFingerprint(candidate);
        if (!identities.insert(fingerprint).second)
            throw std::runtime_error("duplicate Factory music in product order");
        record.object["fingerprint"] = fingerprint;
        record.object["score"] = static_cast<double>(candidate.rankingScore);
        record.object["group"] = static_cast<int>(position.group);
        record.object["product_rank"] = productRank;
        record.object["channel"] = channels.at(static_cast<std::size_t>(candidate.channel));
        record.object["key"] = key(candidate.key);
        record.object["next"] = static_cast<int>(candidate.continuationStart);
        record.object["structure_qualified"] = candidate.structureQualified;
        record.object["exploratory"] = candidate.exploratory;
        record.object["seam_qn"] = candidate.seamQN;
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
        if (argc != 4 && !(argc == 5 && std::string_view(argv[4]) == "--recover-consumed-tail"))
            throw std::runtime_error("usage: b5_factory_replay <frozen factory.db> <queries.jsonl> <new output directory> [--recover-consumed-tail]");
        const auto database = fs::absolute(argv[1]);
        const auto output = fs::absolute(argv[3]);
        std::ifstream queries(argv[2], std::ios::binary);
        if (!queries) throw std::runtime_error("cannot open input JSONL");
        if (!fs::create_directory(output)) throw std::runtime_error("output directory must be new");
        plugin::RecommendationWorker worker(database, output / "empty-user.db", output / "empty-store");
        const auto warm = waitFor(worker, worker.submit({}, {}));
        const auto factory = library::loadFactory(database);
        if (!factory || factory.storageSchemaVersion != 2 || factory.templates.size() != 1288)
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
                replayFile << json::dump(replay(query, worker, factoryIds, argc == 5)) << '\n';
            } catch (const std::exception& error) {
                throw std::runtime_error(id + ": " + error.what());
            }
        }
        if (!queries.eof() || seen.empty()) throw std::runtime_error("failed or empty input JSONL");
        replayFile.close();
        catalogFile.close();
        if (!replayFile || !catalogFile) throw std::runtime_error("failed to write complete result");
        std::cout << product::banner() << "\nFactory V8 / Schema 2 / 1288 / " << warm.factorySha256
                  << "\nWorker queries: " << seen.size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
