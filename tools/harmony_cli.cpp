#include "ProgressionJson.h"
#include "core/HarmonyAnalysis.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>

namespace {
std::optional<harmony::KeySignature> keyFromArg(std::string_view value) {
    constexpr std::string_view names[]{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    for (int i = 0; i < 12; ++i)
        for (auto mode : {harmony::Mode::Major, harmony::Mode::Minor}) {
            const auto suffix = mode == harmony::Mode::Major ? ":major" : ":minor";
            if (value == std::string(names[i]) + suffix)
                return harmony::KeySignature{static_cast<harmony::PitchClass>(i), mode};
        }
    return std::nullopt;
}
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: harmony_cli progression.json [--key C:major]\n";
        return 2;
    }
    harmony::AnalysisContext context;
    if (argc > 2) {
        if (argc != 4 || std::string_view(argv[2]) != "--key" || !(context.forcedKey = keyFromArg(argv[3]))) {
            std::cerr << "invalid key override; use --key C:major or --key A:minor\n";
            return 2;
        }
    }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) { std::cerr << "cannot open input\n"; return 2; }
    const std::string input(std::istreambuf_iterator<char>{file}, {});
    const auto parsed = harmony::dev::parseProgressionJson(input);
    if (!parsed) { std::cerr << "invalid progression: " << parsed.error << '\n'; return 2; }
    const auto result = harmony::analyzeHarmony(parsed.events, context);
    if (!result.selectedKey) { std::cerr << "analysis failed\n"; return 1; }
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "KEY CANDIDATES\n";
    for (std::size_t i = 0; i < std::min<std::size_t>(3, result.keyCandidates.size()); ++i)
        std::cout << harmony::formatKey(result.keyCandidates[i].key) << "  " << result.keyCandidates[i].confidence << '\n';
    std::cout << "SELECTED " << harmony::formatKey(result.selectedKey->key)
              << (context.forcedKey ? " (override)" : " (auto)") << "\nFULL\n";
    for (const auto& event : result.full) {
        std::cout << parsed.events[event.sourceChordIndex].name << "  " << harmony::formatDegree(event)
                  << "  " << harmony::formatFunction(event.function) << "  " << event.structuralWeight;
        if (harmony::hasRole(event.roles, harmony::Role::SecondaryDominant)) std::cout << "  SecondaryDominant";
        if (harmony::hasRole(event.roles, harmony::Role::SecondaryLeadingTone)) std::cout << "  SecondaryLeadingTone";
        if (harmony::hasRole(event.roles, harmony::Role::Borrowed)) std::cout << "  Borrowed";
        if (harmony::hasRole(event.roles, harmony::Role::Approach)) std::cout << "  Approach";
        std::cout << '\n';
    }
    std::cout << "SKELETON\n";
    for (std::size_t i = 0; i < result.skeletonIndices.size(); ++i) {
        if (i) std::cout << " -> ";
        std::cout << harmony::formatDegree(result.full[result.skeletonIndices[i]]);
    }
    std::cout << '\n';
    for (const auto& warning : result.warnings) std::cout << "WARN " << warning << '\n';
    return 0;
}
