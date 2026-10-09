#include "HarmonyColorAnalyzer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace harmony::color {
namespace {
constexpr double tau=2*std::numbers::pi;
double wrap(double a) { a=std::fmod(a,tau);return a<0?a+tau:a; }
int fifth(int pc) { return (5*pc+3)%12; } // A=15 degrees, C=105 degrees.
bool positive(std::optional<double> n) {return n&&std::isfinite(*n)&&*n>0;}
std::optional<double> consonance(int span,int semitones,int wholeTones,bool triad) {
    // PDF p5, figure 1-2 and table 1-2: structural rules, not a chord-name table.
    if(span>=2&&span<=4&&semitones==0) {
        if(wholeTones<=1)return triad?10.:9.67;
        if(wholeTones<=3)return 9.33;
    }
    if(span==5&&semitones==1) {
        if(triad&&wholeTones<=1)return 7.;
        if(triad&&wholeTones==2)return 6.67;
        if(!triad||wholeTones>2)return 6.33;
    }
    if(span==6) {
        if(semitones==0) {
            if(wholeTones<=1)return triad?9.:8.67;
            if(wholeTones==3)return 8.33;
        }
        if(semitones==1) {
            if(triad&&wholeTones==1)return 6.;
            if(triad&&wholeTones==2)return 5.67;
            if(!triad||wholeTones>2)return 5.33;
        }
        if(semitones==2) {
            if(triad&&wholeTones==1)return 4.;
            if(!triad||wholeTones>1)return 3.5;
        }
    }
    return {};
}
}
StaticColor HarmonyColorAnalyzer::analyze(const ChordPitchSet& chord) {
    StaticColor out;
    if(chord.root<0||chord.root>11||chord.bass<0||chord.bass>11||
       !chord.intervals||((chord.intervals|chord.colorMask)&0xf000))return out;
    std::array<bool,12> pitches{};
    for(int i=0;i<12;++i)if((chord.intervals|chord.colorMask)&(1u<<i))pitches[(chord.root+i)%12]=true;
    pitches[chord.bass]=true; // A non-chord bass is a sounding pitch, not discarded.
    std::vector<int> notes,positions;
    for(int pc=0;pc<12;++pc)if(pitches[pc]){notes.push_back(pc);positions.push_back(fifth(pc));}
    const bool power=notes.size()==2&&pitches[(chord.root+7)%12]&&pitches[chord.root];
    if((notes.size()<3&&!power)||notes.size()>6)return out;
    std::sort(positions.begin(),positions.end());
    int largestGap{};
    for(std::size_t i=0;i<positions.size();++i)
        largestGap=std::max(largestGap,(positions[(i+1)%positions.size()]-positions[i]+12)%12);
    const int span=12-largestGap;
    for(std::size_t i=0;i<positions.size();++i) {
        if((positions[(i+1)%positions.size()]-positions[i]+12)%12!=largestGap)continue;
        const int begin=positions[(i+1)%positions.size()];double sum{};
        for(const auto p:positions)sum+=(p<begin?p+12:p)*30.+15.;
        out.directions.push_back(wrap(sum/positions.size()*std::numbers::pi/180.));
    }
    // Equal largest gaps have multiple legitimate unwraps. Never pick one by bass,
    // prior chord, ordinary circular mean or an arbitrary first-element rule.
    if(out.directions.size()!=1) {
        out.status=Status::Uncertain;out.reason=Reason::MultipleDirections;return out;
    }
    // The PDF's Dec 2025 wide-span revision is not fully specified. Do not apply
    // ColorChord's older direction algorithm to those structures as a new-model score.
    if(span>6){out.directions.clear();out.reason=Reason::WideSpan;return out;}
    int semitones{},wholeTones{};bool triad{};
    for(std::size_t i=0;i<notes.size();++i)for(std::size_t j=i+1;j<notes.size();++j) {
        const int distance=std::min(notes[j]-notes[i],12-(notes[j]-notes[i]));
        semitones+=distance==1;wholeTones+=distance==2;
    }
    for(const auto root:notes)triad=triad||(pitches[(root+7)%12]&&
                                             (pitches[(root+3)%12]||pitches[(root+4)%12]));
    out.r=power?std::optional<double>{10.}:consonance(span,semitones,wholeTones,triad);
    if(!out.r){out.directions.clear();out.reason=Reason::UnsupportedGrade;return out;}
    out.status=Status::Known;out.reason=Reason::None;out.theta=out.directions.front();
    out.thetaB=(fifth(chord.bass)*30.+15.)*std::numbers::pi/180.;
    out.T=10-*out.r;out.W=*out.r*std::sin(*out.thetaB-*out.theta);
    out.S=*out.r*std::cos(*out.thetaB-*out.theta);return out;
}
std::optional<DynamicColor> HarmonyColorAnalyzer::transition(const StaticColor& a,const StaticColor& b) {
    if(a.status!=Status::Known||b.status!=Status::Known)return {};
    DynamicColor d;d.deltaT=*b.T-*a.T;d.deltaW=*b.W-*a.W;
    // remainder handles the +/- pi boundary without changing the signed Ws rule.
    const auto angle=std::remainder(*b.theta-*a.theta,tau);
    const double ra=*a.r,rb=*b.r;
    d.Ts=std::sqrt(std::max(0.,ra*ra+rb*rb-2*ra*rb*std::cos(angle)));
    d.Ws=ra*rb/10.*std::sin(-angle);d.Ti=std::abs(d.deltaT)+d.Ts;return d;
}
std::vector<TimedChord> HarmonyColorAnalyzer::prepare(const Progression& events) {
    std::vector<TimedChord> out;out.reserve(events.size());
    for(const auto& event:events)out.push_back({chordPitches(event),
        std::isfinite(event.startQN)?std::optional<double>{event.startQN}:std::nullopt,
        !event.openEnded&&positive(event.durationQN)?event.durationQN:std::nullopt});
    return out;
}
PathColor HarmonyColorAnalyzer::analyzeContinuation(std::span<const TimedChord> source,const ContinuationCandidate& candidate) {
    std::vector<TimedChord> path(source.begin(),source.end());
    if(!path.empty()&&!path.back().durationQN&&positive(candidate.suggestedCurrentChordDurationQN))
        path.back().durationQN=candidate.suggestedCurrentChordDurationQN;
    std::optional<double> end;
    if(!path.empty()&&path.back().startQN&&path.back().durationQN)end=*path.back().startQN+*path.back().durationQN;
    for(const auto& event:candidate.continuation) {
        const auto duration=positive(event.durationQN)?std::optional<double>{event.durationQN}:std::nullopt;
        path.push_back({event.harmonicData?chordPitches(*event.harmonicData):chordPitches(event.label,event.quality),end,duration});
        if(end&&duration)*end+=*duration;else end.reset();
    }
    return analyzePath(path,source.size());
}
PathColor HarmonyColorAnalyzer::analyzePath(std::span<const TimedChord> path,std::size_t boundary) {
    PathColor out;out.recommendationBoundary=std::min(boundary,path.size());
    bool reliable=!path.empty();double duration{},sumW{},sumT{},sumTime{},minW=10,maxW=-10,maxTs{};
    std::size_t upW{},downW{},upT{},downT{};
    for(std::size_t i=0;i<path.size();++i) {
        PositionColor p{path[i].startQN,path[i].durationQN,analyze(path[i].chord),{}};
        if(i)p.fromPrevious=transition(out.positions.back().chord,p.chord);
        if(p.fromPrevious) {
            maxTs=std::max(maxTs,p.fromPrevious->Ts);
            upW+=p.fromPrevious->deltaW>0.05;downW+=p.fromPrevious->deltaW< -0.05;
            upT+=p.fromPrevious->deltaT>0.05;downT+=p.fromPrevious->deltaT< -0.05;
        }
        if(p.chord.status==Status::Uncertain)out.status=Status::Uncertain;
        reliable=reliable&&p.chord.status==Status::Known&&positive(p.durationQN)&&p.startQN&&std::isfinite(*p.startQN);
        if(p.chord.status==Status::Known&&positive(p.durationQN)&&p.startQN&&std::isfinite(*p.startQN)) {
            duration+=*p.durationQN;sumW+=*p.durationQN * *p.chord.W;sumT+=*p.durationQN * *p.chord.T;
            sumTime+=*p.durationQN*(*p.startQN+*p.durationQN/2.);
            minW=std::min(minW,*p.chord.W);maxW=std::max(maxW,*p.chord.W);
        }
        out.positions.push_back(std::move(p));
    }
    // Missing/ambiguous positions are not treated as zero or silently omitted.
    if(!reliable||duration<=0)return out;
    out.status=Status::Known;out.meanW=sumW/duration;out.meanT=sumT/duration;out.maxTs=maxTs;
    if(path.size()<2)return out; // No previous chord, no dynamic or trend claim.
    const auto& last=out.positions.back();
    if(last.fromPrevious){out.endingDeltaW=last.fromPrevious->deltaW;out.endingDeltaT=last.fromPrevious->deltaT;}
    double variance{},covW{},covT{};
    for(const auto& p:out.positions) {
        const double t=*p.startQN+*p.durationQN/2.-sumTime/duration;
        variance+=*p.durationQN*t*t;covW+=*p.durationQN*t*(*p.chord.W-*out.meanW);
        covT+=*p.durationQN*t*(*p.chord.T-*out.meanT);
    }
    const auto& first=out.positions.front();
    const double span=(*last.startQN+*last.durationQN/2.)-(*first.startQN+*first.durationQN/2.);
    if(variance>0&&span>0){out.temperatureTrend=covW/variance*span;out.tensionTrend=covT/variance*span;}
    // Presentation thresholds are product policy, not psychological or author norms.
    const auto links=path.size()-1;
    out.warming=path.size()>=3&&out.temperatureTrend&&*out.temperatureTrend>=1.&&upW*3>=links*2&&downW==0;
    out.cooling=path.size()>=3&&out.temperatureTrend&&*out.temperatureTrend<=-1.&&downW*3>=links*2&&upW==0;
    out.temperatureTurn=minW<=-1.&&maxW>=1.;
    out.tensionRising=out.tensionTrend&&*out.tensionTrend>=1.&&upT>downT;
    out.tensionReleasing=out.endingDeltaT&&*out.endingDeltaT<=-1.;return out;
}
}
