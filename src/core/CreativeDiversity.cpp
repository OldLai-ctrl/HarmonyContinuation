#include "ContextualContinuation.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>

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
float developmentSimilarity(const CreativeRouteProfile& a,const CreativeRouteProfile& b) {
    if(!a.ready||!b.ready)return 0;
    float tones{},bass{},tension{};
    for(std::size_t i=0;i<8;++i) {
        tones+=overlap(a.tones[i],b.tones[i]);
        bass+=1.f-std::min(1.f,std::abs(a.bass[i]-b.bass[i])/6.f);
        tension+=1.f-std::min(1.f,2.f*std::abs(a.tension[i]-b.tension[i]));
    }
    // A change of pitch-class region alone is not a new development mechanism.
    // Same sounding contour in a different key remains an available reprise,
    // with a soft preference for another contour when quality is comparable.
    return std::clamp((.5f*tones+.35f*bass+.15f*tension)/8.f,0.f,1.f);
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
void prepareContextualDiversity(const MatchQuery& query,RecommendationSet& set,std::size_t appendedAfter) {
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
        for(std::size_t i=appendedAfter>=size?appendedAfter-size+1:0;i+size<=sounds.size();++i)
            history.push_back(profile(std::vector<SoundEvent>(sounds.begin()+i,sounds.begin()+i+size)));
    const auto recent=profile(std::vector<SoundEvent>(sounds.end()-std::min<std::size_t>(8,sounds.size()),sounds.end()));
    for(auto& group:set.groups)for(auto& c:group) {
        std::vector<SoundEvent> next;
        for(const auto& e:c.continuation) {
            const auto p=e.harmonicData?chordPitches(*e.harmonicData):chordPitches(e.label,e.quality);
            next.push_back({sounding(p),p.bass,e.durationQN});
        }
        const auto prior=c.routeProfile;
        c.routeProfile=profile(next);
        if(appendedAfter) {
            c.routeProfile.historySimilarity=prior.historySimilarity;
            c.routeProfile.developmentHistorySimilarity=prior.developmentHistorySimilarity;
        }
        const auto center=c.channel==RecommendationChannel::Exact?
            (query.interpretations.empty()?std::optional<KeySignature>{}:query.interpretations.front().key.key):c.originCenter;
        if(!c.continuation.empty())c.routeProfile.contextFit=contextualFit(sounds,next,c,center);
        c.routeProfile.recentSimilarity=similarity(c.routeProfile,recent);
        for(const auto& h:history) {
            c.routeProfile.historySimilarity=std::max(c.routeProfile.historySimilarity,similarity(c.routeProfile,h));
            c.routeProfile.developmentHistorySimilarity=std::max(c.routeProfile.developmentHistorySimilarity,developmentSimilarity(c.routeProfile,h));
        }
    }
}
float creativeRouteSimilarity(const ContinuationCandidate& a,const ContinuationCandidate& b) noexcept {
    return std::max(similarity(a.routeProfile,b.routeProfile),developmentSimilarity(a.routeProfile,b.routeProfile));
}
namespace {
// Minimum one-to-one octave-equivalent assignment. Added/omitted tones are
// described separately by palette/common-tone evidence, not invented voices.
std::pair<float,int> classMotion(unsigned a,unsigned b) {
    if(std::popcount(a)>std::popcount(b))std::swap(a,b);
    std::vector<int> from,to;
    for(int n=0;n<12;++n){if(a&(1u<<n))from.push_back(n);if(b&(1u<<n))to.push_back(n);}
    if(from.empty())return {6.f,6};
    const auto size=std::size_t(1)<<to.size();
    std::vector<std::pair<int,int>> costs(size,{1000,1000});costs[0]={0,0};
    for(std::size_t mask=0;mask<size;++mask) {
        const auto voice=std::size_t(std::popcount(mask));
        if(voice>=from.size()||costs[mask].first==1000)continue;
        for(std::size_t j=0;j<to.size();++j)if(!(mask&(std::size_t(1)<<j))) {
            const auto distance=std::abs(step(from[voice],to[j]));
            const auto next=std::pair(costs[mask].first+distance,std::max(costs[mask].second,distance));
            auto& cost=costs[mask|(std::size_t(1)<<j)];cost=std::min(cost,next);
        }
    }
    std::pair<int,int> best{1000,1000};
    for(std::size_t mask=0;mask<size;++mask)if(std::popcount(mask)==from.size())best=std::min(best,costs[mask]);
    return {float(best.first)/float(from.size()),best.second};
}
}
void prepareHarmonicDevelopment(const MatchQuery& query,RecommendationSet& set) {
    const auto input=inputSounds(query.source,progressionOpenDuration(query.source));
    if(input.empty())return;
    const auto recentSounds=std::vector<SoundEvent>(input.end()-std::min<std::size_t>(8,input.size()),input.end());
    std::map<std::pair<unsigned,unsigned>,std::pair<float,int>> motionCache;
    for(auto& group:set.groups)for(auto& c:group) {
        auto& e=c.harmonicDevelopment;e={};
        if(c.continuation.empty()||!c.routeProfile.ready)continue;
        const auto& first=c.continuation.front();
        const auto pitch=first.harmonicData?chordPitches(*first.harmonicData):chordPitches(first.label,first.quality);
        const auto tones=sounding(pitch);
        if(!tones||pitch.bass<0||input.back().bass<0)continue;
        const auto key=std::pair(input.back().tones,tones);
        auto [found,inserted]=motionCache.try_emplace(key);
        if(inserted)found->second=classMotion(key.first,key.second);
        e.meanClassMotion=found->second.first;e.maximumClassMotion=found->second.second;
        e.commonTones=std::popcount(key.first&key.second);
        e.bassClassStep=step(input.back().bass,pitch.bass);
        const auto bass=std::abs(e.bassClassStep);
        const float bassFit=bass==1?1.f:bass==2?.95f:bass==0?.8f:bass==5?.85f:bass==6?.4f:.7f;
        const float common=float(e.commonTones)/float(std::max(1,std::min(std::popcount(key.first),std::popcount(key.second))));
        const float voiceFit=std::clamp(1.f-e.meanClassMotion/6.f-.05f*std::max(0,e.maximumClassMotion-3),0.f,1.f);
        e.coherence=std::clamp(.4f*c.routeProfile.contextFit+.2f*voiceFit+.15f*common+.25f*bassFit,0.f,1.f);
        const auto& p=c.routeProfile;
        auto candidateContext=recentSounds;
        if(query.source.back().openEnded&&c.suggestedCurrentChordDurationQN&&
            *c.suggestedCurrentChordDurationQN>0)
            candidateContext.back().duration=*c.suggestedCurrentChordDurationQN;
        const auto recent=profile(candidateContext);
        e.repetition=std::max(p.recentSimilarity,p.developmentHistorySimilarity);
        float early{},late{},bassContrast{},internalMovement{};
        for(std::size_t i=0;i<8;++i) {
            (i<4?early:late)+=p.tension[i]/4.f;
            bassContrast+=std::min(1.f,std::abs(p.bass[i]-recent.bass[i])/6.f)/8.f;
            if(i)internalMovement+=p.tones[i]!=p.tones[i-1]?1.f/7.f:0.f;
        }
        e.tensionChange=std::min(1.f,2.f*std::abs(late-early));
        for(std::size_t i=0;i<12;++i)e.paletteChange+=.5f*std::abs(p.palette[i]-recent.palette[i]);
        // Difference earns value only within a phrase that actually develops;
        // same material remains available for deliberate reprise/looping.
        e.novelty=std::clamp((.45f*(1.f-e.repetition)+.2f*e.paletteChange+
            .2f*e.tensionChange+.15f*bassContrast)*(.5f+.5f*internalMovement),0.f,1.f);
        e.ready=true;
    }
}
void rankHarmonicConnections(std::vector<ContinuationCandidate>& group) {
    // Neutral Unified scores and eligibility are untouched. Reorder only
    // neighboring quality bands; a smooth generic join cannot erase a match.
    std::stable_sort(group.begin(),group.end(),[](const auto& a,const auto& b) {
        if(a.exploratory!=b.exploratory)return !a.exploratory;
        const auto band=[](const auto& c){return int(std::floor(c.rankingScore/5.f));};
        if(band(a)!=band(b))return band(a)>band(b);
        const auto coherence=[](const auto& c){return c.harmonicDevelopment.ready?c.harmonicDevelopment.coherence:0.f;};
        return coherence(a)>coherence(b);
    });
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
