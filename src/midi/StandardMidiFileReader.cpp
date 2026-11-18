#include "StandardMidiFileReader.h"
#include <algorithm>
#include <array>
#include <deque>
#include <fstream>
#include <stdexcept>

namespace harmony::midi {
namespace {
constexpr std::size_t maxBytes=32*1024*1024, maxNotes=200000, maxEvents=2000000;
struct Failure:std::runtime_error { ReadStatus status; Failure(ReadStatus s,const char* text):std::runtime_error(text),status(s){} };
struct Cursor {
    std::span<const std::uint8_t> data; std::size_t at{};
    std::uint8_t byte() {if(at>=data.size())throw Failure(ReadStatus::Malformed,"truncated MIDI");return data[at++];}
    std::uint32_t be(int n) {std::uint32_t value{};while(n--)value=(value<<8)|byte();return value;}
    std::uint32_t vlq() {std::uint32_t value{};for(int i=0;i<4;++i){auto b=byte();value=(value<<7)|(b&127);if(!(b&128))return value;}throw Failure(ReadStatus::Malformed,"invalid VLQ");}
    std::span<const std::uint8_t> take(std::size_t n) {if(n>data.size()-at)throw Failure(ReadStatus::Malformed,"truncated MIDI chunk");auto s=data.subspan(at,n);at+=n;return s;}
};
void warn(MidiFile& f,const char* text) {if(f.warnings.size()<256)f.warnings.emplace_back(text);}
}
ReadResult readFromMemory(std::span<const std::uint8_t> bytes) {
    ReadResult result;
    try {
        if(bytes.size()>maxBytes)throw Failure(ReadStatus::ResourceLimit,"MIDI exceeds 32 MiB");
        Cursor in{bytes};
        if(in.be(4)!=0x4d546864)throw Failure(ReadStatus::Malformed,"missing MThd");
        const auto length=in.be(4);if(length<6)throw Failure(ReadStatus::Malformed,"invalid MIDI header");
        Cursor header{in.take(length)};
        auto& file=result.file;file.format=header.be(2);const auto tracks=header.be(2);const auto division=header.be(2);
        if(file.format>1 || (division&0x8000))throw Failure(ReadStatus::Unsupported,"only SMF format 0/1 with PPQ is supported");
        if(!division || !tracks || (file.format==0&&tracks!=1))throw Failure(ReadStatus::Malformed,"invalid MIDI header values");
        if(tracks>128)throw Failure(ReadStatus::ResourceLimit,"too many MIDI tracks");
        file.ppq=division;std::size_t eventCount{};
        for(std::uint32_t t=0;t<tracks;) {
            const auto id=in.be(4);const auto chunk=in.take(in.be(4));
            if(id!=0x4d54726b){warn(file,"unknown chunk skipped");continue;}
            Cursor track{chunk};MidiTrack info{static_cast<int>(t),"Track "+std::to_string(t+1)};
            struct On {std::uint64_t tick{};int velocity{};};
            std::array<std::deque<On>,16*128> active;
            std::uint64_t tick{};std::uint8_t running{};bool ended{};
            auto finish=[&](int channel,int pitch,On on,std::uint64_t end) {
                if(end<=on.tick){warn(file,"zero-length note ignored");return;}
                if(file.notes.size()>=maxNotes)throw Failure(ReadStatus::ResourceLimit,"too many MIDI notes");
                file.notes.push_back({pitch,on.velocity,channel,static_cast<int>(t),double(on.tick)/file.ppq,double(end-on.tick)/file.ppq});
                ++info.notes;if(channel==9)++info.percussionNotes;
            };
            while(track.at<track.data.size()&&!ended) {
                if(++eventCount>maxEvents)throw Failure(ReadStatus::ResourceLimit,"too many MIDI events");
                tick+=track.vlq();if(tick>std::uint64_t(file.ppq)*10000000)throw Failure(ReadStatus::ResourceLimit,"MIDI timeline too long");
                auto status=track.byte();bool firstData=status<128;std::uint8_t first=status;
                if(firstData){if(!running)throw Failure(ReadStatus::Malformed,"running status without channel status");status=running;}
                if(status<0xf0) {
                    running=status;const auto type=status&0xf0;const auto channel=status&15;
                    const auto a=firstData?first:track.byte();const auto b=(type==0xc0||type==0xd0)?0:track.byte();
                    if(a>127||b>127)throw Failure(ReadStatus::Malformed,"invalid channel data");
                    auto& queue=active[channel*128+a];
                    if(type==0x90&&b){if(queue.size()>=256)throw Failure(ReadStatus::ResourceLimit,"too many overlapping same-pitch notes");queue.push_back({tick,b});}
                    else if(type==0x80||(type==0x90&&!b)) {
                        if(queue.empty())warn(file,"unmatched note-off ignored");
                        else {const auto on=queue.front();queue.pop_front();finish(channel,a,on,tick);}
                    }
                } else {
                    running=0;
                    if(status==0xff) {
                        const auto type=track.byte();if(type>127)throw Failure(ReadStatus::Malformed,"invalid meta event");
                        auto data=track.take(track.vlq());
                        if(type==0x2f){if(!data.empty())throw Failure(ReadStatus::Malformed,"invalid End Of Track");ended=true;}
                        else if(type==3){info.name.assign(data.begin(),data.begin()+std::min<std::size_t>(256,data.size()));for(auto& c:info.name)if(static_cast<unsigned char>(c)<32)c=' ';}
                        else if(type==0x51){if(data.size()!=3)throw Failure(ReadStatus::Malformed,"invalid tempo");const auto us=(data[0]<<16)|(data[1]<<8)|data[2];if(!us)throw Failure(ReadStatus::Malformed,"zero tempo");file.tempos.push_back({double(tick)/file.ppq,60000000./us});}
                        else if(type==0x58){if(data.size()!=4||!data[0]||data[1]>7)throw Failure(ReadStatus::Malformed,"invalid time signature");file.meters.push_back({double(tick)/file.ppq,data[0],1<<data[1]});}
                    } else if(status==0xf0||status==0xf7)track.take(track.vlq());
                    else throw Failure(ReadStatus::Malformed,"illegal SMF status");
                }
            }
            if(!ended)warn(file,"missing End Of Track");
            for(int c=0;c<16;++c)for(int p=0;p<128;++p)for(const auto& on:active[c*128+p]){warn(file,"unclosed note ended at track boundary");finish(c,p,on,tick);}
            file.totalQN=std::max(file.totalQN,double(tick)/file.ppq);file.tracks.push_back(std::move(info));++t;
        }
        std::stable_sort(file.notes.begin(),file.notes.end(),[](const auto& a,const auto& b){return a.startQN<b.startQN;});
        std::stable_sort(file.tempos.begin(),file.tempos.end(),[](const auto& a,const auto& b){return a.qn<b.qn;});
        std::stable_sort(file.meters.begin(),file.meters.end(),[](const auto& a,const auto& b){return a.qn<b.qn;});
    } catch(const Failure& e){result.status=e.status;result.error=e.what();result.file={};}
    catch(const std::exception& e){result.status=ReadStatus::Malformed;result.error=e.what();result.file={};}
    return result;
}
ReadResult readFromFile(const std::filesystem::path& path) {
    try {
        const auto size=std::filesystem::file_size(path);
        if(size>maxBytes)return {{},ReadStatus::ResourceLimit,"MIDI exceeds 32 MiB"};
        std::ifstream in(path,std::ios::binary);if(!in)return {{},ReadStatus::FileError,"cannot open MIDI file"};
        std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
        if(size&&!in.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size)))return {{},ReadStatus::FileError,"cannot read MIDI file"};
        return readFromMemory(data);
    } catch(const std::exception& e){return {{},ReadStatus::FileError,e.what()};}
}
}
