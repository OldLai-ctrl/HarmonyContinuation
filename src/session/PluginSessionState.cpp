#include "PluginSessionState.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace harmony::session {
bool PluginSessionState::pin(const std::string& id) {
    if (id.empty() || pinnedCandidateIds.size() >= 3 ||
        std::find(pinnedCandidateIds.begin(), pinnedCandidateIds.end(), id) != pinnedCandidateIds.end()) return false;
    pinnedCandidateIds.push_back(id); return true;
}
bool PluginSessionState::unpin(const std::string& id) {
    const auto old = pinnedCandidateIds.size();
    std::erase(pinnedCandidateIds, id);
    return pinnedCandidateIds.size() != old;
}
AnalysisContext PluginSessionState::analysisContext() const {
    AnalysisContext context;
    context.forcedKey = forcedKey;
    context.timeSigNumerator = meterNumerator;
    context.timeSigDenominator = meterDenominator;
    return context;
}
RecomputeScope recomputeScope(const PluginSessionState& old,const PluginSessionState& next) noexcept {
    const bool key=old.forcedKey.has_value()!=next.forcedKey.has_value() ||
        (old.forcedKey && next.forcedKey && (old.forcedKey->tonic!=next.forcedKey->tonic || old.forcedKey->mode!=next.forcedKey->mode));
    if (key || old.imported.revision!=next.imported.revision ||
        old.meterNumerator!=next.meterNumerator || old.meterDenominator!=next.meterDenominator) return RecomputeScope::Analysis;
    if (old.style!=next.style || old.intent!=next.intent) return RecomputeScope::Ranking;
    return RecomputeScope::None;
}
namespace {
struct Writer {
    std::string bytes{"HCS1"};
    void u8(std::uint8_t n) { bytes.push_back(static_cast<char>(n)); }
    void u32(std::uint32_t n) { for (int i=0;i<4;++i) u8(static_cast<std::uint8_t>(n >> (8*i))); }
    void u64(std::uint64_t n) { for (int i=0;i<8;++i) u8(static_cast<std::uint8_t>(n >> (8*i))); }
    void dbl(double n) { if (!std::isfinite(n)) throw std::runtime_error("nonfinite session time"); u64(std::bit_cast<std::uint64_t>(n)); }
    void str(const std::string& s) { if (s.size()>8192) throw std::runtime_error("session string too long"); u32(static_cast<std::uint32_t>(s.size())); bytes += s; }
    void optionalString(const std::optional<std::string>& s) { u8(s.has_value()); if (s) str(*s); }
    void optionalInt(const std::optional<std::int32_t>& n) { u8(n.has_value()); if (n) u32(static_cast<std::uint32_t>(*n)); }
};
struct Reader {
    std::string_view bytes; std::size_t at{};
    std::uint8_t u8() { if (at>=bytes.size()) throw std::runtime_error("truncated session state"); return static_cast<std::uint8_t>(bytes[at++]); }
    std::uint32_t u32() { std::uint32_t n{}; for (int i=0;i<4;++i) n |= std::uint32_t(u8()) << (8*i); return n; }
    std::uint64_t u64() { std::uint64_t n{}; for (int i=0;i<8;++i) n |= std::uint64_t(u8()) << (8*i); return n; }
    double dbl() { const auto n=std::bit_cast<double>(u64()); if (!std::isfinite(n)) throw std::runtime_error("invalid session time"); return n; }
    std::string str() { const auto len=u32(); if (len>8192 || len>bytes.size()-at) throw std::runtime_error("invalid session string"); std::string s(bytes.substr(at,len)); at+=len; return s; }
    std::optional<std::string> optionalString() { return u8() ? std::optional<std::string>(str()) : std::nullopt; }
    std::optional<std::int32_t> optionalInt() { return u8() ? std::optional<std::int32_t>(static_cast<std::int32_t>(u32())) : std::nullopt; }
};
}
std::string serialize(const PluginSessionState& state) {
    if (state.imported.events.size()>64 || state.pinnedCandidateIds.size()>3) throw std::runtime_error("session limit exceeded");
    Writer w;
    w.u32(state.factoryLibraryVersion);
    w.u8(static_cast<std::uint8_t>(state.imported.coordinateMode));
    w.u64(state.imported.revision);
    w.u8(state.forcedKey.has_value());
    if (state.forcedKey) { w.u8(static_cast<std::uint8_t>(state.forcedKey->tonic)); w.u8(static_cast<std::uint8_t>(state.forcedKey->mode)); }
    w.u8(state.style.has_value()); if (state.style) w.u32(static_cast<StyleFlags>(*state.style));
    w.u8(state.intent.has_value()); if (state.intent) w.u8(static_cast<std::uint8_t>(*state.intent));
    w.u8(state.skeletonView); w.u8(static_cast<std::uint8_t>(state.tab)); w.u8(state.debugExpanded);
    w.u8(static_cast<std::uint8_t>(state.pinnedCandidateIds.size()));
    for (const auto& id : state.pinnedCandidateIds) w.str(id);
    w.optionalInt(state.meterNumerator); w.optionalInt(state.meterDenominator);
    w.u8(static_cast<std::uint8_t>(state.imported.events.size()));
    for (const auto& chord : state.imported.events) {
        w.str(chord.name); w.dbl(chord.startQN); w.u8(chord.durationQN.has_value()); if (chord.durationQN) w.dbl(*chord.durationQN);
        w.u8(chord.openEnded); w.u8(static_cast<std::uint8_t>(chord.quality)); w.u8(static_cast<std::uint8_t>(chord.source));
        w.optionalInt(chord.keyNoteValue); w.optionalInt(chord.bassNoteValue);
        w.u8(chord.root.has_value()); if (chord.root) w.u8(static_cast<std::uint8_t>(*chord.root));
        w.u8(chord.bass.has_value()); if (chord.bass) w.u8(static_cast<std::uint8_t>(*chord.bass));
        w.optionalString(chord.extensions.mask); w.optionalString(chord.extensions.pitches);
        w.optionalString(chord.extensions.type); w.optionalString(chord.extensions.color);
    }
    if (w.bytes.size()>1024*1024) throw std::runtime_error("session state too large");
    return w.bytes;
}
DecodeResult deserialize(std::string_view bytes) {
    DecodeResult result;
    try {
        if (bytes.size()<4 || bytes.size()>1024*1024 || bytes.substr(0,4)!="HCS1") throw std::runtime_error("unsupported session state");
        Reader r{bytes,4}; auto& s=result.state;
        s.factoryLibraryVersion=r.u32();
        const auto coordinate=r.u8(); if (coordinate>2) throw std::runtime_error("invalid coordinate mode");
        s.imported.coordinateMode=static_cast<TimelineCoordinateMode>(coordinate); s.imported.revision=r.u64();
        if (r.u8()) { const auto tonic=r.u8(), mode=r.u8(); if (tonic>11 || mode>1) throw std::runtime_error("invalid key"); s.forcedKey=KeySignature{static_cast<PitchClass>(tonic),static_cast<Mode>(mode)}; }
        if (r.u8()) { const auto style=r.u32(); if (!style || style>32 || (style&(style-1))) throw std::runtime_error("invalid style"); s.style=static_cast<Style>(style); }
        if (r.u8()) { const auto intent=r.u8(); if (intent<1 || intent>4) throw std::runtime_error("invalid intent"); s.intent=static_cast<PhraseIntent>(intent); }
        s.skeletonView=r.u8()!=0; const auto tab=r.u8(); if (tab>3) throw std::runtime_error("invalid tab"); s.tab=static_cast<Tab>(tab); s.debugExpanded=r.u8()!=0;
        const auto pins=r.u8(); if (pins>3) throw std::runtime_error("too many pins");
        for (int i=0;i<pins;++i) if (!s.pin(r.str())) throw std::runtime_error("invalid pin");
        s.meterNumerator=r.optionalInt(); s.meterDenominator=r.optionalInt();
        const auto count=r.u8(); if (count>64) throw std::runtime_error("too many chords");
        for (int i=0;i<count;++i) {
            ChordEvent c; c.name=r.str(); c.startQN=r.dbl(); if (r.u8()) c.durationQN=r.dbl(); c.openEnded=r.u8()!=0;
            const auto quality=r.u8(), source=r.u8(); if (quality>static_cast<int>(ChordQuality::Augmented) || source>2) throw std::runtime_error("invalid chord enum");
            c.quality=static_cast<ChordQuality>(quality); c.source=static_cast<ChordSource>(source);
            c.keyNoteValue=r.optionalInt(); c.bassNoteValue=r.optionalInt();
            if (r.u8()) { const auto v=r.u8(); if (v>11) throw std::runtime_error("invalid root"); c.root=static_cast<PitchClass>(v); }
            if (r.u8()) { const auto v=r.u8(); if (v>11) throw std::runtime_error("invalid bass"); c.bass=static_cast<PitchClass>(v); }
            c.extensions.mask=r.optionalString(); c.extensions.pitches=r.optionalString(); c.extensions.type=r.optionalString(); c.extensions.color=r.optionalString();
            if (c.name.empty() || (c.durationQN && *c.durationQN<=0)) throw std::runtime_error("invalid chord data");
            s.imported.events.push_back(std::move(c));
        }
        if (r.at!=bytes.size() || (count && coordinate==0) || !std::is_sorted(s.imported.events.begin(),s.imported.events.end(),
            [](const auto& a,const auto& b){return a.startQN<b.startQN;})) throw std::runtime_error("invalid session order");
    } catch (const std::exception& e) { result.state={}; result.error=e.what(); }
    return result;
}
} // namespace harmony::session
