#pragma once
#include "PreviewSequence.h"
#include <array>

namespace harmony::preview {
struct Voicing { int bass{}; std::array<int,4> upper{}; int upperCount{}; };
class ChordVoicer {
public:
    Voicing voice(const Chord&);
    void reset() noexcept { previous_={}; hasPrevious_=false; }
private:
    Voicing previous_{};
    bool hasPrevious_{};
};
} // namespace harmony::preview
