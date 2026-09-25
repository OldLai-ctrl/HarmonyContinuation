#include "TemplateJson.h"
#include "library/ProgressionLibrary.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <unordered_map>

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: db_compiler data/factory output/factory.db\n"; return 2; }
    std::vector<harmony::ProgressionTemplate> templates;
    std::set<std::string> ids, exact;
    std::unordered_map<std::string, int> skeletons;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension() != ".json") continue;
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file) throw std::runtime_error("cannot read " + entry.path().string());
            const std::string text(std::istreambuf_iterator<char>{file}, {});
            auto parsed = harmony::dev::parseTemplateJson(text, true);
            if (!parsed) throw std::runtime_error(entry.path().string() + ": " + parsed.error);
            for (auto& item : parsed.templates) {
                if (!ids.insert(item.id).second) throw std::runtime_error("duplicate id: " + item.id);
                if (item.sourceType != "factory") throw std::runtime_error("factory sourceType required: " + item.id);
                if (item.intent == harmony::PhraseIntent::Loop && !item.loopable)
                    throw std::runtime_error("loop intent requires loopable: " + item.id);
                if (item.loopable && item.cadence == harmony::CadenceType::Authentic)
                    std::cerr << "warning: authentic and loopable: " << item.id << '\n';
                std::string progressionKey = item.mode == harmony::Mode::Major ? "M" : "m";
                std::string skeletonKey = progressionKey;
                for (const auto& event : item.full) {
                    progressionKey += '|' + harmony::formatMatchEvent(event) + ':' +
                                      std::to_string(event.durationQN.value_or(0));
                }
                for (const auto index : item.skeletonIndices)
                    skeletonKey += '|' + harmony::formatMatchEvent(item.full[index]);
                auto exactKey = progressionKey + '/' + skeletonKey + '/' + std::to_string(static_cast<int>(item.intent)) + '/' +
                                std::to_string(static_cast<int>(item.cadence));
                for (const auto& [style, weight] : item.styleWeights)
                    exactKey += '/' + std::to_string(static_cast<int>(style)) + ':' + std::to_string(weight);
                exactKey += '/' + std::to_string(item.meterNumerator) + '/' + std::to_string(item.meterDenominator)
                         + '/' + std::to_string(item.loopable) + '/' + std::to_string(item.secondaryIntents)
                         + '/' + std::to_string(item.priorWeight) + '/' + std::to_string(item.complexity);
                for (const auto& tag : item.tags) exactKey += '/' + tag;
                if (!exact.insert(exactKey).second) throw std::runtime_error("exact duplicate metadata/progression: " + item.id);
                ++skeletons[skeletonKey];
                templates.push_back(std::move(item));
            }
        }
        if (templates.empty()) throw std::runtime_error("no JSON source found");
        std::size_t near{};
        for (const auto& [key, count] : skeletons) if (count > 1) near += static_cast<std::size_t>(count - 1);
        std::string error;
        if (!harmony::library::compileFactory(argv[2], templates, error)) throw std::runtime_error(error);
        auto loaded = harmony::library::loadFactory(argv[2]);
        if (!loaded || loaded.templates.size() != templates.size()) throw std::runtime_error("database roundtrip failed: " + loaded.error);
        std::cout << "factory.db READY templates=" << templates.size() << " near_skeleton_variants=" << near << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "factory lint FAIL: " << e.what() << '\n'; return 1; }
}
