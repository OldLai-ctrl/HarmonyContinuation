#include "HostContextAdapter.h"
#include <bit>
#include <iomanip>
#include <sstream>
namespace harmony::plugin {
using Steinberg::Vst::ProcessContext;
void HostContextAdapter::capture(const ProcessContext* c) noexcept {
    HostSnapshot s;
    if (c) {
        s.words[0] = 1; s.words[1] = c->state;
        s.words[2] = std::bit_cast<std::uint64_t>(c->projectTimeSamples);
        if (c->state & ProcessContext::kTempoValid) s.words[3] = std::bit_cast<std::uint64_t>(c->tempo);
        if (c->state & ProcessContext::kProjectTimeMusicValid) s.words[4] = std::bit_cast<std::uint64_t>(c->projectTimeMusic);
        if (c->state & ProcessContext::kTimeSigValid) { s.words[5] = c->timeSigNumerator; s.words[6] = c->timeSigDenominator; }
        if (c->state & ProcessContext::kChordValid) { s.words[7] = c->chord.keyNote; s.words[8] = c->chord.rootNote; s.words[9] = static_cast<std::uint16_t>(c->chord.chordMask); }
        s.words[10] = std::bit_cast<std::uint64_t>(c->sampleRate);
    }
    revision_.fetch_add(1, std::memory_order_acq_rel);
    for (std::size_t i = 0; i < words_.size(); ++i) words_[i].store(s.words[i]);
    s.generation = generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    revision_.fetch_add(1, std::memory_order_release);
}
bool HostContextAdapter::read(HostSnapshot& s) const noexcept {
    for (int retry = 0; retry < 3; ++retry) {
        const auto before = revision_.load(std::memory_order_acquire);
        if (before & 1) continue;
        for (std::size_t i = 0; i < words_.size(); ++i) s.words[i] = words_[i].load(std::memory_order_relaxed);
        s.generation = generation_.load(std::memory_order_relaxed);
        if (revision_.load(std::memory_order_acquire) == before) return true;
    }
    return false;
}
std::string HostContextAdapter::describe(const HostSnapshot& s) {
    if (!s.words[0])
        return "插件格式：VST3\n处理上下文：暂不可用（尚未收到音频处理块）\n"
               "工程位置有效：否\n速度有效：否\n拍号有效：否\n宿主和弦有效：否\n"
               "传输状态：暂无快照";
    const auto flags = s.words[1];
    std::ostringstream out;
    out << "插件格式：VST3\n采样率：" << std::bit_cast<double>(s.words[10]) << " Hz\n速度：";
    if (flags & ProcessContext::kTempoValid) out << std::bit_cast<double>(s.words[3]) << " BPM"; else out << "无效";
    out << "　拍号：";
    if (flags & ProcessContext::kTimeSigValid) out << s.words[5] << '/' << s.words[6]; else out << "无效";
    out << "\n工程采样位置：" << std::bit_cast<std::int64_t>(s.words[2]) << "；工程位置：";
    if (flags & ProcessContext::kProjectTimeMusicValid) out << std::bit_cast<double>(s.words[4]); else out << "无效";
    out << " QN\n宿主和弦有效：" << ((flags & ProcessContext::kChordValid) ? "是" : "否");
    if (flags & ProcessContext::kChordValid)
        out << "\n当前宿主和弦：keyNote=" << std::dec << s.words[7] << " rootNote=" << s.words[8] << " chordMask=0x" << std::hex << s.words[9];
    out << "\n处理上下文有效位\n工程位置有效：" << ((flags & ProcessContext::kProjectTimeMusicValid) ? "是" : "否")
        << "\n速度有效：" << ((flags & ProcessContext::kTempoValid) ? "是" : "否")
        << "\n拍号有效：" << ((flags & ProcessContext::kTimeSigValid) ? "是" : "否")
        << "\n宿主和弦有效：" << ((flags & ProcessContext::kChordValid) ? "是" : "否")
        << "\n传输状态有效位：无（SDK 未单独提供）"
        << "\n传输状态：";
    if (flags & ProcessContext::kPlaying) out << "播放中"; else out << "已停止";
    if (flags & ProcessContext::kCycleActive) out << "，循环开启";
    if (flags & ProcessContext::kRecording) out << "，录音中";
    out << "（SDK ProcessContext 未单独提供传输状态有效位）"
        << "\n标志位 Flags=0x" << std::hex << flags << "（最近音频块；可点击左侧刷新）";
    return out.str();
}

std::string HostContextAdapter::summarize(const HostSnapshot& s) {
    if (!s.words[0]) return "暂无 ProcessContext 快照";
    const auto flags = s.words[1];
    std::ostringstream out;
    out << "QN=";
    if (flags & ProcessContext::kProjectTimeMusicValid) out << std::fixed << std::setprecision(3) << std::bit_cast<double>(s.words[4]);
    else out << "无效";
    out << " | 速度=";
    if (flags & ProcessContext::kTempoValid) out << std::bit_cast<double>(s.words[3]);
    else out << "无效";
    out << " | 拍号=";
    if (flags & ProcessContext::kTimeSigValid) out << s.words[5] << '/' << s.words[6];
    else out << "无效";
    out << " | 宿主和弦=" << ((flags & ProcessContext::kChordValid) ? "有效" : "无效");
    if (flags & ProcessContext::kChordValid)
        out << " key=" << s.words[7] << " root=" << s.words[8] << " mask=0x" << std::hex << s.words[9];
    out << " | 传输=" << ((flags & ProcessContext::kPlaying) ? "播放中" : "已停止");
    if (flags & ProcessContext::kRecording) out << "，录音";
    if (flags & ProcessContext::kCycleActive) out << "，循环";
    return out.str();
}
}
