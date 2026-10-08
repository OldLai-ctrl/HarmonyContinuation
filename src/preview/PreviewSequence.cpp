#include "PreviewSequence.h"
#include "core/HarmonyAnalysis.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace harmony::preview {
namespace {
bool durationOK(double n) { return std::isfinite(n) && n>0 && n<=128; }
Chord convert(const ChordEvent& event) {
    return chordPitches(event);
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
Chord chordFromLabel(const std::string& label,ChordQuality hint) {
    ChordEvent event; event.name=label; event.quality=hint;
    return convert(event);
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
