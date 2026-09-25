#include "preview/OfflinePreviewRenderer.h"
#include "session/PluginSessionState.h"
#include "session/ProductServices.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
#include <stdexcept>

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
ChordEvent event(const char* name,double start,std::optional<double> duration) {
    ChordEvent e; e.name=name; e.startQN=start; e.durationQN=duration; e.openEnded=!duration;
    return e;
}
}
int main() {
    try {
        ImportedProgressionSession imported;
        check(imported.replace({event("C",32,4),event("Am",36,2),event("Dm",38,{})},TimelineCoordinateMode::AbsoluteProjectQN),"import");
        ContinuationCandidate candidate; candidate.id="example"; candidate.suggestedCurrentChordDurationQN=2;
        candidate.continuation={{"G",2},{"C",4}};
        auto built=preview::buildSequence(imported,&candidate,120);
        check(built && built.sequence.events.size()==5 && built.sequence.totalQN==14,"sequence size");
        for (int i=0;i<5;++i) check(built.sequence.events[i].startQN==std::array<double,5>{0,4,6,8,10}[i],"relative start");
        for (int i=0;i<5;++i) check(built.sequence.events[i].durationQN==std::array<double,5>{4,2,2,2,4}[i],"durations");
        check(built.sequence.recommendationBoundary==3 && built.sequence.events[2].segment==preview::Segment::CurrentOpen,"boundary");
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
        candidate.suggestedCurrentChordDurationQN=2;
        session::PluginSessionState state; state.pin("old",session::continuationFingerprint(candidate));
        const auto bytes=session::serialize(state);
        auto decoded=session::deserialize(bytes);
        check(decoded && decoded.state.schemaVersion==3 && decoded.state.pinnedFingerprints.size()==1,"state v3 roundtrip");
        auto future=bytes; future[4]=4;
        check(session::deserialize(future).error=="UnsupportedVersion","future version rejected");
        auto old=bytes; old[3]='2'; old[4]=2; old.resize(old.size()-8);
        const auto migrated=session::deserialize(old);
        check(migrated&&migrated.state.editorWidth==1100&&migrated.state.editorHeight==900,"v2 size migration");
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
