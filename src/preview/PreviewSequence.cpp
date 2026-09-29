#include "PreviewSequence.h"
#include "core/HarmonyAnalysis.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace harmony::preview {
namespace {
bool durationOK(double n) { return std::isfinite(n) && n>0 && n<=128; }
int pc(int n) { return (n%12+12)%12; }
int spelled(std::string_view text) {
    if (text.empty()) return -1;
    int n;
    switch (text.front()) {
        case 'C': n=0; break; case 'D': n=2; break; case 'E': n=4; break;
        case 'F': n=5; break; case 'G': n=7; break; case 'A': n=9; break;
        case 'B': n=11; break; default: return -1;
    }
    if (text.size()>1 && text[1]=='#') ++n;
    if (text.size()>1 && text[1]=='b') --n;
    return pc(n);
}
Chord convert(const ChordEvent& event) {
    auto analysisEvent=event;
    if (const auto slash=analysisEvent.name.find('/'); slash!=std::string::npos)
        analysisEvent.name.resize(slash);
    const auto normalized=normalizeChord(analysisEvent);
    Chord c; c.label=event.name; c.quality=normalized.quality;
    c.root=normalized.root?static_cast<int>(*normalized.root):-1;
    c.bass=normalized.bass?static_cast<int>(*normalized.bass):c.root;
    if (const auto slash=event.name.find('/'); slash!=std::string::npos) {
        const int explicitBass=spelled(std::string_view(event.name).substr(slash+1));
        if (explicitBass>=0) c.bass=explicitBass;
    }
    c.intervals=normalized.intervalMask;
    return c;
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
BuildResult buildSequence(const ImportedProgressionSession& imported,const ContinuationCandidate* candidate,double tempo) {
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
        auto chord=chordFromLabel(e.label,e.quality);
        if (chord.root<0 || chord.intervals==0) { result.error="unsupported continuation chord: "+e.label; result.sequence={}; return result; }
        out.events.push_back({std::move(chord),end,e.durationQN,Segment::Recommended});
        end+=e.durationQN;
    }
    out.totalQN=end;
    return result;
}
} // namespace harmony::preview
