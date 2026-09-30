#include "MidiHarmonyExtractor.h"
#include "core/ChordPitchSet.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>

namespace harmony::midi {
namespace {
constexpr const char* roots[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
struct Shape {std::string suffix;ChordPitchSet pitches;};
const std::vector<Shape>& shapes() {
    static const auto all=[] {std::vector<Shape> out;for(const char* suffix:{"","m","dim","aug","6","m6","7","maj7","m7","dim7","m7b5","sus2","sus4","9","maj9","m9"}) {
        auto p=chordPitches(std::string("C")+suffix);if(p.quality!=ChordQuality::Unknown)out.push_back({suffix,p});}return out;}();return all;
}
struct Fit {int root{};const Shape* shape{};double confidence{},score{-1e9};};
Fit fit(std::uint16_t mask,int bass) {
    Fit best;
    for(int root=0;root<12;++root) {
      const auto relative=static_cast<std::uint16_t>(((mask>>root)|(mask<<(12-root)))&0xfff);
      for(const auto& shape:shapes()) {
        const auto expected=shape.pitches.intervals;
        if(!(relative&1))continue; // Rootless performance inference is outside this first version.
        const auto missing=expected&~relative, extras=relative&~expected;
        const int common=std::popcount(static_cast<unsigned>(expected&relative));
        const int importantMissing=std::popcount(static_cast<unsigned>(missing&~(1<<7)));
        if(importantMissing>0 || common<3)continue;
        const int extraCount=std::popcount(static_cast<unsigned>(extras));
        const bool noFifth=missing&(1<<7);
        const double score=common*3.-extraCount*4.-(noFifth?1.:0.)+(bass%12==root?.3:0.);
        if(score>best.score)best={root,&shape,std::clamp(1.-extraCount*.18-(noFifth?.16:0.),.25,1.),score};
      }
    }
    return best;
}
}
ExtractionResult extractHarmony(const MidiFile& file,MidiHarmonyExtractionConfig config) {
    ExtractionResult result;
    if(!std::isfinite(config.onsetToleranceQN)||config.onsetToleranceQN<0||config.onsetToleranceQN>1 ||
       !std::isfinite(config.minimumPersistenceQN)||config.minimumPersistenceQN<0||config.minimumPersistenceQN>4 ||file.notes.size()>200000) {
        result.error="invalid extraction config or resource limit";return result;
    }
    for(const auto& n:file.notes)if(n.pitch<0||n.pitch>127||n.channel<0||n.channel>15||n.track<0||n.track>127||
       !std::isfinite(n.startQN)||n.startQN<0||!std::isfinite(n.durationQN)||n.durationQN<=0||!std::isfinite(n.startQN+n.durationQN)) {
        result.error="invalid MIDI note";return result;
    }
    std::map<int,std::vector<MidiNoteEvent>> byTrack;
    for(const auto& note:file.notes)if(!config.ignoreDrums||note.channel!=9)byTrack[note.track].push_back(note);
    double bestScore=-1;
    for(auto& [id,notes]:byTrack) {
        std::sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.startQN<b.startQN;});
        double evidence{},groupStart=-1;int simultaneous{};std::uint16_t groupMask{};
        const auto flush=[&]{if(std::popcount(static_cast<unsigned>(groupMask))>=3)evidence+=simultaneous;};
        for(const auto& n:notes) {if(groupStart<0||n.startQN-groupStart>config.onsetToleranceQN){flush();groupStart=n.startQN;simultaneous=0;groupMask=0;}++simultaneous;groupMask|=1<<(n.pitch%12);}
        flush();
        double held{};std::map<double,int> changes;
        for(const auto& n:notes){held+=std::min(n.durationQN,4.);++changes[n.startQN];--changes[n.startQN+n.durationQN];}
        int voices{};double polyphonic{},occupied{};
        for(auto it=changes.begin();it!=changes.end();++it){voices+=it->second;const auto next=std::next(it);if(next==changes.end())break;
            const double duration=next->first-it->first;if(voices>0)occupied+=duration;if(voices>=3)polyphonic+=duration;}
        const double score=evidence/notes.size()*2+(occupied>0?polyphonic/occupied:0)+held/notes.size()*.1;
        std::string name="Track "+std::to_string(id+1);for(const auto& t:file.tracks)if(t.index==id)name=t.name;
        result.tracks.push_back({id,name,score,notes.size()});
        if(score>bestScore){bestScore=score;result.selectedTrack=id;result.selectedTrackName=name;}
    }
    if(config.selectedTrack) {
        result.selectedTrack=-1;
        for(const auto& t:result.tracks)if(t.track==*config.selectedTrack){result.selectedTrack=t.track;result.selectedTrackName=t.name;}
    }
    if(result.selectedTrack<0){result.error="no pitched note track";return result;}
    if(result.tracks.size()>1)result.warnings.push_back("multiple tracks: selected chord-like track, not merged");
    auto notes=byTrack[result.selectedTrack];double anchor=-1;
    for(auto& n:notes){const double end=n.startQN+n.durationQN;if(anchor<0||n.startQN-anchor>config.onsetToleranceQN)anchor=n.startQN;n.startQN=anchor;n.durationQN=end-anchor;}
    struct Edge {int note{};bool on{};};std::map<double,std::vector<Edge>> edges;
    for(std::size_t i=0;i<notes.size();++i){edges[notes[i].startQN].push_back({static_cast<int>(i),true});edges[notes[i].startQN+notes[i].durationQN].push_back({static_cast<int>(i),false});}
    std::map<int,int> active,sustained;std::optional<Fit> previousFit;double previousEnd{};
    auto warning=[&](const char* text){if(result.warnings.size()<256)result.warnings.push_back(text);};
    for(auto it=edges.begin();it!=edges.end();++it) {
        for(const auto& edge:it->second){const auto& note=notes[edge.note];
            const auto update=[&](auto& set){if(edge.on)++set[note.pitch];else {auto found=set.find(note.pitch);if(found!=set.end()&&!--found->second)set.erase(found);}};
            update(active);if(note.durationQN>=config.minimumPersistenceQN)update(sustained);}
        const auto next=std::next(it);if(next==edges.end())break;
        const double start=it->first,end=next->first,duration=end-start;if(duration<=1e-9)continue;
        std::uint16_t mask{};for(const auto& [p,count]:active)mask|=1<<(p%12);
        std::uint16_t sustainedMask{};for(const auto& [p,count]:sustained)sustainedMask|=1<<(p%12);
        const auto* harmonic=&active;
        if(mask!=sustainedMask&&std::popcount(static_cast<unsigned>(sustainedMask))>=3&&fit(sustainedMask,sustained.begin()->first).shape) {
            mask=sustainedMask;harmonic=&sustained;
        }
        if(std::popcount(static_cast<unsigned>(mask))<3)continue;
        auto match=fit(mask,harmonic->begin()->first);
        // A short extra pitch should not replace an already-supported chord.
        if(duration<config.minimumPersistenceQN&&previousFit&&previousEnd>=start-1e-8) {
            std::uint16_t previousMask{};for(int p=0;p<12;++p)if(previousFit->shape->pitches.intervals&(1<<p))previousMask|=1<<((p+previousFit->root)%12);
            if((mask&previousMask)==previousMask)match=*previousFit;
        }
        if(!match.shape){warning("unrecognized harmonic slice");continue;}
        std::string label=roots[match.root]+match.shape->suffix;
        const int bass=harmonic->begin()->first%12;if(bass!=match.root)label+="/"+std::string(roots[bass]);
        if(match.confidence<.85)warning("uncertain chord fit");
        result.slices.push_back({start,duration,match.confidence,label});
        if(!result.chords.empty()&&result.chords.back().name==label&&std::abs(previousEnd-start)<1e-8)result.chords.back().durationQN=end-result.chords.back().startQN;
        else {
            if(result.chords.size()>=64){result.error="more than 64 extracted chords";result.chords.clear();return result;}
            ChordEvent chord;chord.name=label;chord.root=static_cast<PitchClass>(match.root);chord.bass=static_cast<PitchClass>(bass);
            chord.quality=match.shape->pitches.quality;chord.startQN=start;chord.durationQN=duration;chord.openEnded=false;
            result.chords.push_back(std::move(chord));
        }
        previousFit=match;previousEnd=end;
    }
    if(result.chords.empty()){result.error="no recognizable chord progression";return result;}
    if(std::any_of(result.chords.begin(),result.chords.end(),[](const auto& c){return c.durationQN.value_or(0)>128;})) {
        result.error="chord duration exceeds the existing 128 QN limit";result.chords.clear();return result;
    }
    if(config.openEnded){result.chords.back().durationQN.reset();result.chords.back().openEnded=true;}
    return result;
}
}
