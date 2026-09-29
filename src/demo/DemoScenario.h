#pragma once
#include "session/PluginSessionState.h"
#include <filesystem>

namespace harmony::demo {
struct Scenario {
    std::string name;
    double tempo{120};
    int meterNumerator{4}, meterDenominator{4};
    Progression chords;
    std::optional<KeySignature> forcedKey;
    std::optional<Style> style;
    std::optional<PhraseIntent> intent;
};
struct ScenarioResult { Scenario scenario; std::string error; explicit operator bool() const noexcept { return error.empty(); } };
ScenarioResult loadScenario(const std::filesystem::path&);
} // namespace harmony::demo
