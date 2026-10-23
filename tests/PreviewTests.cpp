#include "preview/OfflinePreviewRenderer.h"
#include "preview/PreviewExchange.h"
#include "preview/PreviewTransport.h"
#include "preview/WindowsPreview.h"
#include "session/PluginSessionState.h"
#include "session/ProductServices.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace { std::atomic<std::size_t> allocations{}; }
void* operator new(std::size_t size) {
    allocations.fetch_add(1,std::memory_order_relaxed);
    if (void* value=std::malloc(size)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value,std::size_t) noexcept { std::free(value); }
namespace {
using namespace harmony;
int checks{};
void check(bool yes,const char* message) { if (!yes) throw std::runtime_error(message); ++checks; }
bool has(const preview::Voicing& v,int pc) {
    for (int i=0;i<v.upperCount;++i) if (v.upper[i]%12==pc) return true;
    return false;
}
bool sameVoicing(const preview::Voicing& a,const preview::Voicing& b) {
    return a.bass==b.bass&&a.upperCount==b.upperCount&&a.melodySatisfied==b.melodySatisfied&&
        std::equal(a.upper.begin(),a.upper.begin()+a.upperCount,b.upper.begin());
}
bool near(double a,double b,double tolerance=1e-8) { return std::abs(a-b)<=tolerance; }
bool sameAudio(const float* a,const float* b,std::size_t count,float tolerance=1e-5f) {
    for(std::size_t i=0;i<count;++i)if(std::abs(a[i]-b[i])>tolerance)return false;
    return true;
}
ChordEvent event(const char* name,double start,std::optional<double> duration) {
    ChordEvent e; e.name=name; e.startQN=start; e.durationQN=duration; e.openEnded=!duration;
    return e;
}
preview::TransportRequest transportRequest(std::uint64_t serial,std::uint64_t clipId,std::uint64_t seekId,
    preview::PlaybackState state,double position,const preview::PreparedPreview& clip) {
    preview::TransportRequest request;
    request.serial=serial;request.clipId=clipId;request.seekId=seekId;request.state=state;
    request.positionQN=position;request.clip=clip;return request;
}
preview::TransportRequest patternedRequest(std::uint64_t serial,const preview::PreparedPreview& base) {
    auto request=transportRequest(serial,serial,serial*7+3,
        static_cast<preview::PlaybackState>(serial%3),static_cast<double>(serial%128)/4.0,base);
    request.clip.tempoBPM=20.0+static_cast<double>(serial%381);
    request.clip.count=static_cast<std::uint32_t>(preview::maximumPreviewChords);
    request.clip.startSample=static_cast<std::int64_t>(serial%50000);
    request.clip.endSample=request.clip.startSample+96000;
    for(std::size_t i=0;i<preview::maximumPreviewChords;++i) {
        auto& chord=request.clip.chords[i];
        const auto marker=serial+i;
        chord.start=request.clip.startSample;chord.end=request.clip.endSample;
        chord.voicing.bass=static_cast<int>(marker%48);
        chord.voicing.upperCount=2;
        chord.voicing.upper[0]=48+static_cast<int>(marker%48);
        chord.voicing.upper[1]=chord.voicing.upper[0]+32;
        chord.voicing.melodySatisfied=(marker&1)!=0;
    }
    return request;
}
bool patternedRequestIsCoherent(const preview::TransportRequest& request,std::uint64_t previousSerial) {
    const auto serial=request.serial;
    if(serial<=previousSerial||request.clipId!=serial||request.seekId!=serial*7+3||
       request.state!=static_cast<preview::PlaybackState>(serial%3)||
       request.positionQN!=static_cast<double>(serial%128)/4.0||!preview::validTransportRequest(request))return false;
    const auto& clip=request.clip;
    const auto start=static_cast<std::int64_t>(serial%50000);
    if(clip.version!=1||clip.count!=preview::maximumPreviewChords||
       clip.tempoBPM!=20.0+static_cast<double>(serial%381)||
       clip.startSample!=start||clip.endSample!=start+96000)return false;
    for(std::size_t i=0;i<preview::maximumPreviewChords;++i) {
        const auto& chord=clip.chords[i];
        const auto marker=serial+i;
        const int note=48+static_cast<int>(marker%48);
        if(chord.start!=start||chord.end!=start+96000||
           chord.voicing.bass!=static_cast<int>(marker%48)||chord.voicing.upperCount!=2||
           chord.voicing.upper[0]!=note||chord.voicing.upper[1]!=note+32||
           chord.voicing.melodySatisfied!=((marker&1)!=0))return false;
    }
    return true;
}
void previewExchangeContract(const preview::PreparedPreview& firstClip,const preview::PreparedPreview& latestClip) {
    preview::PreviewMailbox mailbox;
    auto older=transportRequest(1,101,1,preview::PlaybackState::Stopped,0,firstClip);
    auto middle=transportRequest(2,202,2,preview::PlaybackState::Paused,1,firstClip);
    auto latest=transportRequest(3,303,3,preview::PlaybackState::Playing,2,latestClip);
    preview::TransportRequest received;
    preview::PreviewReceiver receiver;receiver.transport.prepare(48000);
    const auto beforeRealtimeExchange=allocations.load(std::memory_order_relaxed);
    const bool publicationsAccepted=mailbox.publish(older)&&mailbox.publish(middle)&&mailbox.publish(latest);
    const bool receivedLatest=mailbox.take(received)&&received.serial==latest.serial&&
        received.clipId==latest.clipId&&received.seekId==latest.seekId&&
        received.positionQN==latest.positionQN&&received.state==latest.state&&
        received.clip.tempoBPM==latestClip.tempoBPM;
    const bool receiverApplied=receiver.apply(received)&&receiver.serial==3&&receiver.clipId==303&&
        receiver.transport.state()==preview::PlaybackState::Playing&&
        near(receiver.transport.positionQN(),2.0)&&receiver.transport.tempoBPM()==latestClip.tempoBPM;
    const auto afterRealtimeExchange=allocations.load(std::memory_order_relaxed);
    check(publicationsAccepted&&receivedLatest&&receiverApplied&&
        beforeRealtimeExchange==afterRealtimeExchange,
        "latest mailbox snapshot applies load, play and seek without heap allocation");
    std::array<float,960> left{},right{};
    receiver.transport.process(left.data(),right.data(),static_cast<int>(left.size()));
    const auto beforePause=receiver.transport.positionQN();
    const double blockQN=double(left.size())/48000.*latestClip.tempoBPM/60.;
    check(near(beforePause,2.+blockQN,1e-8),"receiver transport advances at the clip tempo before pause");
    auto paused=transportRequest(4,303,3,preview::PlaybackState::Paused,beforePause,latestClip);
    check(receiver.apply(paused)&&receiver.transport.state()==preview::PlaybackState::Paused,
        "receiver accepts pause at the current transport position");
    const auto pausedPosition=receiver.transport.positionQN();
    receiver.transport.process(left.data(),right.data(),static_cast<int>(left.size()));
    check(near(receiver.transport.positionQN(),pausedPosition,1e-10),
        "receiver pause holds sample position while processing silent blocks");
    auto resumed=transportRequest(5,303,3,preview::PlaybackState::Playing,pausedPosition,latestClip);
    check(receiver.apply(resumed)&&receiver.transport.state()==preview::PlaybackState::Playing,
        "receiver resumes the paused transport");
    receiver.transport.process(left.data(),right.data(),static_cast<int>(left.size()));
    const auto resumedPosition=receiver.transport.positionQN();
    check(near(resumedPosition,pausedPosition+blockQN,1e-8),"receiver resume advances from the paused sample at clip tempo");
    auto repeatedPlay=transportRequest(6,303,3,preview::PlaybackState::Playing,1,latestClip);
    check(receiver.apply(repeatedPlay)&&near(receiver.transport.positionQN(),resumedPosition),
        "repeated playing snapshot without a new seek does not restart or seek");
    auto seek=transportRequest(7,303,4,preview::PlaybackState::Playing,3,latestClip);
    check(receiver.apply(seek)&&receiver.transport.state()==preview::PlaybackState::Playing&&
        near(receiver.transport.positionQN(),3.0),"new seek identity applies its QN position");

    preview::PreviewMailbox concurrentMailbox;
    std::atomic<bool> producerDone{};
    std::atomic<bool> corrupted{};
    std::uint64_t finalSerial{};
    constexpr std::uint64_t packetCount=20000;
    std::thread consumer([&] {
        std::uint64_t previous{};
        const auto observe=[&](const preview::TransportRequest& packet) {
            if(!patternedRequestIsCoherent(packet,previous))corrupted.store(true,std::memory_order_relaxed);
            previous=packet.serial;finalSerial=previous;
        };
        bool finished{};
        while(!finished) {
            preview::TransportRequest packet;
            if(concurrentMailbox.take(packet))observe(packet);
            else if(producerDone.load(std::memory_order_acquire)) {
                if(concurrentMailbox.take(packet))observe(packet);
                else finished=true;
            } else std::this_thread::yield();
        }
    });
    const auto publishDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    bool publisherStalled{};
    for(std::uint64_t serial=1;serial<=packetCount&&!publisherStalled;++serial) {
        const auto packet=patternedRequest(serial,firstClip);
        while(!concurrentMailbox.publish(packet)) {
            if(std::chrono::steady_clock::now()>=publishDeadline){publisherStalled=true;break;}
            std::this_thread::yield();
        }
    }
    producerDone.store(true,std::memory_order_release);
    consumer.join();
    check(!publisherStalled&&!corrupted.load(std::memory_order_relaxed)&&finalSerial==packetCount,
        "single-producer three-slot mailbox preserves complete concurrent payloads and final state");
}
template<class Predicate>
preview::TransportFeedback waitWindowsFeedback(preview::WindowsPreview& output,std::uint64_t serial,
    Predicate predicate,std::chrono::milliseconds timeout=std::chrono::milliseconds(6000)) {
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(std::chrono::steady_clock::now()<deadline) {
        const auto status=output.feedback();
        if(status.serial==serial) {
            if(status.error==preview::AudioError::DeviceUnavailable)
                throw std::runtime_error("Windows preview returned DeviceUnavailable");
            if(status.error!=preview::AudioError::None)
                throw std::runtime_error("Windows preview returned a non-success audio error");
            if(predicate(status))return status;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("timed out waiting for Windows preview device feedback");
}
void windowsDeviceContract() {
    preview::Sequence sequence;sequence.tempoBPM=120;sequence.totalQN=8;
    sequence.events.push_back({preview::chordFromLabel("Cmaj7"),0,8,preview::Segment::Existing,{}});
    const auto prepared=preview::preparePreview(sequence);
    check(static_cast<bool>(prepared),"Windows device fixture prepares");

    preview::WindowsPreview output;
    auto request=transportRequest(1,1001,1,preview::PlaybackState::Playing,0,prepared.clip);
    check(output.submit(request),"Windows device playback request accepted");
    auto playing=waitWindowsFeedback(output,request.serial,[](const auto& s) {
        return s.state==preview::PlaybackState::Playing&&s.outputActive&&s.positionQN>0.10;
    });
    check(playing.clipId==request.clipId&&playing.sampleRate==48000,
        "real Windows device reports active playback and advancing waveOut position");

    request.serial=2;request.state=preview::PlaybackState::Paused;
    check(output.submit(request),"Windows device pause request accepted");
    auto paused=waitWindowsFeedback(output,request.serial,[](const auto& s) {
        return s.state==preview::PlaybackState::Paused&&s.outputActive;
    });
    const auto pausedPosition=paused.positionQN;
    request.serial=3;
    check(output.submit(request),"repeated Windows pause-position query accepted");
    const auto pausedAgain=waitWindowsFeedback(output,request.serial,[](const auto& s) {
        return s.state==preview::PlaybackState::Paused&&s.outputActive;
    });
    check(std::abs(pausedAgain.positionQN-pausedPosition)<0.02,
        "waveOutGetPosition remains stable while the device is paused");

    request.serial=4;request.seekId=2;request.positionQN=2.0;
    check(output.submit(request),"Windows device seek request accepted");
    const auto sought=waitWindowsFeedback(output,request.serial,[](const auto& s) {
        return s.state==preview::PlaybackState::Paused&&s.outputActive&&std::abs(s.positionQN-2.0)<0.03;
    });
    check(std::abs(sought.positionQN-2.0)<0.03,"paused Windows seek reports the requested middle QN");

    request.serial=5;request.state=preview::PlaybackState::Playing;
    check(output.submit(request),"Windows device resume request accepted");
    const auto resumed=waitWindowsFeedback(output,request.serial,[](const auto& s) {
        return s.state==preview::PlaybackState::Playing&&s.outputActive&&s.positionQN>2.10;
    });
    check(resumed.positionQN>2.10,"Windows resume advances from the middle seek");

    request.serial=6;request.state=preview::PlaybackState::Stopped;
    check(output.submit(request),"Windows device stop request accepted");
    const auto stopped=waitWindowsFeedback(output,request.serial,[](const auto& s) {
        return s.state==preview::PlaybackState::Stopped&&!s.outputActive;
    });
    check(near(stopped.positionQN,0.0),"Windows stop acknowledges a stopped transport and closed output");

    preview::WindowsPreview first,second;
    auto firstRequest=transportRequest(1,2001,1,preview::PlaybackState::Playing,0,prepared.clip);
    auto secondRequest=transportRequest(1,3001,1,preview::PlaybackState::Playing,2,prepared.clip);
    check(first.submit(firstRequest)&&second.submit(secondRequest),"independent Windows instances accept playback");
    const auto firstPlaying=waitWindowsFeedback(first,1,[](const auto& s) {
        return s.state==preview::PlaybackState::Playing&&s.outputActive&&s.positionQN>0.10;
    });
    const auto secondPlaying=waitWindowsFeedback(second,1,[](const auto& s) {
        return s.state==preview::PlaybackState::Playing&&s.outputActive&&s.positionQN>2.10;
    });
    check(firstPlaying.clipId==2001&&secondPlaying.clipId==3001&&
        secondPlaying.positionQN-firstPlaying.positionQN>1.0,
        "simultaneous Windows instances retain independent clip and position state");
    firstRequest.serial=2;firstRequest.state=preview::PlaybackState::Paused;
    check(first.submit(firstRequest),"first Windows instance pause accepted");
    const auto firstPaused=waitWindowsFeedback(first,2,[](const auto& s) {
        return s.state==preview::PlaybackState::Paused&&s.outputActive;
    });
    const auto secondBefore=second.feedback().positionQN;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto firstAfter=first.feedback();
    const auto secondAfter=second.feedback();
    check(firstAfter.serial==2&&firstAfter.state==preview::PlaybackState::Paused&&
        std::abs(firstAfter.positionQN-firstPaused.positionQN)<0.02&&
        secondAfter.serial==1&&secondAfter.state==preview::PlaybackState::Playing&&
        secondAfter.positionQN>secondBefore+0.05,
        "pausing one Windows instance leaves the other device transport advancing");
    firstRequest.serial=3;firstRequest.state=preview::PlaybackState::Stopped;
    check(first.submit(firstRequest),"first Windows instance stop accepted");
    waitWindowsFeedback(first,3,[](const auto& s) {
        return s.state==preview::PlaybackState::Stopped&&!s.outputActive;
    });
    secondRequest.serial=2;secondRequest.state=preview::PlaybackState::Stopped;
    check(second.submit(secondRequest),"second Windows instance stop accepted");
    waitWindowsFeedback(second,2,[](const auto& s) {
        return s.state==preview::PlaybackState::Stopped&&!s.outputActive;
    });
 }
void windowsStressContract(){
    preview::Sequence sequence;sequence.tempoBPM=120;sequence.totalQN=16;
    for(int i=0;i<4;++i)sequence.events.push_back({preview::chordFromLabel(i%2?"Fm7":"Cmaj7"),double(i*4),4,preview::Segment::Existing,{}});
    const auto prepared=preview::preparePreview(sequence);check(bool(prepared),"stress fixture prepares");
    preview::WindowsPreview output;auto request=transportRequest(1,1,1,preview::PlaybackState::Playing,0,prepared.clip);
    std::size_t transitions{};const auto began=std::chrono::steady_clock::now();
    while(std::chrono::steady_clock::now()-began<std::chrono::seconds(120)){
        ++request.serial;++request.seekId;request.positionQN=double(transitions%12);request.state=preview::PlaybackState::Playing;
        check(output.submit(request),"stress play/seek submitted");
        const auto playing=waitWindowsFeedback(output,request.serial,[&](const auto& f){return f.state==preview::PlaybackState::Playing&&f.outputActive&&f.positionQN>=request.positionQN;});
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        ++request.serial;request.state=preview::PlaybackState::Paused;check(output.submit(request),"stress pause submitted");
        const auto paused=waitWindowsFeedback(output,request.serial,[](const auto& f){return f.state==preview::PlaybackState::Paused&&f.outputActive;});
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        check(std::abs(output.feedback().positionQN-paused.positionQN)<.02,"stress pause position stable");
        ++request.serial;request.state=preview::PlaybackState::Playing;check(output.submit(request),"stress resume submitted");
        waitWindowsFeedback(output,request.serial,[&](const auto& f){return f.state==preview::PlaybackState::Playing&&f.positionQN>paused.positionQN;});
        std::this_thread::sleep_for(std::chrono::milliseconds(60));++transitions;
    }
    ++request.serial;request.state=preview::PlaybackState::Stopped;check(output.submit(request),"stress stop");
    waitWindowsFeedback(output,request.serial,[](const auto& f){return f.state==preview::PlaybackState::Stopped&&!f.outputActive;});
    std::cout<<"Windows native stress 120s PASS cycles="<<transitions<<" (seek/pause/resume, real device feedback; no listening/dropout claim)\n";
}
}
int main(int argc,char** argv) {
    if(argc==2&&std::string_view(argv[1])=="--windows-stress") {
        try {windowsStressContract();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    if(argc==2&&std::string_view(argv[1])=="--windows-device") {
        try { windowsDeviceContract();std::cout<<"Windows preview device contract passed\n";return 0; }
        catch(const std::exception& e) { std::cerr<<"Windows preview device contract failed: "<<e.what()<<'\n';return 1; }
    }
    if(argc!=1) { std::cerr<<"Usage: PreviewTests [--windows-device]\n";return 2; }
    try {
        ImportedProgressionSession imported;
        check(imported.replace({event("C",32,4),event("Am",36,2),event("Dm",38,{})},TimelineCoordinateMode::AbsoluteProjectQN),"import");
        ContinuationCandidate candidate; candidate.id="example"; candidate.suggestedCurrentChordDurationQN=2;
        candidate.continuation={{"G",2},{"C",4}};
        auto built=preview::buildSequence(imported,&candidate,120);
        check(built && built.sequence.events.size()==5 && built.sequence.totalQN==14,"sequence size");
        const auto completeSequence=built.sequence;
        const auto customTempo=preview::buildSequence(imported,&candidate,90);
        bool sameQN=customTempo&&customTempo.sequence.events.size()==completeSequence.events.size()&&
            customTempo.sequence.totalQN==completeSequence.totalQN;
        if (sameQN) for (std::size_t i=0;i<completeSequence.events.size();++i)
            sameQN=sameQN&&customTempo.sequence.events[i].startQN==completeSequence.events[i].startQN&&
                customTempo.sequence.events[i].durationQN==completeSequence.events[i].durationQN;
        check(customTempo&&customTempo.sequence.tempoBPM==90&&sameQN,"custom BPM changes preview tempo without changing QN");
        for (int i=0;i<5;++i) check(built.sequence.events[i].startQN==std::array<double,5>{0,4,6,8,10}[i],"relative start");
        for (int i=0;i<5;++i) check(built.sequence.events[i].durationQN==std::array<double,5>{4,2,2,2,4}[i],"durations");
        check(built.sequence.recommendationBoundary==3 && built.sequence.events[2].segment==preview::Segment::CurrentOpen,"boundary");
        const auto fullRange=preview::previewRange(completeSequence,preview::PreviewScope::Full);
        const auto suffixRange=preview::previewRange(completeSequence,preview::PreviewScope::Suffix);
        check(fullRange.startQN==0&&fullRange.endQN==14&&suffixRange.startQN==8&&suffixRange.endQN==14,
            "suffix begins at recommendation boundary");
        check(imported.replace({event("C",32,8),event("Am",40,3),event("A7",43,1),event("Dm",44,{})},TimelineCoordinateMode::AbsoluteProjectQN),"reimport");
        built=preview::buildSequence(imported,&candidate,120);
        check(built && built.sequence.events[0].durationQN==8 && built.sequence.events[1].durationQN==3 &&
            built.sequence.events[2].durationQN==1,"preserve existing");
        candidate.suggestedCurrentChordDurationQN=std::numeric_limits<double>::quiet_NaN();
        built=preview::buildSequence(imported,&candidate,120);
        check(built && built.sequence.events[3].durationQN==3,"median fallback");
        check(imported.replace({event("C",0,4),event("Am",4,2),event("G",6,4),event("C",10,{})},TimelineCoordinateMode::RelativeToSelection),"median fixture");
        built=preview::buildSequence(imported,nullptr,120);
        check(built && built.sequence.events.back().durationQN==4,"4/2/4 fallback");
        check(imported.replace({event("Dm",100,{})},TimelineCoordinateMode::AbsoluteProjectQN),"open only import");
        built=preview::buildSequence(imported,nullptr,0);
        check(built && built.sequence.events[0].durationQN==4 && built.sequence.tempoBPM==120,"default fallback");
        const auto noRecommendationRange=preview::previewRange(built.sequence,preview::PreviewScope::Suffix);
        check(noRecommendationRange.startQN==0&&noRecommendationRange.endQN==4,
            "suffix without a recommendation keeps the available phrase");
        check(std::abs(preview::qnToSeconds(1,120)-0.5)<1e-12 && preview::qnToSeconds(1,60)==1 &&
            preview::qnToSeconds(1,240)==0.25,"tempo seconds");
        for (double rate:{44100.0,48000.0,96000.0}) check(preview::qnToSamples(1,120,rate)==rate/2,"sample timing");
        check(preview::sanitizeTempo(1)==20 && preview::sanitizeTempo(999)==400 &&
            preview::sanitizeTempo(std::numeric_limits<double>::infinity())==120,"tempo sanitize");
        preview::ChordVoicer voicer;
        auto v=voicer.voice(preview::chordFromLabel("C")); check(has(v,4),"C third");
        v=voicer.voice(preview::chordFromLabel("Am")); check(has(v,0),"Am third");
        v=voicer.voice(preview::chordFromLabel("G7")); check(has(v,11)&&has(v,5),"G7 guide tones");
        v=voicer.voice(preview::chordFromLabel("Cmaj7")); check(has(v,4)&&has(v,11),"Cmaj7 identity");
        v=voicer.voice(preview::chordFromLabel("Bm7b5")); check(has(v,2)&&has(v,5)&&has(v,9),"half diminished identity");
        v=voicer.voice(preview::chordFromLabel("C#dim7")); check(has(v,4)&&has(v,7)&&has(v,10),"dim7 identity");
        v=voicer.voice(preview::chordFromLabel("C/E")); check(v.bass%12==4,"slash bass");
        for (const char* label:{"C6","C9","Cmaj9","Cm9","G7b9","G7#9","G13","Csus2","Csus4","Caug","Cdim","Cdim7","Cm7b5"}) {
            auto c=preview::chordFromLabel(label); check(c.root>=0 && c.intervals!=0,"extension parsed");
            auto voiced=voicer.voice(c); check(voiced.upperCount>=3 && voiced.upperCount<=4,"extension voiced");
        }
        voicer.reset(); int totalMovement{}; preview::Voicing prev{}; bool first=true;
        for (const char* label:{"Cmaj7","Am7","Dm7","G7","Cmaj7"}) {
            const auto now=voicer.voice(preview::chordFromLabel(label));
            if (!first) for (int i=0;i<std::min(prev.upperCount,now.upperCount);++i) totalMovement+=std::abs(now.upper[i]-prev.upper[i]);
            first=false; prev=now;
        }
        check(totalMovement<70,"voice leading movement");
        built=preview::buildSequence(imported,nullptr,120);
        auto audio=preview::renderOffline(built.sequence,44100);
        check(!audio.left.empty() && audio.peak>0.01 && audio.peak<=0.95 && audio.peakVoices<=preview::PreviewSynth::maxVoices,"audio safety");
        const auto completeAudio=preview::renderOffline(completeSequence,8000,preview::PreviewScope::Full);
        const auto suffixAudio=preview::renderOffline(completeSequence,8000,preview::PreviewScope::Suffix);
        const auto prefixFrames=static_cast<std::size_t>(preview::qnToSamples(suffixRange.startQN,completeSequence.tempoBPM,8000));
        check(prefixFrames<completeAudio.left.size()&&suffixAudio.left.size()==completeAudio.left.size()-prefixFrames,
            "suffix audio omits the input prefix");
        check(std::equal(suffixAudio.left.begin(),suffixAudio.left.end(),completeAudio.left.begin()+prefixFrames)&&
            std::equal(suffixAudio.right.begin(),suffixAudio.right.end(),completeAudio.right.begin()+prefixFrames),
            "suffix is cropped after full-sequence voicing");
        const auto again=preview::renderOffline(built.sequence,44100);
        check(audio.left==again.left,"deterministic audio");
        preview::PreviewSynth synth; synth.prepare(48000); check(synth.start(built.sequence),"scheduler start");
        float l[128]{},r[128]{};
        const auto beforeProcess=allocations.load(std::memory_order_relaxed);
        synth.process(l,r,128);
        const auto afterProcess=allocations.load(std::memory_order_relaxed);
        check(beforeProcess==afterProcess,"process has no heap allocation");
        check(synth.currentSample()==128 && synth.playing(),"sample progress");
        synth.stop(); check(!synth.playing(),"scheduler stop");
        synth.process(l,r,128); check(synth.replace(built.sequence) && synth.currentSample()==0,"replace resets timeline");
        for (int i=0;i<100000 && !synth.finished();++i) synth.process(l,r,128);
        check(synth.finished(),"play once then finish");

        auto preparedFull=preview::preparePreview(completeSequence);
        auto preparedSuffix=preview::preparePreview(completeSequence,preview::PreviewScope::Suffix);
        check(preparedFull&&preparedFull.clip.count==5&&preparedFull.clip.startSample==0&&
            preparedFull.clip.endSample==preview::qnToSamples(14,120,preview::previewReferenceRate),
            "prepared full preview bounds");
        check(preparedSuffix&&preparedSuffix.clip.count==2&&
            preparedSuffix.clip.startSample==preview::qnToSamples(8,120,preview::previewReferenceRate)&&
            preparedSuffix.clip.endSample==preparedFull.clip.endSample&&
            sameVoicing(preparedSuffix.clip.chords[0].voicing,preparedFull.clip.chords[3].voicing),
            "suffix keeps full-route voicing and clips only its range");

        preview::PreviewTransport referenceTransport;
        referenceTransport.prepare(48000);
        check(referenceTransport.load(preparedFull.clip),"prepared load");
        std::array<float,192> uninterruptedLeft{},uninterruptedRight{};
        referenceTransport.play();
        referenceTransport.process(uninterruptedLeft.data(),uninterruptedRight.data(),
            static_cast<int>(uninterruptedLeft.size()));
        check(referenceTransport.state()==preview::PlaybackState::Playing&&
            near(referenceTransport.positionQN(),192.0/48000.0*2.0),"transport sample position");

        preview::PreviewTransport pausedTransport;
        pausedTransport.prepare(48000);
        check(pausedTransport.load(preparedFull.clip),"pause fixture load");
        pausedTransport.play();
        std::array<float,128> prefixLeft{},prefixRight{};
        pausedTransport.process(prefixLeft.data(),prefixRight.data(),128);
        const auto pausedPosition=pausedTransport.positionQN();
        pausedTransport.pause();
        std::array<float,64> pauseLeft{},pauseRight{};
        pauseLeft.fill(1.0f);pauseRight.fill(1.0f);
        pausedTransport.process(pauseLeft.data(),pauseRight.data(),64);
        const bool pausedSilent=std::all_of(pauseLeft.begin(),pauseLeft.end(),[](float v){return v==0.0f;})&&
            std::all_of(pauseRight.begin(),pauseRight.end(),[](float v){return v==0.0f;});
        check(pausedTransport.state()==preview::PlaybackState::Paused&&pausedPosition==pausedTransport.positionQN()&&pausedSilent,
            "pause preserves voices and position while outputting silence");
        pausedTransport.play();
        std::array<float,64> resumedLeft{},resumedRight{};
        pausedTransport.process(resumedLeft.data(),resumedRight.data(),64);
        check(pausedTransport.state()==preview::PlaybackState::Playing&&
            std::equal(resumedLeft.begin(),resumedLeft.end(),uninterruptedLeft.begin()+128)&&
            std::equal(resumedRight.begin(),resumedRight.end(),uninterruptedRight.begin()+128),
            "resume continues the same sample and voice state");

        preview::PreviewTransport seekTransport;
        seekTransport.prepare(48000);
        check(seekTransport.load(preparedFull.clip),"seek fixture load");
        seekTransport.play();
        std::vector<float> baselineLeft(static_cast<std::size_t>(preview::qnToSamples(4,120,48000)+32));
        std::vector<float> baselineRight(baselineLeft.size());
        preview::PreviewTransport baselineTransport;
        baselineTransport.prepare(48000);baselineTransport.load(preparedFull.clip);baselineTransport.play();
        baselineTransport.process(baselineLeft.data(),baselineRight.data(),static_cast<int>(baselineLeft.size()));
        seekTransport.seekQN(0.0);
        std::array<float,16> seekLeft{},seekRight{};
        seekTransport.process(seekLeft.data(),seekRight.data(),16);
        check(std::equal(seekLeft.begin(),seekLeft.end(),baselineLeft.begin())&&
            std::equal(seekRight.begin(),seekRight.end(),baselineRight.begin()),"seek to start rebuilds active voicing");
        seekTransport.seekQN(1.0);
        seekTransport.process(seekLeft.data(),seekRight.data(),16);
        check(sameAudio(seekLeft.data(),baselineLeft.data()+24000,seekLeft.size())&&
            sameAudio(seekRight.data(),baselineRight.data()+24000,seekRight.size()),"mid-chord seek reconstructs voice phase");
        seekTransport.seekQN(4.0);
        seekTransport.process(seekLeft.data(),seekRight.data(),16);
        check(sameAudio(seekLeft.data(),baselineLeft.data()+96000,seekLeft.size())&&
            sameAudio(seekRight.data(),baselineRight.data()+96000,seekRight.size()),"event-boundary seek starts the boundary chord");
        seekTransport.seekQN(-10.0);
        check(seekTransport.state()==preview::PlaybackState::Playing&&near(seekTransport.positionQN(),0.0),
            "seek before range clamps to start and preserves playing state");
        seekTransport.seekQN(100.0);
        check(seekTransport.state()==preview::PlaybackState::Stopped&&
            near(seekTransport.positionQN(),seekTransport.endQN()),"seek past end clamps and stops at 100 percent");
        seekLeft.fill(1.0f);seekRight.fill(1.0f);
        seekTransport.process(seekLeft.data(),seekRight.data(),16);
        check(std::all_of(seekLeft.begin(),seekLeft.end(),[](float v){return v==0.0f;}),"end seek is silent");
        seekTransport.play();
        check(seekTransport.state()==preview::PlaybackState::Playing&&near(seekTransport.positionQN(),0.0),
            "play after stop restarts at range start");

        auto slowSequence=completeSequence;slowSequence.tempoBPM=90.0;
        const auto slowSuffix=preview::preparePreview(slowSequence,preview::PreviewScope::Suffix);
        preview::PreviewTransport rateTransport;
        rateTransport.prepare(44100);
        check(slowSuffix&&rateTransport.load(slowSuffix.clip)&&near(rateTransport.startQN(),8.0)&&
            near(rateTransport.endQN(),14.0)&&rateTransport.tempoBPM()==90.0,"48 kHz reference maps to 44.1 kHz QN");
        rateTransport.play();
        std::vector<float> rateLeft(14700),rateRight(14700);
        rateTransport.process(rateLeft.data(),rateRight.data(),14700);
        check(near(rateTransport.positionQN(),8.5)&&rateTransport.state()==preview::PlaybackState::Playing,
            "different rate advances the expected QN");
        rateTransport.seekQN(9.0);
        check(near(rateTransport.positionQN(),9.0)&&rateTransport.state()==preview::PlaybackState::Playing,
            "seek preserves playing state at a different rate");
        rateTransport.seekQN(0.0);
        check(near(rateTransport.positionQN(),8.0)&&rateTransport.state()==preview::PlaybackState::Playing,
            "seek before a suffix clamps to its nonzero range start");
        rateTransport.seekQN(9.0);
        rateTransport.pause();rateTransport.seekQN(10.0);
        check(near(rateTransport.positionQN(),10.0)&&rateTransport.state()==preview::PlaybackState::Paused,
            "seek preserves paused state");
        rateTransport.stop();
        check(rateTransport.state()==preview::PlaybackState::Stopped&&near(rateTransport.positionQN(),8.0),
            "stop resets to the scoped start");
        preview::PreviewTransport slowFullTransport;
        slowFullTransport.prepare(44100);
        const auto slowFull=preview::preparePreview(slowSequence);
        check(slowFull&&slowFullTransport.load(slowFull.clip)&&near(slowFullTransport.startQN(),0.0)&&
            near(slowFullTransport.endQN(),14.0),"full preview keeps QN at a different BPM and rate");
        previewExchangeContract(preparedFull.clip,slowFull.clip);
        preview::Sequence cmaj7Sequence;cmaj7Sequence.tempoBPM=120;cmaj7Sequence.totalQN=4;
        cmaj7Sequence.events.push_back({preview::chordFromLabel("Cmaj7"),0,4,preview::Segment::Existing,{}});
        const auto cmaj7Prepared=preview::preparePreview(cmaj7Sequence);
        check(cmaj7Prepared&&preview::validPreparedPreview(cmaj7Prepared.clip),
            "Cmaj7 prepared preview accepts existing ChordVoicer output order");

        preview::Sequence shortSequence;shortSequence.tempoBPM=120.0;shortSequence.totalQN=2.0;
        shortSequence.events.push_back({preview::chordFromLabel("C"),0.0,2.0,preview::Segment::Existing,{}});
        const auto shortClip=preview::preparePreview(shortSequence);
        preview::PreviewTransport naturalEndTransport;
        naturalEndTransport.prepare(48000);
        check(shortClip&&naturalEndTransport.load(shortClip.clip),"natural-end fixture load");
        naturalEndTransport.play();
        std::array<float,512> naturalLeft{},naturalRight{};
        while(naturalEndTransport.state()==preview::PlaybackState::Playing)
            naturalEndTransport.process(naturalLeft.data(),naturalRight.data(),static_cast<int>(naturalLeft.size()));
        check(naturalEndTransport.state()==preview::PlaybackState::Stopped&&
            near(naturalEndTransport.positionQN(),2.0),"natural end stops at the exact scoped end");
        naturalLeft.fill(1.0f);naturalRight.fill(1.0f);
        naturalEndTransport.process(naturalLeft.data(),naturalRight.data(),static_cast<int>(naturalLeft.size()));
        check(std::all_of(naturalLeft.begin(),naturalLeft.end(),[](float v){return v==0.0f;}),
            "natural end leaves subsequent blocks silent");

        preview::PreparedPreview invalidClip=preparedFull.clip;
        invalidClip.version=2;check(!preview::validPreparedPreview(invalidClip),"prepared version checked");
        invalidClip=preparedFull.clip;invalidClip.count=preview::maximumPreviewChords+1;
        check(!preview::validPreparedPreview(invalidClip),"prepared count checked");
        invalidClip=preparedFull.clip;invalidClip.chords[0].voicing.upperCount=13;
        check(!preview::validPreparedPreview(invalidClip),"prepared upper voice count checked");
        invalidClip=preparedFull.clip;invalidClip.chords[0].voicing.bass=128;
        check(!preview::validPreparedPreview(invalidClip),"prepared bass range checked");
        invalidClip=preparedFull.clip;invalidClip.chords[0].voicing.upper[0]=128;
        check(!preview::validPreparedPreview(invalidClip),"prepared upper note range checked");
        invalidClip=preparedFull.clip;invalidClip.endSample=invalidClip.startSample+
            static_cast<std::int64_t>(601.0*preview::previewReferenceRate);
        check(!preview::validPreparedPreview(invalidClip),"prepared duration limit checked");
        invalidClip=preparedFull.clip;invalidClip.startSample=-1;
        check(!preview::validPreparedPreview(invalidClip),"prepared negative range start rejected");
        invalidClip=preparedFull.clip;invalidClip.endSample=invalidClip.startSample;
        check(!preview::validPreparedPreview(invalidClip),"prepared empty range rejected");
        invalidClip=preparedFull.clip;invalidClip.chords[0].end=invalidClip.endSample+1;
        check(!preview::validPreparedPreview(invalidClip),"prepared chord beyond range rejected");
        preview::PreviewTransport invalidRateTransport;invalidRateTransport.prepare(1000.0);
        check(!invalidRateTransport.load(preparedFull.clip),"unsupported playback rate rejected");
        auto tooManySequence=completeSequence;
        tooManySequence.events.resize(preview::maximumPreviewChords+1,tooManySequence.events.front());
        check(!preview::preparePreview(tooManySequence),"overlength event count rejected");
        preview::Sequence tooLongSequence;tooLongSequence.tempoBPM=120.0;tooLongSequence.totalQN=1202.0;
        tooLongSequence.events.push_back({preview::chordFromLabel("C"),0.0,1202.0,preview::Segment::Existing,{}});
        check(!preview::preparePreview(tooLongSequence),"overlength duration rejected");

        preview::PreviewTransport allocationTransport;
        allocationTransport.prepare(48000);
        const auto beforeRealtimeControls=allocations.load(std::memory_order_relaxed);
        const bool loadedWithoutAllocation=allocationTransport.load(preparedFull.clip);
        allocationTransport.play();allocationTransport.pause();allocationTransport.seekQN(2.0);
        allocationTransport.play();allocationTransport.process(l,r,128);allocationTransport.stop();
        const auto afterRealtimeControls=allocations.load(std::memory_order_relaxed);
        check(loadedWithoutAllocation&&beforeRealtimeControls==afterRealtimeControls,
            "load, seek, controls and process use no heap allocation");

        candidate.suggestedCurrentChordDurationQN=2;
        session::PluginSessionState state; state.pin("old",session::continuationFingerprint(candidate));
        const auto bytes=session::serialize(state);
        auto decoded=session::deserialize(bytes);
        check(decoded && decoded.state.schemaVersion==session::PluginSessionState::currentSchemaVersion &&
            decoded.state.pinnedFingerprints.size()==1,"current state roundtrip");
        auto future=bytes; future[4]=static_cast<char>(session::PluginSessionState::currentSchemaVersion+1);
        check(session::deserialize(future).error=="UnsupportedVersion","future version rejected");
        auto version4=bytes;version4[3]='4';version4[4]=4;version4.resize(version4.size()-6);
        check(session::deserialize(version4)&&session::deserialize(version4).state.uiZoomPercent==100,
            "v4 harmony preferences migration");
        auto version3=version4; version3[3]='3'; version3[4]=3; version3.resize(version3.size()-2);
        check(session::deserialize(version3) && session::deserialize(version3).state.locale==session::Locale::ZhCN,
            "v3 preference migration");
        auto old=version4; old[3]='2'; old[4]=2; old.resize(old.size()-10);
        const auto migrated=session::deserialize(old);
        check(migrated&&migrated.state.editorWidth==1100&&migrated.state.editorHeight==900,"v2 size migration");
        state.editorWidth=800;state.editorHeight=780;
        const auto compactState=session::deserialize(session::serialize(state));
        check(compactState&&compactState.state.editorWidth==800&&compactState.state.editorHeight==780,"A3 editor size roundtrip");
        state.editorWidth=1500; state.editorHeight=850;
        const auto sized=session::deserialize(session::serialize(state));
        check(sized&&sized.state.editorWidth==1500&&sized.state.editorHeight==850,"editor size roundtrip");
        RecommendationSet set; candidate.id="new"; set.groups[0].push_back(candidate);
        decoded.state.resolvePins(set);
        check(decoded.state.pinnedCandidateIds.size()==1 && decoded.state.pinnedCandidateIds[0]=="new","pin stable match");
        set.groups[0][0].continuation[0].label="F"; decoded.state.resolvePins(set);
        check(decoded.state.pinnedCandidateIds.empty(),"stale pin dropped");
        std::cout << checks << " preview checks passed\n";
    } catch (const std::exception& e) { std::cerr << "PreviewTests: " << e.what() << '\n'; return 1; }
}
