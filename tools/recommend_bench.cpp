#include "ProgressionJson.h"
#include "library/ProgressionLibrary.h"
#include "core/ContinuationEngine.h"
#include <chrono>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: recommend_bench factory.db\n"; return 2; }
    auto library = harmony::library::loadFactory(argv[1]);
    if (!library) { std::cerr << library.error << '\n'; return 1; }
    const auto sample = harmony::dev::parseProgressionJson(R"([{"name":"C","start":0,"duration":4},{"name":"Am","start":4,"duration":2},{"name":"A7","start":6,"duration":1},{"name":"Dm","start":7}])");
    if (!sample) return 1;
    using Clock = std::chrono::steady_clock;
    for (std::size_t count : {library.templates.size(), std::size_t{1000}, std::size_t{10000}}) {
        std::vector<harmony::ProgressionTemplate> items;
        items.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto item = library.templates[i % library.templates.size()];
            item.id += "_" + std::to_string(i);
            if (i >= library.templates.size()) {
                const auto shift = static_cast<int>((i / library.templates.size()) % 7);
                for (auto& event : item.full) if (event.degree)
                    event.degree->degree = 1 + (event.degree->degree - 1 + shift) % 7;
            }
            items.push_back(std::move(item));
        }
        const auto start = Clock::now();
        const auto query = harmony::makeMatchQuery(sample.events);
        const harmony::CandidateIndex index(std::move(items));
        const auto result = harmony::recommendContinuations(query, index);
        const auto end = Clock::now();
        std::size_t candidates{};
        for (const auto& group : result.groups) candidates += group.size();
        std::cout << "templates=" << count << " totalMs="
                  << std::chrono::duration<double, std::milli>(end - start).count()
                  << " matchMs=" << result.stats.totalMs << " shortlist=" << result.stats.shortlistSize
                  << " candidates=" << candidates << '\n';
    }
}
