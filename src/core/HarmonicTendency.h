#pragma once
#include <array>
#include <cstdint>
namespace harmony {
enum class HarmonicTendency : std::uint8_t { Conservative, Balanced, Bold };
struct HarmonicTendencyProfile {
    std::array<int,3> operations,insertions,substitutions;
    float skeletonMinimum{},colorBias{},chromaticBias{};
};
const HarmonicTendencyProfile& tendencyProfile(HarmonicTendency) noexcept;
}
