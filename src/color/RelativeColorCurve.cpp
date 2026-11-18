#include "RelativeColorCurve.h"

#include "core/ChordPitchSet.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace harmony::color {
namespace {
constexpr std::uint16_t majorScaleMask = (1u << 0) | (1u << 2) | (1u << 4) |
    (1u << 5) | (1u << 7) | (1u << 9) | (1u << 11);
constexpr std::uint16_t minorScaleMask = (1u << 0) | (1u << 2) | (1u << 3) |
    (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10);

constexpr std::size_t component(RelativeColorComponent value) {
    return static_cast<std::size_t>(value);
}

bool validKey(const KeySignature& key) {
    const auto tonic = static_cast<unsigned>(key.tonic);
    return tonic < 12 && (key.mode == Mode::Major || key.mode == Mode::Minor);
}

bool validChord(const ChordPitchSet& chord) {
    const auto intervals = static_cast<unsigned>(chord.intervals);
    const auto colors = static_cast<unsigned>(chord.colorMask);
    const auto mask = intervals | colors;
    return chord.root >= 0 && chord.root < 12 && chord.bass >= 0 && chord.bass < 12 &&
        (intervals & 0xf000u) == 0 && (colors & 0xf000u) == 0 &&
        (mask & 0x0fffu) != 0 && (mask & 1u) != 0;
}

unsigned absolutePitchSet(const ChordPitchSet& chord) {
    const auto relative = static_cast<unsigned>(chord.intervals | chord.colorMask);
    unsigned result = 1u << chord.bass;
    for (int interval = 0; interval < 12; ++interval)
        if ((relative & (1u << interval)) != 0) result |= 1u << ((chord.root + interval) % 12);
    return result;
}

int countBits(unsigned mask) {
    int count{};
    for (; mask != 0; mask &= mask - 1) ++count;
    return count;
}

bool inKey(int pitchClass, const KeySignature& key) {
    const auto tonic = static_cast<int>(key.tonic);
    const auto offset = (pitchClass - tonic + 12) % 12;
    const auto mask = key.mode == Mode::Major ? majorScaleMask : minorScaleMask;
    return (mask & (1u << offset)) != 0;
}

void addFactor(RelativeColorPoint& point, RelativeColorComponent slot,
               RelativeColorFactor factor, double amount) {
    if (amount <= 0.0) return;
    point.contributions[component(slot)] += amount;
    point.factors |= relativeColorFlag(factor);
}

std::optional<double> finiteDuration(const ChordEvent& event) {
    if (event.openEnded || !event.durationQN || !std::isfinite(*event.durationQN) || *event.durationQN <= 0.0)
        return std::nullopt;
    return event.durationQN;
}
} // namespace

std::vector<RelativeColorPoint> analyzeRelativeColorCurve(
    const Progression& progression, std::optional<KeySignature> key) {
    std::vector<RelativeColorPoint> result;
    result.reserve(progression.size());
    std::optional<unsigned> previousPitchSet;
    const bool usableKey = key && validKey(*key);

    for (const auto& event : progression) {
        RelativeColorPoint point;
        if (std::isfinite(event.startQN)) point.startQN = event.startQN;
        point.durationQN = finiteDuration(event);
        const auto chord = chordPitches(event);
        if (!usableKey || !point.startQN || !point.durationQN || !validChord(chord)) {
            previousPitchSet.reset();
            result.push_back(point);
            continue;
        }

        const auto intervals = static_cast<unsigned>(chord.intervals | chord.colorMask);
        const auto pitchSet = absolutePitchSet(chord);
        const bool minorThird = (intervals & (1u << 3)) != 0;
        const bool majorThird = (intervals & (1u << 4)) != 0;
        const bool diminishedFifth = (intervals & (1u << 6)) != 0;
        const bool augmentedFifth = (intervals & (1u << 8)) != 0;
        const bool perfectFifth = (intervals & (1u << 7)) != 0;
        const bool diminished = minorThird && diminishedFifth && !majorThird && !perfectFifth;
        const bool augmented = majorThird && augmentedFifth && !minorThird && !perfectFifth;
        const bool diminishedSeventh = diminished && (intervals & (1u << 9)) != 0;

        point.contributions[component(RelativeColorComponent::Base)] = 10.0;
        if ((intervals & ((1u << 10) | (1u << 11))) != 0 || diminishedSeventh)
            addFactor(point, RelativeColorComponent::Seventh, RelativeColorFactor::Seventh, 12.0);

        if (minorThird || majorThird) {
            const auto extensionMask = (1u << 2) | (1u << 5) | (diminishedSeventh ? 0u : (1u << 9));
            const int extensions = countBits(intervals & extensionMask);
            addFactor(point, RelativeColorComponent::Extensions, RelativeColorFactor::Extensions,
                static_cast<double>(std::min(24, extensions * 8)));
        }

        unsigned altered = intervals & ((1u << 1) | (1u << 6) | (1u << 8));
        if (diminished) altered &= ~(1u << 6);
        if (augmented) altered &= ~(1u << 8);
        addFactor(point, RelativeColorComponent::Altered, RelativeColorFactor::Altered,
            static_cast<double>(std::min(16, countBits(altered) * 8)));

        if (diminished) addFactor(point, RelativeColorComponent::SpecialStructure,
            RelativeColorFactor::Diminished, 20.0);
        else if (augmented) addFactor(point, RelativeColorComponent::SpecialStructure,
            RelativeColorFactor::Augmented, 18.0);

        int outsideKey{};
        for (int pitch = 0; pitch < 12; ++pitch)
            if ((pitchSet & (1u << pitch)) != 0 && !inKey(pitch, *key)) ++outsideKey;
        addFactor(point, RelativeColorComponent::OutsideKey, RelativeColorFactor::OutsideKey,
            static_cast<double>(std::min(20, outsideKey * 10)));

        if (previousPitchSet) {
            const auto intersection = countBits(pitchSet & *previousPitchSet);
            const auto unionCount = countBits(pitchSet | *previousPitchSet);
            if (unionCount > 0) {
                const auto distance = 1.0 - static_cast<double>(intersection) / unionCount;
                addFactor(point, RelativeColorComponent::Contrast, RelativeColorFactor::Contrast, distance * 10.0);
            }
        }

        double total{};
        for (const auto contribution : point.contributions) total += contribution;
        point.value = std::clamp(total, 0.0, 100.0);
        previousPitchSet = pitchSet;
        result.push_back(point);
    }
    return result;
}

} // namespace harmony::color
