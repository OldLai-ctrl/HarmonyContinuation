#include "ContextualContinuation.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace harmony {
namespace {
int pc(int n){return (n%12+12)%12;}
int step(int a,int b){const int d=pc(b-a);return d>6?d-12:d;}
unsigned sounding(const ChordPitchSet& c) {
    unsigned mask{};
    for(int n=0;n<12;++n)if((c.intervals|c.colorMask)&(1u<<n))mask|=1u<<pc(c.root+n);
    if(c.bass>=0)mask|=1u<<c.bass;
    return mask;
}
unsigned rotate(unsigned mask,int shift) {
    unsigned out{};for(int n=0;n<12;++n)if(mask&(1u<<n))out|=1u<<pc(n+shift);return out;
}
float overlap(unsigned a,unsigned b) {
    return float(std::popcount(a&b))/float(std::max(1,std::popcount(a|b)));
}
float surfaceTension(unsigned mask) {
    float pairs{};
    for(int a=0;a<12;++a)if(mask&(1u<<a))for(int b=a+1;b<12;++b)if(mask&(1u<<b)) {
        const auto d=std::abs(step(a,b));if(d==1)pairs+=1.f;else if(d==6)pairs+=.7f;
    }
    return pairs/float(std::max(1,std::popcount(mask)));
}
struct SoundEvent {unsigned tones{};int bass{};double duration{};};
CreativeRouteProfile profile(const std::vector<SoundEvent>& events) {
    CreativeRouteProfile p;if(events.empty())return p;
    double total{};for(const auto& e:events)total+=e.duration;
    if(!(total>0))return p;p.ready=true;
    for(const auto& e:events)for(int n=0;n<12;++n)if(e.tones&(1u<<n))
        p.palette[n]+=float(e.duration/total)/float(std::max(1,std::popcount(e.tones)));
    const auto anchor=events.front().bass;
    std::size_t event{};double end=events[0].duration;float bassPath{};
    for(std::size_t sample=0;sample<8;++sample) {
        const auto at=total*(double(sample)+.5)/8.;
        while(event+1<events.size()&&at>=end) {
            bassPath+=float(step(events[event].bass,events[event+1].bass));
            ++event;end+=events[event].duration;
        }
        p.tones[sample]=std::uint16_t(rotate(events[event].tones,-anchor));
        p.bass[sample]=bassPath;p.tension[sample]=surfaceTension(events[event].tones);
    }
    return p;
}
float similarity(const CreativeRouteProfile& a,const CreativeRouteProfile& b) {
    if(!a.ready||!b.ready)return 0;
    float bass{},tones{},tension{},intersection{},unionUse{};
    for(std::size_t i=0;i<8;++i) {
        bass+=1.f-std::min(1.f,std::abs(a.bass[i]-b.bass[i])/6.f);
        tones+=overlap(a.tones[i],b.tones[i]);
        tension+=1.f-std::min(1.f,2.f*std::abs(a.tension[i]-b.tension[i]));
    }
    // Identical sounding material after transposition stays one idea even if
    // the inferred MIDI chord roots, names or source IDs differ.
    if(a.tones==b.tones&&a.bass==b.bass)return 1.f;
    for(std::size_t i=0;i<12;++i){intersection+=std::min(a.palette[i],b.palette[i]);unionUse+=std::max(a.palette[i],b.palette[i]);}
    const auto palette=intersection/std::max(.001f,unionUse);
    return std::clamp(.4f*palette+.3f*bass/8.f+.2f*tones/8.f+.1f*tension/8.f,0.f,1.f);
}
std::vector<SoundEvent> inputSounds(const Progression& input,double hold) {
    std::vector<SoundEvent> result;
    for(std::size_t i=0;i<input.size();++i) {
        const auto p=chordPitches(input[i]);
        const auto duration=input[i].durationQN&&!input[i].openEnded?*input[i].durationQN:
            i+1<input.size()?input[i+1].startQN-input[i].startQN:hold;
        if(duration>0)result.push_back({sounding(p),p.bass,duration});
    }
    return result;
}
float nearestMovement(unsigned a,unsigned b) {
    float movement{};for(int n=0;n<12;++n)if(a&(1u<<n)) {
        int best=6;for(int m=0;m<12;++m)if(b&(1u<<m))best=std::min(best,std::abs(step(n,m)));
        movement+=float(best);
    }
    return movement/float(std::max(1,std::popcount(a)));
}
float contextualFit(const std::vector<SoundEvent>& input,const std::vector<SoundEvent>& next,
    const ContinuationCandidate& candidate,std::optional<KeySignature> contextCenter) {
    if(input.empty()||next.empty())return 0;
    const auto& last=input.back();
    const float carry=1.f-std::min(1.f,nearestMovement(last.tones,next[0].tones)/3.f);
    unsigned recent{},early{},late{};
    const auto begin=input.size()>8?input.size()-8:0;
    for(std::size_t i=begin;i<input.size();++i)recent|=input[i].tones;
    for(std::size_t i=0;i<std::min<std::size_t>(2,next.size());++i)early|=next[i].tones;
    for(std::size_t i=next.size()/2;i<next.size();++i)late|=next[i].tones;
    float flowIn{},flowOut{};
    const auto inBegin=input.size()>4?input.size()-4:0;
    for(std::size_t i=inBegin+1;i<input.size();++i)flowIn+=float(step(input[i-1].bass,input[i].bass));
    flowIn/=float(std::max<std::size_t>(1,input.size()-inBegin-1));
    for(std::size_t i=1;i<std::min<std::size_t>(4,next.size());++i)flowOut+=float(step(next[i-1].bass,next[i].bass));
    flowOut/=float(std::max<std::size_t>(1,std::min<std::size_t>(4,next.size())-1));
    float flow=1.f-std::min(1.f,std::abs(flowIn-flowOut)/6.f);
    const bool pedal=std::all_of(next.begin(),next.begin()+std::min<std::size_t>(4,next.size()),
        [&](const auto& e){return e.bass==next.front().bass;});
    const bool activeBass=std::any_of(input.begin()+inBegin,input.end(),[&](const auto& e){return e.bass!=last.bass;});
    if(pedal&&activeBass) {
        std::vector<double> durations;for(std::size_t i=begin;i<input.size();++i)durations.push_back(input[i].duration);
        std::sort(durations.begin(),durations.end());
        // A new pedal after a deliberately sustained endpoint has different
        // contextual evidence from a generic common-tone stop in a moving line.
        const bool sustained=last.duration>=2*durations[durations.size()/2]-1e-6;
        flow=sustained?.9f:.35f;
    }
    const float tension=1.f-std::min(1.f,2.f*std::abs(surfaceTension(last.tones)-surfaceTension(next[0].tones)));
    const float change=std::max(0.f,overlap(early,recent)-overlap(late,recent));
    // Continuity is specific to the recent phrase, including its bass motion
    // and surface tension. Later palette movement may develop that continuity.
    float functional=.7f,center=.8f;
    if(contextCenter) {
        const int tonic=int(contextCenter->tonic);
        const auto has=[&](unsigned tones,int degree){return (tones&(1u<<pc(tonic+degree)))!=0;};
        // Plausible function from actual tones in a supported center, not a
        // uniquely inferred chord root/name. Other interpretations remain legal.
        const bool dominant=has(last.tones,7)&&has(last.tones,11)&&has(last.tones,2)&&!has(last.tones,0);
        if(dominant) {
            const bool tonicTone=next[0].tones&(1u<<tonic);
            const bool carriesLeadingTone=next[0].tones&(1u<<pc(tonic+11));
            functional=tonicTone?.95f:carriesLeadingTone?.8f:.6f;
        } else if(has(last.tones,5)&&has(last.tones,9))
            functional=has(next[0].tones,7)&&has(next[0].tones,11)?.95f:has(next[0].tones,5)?.8f:.7f;
        center=candidate.centerRelation==CenterRelation::Same||candidate.centerRelation==CenterRelation::Relative?.9f:
            candidate.centerRelation==CenterRelation::Modulation&&candidate.targetCenter?1.f:.8f;
    }
    return std::clamp(.25f*carry+.15f*overlap(early,recent)+.15f*flow+.15f*tension+
        .1f*change+.1f*functional+.1f*center,0.f,1.f);
}
}
void prepareContextualDiversity(const MatchQuery& query,RecommendationSet& set) {
    double hold=progressionOpenDuration(query.source);
    bool supportedHold{};
    for(const auto& group:set.groups)for(const auto& c:group)
        if(!supportedHold&&c.structureQualified&&c.channel!=RecommendationChannel::Exact&&c.suggestedCurrentChordDurationQN) {
            hold=*c.suggestedCurrentChordDurationQN;supportedHold=true;
        }
    const auto sounds=inputSounds(query.source,hold);
    if(sounds.empty())return;
    std::vector<CreativeRouteProfile> history;
    for(const auto size:{std::size_t(4),std::size_t(8)})if(sounds.size()>=size)
        for(std::size_t i=0;i+size<=sounds.size();++i)
            history.push_back(profile(std::vector<SoundEvent>(sounds.begin()+i,sounds.begin()+i+size)));
    const auto recent=profile(std::vector<SoundEvent>(sounds.end()-std::min<std::size_t>(8,sounds.size()),sounds.end()));
    for(auto& group:set.groups)for(auto& c:group) {
        std::vector<SoundEvent> next;
        for(const auto& e:c.continuation) {
            const auto p=e.harmonicData?chordPitches(*e.harmonicData):chordPitches(e.label,e.quality);
            next.push_back({sounding(p),p.bass,e.durationQN});
        }
        c.routeProfile=profile(next);
        const auto center=c.channel==RecommendationChannel::Exact?
            (query.interpretations.empty()?std::optional<KeySignature>{}:query.interpretations.front().key.key):c.originCenter;
        if(!c.continuation.empty())c.routeProfile.contextFit=contextualFit(sounds,next,c,center);
        c.routeProfile.recentSimilarity=similarity(c.routeProfile,recent);
        for(const auto& h:history)c.routeProfile.historySimilarity=std::max(c.routeProfile.historySimilarity,similarity(c.routeProfile,h));
    }
}
float creativeRouteSimilarity(const ContinuationCandidate& a,const ContinuationCandidate& b) noexcept {
    return similarity(a.routeProfile,b.routeProfile);
}
std::size_t inspirationFirstPageSize(const RecommendationSet& set,std::size_t maximum) {
    const auto order=mixedInspirationOrder(set);
    if(order.empty())return 0;
    std::vector<const ContinuationCandidate*> distinct;
    for(const auto position:order) {
        const auto& c=set.groups[position.group][position.index];
        if(!c.routeProfile.ready)return std::min(maximum,order.size()); // frozen/legacy fallback
        if(!c.structureQualified||c.exploratory)break;
        if(std::any_of(distinct.begin(),distinct.end(),[&](const auto* prior){return creativeRouteSimilarity(c,*prior)>=.9f;}))break;
        distinct.push_back(&c);if(distinct.size()==maximum)break;
    }
    return std::max<std::size_t>(1,distinct.size());
}
}
