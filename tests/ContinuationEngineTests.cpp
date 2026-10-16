#include "TemplateJson.h"
#include "ProgressionJson.h"
#include "core/ContinuationEngine.h"
#include "library/ProgressionLibrary.h"
#include "plugin/RecommendationWorker.h"
#include <algorithm>
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
    // Synthetic factory fixtures must satisfy the current bilingual library contract.
    for(auto& item:parsed.templates){if(item.nameZh.empty())item.nameZh="测试 "+item.id;if(item.nameEn.empty())item.nameEn="Fixture "+item.id;}
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
const MatchResult* findMatch(const RecommendationSet& set, std::string_view id) {
    const auto it=std::find_if(set.matches.begin(),set.matches.end(),[&](const auto& match){return match.templateId==id;});
    return it==set.matches.end()?nullptr:&*it;
}
const ContinuationCandidate* findCandidate(const RecommendationSet& set, std::string_view id) {
    for (const auto& group : set.groups) for (const auto& item : group)
        if (item.primaryTemplate == std::string(id)) return &item;
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
    const auto loopPrefix = progression(R"([{"name":"C","start":0,"duration":4},{"name":"G","start":4,"duration":4},{"name":"Am","start":8}])");
    const auto fromPrefix = recommend(loopPrefix, loop);
    const auto* prefixPath = findSuffix(fromPrefix, "F", "F");
    check(prefixPath && prefixPath->continuation.size() == 1, "G1 partial loop prefix does not replay tonic");
    const auto loopTonic = progression(R"([{"name":"Am","start":0}])");
    const auto fromTonic = recommend(loopTonic, loop);
    const auto* tonicPath = findSuffix(fromTonic, "F", "G");
    check(tonicPath && tonicPath->continuation.size() == 3 && tonicPath->continuation[1].label == "C",
          "G2 middle loop match wraps to the event before its start");
    const auto loopQuery = progression(R"([{"name":"Am","start":0,"duration":4},{"name":"F","start":4}])");
    check(findSuffix(recommend(loopQuery, loop), "C", "G") != nullptr, "G loop wrap");
    check(!recommend(loopQuery, loop).groups[2].empty(), "loop grouping");
    const auto alteredLoop = templates(R"([{"id":"altered_loop","mode":"major","sequence":"I IV V ii","intent":"loop","cadence":"loop_closure","loopable":true}])");
    const auto borrowedInput = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Fm","start":4,"duration":4},{"name":"G","start":8}])");
    const auto borrowedSet = recommend(borrowedInput, alteredLoop);
    const auto* borrowedPath = findSuffix(borrowedSet, "Dm", "Dm");
    check(borrowedPath && borrowedPath->intent == PhraseIntent::Develop &&
          borrowedPath->cadence == CadenceType::None,
          "changed structural quality does not inherit the prototype loop claim");
    const auto completeLoop = templates(R"([{"id":"complete_loop","mode":"major","sequence":"I vi IV V","intent":"loop","cadence":"loop_closure","loopable":true}])");
    const auto completedInput = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4,"duration":4},{"name":"F","start":8,"duration":4},{"name":"G","start":12}])");
    const auto nextRound = recommend(completedInput, completeLoop);
    const auto* roundPath = findSuffix(nextRound, "C", "G");
    check(roundPath && roundPath->continuation.size() == 4 && roundPath->continuation[0].label == "C" &&
          roundPath->continuation[1].label == "Am" && roundPath->continuation[2].label == "F",
          "G3 complete loop returns a full next round");

    const auto diversityInput = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4}])");
    const auto runDiversityPair = [&](std::string_view fixture, float penalty) {
        auto pair = templates(fixture);
        RecommendationWeights weights;
        weights.diversityPenalty = penalty;
        RecommendationRequest request; request.preferredIntent = PhraseIntent::Develop;
        return recommendContinuations(query(diversityInput), CandidateIndex(std::move(pair)), request, weights);
    };
    const auto scoreFor = [&](const RecommendationSet& set, std::string_view id) {
        const auto* match = findMatch(set, id);
        check(match != nullptr, "diversity match exists");
        return match ? match->candidateRankingScore : -1.f;
    };
    const auto retained = [&](const RecommendationSet& set, std::string_view id) {
        const auto* match = findMatch(set, id);
        return match && match->disposition == "TARGET_RETAINED";
    };
    const auto fourFive = R"([{"id":"prefix4","mode":"major","sequence":"I vi iii IV V I","intent":"develop"},{"id":"prefix5","mode":"major","sequence":"I vi iii IV I IV V","intent":"develop"}])";
    const auto fourFiveDefault = runDiversityPair(fourFive, 13.f);
    const auto fourFiveNoPenalty = runDiversityPair(fourFive, 0.f);
    check(std::abs(scoreFor(fourFiveDefault, "prefix4")-scoreFor(fourFiveNoPenalty, "prefix4")) < 0.001f &&
          std::abs(scoreFor(fourFiveDefault, "prefix5")-scoreFor(fourFiveNoPenalty, "prefix5")) < 0.001f,
          "J1 shared two of four is not a majority of a five-event path");
    check(retained(fourFiveDefault,"prefix4")&&retained(fourFiveDefault,"prefix5"),
          "J1 both branching continuations remain candidates");
    const auto twoFour = R"([{"id":"short2","mode":"major","sequence":"I vi iii IV","intent":"develop"},{"id":"long4","mode":"major","sequence":"I vi iii IV V I","intent":"develop"}])";
    const auto twoFourDefault = runDiversityPair(twoFour, 13.f);
    const auto twoFourNoPenalty = runDiversityPair(twoFour, 0.f);
    check(std::abs(scoreFor(twoFourDefault, "short2")-scoreFor(twoFourNoPenalty, "short2")) < 0.001f &&
          std::abs(scoreFor(twoFourDefault, "long4")-scoreFor(twoFourNoPenalty, "long4")) < 0.001f,
          "J2 shared two of four does not suppress a short complete path");
    check(retained(twoFourDefault,"short2")&&retained(twoFourDefault,"long4"),
          "J2 both short and long continuations remain candidates");
    const auto threeThree = R"([{"id":"branch_g","mode":"major","sequence":"I vi iii IV V","intent":"develop"},{"id":"branch_i","mode":"major","sequence":"I vi iii IV I","intent":"develop"}])";
    const auto threeThreeDefault = runDiversityPair(threeThree, 13.f);
    const auto threeThreeNoPenalty = runDiversityPair(threeThree, 0.f);
    check(retained(threeThreeNoPenalty,"branch_g")&&retained(threeThreeNoPenalty,"branch_i"),
          "J3 zero-penalty control retains both distinct endpoints");
    const float branchGDelta=scoreFor(threeThreeNoPenalty,"branch_g")-scoreFor(threeThreeDefault,"branch_g");
    const float branchIDelta=scoreFor(threeThreeNoPenalty,"branch_i")-scoreFor(threeThreeDefault,"branch_i");
    check((std::abs(branchGDelta-13.f)<0.001f&&std::abs(branchIDelta)<0.001f)||
          (std::abs(branchIDelta-13.f)<0.001f&&std::abs(branchGDelta)<0.001f),
          "J3 shared two of three still penalizes the competing endpoint");
    const auto tieInput = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4}])");
    const auto tieRoutes = templates(R"([{"id":"a_short","mode":"major","sequence":"I vi ii IV V","intent":"develop","priorWeight":0.8},{"id":"z_long","mode":"major","sequence":"I vi iii IV V I","intent":"develop","priorWeight":0.65}])");
    const auto tied = recommend(tieInput, tieRoutes, {{}, PhraseIntent::Develop});
    const auto* shortRoute = findCandidate(tied, "a_short");
    const auto* longRoute = findCandidate(tied, "z_long");
    const auto scoreBucket = [](float score) { return std::round(score * 1000.f); };
    check(shortRoute && longRoute && shortRoute->continuation.size() == 3 && longRoute->continuation.size() == 4 &&
          scoreBucket(shortRoute->rankingScore) == scoreBucket(longRoute->rankingScore),
          "J4 prior offsets the continuation score difference into one rank bucket");
    check(tied.groups[1].size() == 2 && tied.groups[1][0].id == "z_long",
          "J4 near-tied longer complete suffix ranks before the shorter lexical ID");
    const auto developed = templates(R"([{"id":"develop","mode":"major","sequence":"I vi ii iii VI7 ii V I","intent":"develop","cadence":"authentic"}])");
    check(!recommend(a, developed).groups[1].empty(), "I develop grouping");
    const auto colored = templates(R"([{"id":"color","mode":"major","sequence":"I IV iv I","intent":"color","cadence":"plagal"}])");
    const auto colorQuery = progression(R"([{"name":"C","start":0,"duration":4},{"name":"F","start":4}])");
    const auto borrowedColor = recommend(colorQuery, colored);
    const auto* borrowedColorRoute = findCandidate(borrowedColor, "color");
    check(borrowedColorRoute && borrowedColorRoute->intent == PhraseIntent::Color &&
          borrowedColorRoute->continuation.size() == 2 && borrowedColorRoute->continuation[0].label == "Fm" &&
          borrowedColorRoute->continuation[1].label == "C" && !borrowedColor.groups[3].empty(),
          "J borrowed Fm-C suffix remains Color");
    const auto resolveAfterColor = templates(R"([{"id":"color_to_resolve","mode":"major","sequence":"I iv V I","intent":"color"}])");
    const auto developAfterColor = templates(R"([{"id":"color_to_develop","mode":"major","sequence":"I iv V ii","intent":"color"}])");
    const auto colorPrefix = progression(R"([{"name":"C","start":0,"duration":4},{"name":"Fm","start":4,"duration":4},{"name":"G","start":8}])");
    const auto resolveFromColor = recommend(colorPrefix, resolveAfterColor);
    const auto* resolvedColorRoute = findCandidate(resolveFromColor, "color_to_resolve");
    check(resolvedColorRoute && resolvedColorRoute->intent == PhraseIntent::Resolve &&
          resolvedColorRoute->continuation.size() == 1 && resolvedColorRoute->continuation[0].label == "C" &&
          resolveFromColor.groups[3].empty(),
          "J1 evidence-free color suffix ending on tonic is Resolve");
    const auto developFromColor = recommend(colorPrefix, developAfterColor);
    const auto* developedColorRoute = findCandidate(developFromColor, "color_to_develop");
    check(developedColorRoute && developedColorRoute->intent == PhraseIntent::Develop &&
          developedColorRoute->continuation.size() == 1 && developedColorRoute->continuation[0].label == "Dm",
          "J2 evidence-free color suffix ending on a non-tonic is Develop");
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
    const auto untagged = templates(R"([{"id":"untagged_exact","mode":"major","sequence":"I vi ii V I","rhythm":"4 2 2 2 4","intent":"resolve","cadence":"authentic"}])");
    const auto untaggedPop = recommend(a, untagged, {Style::Pop, {}});
    const auto untaggedAuto = recommend(a, untagged);
    const auto* untaggedPopMatch = findMatch(untaggedPop, "untagged_exact");
    const auto* untaggedPopCandidate = findCandidate(untaggedPop, "untagged_exact");
    const auto* untaggedAutoCandidate = findCandidate(untaggedAuto, "untagged_exact");
    check(untaggedPopMatch && untaggedPopMatch->rawCost <= 0.00001f,
          "M1 untagged exact route is identified by zero raw cost");
    check(untaggedPopCandidate && untaggedAutoCandidate &&
          std::abs(untaggedPopCandidate->subscores.style - 0.5f) < 0.001f &&
          std::abs(untaggedAutoCandidate->subscores.style - 0.5f) < 0.001f,
          "M1 untagged exact route stays neutral for Pop and Auto");
    const auto nonExactInput = progression(R"([{"name":"C","start":4,"duration":4},{"name":"Am","start":8,"duration":2},{"name":"F","start":10}])");
    RecommendationWeights nonExactWeights;
    nonExactWeights.minimumMatch = 0.f;
    nonExactWeights.minimumScore = 0.f;
    const auto untaggedNonExact = recommendContinuations(query(nonExactInput), CandidateIndex(untagged),
        {Style::Pop, {}}, nonExactWeights);
    const auto* untaggedNonExactMatch = findMatch(untaggedNonExact, "untagged_exact");
    const auto* untaggedNonExactCandidate = findCandidate(untaggedNonExact, "untagged_exact");
    check(untaggedNonExactMatch && untaggedNonExactMatch->rawCost > 0.00001f &&
          untaggedNonExactCandidate && std::abs(untaggedNonExactCandidate->subscores.style - 0.25f) < 0.001f,
          "M2 untagged non-exact route retains the lower style fallback");
    auto styleScore = [&](std::optional<Style> style) {
        auto result = recommend(a, styled, {style, {}});
        check(!result.groups[0].empty(), "style candidate available");
        return result.groups[0][0].rankingScore;
    };
    check(styleScore(Style::Pop) > styleScore(std::nullopt), "M style boosts selected template");
    const auto taggedPop = recommend(a, styled, {Style::Pop, {}});
    const auto* taggedPopCandidate = findCandidate(taggedPop, "pop");
    check(taggedPopCandidate && std::abs(taggedPopCandidate->subscores.style - 1.f) < 0.001f,
          "M3 explicit Pop metadata keeps its full style weight");
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
    const auto temp = std::filesystem::temp_directory_path() / ("HarmonyContinuationPhase3Tests-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);
    const auto factoryPath = temp / "factory.db", userPath = temp / "user.db";
    std::string error;
    const bool compiled=library::compileFactory(factoryPath, basic, error);
    if(!compiled)std::cerr<<"factory compile: "<<error<<'\n';
    check(compiled, "factory compile");
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
    {
        library::UserLibrary user(userPath);
        check(user.addProgression(custom, error), "R user save");
        check(!library::loadFactory(userPath), "factory/user database type isolation");
        check(user.loadAll().templates.size() == 1, "R user reload");
        check(library::compileFactory(factoryPath, circle, error), "S factory rebuild");
        check(user.loadAll().templates.size() == 1, "S user isolated");
        custom.name = "revised";
        check(user.updateProgression(custom, error) && user.loadAll().templates.front().name == "revised", "user update");
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
        check(user.removeProgression(custom.id, error) && user.loadAll().templates.empty(), "user delete");
    }
    for(const auto& path:{factoryPath,userPath,temp}) {
        std::error_code cleanupError;
        for(int attempt=0;attempt<20;++attempt){cleanupError.clear();std::filesystem::remove(path,cleanupError);if(!cleanupError)break;std::this_thread::sleep_for(std::chrono::milliseconds(25));}
        if(cleanupError)throw std::filesystem::filesystem_error("test cleanup failed",path,cleanupError);
    }
    std::cout << checks << '/' << checks << " PASS\n";
}
