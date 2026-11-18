#include "HarmonicTendency.h"
namespace harmony {
const HarmonicTendencyProfile& tendencyProfile(HarmonicTendency tendency) noexcept {
    static constexpr HarmonicTendencyProfile profiles[]{
        {{1,2,3},{0,1,1},{0,0,1},0.88f,-4.f,-3.f},
        {{2,3,5},{0,1,2},{0,0,1},0.75f,0.f,0.f},
        {{2,4,6},{0,2,3},{0,0,1},0.75f,4.f,3.f}};
    const auto index=static_cast<unsigned>(tendency);return profiles[index<3?index:1];
}
}
