#include "StandardMidiFileWriter.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace harmony::midi {
namespace {
using Bytes=std::vector<std::uint8_t>;
void be16(Bytes& b,std::uint16_t value) { b.push_back(static_cast<std::uint8_t>(value>>8)); b.push_back(static_cast<std::uint8_t>(value)); }
void be32(Bytes& b,std::uint32_t value) { be16(b,static_cast<std::uint16_t>(value>>16)); be16(b,static_cast<std::uint16_t>(value)); }
void vlq(Bytes& b,std::uint32_t value) {
    std::uint8_t buffer[4]{}; int n{};
    buffer[n++]=static_cast<std::uint8_t>(value&0x7f);
    while ((value>>=7)!=0) buffer[n++]=static_cast<std::uint8_t>((value&0x7f)|0x80);
    while (n) b.push_back(buffer[--n]);
}
void meta(Bytes& b,std::uint8_t type,const Bytes& data) {
    b.push_back(0xff); b.push_back(type); vlq(b,static_cast<std::uint32_t>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
}
Bytes textBytes(const std::string& s) { return Bytes(s.begin(),s.end()); }
struct Item { std::int64_t tick{}; int order{}; Bytes bytes; };
void emitTrack(Bytes& output,std::vector<Item>& items,std::int64_t totalTick) {
    std::stable_sort(items.begin(),items.end(),[](const auto& a,const auto& b){
        if (a.tick!=b.tick) return a.tick<b.tick;
        return a.order<b.order;
    });
    Bytes track; std::int64_t previous{};
    for (const auto& e:items) {
        const auto delta=e.tick-previous;
        if (delta<0||delta>0x0fffffff) throw std::runtime_error("MIDI delta out of range");
        vlq(track,static_cast<std::uint32_t>(delta));
        track.insert(track.end(),e.bytes.begin(),e.bytes.end());
        previous=e.tick;
    }
    const auto ending=std::max(previous,totalTick);
    if (ending-previous>0x0fffffff) throw std::runtime_error("MIDI end out of range");
    vlq(track,static_cast<std::uint32_t>(ending-previous));
    meta(track,0x2f,{});
    if (track.size()>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("MIDI track too large");
    output.insert(output.end(),{'M','T','r','k'});
    be32(output,static_cast<std::uint32_t>(track.size()));
    output.insert(output.end(),track.begin(),track.end());
}
std::optional<std::int8_t> keyFlatsSharps(KeySignature key) {
    constexpr std::int8_t major[]{0,-5,2,-3,4,-1,-6,1,-4,3,-2,5};
    constexpr std::int8_t minor[]{-3,4,-1,-6,1,-4,3,-2,5,0,-5,2};
    const int tonic=static_cast<int>(key.tonic);
    if (tonic<0||tonic>11) return std::nullopt;
    return key.mode==Mode::Minor?minor[tonic]:major[tonic];
}
}
WriteResult writeToMemory(const ExportSequence& s,ExportConfig config) {
    WriteResult result;
    try {
        if (config.ppq<24||config.ppq>9600||s.notes.empty()||s.chordCount==0||
            !std::isfinite(s.tempoBPM)||s.tempoBPM<20||s.tempoBPM>400||
            s.meter.numerator<1||s.meter.numerator>32||s.meter.denominator<1||s.meter.denominator>32||
            (s.meter.denominator&(s.meter.denominator-1))) throw std::runtime_error("invalid MIDI metadata");
        const auto total=qnToTicks(s.totalQN,config.ppq);
        if (total<0||total>0x0fffffff) throw std::runtime_error("invalid MIDI length");
        std::vector<Item> conductor,notes;
        const auto micros=static_cast<std::uint32_t>(std::lround(60000000.0/s.tempoBPM));
        Bytes tempo; meta(tempo,0x51,{static_cast<std::uint8_t>(micros>>16),static_cast<std::uint8_t>(micros>>8),static_cast<std::uint8_t>(micros)});
        conductor.push_back({0,0,std::move(tempo)});
        std::uint8_t exponent{}; for (int n=s.meter.denominator;n>1;n>>=1) ++exponent;
        Bytes meter; meta(meter,0x58,{static_cast<std::uint8_t>(s.meter.numerator),exponent,24,8});
        conductor.push_back({0,1,std::move(meter)});
        if (s.key) {
            const auto sf=keyFlatsSharps(*s.key);
            if (!sf) throw std::runtime_error("invalid key signature");
            Bytes key; meta(key,0x59,{static_cast<std::uint8_t>(*sf),static_cast<std::uint8_t>(s.key->mode==Mode::Minor)});
            conductor.push_back({0,2,std::move(key)});
        }
        if (s.intent) {
            Bytes intent; meta(intent,0x01,textBytes(std::string("HarmonyContinuation Intent: ")+intentName(*s.intent)));
            conductor.push_back({0,3,std::move(intent)});
        }
        for (const auto& marker:s.markers) {
            const auto tick=qnToTicks(marker.qn,config.ppq);
            if (tick<0||tick>total||marker.text.size()>4096) throw std::runtime_error("invalid MIDI marker");
            Bytes bytes; meta(bytes,marker.type==MarkerType::RecommendedStart?0x06:0x01,textBytes(marker.text));
            conductor.push_back({tick,marker.type==MarkerType::RecommendedStart?4:5,std::move(bytes)});
        }
        for (const auto& note:s.notes) {
            const auto start=qnToTicks(note.startQN,config.ppq);
            const auto end=qnToTicks(note.startQN+note.durationQN,config.ppq);
            if (note.midiNote<0||note.midiNote>127||note.velocity<1||note.velocity>127||note.channel>15||
                !std::isfinite(note.durationQN)||note.durationQN<=0||start<0||end<=start||end>total)
                throw std::runtime_error("invalid MIDI note");
            notes.push_back({end,0,{static_cast<std::uint8_t>(0x80|note.channel),static_cast<std::uint8_t>(note.midiNote),0}});
            notes.push_back({start,1,{static_cast<std::uint8_t>(0x90|note.channel),static_cast<std::uint8_t>(note.midiNote),note.velocity}});
        }
        auto& output=result.bytes;
        output.insert(output.end(),{'M','T','h','d'}); be32(output,6); be16(output,1); be16(output,2); be16(output,config.ppq);
        emitTrack(output,conductor,total); emitTrack(output,notes,total);
    } catch (const std::exception& e) { result.bytes.clear(); result.error=e.what(); }
    return result;
}
bool writeToFile(const std::vector<std::uint8_t>& bytes,const std::filesystem::path& path,std::string& error) {
    if (bytes.size()<22) { error="empty MIDI payload"; return false; }
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if (!out) { error="cannot open MIDI output"; return false; }
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if (!out) { error="MIDI write failed"; return false; }
    return true;
}
MidiClipPayload makePayload(const ExportSequence& sequence,std::string filename,ExportConfig config) {
    MidiClipPayload payload;
    const auto written=writeToMemory(sequence,config);
    if (!written) return payload;
    payload.smfBytes=written.bytes;
    payload.suggestedFilename=std::move(filename);
    payload.chordCount=sequence.chordCount; payload.noteCount=sequence.notes.size();
    if (sequence.boundaryQN) payload.boundaryTick=qnToTicks(*sequence.boundaryQN,config.ppq);
    return payload;
}
} // namespace harmony::midi
