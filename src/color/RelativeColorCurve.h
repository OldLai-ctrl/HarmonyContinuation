#pragma once

#include "core/HarmonyAnalysis.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace harmony::color {

enum class RelativeColorComponent : std::size_t {
    Base, Seventh, Extensions, Altered, SpecialStructure, OutsideKey, Contrast, Count
};

enum class RelativeColorFactor : std::uint32_t {
    Seventh = 1u << 0, Extensions = 1u << 1, Altered = 1u << 2,
    Diminished = 1u << 3, Augmented = 1u << 4, OutsideKey = 1u << 5,
    Contrast = 1u << 6
};

constexpr std::uint32_t relativeColorFlag(RelativeColorFactor factor) noexcept {
    return static_cast<std::uint32_t>(factor);
}

struct RelativeColorPoint {
    std::optional<double> startQN;
    std::optional<double> durationQN;
    std::optional<double> value;
    std::uint32_t factors{};
    std::array<double, static_cast<std::size_t>(RelativeColorComponent::Count)> contributions{};
};

// A fixed explanatory structure index for a resolved route. It does not rank,
// mutate, or feed back into any recommendation score.
std::vector<RelativeColorPoint> analyzeRelativeColorCurve(
    const Progression& progression, std::optional<KeySignature> key);

} // namespace harmony::color
