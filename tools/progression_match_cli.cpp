#include "ProgressionJson.h"
#include "TemplateJson.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>

namespace {
std::optional<harmony::KeySignature> keyFromArg(std::string_view value) {
    constexpr std::string_view names[]{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    for (int i = 0; i < 12; ++i) for (auto mode : {harmony::Mode::Major, harmony::Mode::Minor}) {
        const auto suffix = mode == harmony::Mode::Major ? ":major" : ":minor";
        if (value == std::string(names[i]) + suffix)
            return harmony::KeySignature{static_cast<harmony::PitchClass>(i), mode};
    }
    return std::nullopt;
}
std::optional<std::string> readFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>{file}, {});
}
}
int main(int argc, char** argv) {
    if (argc != 3 && argc != 5) {
        std::cerr << "usage: progression_match_cli query.json templates.json [--key C:major]\n";
        return 2;
    }
    harmony::AnalysisContext context;
    if (argc == 5 && (std::string_view(argv[3]) != "--key" || !(context.forcedKey = keyFromArg(argv[4])))) {
        std::cerr << "invalid key override\n"; return 2;
    }
    const auto queryText = readFile(argv[1]), templateText = readFile(argv[2]);
    if (!queryText || !templateText) { std::cerr << "cannot open input\n"; return 2; }
    const auto progression = harmony::dev::parseProgressionJson(*queryText);
    if (!progression) { std::cerr << "invalid query: " << progression.error << '\n'; return 2; }
    auto templates = harmony::dev::parseTemplateJson(*templateText);
    if (!templates) { std::cerr << "invalid templates: " << templates.error << '\n'; return 2; }
    const auto query = harmony::makeMatchQuery(progression.events, context);
    const harmony::CandidateIndex index(std::move(templates.templates));
    harmony::MatchRunStats stats;
    const auto results = harmony::matchProgression(query, index, {}, 3, &stats);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "QUERY\n";
    for (std::size_t i = 0; i < progression.events.size(); ++i)
        std::cout << i << "  " << progression.events[i].name << '\n';
    std::cout << "KEY INTERPRETATIONS" << (query.forcedKey ? " (forced)" : "") << '\n';
    for (const auto& interpretation : query.interpretations)
        std::cout << harmony::formatKey(interpretation.key.key) << "  prior=" << interpretation.key.confidence << '\n';
    std::cout << "TOP MATCHES\n";
    for (const auto& result : results) {
        const auto templateIt = std::find_if(index.templates().begin(), index.templates().end(),
            [&](const auto& candidate) { return candidate.id == result.templateId; });
        std::cout << result.templateId << "  " << result.templateName << "  similarity=" << result.similarity
                  << "  skeleton=" << result.subScores.skeletonHarmony
                  << "  full=" << result.subScores.fullHarmony
                  << "  rhythm=" << result.subScores.rhythmSimilarity
                  << "  key=" << harmony::formatKey(result.key)
                  << "  matchStart=" << result.templateMatchStart
                  << "  matchEnd=" << result.templateMatchEnd
                  << "  continuationStart=" << result.continuationStartIndex << '\n';
        for (const auto& step : result.alignmentTrace) {
            std::cout << "  " << harmony::alignmentOpName(step.operation) << "  ";
            if (step.queryIndex) std::cout << progression.events[*step.queryIndex].name;
            else std::cout << "GAP";
            std::cout << " <-> ";
            if (step.templateIndex && templateIt != index.templates().end())
                std::cout << harmony::formatMatchEvent(templateIt->full[*step.templateIndex]);
            else std::cout << "GAP";
            std::cout << "  cost=" << step.cost;
            const auto reasons = harmony::reasonNames(step.reasons);
            if (!reasons.empty()) std::cout << "  " << reasons;
            std::cout << '\n';
        }
    }
    std::cout << "PERF prefilterMs=" << stats.prefilterMs << " alignmentMs=" << stats.alignmentMs
              << " totalMs=" << stats.totalMs << " shortlist=" << stats.shortlistSize << '\n';
    return 0;
}
