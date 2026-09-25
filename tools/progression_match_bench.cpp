#include "ProgressionJson.h"
#include "TemplateJson.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: progression_match_bench query.json\n"; return 2; }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) { std::cerr << "cannot open query\n"; return 2; }
    const std::string source(std::istreambuf_iterator<char>{file}, {});
    const auto progression = harmony::dev::parseProgressionJson(source);
    const auto fixture = harmony::dev::loadDevelopmentTemplates();
    if (!progression || !fixture) { std::cerr << "invalid fixture\n"; return 2; }
    const auto query = harmony::makeMatchQuery(progression.events);
    if (query.interpretations.empty()) { std::cerr << "no key interpretations\n"; return 1; }
    using Clock = std::chrono::steady_clock;
    for (const std::size_t count : {100u, 1000u, 10000u}) {
        std::vector<harmony::ProgressionTemplate> templates;
        templates.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto item = fixture.templates[i % fixture.templates.size()];
            item.id += "_" + std::to_string(i);
            // Vary most entries so the prefilter has a realistic ordering task.
            if (i >= fixture.templates.size()) {
                const auto shift = static_cast<int>((i / fixture.templates.size()) % 7);
                for (auto& event : item.full) if (event.degree)
                    event.degree->degree = 1 + (event.degree->degree - 1 + shift) % 7;
            }
            templates.push_back(std::move(item));
        }
        const auto start = Clock::now();
        const harmony::CandidateIndex index(std::move(templates));
        const auto built = Clock::now();
        harmony::MatchRunStats stats;
        const auto results = harmony::matchProgression(query, index, {}, 3, &stats);
        const auto buildMs = std::chrono::duration<double, std::milli>(built - start).count();
        std::cout << "templates=" << count << " fingerprintBuildMs=" << buildMs
                  << " prefilterMs=" << stats.prefilterMs << " alignmentMs=" << stats.alignmentMs
                  << " totalMs=" << buildMs + stats.totalMs << " shortlist=" << stats.shortlistSize;
        if (!results.empty()) std::cout << " top=" << results.front().templateId << " score=" << results.front().similarity;
        std::cout << '\n';
    }
}
