#include "PreviewSequence.h"
#include "core/HarmonyAnalysis.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace harmony::preview {
namespace {
bool durationOK(double n) { return std::isfinite(n) && n>0 && n<=128; }
Chord convert(const ChordEvent& event) {
    return chordPitches(event);
}
bool templateProgression(const ProgressionTemplate& item,KeySignature key,ImportedProgressionSession& imported,
    std::string& error) {
    if(item.full.empty()||item.full.size()>64){error="invalid library progression";return false;}
    Progression chords;double at{};
    if(!item.rawChords.empty()) {
        if(!item.rawChordKey||item.rawChords.size()!=item.full.size()){error="invalid stored chord data";return false;}
        chords=item.rawChords;
        const int shift=(static_cast<int>(key.tonic)-static_cast<int>(item.rawChordKey->tonic)+12)%12;
        if(shift)for(auto& e:chords) {
            const auto pitch=chordPitches(e);
            if(pitch.root<0||pitch.bass<0){error="unsupported stored chord";return false;}
            constexpr const char* notes[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
            e.root=static_cast<PitchClass>((pitch.root+shift)%12);e.bass=static_cast<PitchClass>((pitch.bass+shift)%12);
            e.quality=pitch.quality;
            const auto slash=e.name.find('/');
            std::size_t rootEnd=(!e.name.empty()&&e.name[0]>='A'&&e.name[0]<='G')?1:0;
            while(rootEnd<e.name.size()&&(e.name[rootEnd]=='#'||e.name[rootEnd]=='b'))++rootEnd;
            const auto suffix=rootEnd?e.name.substr(rootEnd,slash==std::string::npos?slash:slash-rootEnd):std::string{};
            e.name=notes[static_cast<int>(*e.root)]+suffix;
            if(pitch.bass!=pitch.root||slash!=std::string::npos){e.name+="/";e.name+=notes[static_cast<int>(*e.bass)];}
            e.keyNoteValue.reset();e.bassNoteValue.reset();e.extensions.pitches.reset();
            std::ostringstream mask;mask<<"0x"<<std::hex<<(pitch.intervals|pitch.colorMask);e.extensions.mask=mask.str();
        }
    } else for(const auto& e:item.full) {
        const double duration=e.durationQN.value_or(defaultOpenQN);
        if(!durationOK(duration)){error="invalid library duration";return false;}
        const auto concrete=realizeContinuation(e,key,duration);
        ChordEvent chord=concrete.harmonicData.value_or(ChordEvent{});
        chord.name=concrete.label;chord.quality=concrete.quality;
        chord.startQN=at;chord.durationQN=duration;chord.openEnded=false;
        chords.push_back(std::move(chord));at+=duration;
    }
    if(!imported.replace(std::move(chords),TimelineCoordinateMode::RelativeToSelection)) {
        error="invalid library timeline";return false;
    }
    const double anchor=imported.events.front().startQN;
    for(auto& event:imported.events)event.startQN-=anchor;
    return true;
}
}
double sanitizeTempo(double bpm) noexcept {
    return std::isfinite(bpm)&&bpm>0?std::clamp(bpm,minimumTempoBPM,maximumTempoBPM):defaultTempoBPM;
}
double qnToSeconds(double qn,double bpm) noexcept {
    return std::isfinite(qn)&&qn>=0?qn*60.0/sanitizeTempo(bpm):0.0;
}
std::int64_t qnToSamples(double qn,double bpm,double sampleRate) noexcept {
    if (!std::isfinite(sampleRate)||sampleRate<=0) return 0;
    const auto samples=qnToSeconds(qn,bpm)*sampleRate;
    if (!std::isfinite(samples)||samples>static_cast<double>(std::numeric_limits<std::int64_t>::max())) return 0;
    return static_cast<std::int64_t>(std::llround(samples));
}
PreviewRange previewRange(const Sequence& sequence,PreviewScope scope) noexcept {
    PreviewRange range; range.endQN=sequence.totalQN;
    for (const auto& event:sequence.events)
        if (std::isfinite(event.startQN)&&std::isfinite(event.durationQN))
            range.endQN=std::max(range.endQN,event.startQN+event.durationQN);
    if (scope==PreviewScope::Suffix && sequence.recommendationBoundary<sequence.events.size()) {
        const auto start=sequence.events[sequence.recommendationBoundary].startQN;
        if (std::isfinite(start)&&start>=0&&start<range.endQN) range.startQN=start;
    }
    return range;
}
Chord chordFromLabel(const std::string& label,ChordQuality hint) {
    ChordEvent event; event.name=label; event.quality=hint;
    return convert(event);
}
BuildResult buildTemplateSequence(const ProgressionTemplate& item,KeySignature key,double tempo) {
    ImportedProgressionSession imported;std::string error;
    if(!templateProgression(item,key,imported,error))return {{},error};
    return buildSequence(imported,nullptr,tempo);
}
SourceRouteResult sourceRoute(const ImportedProgressionSession& input,const ContinuationCandidate& candidate,
    const ProgressionTemplate& item) {
    SourceRouteResult result;result.candidate=candidate;
    result.meterNumerator=item.meterNumerator;result.meterDenominator=item.meterDenominator;
    const auto boundary=candidate.continuationStart;
    if(item.id!=candidate.primaryTemplate||boundary==0||boundary>item.full.size()||
       candidate.matchStart>candidate.matchEnd||candidate.matchEnd+1!=boundary) {
        result.error="source representative no longer matches";return result;
    }
    ImportedProgressionSession full;
    if(!templateProgression(item,candidate.key,full,result.error))return result;
    // Resolve OPEN data using the same library preview rules, before splitting the route.
    const auto realized=buildSequence(full,nullptr,defaultTempoBPM);
    if(!realized){result.error=realized.error;return result;}
    for(std::size_t i=0;i<full.events.size();++i) {
        full.events[i].durationQN=realized.sequence.events[i].durationQN;
        full.events[i].openEnded=false;
    }
    result.imported=full;
    result.imported.events.resize(boundary);
    auto& projected=result.candidate;
    projected.continuation.clear();projected.suggestedCurrentChordDurationQN.reset();
    projected.rhythmScale=1;projected.sourceAnchorQN=0;
    auto append=[&](std::size_t index) {
        const auto& event=full.events[index];
        auto concrete=realizeContinuation(item.full[index],candidate.key,*event.durationQN);
        concrete.harmonicData=event;concrete.label=event.name;concrete.quality=event.quality;
        projected.continuation.push_back(std::move(concrete));
    };
    for(std::size_t i=boundary;i<full.events.size();++i)append(i);
    // Preserve the ranked route's finite loop return, now using the author's rhythm.
    if(item.loopable) {
        if(candidate.continuation.size()>64){result.error="too many continuation chords";return result;}
        const auto remaining=full.events.size()-boundary;
        if(candidate.continuation.size()<remaining){result.error="invalid loop representative";return result;}
        for(std::size_t i=0;i<candidate.continuation.size()-remaining;++i)append(i%full.events.size());
    }
    if(!projected.constraints.melody.empty()) {
        const auto original=buildSequence(input,&candidate,defaultTempoBPM);
        if(!original){result.error=original.error;return result;}
        double inputEnd{};
        for(const auto& event:original.sequence.events)
            if(event.segment!=Segment::Recommended)inputEnd=std::max(inputEnd,event.startQN+event.durationQN);
        const double anchor=input.events.front().startQN;
        const double begin=full.events[candidate.matchStart].startQN;
        const auto& last=full.events[boundary-1];
        const double end=last.startQN+*last.durationQN;
        if(inputEnd<=0||end<=begin||candidate.rhythmScale<=0){result.error="invalid constraint alignment";return result;}
        const auto mapTime=[&](double time) {
            const double relative=time-anchor;
            return relative<=inputEnd?begin+relative*(end-begin)/inputEnd:
                end+(relative-inputEnd)/candidate.rhythmScale;
        };
        for(auto& melody:projected.constraints.melody) {
            const double endQN=mapTime(melody.startQN+melody.durationQN);
            melody.startQN=mapTime(melody.startQN);melody.durationQN=endQN-melody.startQN;
        }
    }
    const auto checked=buildSequence(result.imported,&projected,defaultTempoBPM);
    if(!checked)result.error=checked.error;
    return result;
}
BuildResult buildSequence(const ImportedProgressionSession& imported,const ContinuationCandidate* candidate,double tempo,
    const HarmonyConstraintSet& explicitConstraints) {
    BuildResult result; auto& out=result.sequence;
    out.tempoBPM=sanitizeTempo(tempo);
    if (candidate) out.sourceRecommendation=candidate->id;
    const auto& source=imported.events;
    if (source.empty()) { result.error="empty progression"; return result; }
    if (source.size()>64 || (candidate && candidate->continuation.size()>64)) { result.error="too many chords"; return result; }
    const auto anchor=source.front().startQN;
    if (!std::isfinite(anchor)) { result.error="invalid start"; return result; }
    std::vector<double> known, structural;
    for (std::size_t i=0;i<source.size();++i) {
        const auto& e=source[i];
        if (!std::isfinite(e.startQN) || (i && e.startQN<source[i-1].startQN)) { result.error="invalid event order"; return result; }
        if (e.durationQN && !e.openEnded && !durationOK(*e.durationQN)) { result.error="invalid existing duration"; return result; }
    }
    const auto analysis=analyzeHarmony(source);
    for (std::size_t i=0;i<source.size();++i) {
        const auto& e=source[i];
        if (!e.durationQN || e.openEnded) continue;
        known.push_back(*e.durationQN);
        if (i<analysis.full.size() && hasRole(analysis.full[i].roles,Role::Structural))
            structural.push_back(*e.durationQN);
    }
    double fallback=defaultOpenQN;
    if (structural.empty()) structural=std::move(known);
    if (!structural.empty()) {
        std::sort(structural.begin(),structural.end());
        const auto mid=structural.size()/2;
        fallback=structural.size()%2?structural[mid]:(structural[mid-1]+structural[mid])/2;
    }
    double end{};
    for (std::size_t i=0;i<source.size();++i) {
        const auto& e=source[i];
        double duration{};
        if (e.openEnded || !e.durationQN) {
            const double suggested=(i+1==source.size() && candidate && candidate->suggestedCurrentChordDurationQN)
                ?*candidate->suggestedCurrentChordDurationQN:0;
            duration=durationOK(suggested)?suggested:fallback;
        } else duration=*e.durationQN;
        if (!durationOK(duration)) { result.error="invalid existing duration"; result.sequence={}; return result; }
        auto chord=convert(e);
        if (chord.root<0 || chord.intervals==0) { result.error="unsupported chord: "+e.name; result.sequence={}; return result; }
        const double start=e.startQN-anchor;
        if (start<0 || !std::isfinite(start)) { result.error="invalid relative start"; result.sequence={}; return result; }
        out.events.push_back({std::move(chord),start,duration,e.openEnded?Segment::CurrentOpen:Segment::Existing});
        end=std::max(end,start+duration);
    }
    out.recommendationBoundary=out.events.size();
    if (candidate) for (const auto& e:candidate->continuation) {
        if (!durationOK(e.durationQN)) { result.error="invalid continuation duration"; result.sequence={}; return result; }
        auto chord=e.harmonicData?convert(*e.harmonicData):chordFromLabel(e.label,e.quality);
        if (chord.root<0 || chord.intervals==0) { result.error="unsupported continuation chord: "+e.label; result.sequence={}; return result; }
        out.events.push_back({std::move(chord),end,e.durationQN,Segment::Recommended});
        end+=e.durationQN;
    }
    out.totalQN=end;
    const auto& constraints=candidate?candidate->constraints:explicitConstraints;
    if(!validConstraints(constraints)){result.error="invalid melody constraints";return result;}
    if(!constraints.melody.empty()) {
        const auto heard=candidate?continuationProgression(imported.events,*candidate):imported.events;
        if(!evaluateMelody(heard,constraints).hardSatisfied) {
            result.error="strict melody constraint cannot be satisfied";result.sequence={};return result;
        }
        std::vector<Event> split;
        for(const auto& event:out.events) {
            std::vector<double> cuts{event.startQN,event.startQN+event.durationQN};
            for(const auto& melody:constraints.melody) {
                for(const double edge:{melody.startQN-anchor,melody.startQN+melody.durationQN-anchor})
                    if(edge>cuts.front()+1e-7&&edge<event.startQN+event.durationQN-1e-7)cuts.push_back(edge);
            }
            std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
            for(std::size_t i=1;i<cuts.size();++i) {
                auto next=event;next.startQN=cuts[i-1];next.durationQN=cuts[i]-cuts[i-1];
                const MelodyConstraint* chosen=nullptr;
                for(const auto& melody:constraints.melody)
                    if(melody.role==ConstraintRole::TopVoice&&melody.startQN<anchor+cuts[i]-1e-7&&
                       melody.startQN+melody.durationQN>anchor+cuts[i-1]+1e-7&&
                       classifyMelody(event.chord,melody.pitchClass)!=MelodyRelation::Conflict&&
                       (!chosen||melody.strictness==ConstraintStrictness::Hard))chosen=&melody;
                if(chosen)next.topVoice=melodyTopPitch(*chosen);
                split.push_back(std::move(next));
            }
        }
        out.events=std::move(split);
        out.recommendationBoundary=static_cast<std::size_t>(std::count_if(out.events.begin(),out.events.end(),
            [](const auto& event){return event.segment!=Segment::Recommended;}));
    }
    return result;
}
} // namespace harmony::preview
