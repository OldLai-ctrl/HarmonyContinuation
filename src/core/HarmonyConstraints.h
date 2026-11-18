#pragma once
#include "Progression.h"
#include "ChordPitchSet.h"
#include <string_view>

namespace harmony {
inline constexpr float melodySoftPenaltyPoints=12.f;
enum class ConstraintRole : std::uint8_t { Present, TopVoice };
enum class ConstraintStrictness : std::uint8_t { Soft, Hard };
enum class MelodyRelation : std::uint8_t { ChordTone, AvailableTension, Uncertain, Conflict };
struct MelodyConstraint {
    double startQN{},durationQN{4};
    std::optional<int> pitch; // absolute MIDI note; otherwise pitchClass
    int pitchClass{};
    ConstraintRole role{ConstraintRole::Present};
    ConstraintStrictness strictness{ConstraintStrictness::Soft};
    bool operator==(const MelodyConstraint&) const = default;
};
struct HarmonyConstraintSet {
    std::vector<MelodyConstraint> melody;
    bool operator==(const HarmonyConstraintSet&) const = default;
};
struct MelodyObservation {
    double startQN{};
    std::string chord;
    int pitchClass{},interval{};
    MelodyRelation relation{MelodyRelation::Uncertain};
    bool topVoiceSatisfied{true};
};
struct MelodyCompatibility {
    float score{1};
    bool hardSatisfied{true};
    std::vector<MelodyObservation> observations;
};
bool validConstraints(const HarmonyConstraintSet&) noexcept;
MelodyRelation classifyMelody(const ChordEvent&,int pitchClass);
MelodyRelation classifyMelody(const ChordPitchSet&,int pitchClass);
double progressionOpenDuration(const Progression&);
MelodyCompatibility evaluateMelody(const Progression&,const HarmonyConstraintSet&,double openDurationQN=0);
std::optional<int> melodyTopPitch(const MelodyConstraint&) noexcept;
std::optional<MelodyConstraint> parseMelodyNote(std::string_view);
std::string melodyNoteName(const MelodyConstraint&);
std::string encodeConstraints(const HarmonyConstraintSet&);
HarmonyConstraintSet decodeConstraints(std::string_view);
}
