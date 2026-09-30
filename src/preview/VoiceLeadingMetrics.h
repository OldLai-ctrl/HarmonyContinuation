#pragma once
#include "core/Progression.h"
#include <optional>
#include <vector>

namespace harmony::preview {
struct VoiceLeadingTransition {
    int commonToneCount{};
    float commonToneRatio{};
    int bassMotionSemitones{};
    int bassDirection{}; // -1 down, 0 still, +1 up
    int upperVoiceTotalMotion{};
    int maximumVoiceLeap{};
    int voiceCrossingPenalty{};
    float score{}; // 0..1
};
struct VoiceLeadingMetrics {
    std::vector<VoiceLeadingTransition> transitions;
    std::vector<int> bassMidi;
    int commonToneCount{};
    float commonToneRatio{};
    int bassMotionSemitones{};
    int bassDirection{};
    int upperVoiceTotalMotion{};
    int maximumVoiceLeap{};
    int voiceCrossingPenalty{};
    float score{0.5f}; // neutral for a one-chord phrase
};
std::optional<VoiceLeadingMetrics> measureVoiceLeading(const Progression&);
} // namespace harmony::preview
