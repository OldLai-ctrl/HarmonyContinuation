#include "ProgressionJson.h"
#include "TemplateJson.h"
#include "library/ProgressionLibrary.h"
#include "core/ContinuationEngine.h"
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <unordered_set>

namespace {
std::optional<harmony::KeySignature> parseKey(std::string_view arg) {
    constexpr std::string_view names[]{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    for (int i = 0; i < 12; ++i) {
        if (arg == std::string(names[i]) + ":major") return harmony::KeySignature{static_cast<harmony::PitchClass>(i), harmony::Mode::Major};
        if (arg == std::string(names[i]) + ":minor") return harmony::KeySignature{static_cast<harmony::PitchClass>(i), harmony::Mode::Minor};
    }
    return std::nullopt;
}
std::optional<harmony::Style> parseStyle(std::string_view arg) {
    if (arg == "pop") return harmony::Style::Pop;
    if (arg == "rock") return harmony::Style::Rock;
    if (arg == "rnb") return harmony::Style::Rnb;
    if (arg == "jazz") return harmony::Style::Jazz;
    if (arg == "citypop") return harmony::Style::CityPop;
    if (arg == "functional") return harmony::Style::Functional;
    return std::nullopt;
}
std::optional<harmony::PhraseIntent> parseIntent(std::string_view arg) {
    if (arg == "resolve") return harmony::PhraseIntent::Resolve;
    if (arg == "develop") return harmony::PhraseIntent::Develop;
    if (arg == "loop") return harmony::PhraseIntent::Loop;
    if (arg == "color") return harmony::PhraseIntent::Color;
    return std::nullopt;
}
std::string readText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open " + path.string());
    return std::string(std::istreambuf_iterator<char>{file}, {});
}
std::string styleNames(harmony::StyleFlags flags) {
    constexpr std::pair<harmony::Style, const char*> names[]{
        {harmony::Style::Pop,"Pop"}, {harmony::Style::Rock,"Rock"}, {harmony::Style::Rnb,"R&B"},
        {harmony::Style::Jazz,"Jazz"}, {harmony::Style::CityPop,"City Pop"},
        {harmony::Style::Functional,"Functional"}};
    std::string out;
    for (const auto& [style, name] : names) if (flags & static_cast<harmony::StyleFlags>(style)) {
        if (!out.empty()) out += ',';
        out += name;
    }
    return out.empty() ? "Unspecified" : out;
}
void printGroup(const char* title, const std::vector<harmony::ContinuationCandidate>& group,
                const std::vector<harmony::MatchResult>& matches, bool debug) {
    std::cout << title << '\n';
    if (group.empty()) { std::cout << "  (none)\n"; return; }
    for (std::size_t i = 0; i < group.size(); ++i) {
        const auto& candidate = group[i];
        std::cout << std::fixed << "#" << i + 1 << "  score=" << std::setprecision(1) << candidate.rankingScore
                  << "  key=" << harmony::formatKey(candidate.key)
                  << "  match=" << std::setprecision(3) << candidate.matchSimilarity
                  << "  style=" << styleNames(candidate.styles)
                  << "  cadence=" << harmony::cadenceName(candidate.cadence)
                  << "  support=" << candidate.supportCount << '\n';
        if (candidate.suggestedCurrentChordDurationQN)
            std::cout << "  Current OPEN hold: " << *candidate.suggestedCurrentChordDurationQN << " QN\n";
        std::cout << "  Continuation: ";
        for (std::size_t j = 0; j < candidate.continuation.size(); ++j) {
            if (j) std::cout << " -> ";
            std::cout << candidate.continuation[j].label << '(' << candidate.continuation[j].durationQN << " QN)";
        }
        std::cout << "\n  Sources: ";
        for (const auto& id : candidate.supportingTemplates) std::cout << id << ' ';
        std::cout << '\n';
        if (debug) {
            const auto& s = candidate.subscores;
            std::cout << "  Subscores: match=" << s.match << " skeleton=" << s.skeleton
                      << " style=" << s.style << " intent=" << s.intent
                      << " cadence=" << s.cadence << " continuation=" << s.continuation
                      << " prior=" << s.prior << " rhythm=" << s.rhythm << " support=" << s.support << '\n';
            const auto found = std::find_if(matches.begin(), matches.end(), [&](const auto& match) {
                return match.templateId == candidate.primaryTemplate;
            });
            if (found != matches.end()) {
                std::cout << "  Alignment:";
                for (const auto& step : found->alignmentTrace) {
                    std::cout << ' ' << harmony::alignmentOpName(step.operation) << '(';
                    if (step.queryIndex) std::cout << *step.queryIndex; else std::cout << '-';
                    std::cout << ':';
                    if (step.templateIndex) std::cout << *step.templateIndex; else std::cout << '-';
                    std::cout << ')';
                }
                std::cout << '\n';
            }
        }
    }
}
}
int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "usage: recommend_cli [save] progression.json [--factory factory.db] [--user user.db] [--key C:major] [--style pop] [--intent resolve] [--name name] [--id id] [--debug]\n";
            return 2;
        }
        int next = 1;
        bool save = std::string_view(argv[next]) == "save";
        if (save) ++next;
        if (next >= argc) throw std::runtime_error("missing progression JSON");
        const std::filesystem::path queryPath = argv[next++];
        std::filesystem::path factoryPath = std::filesystem::path(argv[0]).parent_path() / "factory.db";
        std::filesystem::path userPath = std::filesystem::path(argv[0]).parent_path() / "user.db";
#if defined(_WIN32)
        if (const auto* appData = std::getenv("LOCALAPPDATA"))
            userPath = std::filesystem::path(appData) / "HarmonyContinuation" / "user.db";
#endif
        harmony::AnalysisContext context;
        harmony::RecommendationRequest request;
        bool debug{};
        std::string name = queryPath.stem().string(), id = "USER_" + name;
        for (; next < argc;) {
            if (std::string_view(argv[next]) == "--debug") { debug = true; ++next; continue; }
            if (next + 1 >= argc) throw std::runtime_error("option requires value");
            const std::string_view option = argv[next], value = argv[next + 1];
            if (option == "--factory") factoryPath = argv[next + 1];
            else if (option == "--user") userPath = argv[next + 1];
            else if (option == "--key") {
                context.forcedKey = parseKey(value);
                if (!context.forcedKey) throw std::runtime_error("invalid key");
            } else if (option == "--style") {
                if (value != "auto" && !(request.style = parseStyle(value))) throw std::runtime_error("invalid style");
            } else if (option == "--intent") {
                if (value != "auto" && !(request.preferredIntent = parseIntent(value))) throw std::runtime_error("invalid intent");
            } else if (option == "--name") name = value;
            else if (option == "--id") id = value;
            else throw std::runtime_error("unknown option");
            next += 2;
        }
        auto parsed = harmony::dev::parseProgressionJson(readText(queryPath));
        if (!parsed) throw std::runtime_error("invalid progression: " + parsed.error);
        if (!harmony::sortAndInferDurations(parsed.events)) throw std::runtime_error("invalid progression time");
        if (save) {
            // Remove host project position before building a reusable, key independent template.
            const double origin = parsed.events.front().startQN;
            for (auto& chord : parsed.events) chord.startQN -= origin;
            const auto query = harmony::makeMatchQuery(parsed.events, context);
            if (query.interpretations.empty()) throw std::runtime_error("no key interpretation");
            auto item = harmony::ProgressionTemplate{};
            item.id = id; item.name = name; item.sourceType = "user";
            item.mode = query.interpretations.front().key.key.mode;
            item.full = query.interpretations.front().full;
            item.skeletonIndices = query.interpretations.front().skeletonIndices;
            for (auto& event : item.full) if (!event.durationQN) event.durationQN = 4.0;
            item.phraseLength = item.full.size();
            item.intent = request.preferredIntent.value_or(harmony::PhraseIntent::Neutral);
            if (request.style) { item.styles = static_cast<harmony::StyleFlags>(*request.style);
                item.styleWeights.emplace_back(*request.style, 1.f); }
            harmony::prepareTemplate(item);
            if (!userPath.parent_path().empty()) std::filesystem::create_directories(userPath.parent_path());
            std::string error;
            if (!harmony::library::UserLibrary(userPath).addProgression(item, error)) throw std::runtime_error(error);
            std::cout << "saved " << item.id << " relative QN, key independent; user.db=" << userPath << '\n';
            return 0;
        }
        auto factory = harmony::library::loadFactory(factoryPath);
        if (!factory) throw std::runtime_error("factory.db: " + factory.error);
        auto user = harmony::library::UserLibrary(userPath).loadAll();
        if (!user) throw std::runtime_error("user.db: " + user.error);
        std::unordered_set<harmony::TemplateID> ids;
        for (const auto& item : factory.templates) ids.insert(item.id);
        for (const auto& item : user.templates)
            if (!ids.insert(item.id).second) throw std::runtime_error("duplicate factory/user template ID: " + item.id);
        factory.templates.insert(factory.templates.end(),
            std::make_move_iterator(user.templates.begin()), std::make_move_iterator(user.templates.end()));
        const harmony::CandidateIndex index(std::move(factory.templates));
        const auto query = harmony::makeMatchQuery(parsed.events, context);
        const auto result = harmony::recommendContinuations(query, index, request);
        std::cout << "QUERY\n";
        for (const auto& chord : parsed.events) std::cout << "  " << chord.name << '\n';
        std::cout << "KEY\n";
        for (const auto& key : query.interpretations) std::cout << "  " << harmony::formatKey(key.key.key) << '\n';
        constexpr const char* names[]{"RESOLVE", "DEVELOP", "LOOP", "COLOR"};
        for (std::size_t i = 0; i < 4; ++i) printGroup(names[i], result.groups[i], result.matches, debug);
        std::cout << "PERF totalMatchMs=" << result.stats.totalMs << " shortlist=" << result.stats.shortlistSize << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "recommend FAIL: " << e.what() << '\n'; return 1; }
}
