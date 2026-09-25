#include "OfflinePreviewRenderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace harmony::preview {
AudioBuffer renderOffline(const Sequence& sequence,double rate) {
    AudioBuffer result; result.sampleRate=rate;
    if (!std::isfinite(rate)||rate<8000||rate>192000||sequence.events.empty()) return result;
    double endQN=sequence.totalQN;
    for (const auto& event:sequence.events) endQN=std::max(endQN,event.startQN+event.durationQN);
    const auto frames=qnToSamples(endQN,sequence.tempoBPM,rate)+
        static_cast<std::int64_t>(rate*0.15);
    if (frames<=0 || frames>static_cast<std::int64_t>(rate*600)) return result;
    PreviewSynth synth; synth.prepare(rate);
    if (!synth.start(sequence)) return result;
    result.left.resize(static_cast<std::size_t>(frames)); result.right.resize(static_cast<std::size_t>(frames));
    constexpr int block=512;
    for (std::int64_t pos=0;pos<frames;pos+=block) {
        const int count=static_cast<int>(std::min<std::int64_t>(block,frames-pos));
        synth.process(result.left.data()+pos,result.right.data()+pos,count);
    }
    result.peakVoices=synth.peakVoices();
    long double squares{};
    for (float sample:result.left) {
        if (!std::isfinite(sample)) { result.left.clear(); result.right.clear(); return result; }
        result.peak=std::max(result.peak,static_cast<double>(std::abs(sample)));
        squares+=static_cast<long double>(sample)*sample;
    }
    result.rms=std::sqrt(static_cast<double>(squares/result.left.size()));
    return result;
}
bool writeWav16(const AudioBuffer& audio,const std::filesystem::path& path,std::string& error) {
    if (audio.left.empty()||audio.left.size()!=audio.right.size()||audio.sampleRate<8000||
        audio.left.size()>(std::numeric_limits<std::uint32_t>::max()-36)/4) { error="invalid audio"; return false; }
    std::ofstream out(path,std::ios::binary);
    if (!out) { error="cannot open output"; return false; }
    auto u16=[&](std::uint16_t n) { out.put(static_cast<char>(n)); out.put(static_cast<char>(n>>8)); };
    auto u32=[&](std::uint32_t n) { u16(static_cast<std::uint16_t>(n)); u16(static_cast<std::uint16_t>(n>>16)); };
    out.write("RIFF",4); u32(static_cast<std::uint32_t>(36+audio.left.size()*4)); out.write("WAVEfmt ",8);
    u32(16); u16(1); u16(2); const auto rate=static_cast<std::uint32_t>(std::lround(audio.sampleRate));
    u32(rate); u32(rate*4); u16(4); u16(16);
    out.write("data",4); u32(static_cast<std::uint32_t>(audio.left.size()*4));
    for (std::size_t i=0;i<audio.left.size();++i) for (float sample:{audio.left[i],audio.right[i]}) {
        if (!std::isfinite(sample)) { error="nonfinite audio"; return false; }
        const auto value=static_cast<std::int16_t>(std::lround(std::clamp(sample,-1.f,1.f)*32767));
        u16(static_cast<std::uint16_t>(value));
    }
    if (!out) { error="write failed"; return false; }
    return true;
}
} // namespace harmony::preview
