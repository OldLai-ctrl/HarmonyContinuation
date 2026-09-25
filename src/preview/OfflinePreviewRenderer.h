#pragma once
#include "PreviewSynth.h"
#include <filesystem>

namespace harmony::preview {
struct AudioBuffer {
    std::vector<float> left, right;
    double sampleRate{};
    double peak{}, rms{};
    int peakVoices{};
};
AudioBuffer renderOffline(const Sequence&,double sampleRate=48000);
bool writeWav16(const AudioBuffer&,const std::filesystem::path&,std::string& error);
} // namespace harmony::preview
