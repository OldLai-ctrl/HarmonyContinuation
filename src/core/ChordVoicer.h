#pragma once
#include "ChordPitchSet.h"
#include <array>
namespace harmony {
struct Voicing {
    int bass{};
    std::array<int,8> upper{};
    int upperCount{};
    bool melodySatisfied{true};
};
class ChordVoicer {
public:
    Voicing voice(const ChordPitchSet&,std::optional<int> topVoice={});
    void reset() noexcept { previous_={};hasPrevious_=false; }
private:
    Voicing previous_{};
    bool hasPrevious_{};
};
}
