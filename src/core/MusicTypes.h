#pragma once
#include <cstdint>

namespace harmony {
// Only validated integers in [0, 11] enter this enum. C == 0.
enum class PitchClass : std::uint8_t { C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B };
enum class ChordQuality {
    Unknown, Major, Minor, Dominant7, Major7, Minor7,
    Diminished, Diminished7, HalfDiminished7, Sus2, Sus4, Augmented
};
enum class ChordSource { SyntheticFixture, CubaseDrop, Clipboard };
}
