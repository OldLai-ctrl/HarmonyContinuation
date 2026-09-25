#include "TemplateJson.h"
#include "ProgressionJson.h"
#include "core/ContinuationEngine.h"
#include "library/ProgressionLibrary.h"
#include "plugin/RecommendationWorker.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace harmony;
namespace {
int checks{};
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) { std::cerr << "FAIL " << name << '\n'; std::exit(1); }
}
Progression progression(std::string_view text) {
    auto parsed = dev::parseProgressionJson(text);
    if (!parsed || !sortAndInferDurations(parsed.events)) throw std::runtime_error(parsed.error);
    return std::move(parsed.events);
}
std::vector<ProgressionTemplate> templates(std::string_view text) {
    auto parsed = dev::parseTemplateJson(text);
    if (!parsed) throw std::runtime_error(parsed.error);
    return std::move(parsed.templates);
}
MatchQuery query(const Progression& input, KeySignature key = {PitchClass::C, Mode::Major}) {
    AnalysisContext ctx; ctx.forcedKey = key;
    return makeMatchQuery(input, ctx);
}
RecommendationSet recommend(const Progression& input, std::vector<ProgressionTemplate> items,
                            RecommendationRequest request = {}) {
    return recommendContinuations(query(input), CandidateIndex(std::move(items)), request);
}
const ContinuationCandidate* findSuffix(const RecommendationSet& set, std::string_view first,
                                        std::string_view last) {
    for (const auto& group : set.groups) for (const auto& item : group)
        if (!item.continuation.empty() && item.continuation.front().label == first &&
            item.continuation.back().label == last) return &item;
    return nullptr;
}
}
int main() {
    const auto a = progression(R"([{"name":"C","start":4,"duration":4},{"name":"Am","start":8,"duration":2},{"name":"Dm","start":10}])");
    const auto basic = templates(R"([{"id":"basic","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic"}])");
    const auto first = recommend(a, basic);
    const auto* path = findSuffix(first, "G", "C");
    check(path && path->continuation.size() == 2, "A exact continuation");
    check(path->suggestedCurrentChordDurationQN && std::abs(*path->suggestedCurrentChordDurationQN - 2.) < 0.001,
          "C OPEN duration");
    check(std::abs(path->continuation[0].durationQN - 2.) < 0.001, "A suffix duration");
    check(first.groups[0].size() == 1, "H resolve grouping");
    check(makeMatchQuery(a).interpretations.size() >= 2, "P ambiguous key interpretations retained");
    check(!dev::parseTemplateJson(R"([{"id":"bad","mode":"major","sequence":"I V","styles":"unknown:1"}])"),
          "factory rejects unknown style");
    check(!dev::parseTemplateJson(R"([{"id":"bad","id":"again","mode":"major","sequence":"I V"}])"),
          "duplicate JSON field rejected");
    check(!dev::parseTemplateJson(R"([{"id":"bad","mode":"major","sequence":"I V"}])", true),
          "strict factory metadata required");
    const auto inserted = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4,"duration":2},{"name":"A7","start":6,"duration":1},{"name":"Dm","start":7}])");
    check(findSuffix(recommend(inserted, basic), "G", "C") != nullptr, "B secondary insertion");
    const auto scaled = progression(R"([{"name":"C","start":0,"duration":8},{"name":"Am","start":8,"duration":4},{"name":"Dm","start":12}])");
    const auto scaledSet = recommend(scaled, basic);
    const auto* scaledPath = findSuffix(scaledSet, "G", "C");
    check(scaledPath && std::abs(scaledPath->rhythmScale - 2.f) < 0.001f, "D rhythm scaling");
    check(scaledPath && std::abs(scaledPath->continuation[0].durationQN - 4.) < 0.001, "D scaled suffix");
    const auto mid = progression(R"([{"name":"Am","start":0,"duration":4},{"name":"Dm","start":4}])");
    const auto circle = templates(R"([{"id":"circle","mode":"major","sequence":"I iii vi ii V I","intent":"resolve","cadence":"authentic"}])");
    check(findSuffix(recommend(mid, circle), "G", "C") != nullptr, "E mid-template");
    const auto ended = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4,"duration":2},{"name":"Dm","start":6,"duration":2},{"name":"G","start":8,"duration":2},{"name":"C","start":10,"duration":4}])");
    const auto noEnd = recommend(ended, basic);
    check(noEnd.groups[0].empty(), "F no non-loop suffix");
    const auto loop = templates(R"([{"id":"loop","mode":"major","sequence":"I V vi IV","intent":"loop","cadence":"loop_closure","loopable":true}])");
    const auto loopQuery = progression(R"([{"name":"Am","start":0,"duration":4},{"name":"F","start":4}])");
    check(findSuffix(recommend(loopQuery, loop), "C", "G") != nullptr, "G loop wrap");
    check(!recommend(loopQuery, loop).groups[2].empty(), "loop grouping");
    const auto developed = templates(R"([{"id":"develop","mode":"major","sequence":"I vi ii iii VI7 ii V I","intent":"develop","cadence":"authentic"}])");
    check(!recommend(a, developed).groups[1].empty(), "I develop grouping");
    const auto colored = templates(R"([{"id":"color","mode":"major","sequence":"I IV iv I","intent":"color","cadence":"plagal"}])");
    const auto colorQuery = progression(R"([{"name":"C","start":0,"duration":4},{"name":"F","start":4}])");
    check(!recommend(colorQuery, colored).groups[3].empty(), "J borrowed iv color grouping");
    check(realizeContinuation(dev::parseNotationEvent("V7/ii", Mode::Major), {PitchClass::C,Mode::Major}, 2).label == "A7",
          "secondary dominant realization");
    check(realizeContinuation(dev::parseNotationEvent("IV", Mode::Major), {PitchClass::Db,Mode::Major}, 2).label == "Gb",
          "Db flat spelling");
    check(realizeContinuation(dev::parseNotationEvent("iv", Mode::Major), {PitchClass::C,Mode::Major}, 2).label == "Fm",
          "borrowed iv realization");
    auto dedup = templates(R"([{"id":"x","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic"},{"id":"y","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic"},{"id":"z","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic"}])");
    const auto merged = recommend(a, dedup);
    check(merged.groups[0].size() == 1 && merged.groups[0][0].supportCount == 3, "K dedup support");
    auto styled = templates(R"([{"id":"pop","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic","styles":"pop:1"},{"id":"rock","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic","styles":"rock:1"}])");
    auto styleScore = [&](std::optional<Style> style) {
        auto result = recommend(a, styled, {style, {}});
        check(!result.groups[0].empty(), "style candidate available");
        return result.groups[0][0].rankingScore;
    };
    check(styleScore(Style::Pop) > styleScore(std::nullopt), "M style boosts selected template");
    const auto rockPreferred = recommend(a, styled, {Style::Rock, {}});
    check(!rockPreferred.groups[0].empty() && rockPreferred.groups[0][0].primaryTemplate == "rock",
          "style picks best supporting source");
    check(std::abs(styleScore(std::nullopt) - styleScore(std::nullopt)) < 0.001f, "N auto style deterministic");
    auto intentSet = recommend(a, basic, {{}, PhraseIntent::Resolve});
    check(intentSet.groups[0][0].rankingScore > first.groups[0][0].rankingScore, "O intent boost");
    AnalysisContext forced; forced.forcedKey = KeySignature{PitchClass::A,Mode::Minor};
    const auto minorQuery = makeMatchQuery(a, forced);
    check(minorQuery.interpretations.size() == 1 && minorQuery.interpretations.front().key.key.mode == Mode::Minor,
          "Q forced key");
    const auto temp = std::filesystem::temp_directory_path() / "HarmonyContinuationPhase3Tests";
    std::filesystem::create_directories(temp);
    const auto factoryPath = temp / "factory.db", userPath = temp / "user.db";
    std::string error;
    check(library::compileFactory(factoryPath, basic, error), "factory compile");
    check(library::loadFactory(factoryPath).templates.size() == 1, "factory read");
    const auto shipped = library::loadFactory(HC_FACTORY_DB_PATH);
    check(shipped && shipped.templates.size() == 161, "factory seed count and SQLite roundtrip");
    const auto wrong = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4,"duration":2},{"name":"F#","start":6,"duration":1},{"name":"Dm","start":7}])");
    const auto goodFactory = recommend(inserted, shipped.templates);
    const auto badFactory = recommend(wrong, shipped.templates);
    check(!goodFactory.groups[0].empty() && !badFactory.groups[0].empty() &&
          goodFactory.groups[0][0].rankingScore > badFactory.groups[0][0].rankingScore + 5.f,
          "unexplained input does not rank like resolved secondary chord");
    auto custom = basic.front(); custom.id = "user1"; custom.sourceType = "user";
    library::UserLibrary user(userPath);
    check(user.addProgression(custom, error), "R user save");
    check(!library::loadFactory(userPath), "factory/user database type isolation");
    check(user.loadAll().templates.size() == 1, "R user reload");
    check(library::compileFactory(factoryPath, circle, error), "S factory rebuild");
    check(user.loadAll().templates.size() == 1, "S user isolated");
    custom.name = "revised";
    check(user.updateProgression(custom, error) && user.loadAll().templates.front().name == "revised", "user update");
    {
        plugin::RecommendationWorker worker(factoryPath, userPath);
        AnalysisContext ctx; ctx.forcedKey = KeySignature{PitchClass::C, Mode::Major};
        worker.submit(ended, ctx);
        const auto latestGeneration = worker.submit(a, ctx);
        std::optional<plugin::WorkerResult> latest;
        for (int wait = 0; wait < 100 && !latest; ++wait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            latest = worker.takeLatest();
        }
        check(latest && latest->generation == latestGeneration, "T stale worker result discarded");
    }
    check(user.removeProgression(custom.id, error) && user.loadAll().templates.empty(), "user delete");
    std::filesystem::remove(factoryPath); std::filesystem::remove(userPath);
    std::cout << checks << '/' << checks << " PASS\n";
}
