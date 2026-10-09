#include "ColorPreferenceReranker.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <numbers>
#include "preview/VoiceLeadingMetrics.h"

namespace harmony::color {
namespace {
bool sameKey(KeySignature a,KeySignature b){return a.tonic==b.tonic&&a.mode==b.mode;}
double unit(double n){return std::clamp(n,0.,1.);}
struct Track {
    std::vector<double> w,t,u,weight;
    double spatial{},rotation{},duration{};
};
std::optional<Track> track(const PathColor& p,std::size_t begin) {
    if(p.status!=Status::Known||begin>=p.positions.size()||p.positions.size()-begin<2)return {};
    Track out;const auto& first=p.positions[begin];const auto& last=p.positions.back();
    if(!first.startQN||!first.durationQN||!last.startQN||!last.durationQN)return {};
    const double start=*first.startQN+*first.durationQN/2.;
    const double length=*last.startQN+*last.durationQN/2.-start;
    if(!std::isfinite(length)||length<=0)return {};
    double previous=-1.;
    for(std::size_t i=begin;i<p.positions.size();++i) {
        const auto& x=p.positions[i];
        if(x.chord.status!=Status::Known||!x.chord.W||!x.chord.T||!x.startQN||!x.durationQN||
           !std::isfinite(*x.durationQN)||*x.durationQN<=0)return {};
        const double u=(*x.startQN+*x.durationQN/2.-start)/length;
        if(!std::isfinite(u)||u<=previous)return {};previous=u;
        out.w.push_back(*x.chord.W);out.t.push_back(*x.chord.T);out.u.push_back(u);
        out.weight.push_back(*x.durationQN);out.duration+=*x.durationQN;
        if(i>begin) {
            if(!x.fromPrevious)return {};
            // Ts and Ws describe spatial movement, not extra copies of T or deltaT.
            out.spatial+=x.fromPrevious->Ts;out.rotation+=x.fromPrevious->Ws;
        }
    }
    out.spatial/=out.w.size()-1;out.rotation/=out.w.size()-1;
    return out;
}
double error(const Track& x,const std::vector<double>& observed,double change,double scale,bool arc=false) {
    double sum{};
    for(std::size_t i=0;i<observed.size();++i) {
        const double target=observed.front()+change*x.u[i]+(arc?3.*std::sin(std::numbers::pi*x.u[i]):0.);
        sum+=x.weight[i]*unit(std::abs(observed[i]-target)/scale);
    }
    return sum/x.duration;
}
double laterTrend(const Track& x,const std::vector<double>& values,std::size_t begin) {
    double weight{},time{},value{};
    for(std::size_t i=begin;i<values.size();++i){weight+=x.weight[i];time+=x.weight[i]*x.u[i];value+=x.weight[i]*values[i];}
    if(weight<=0)return 0.;time/=weight;value/=weight;
    double variance{},covariance{};
    for(std::size_t i=begin;i<values.size();++i){const double d=x.u[i]-time;variance+=x.weight[i]*d*d;covariance+=x.weight[i]*d*(values[i]-value);}
    return variance>0?covariance/variance*(x.u.back()-x.u[begin]):0.;
}
std::optional<double> fit(const Track& x,const PathColor& source,const std::optional<Track>& original,
                          Preference preference) {
    const double movement=.05*unit(x.spatial/20.)+.05*unit(std::abs(x.rotation)/10.);
    if(preference==Preference::ContinueSource) {
        if(!original||!source.temperatureTrend||!source.tensionTrend)return {};
        const double ratio=x.duration/original->duration;
        return .4*error(x,x.w,std::clamp(*source.temperatureTrend*ratio,-4.,4.),20.)+
            .4*error(x,x.t,std::clamp(*source.tensionTrend*ratio,-2.,2.),10.)+
            .1*unit(std::abs(x.spatial-original->spatial)/20.)+
            .1*unit(std::abs(x.rotation-original->rotation)/20.);
    }
    if(preference==Preference::Warmer||preference==Preference::Cooler)
        return .9*error(x,x.w,preference==Preference::Warmer?4.:-4.,20.)+movement;
    if(x.t.size()<3)return {};
    const double peak=*std::max_element(x.t.begin()+1,x.t.end()-1);
    const double rise=peak-x.t.front(),release=peak-x.t.back();
    // A low flat path cannot satisfy accumulation/release. One model unit is the tolerance.
    const double arc=.75*error(x,x.t,x.t.back()-x.t.front(),10.,true)+
        .125*unit(std::max(0.,1.-rise)/1.)+.125*unit(std::max(0.,1.-release)/1.);
    if(preference==Preference::TensionArc)return .9*arc+movement;
    if(rise<1.||release<1.)return {}; // Warm closure requires actual accumulation AND release.
    const std::size_t later=x.w.size()/2;double sum{},duration{};
    for(std::size_t i=later;i<x.w.size();++i){sum+=x.weight[i]*x.w[i];duration+=x.weight[i];}
    if(x.w.back()<1.||x.w.back()-sum/duration<.5||
       laterTrend(x,x.w,later)<.5||laterTrend(x,x.t,later)>-.5)return {};
    return .6*arc+.3*error(x,x.w,4.,20.)+movement;
}
template<class Candidate,class Comparable>
void sortWindows(std::span<const Candidate> candidates,ColorRankResult& result,Comparable comparable) {
    // Disjoint original-order windows: <=3 slots, <=2 displacement, no unknown anchors crossed.
    for(std::size_t begin=0;begin<candidates.size();) {
        if(!result.decisions[begin].cost){++begin;continue;}
        std::size_t end=begin+1;
        while(end<candidates.size()&&end-begin<3&&result.decisions[end].cost) {
            bool matches=true;
            for(std::size_t j=begin;j<end;++j)matches=matches&&comparable(candidates[j],candidates[end]);
            if(!matches)break;++end;
        }
        std::stable_sort(result.order.begin()+begin,result.order.begin()+end,[&](auto a,auto b){
            // Quantization gives a strict weak order with stable ties; no epsilon comparator.
            return std::llround(*result.decisions[a].cost*1000000.)<std::llround(*result.decisions[b].cost*1000000.);
        });begin=end;
    }
}
bool eligible(const ContinuationCandidate& c,std::span<const KeyCandidate> keys,std::optional<KeySignature> selected) {
    if(!std::isfinite(c.rankingScore)||c.rankingScore<60.f||!c.melodyCompatibility.hardSatisfied)return false;
    return keys.empty()||std::any_of(keys.begin(),keys.end(),[&](const auto& k){return sameKey(c.key,k.key);})||
        (selected&&sameKey(c.key,*selected));
}
bool comparable(const ContinuationCandidate& a,const ContinuationCandidate& b) {
    const auto close=[](float x,float y){return std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=.05f;};
    // Fixed original-quality bands, independent of the current candidate pool.
    return std::floor(a.rankingScore/5.f)==std::floor(b.rankingScore/5.f)&&
        std::abs(a.rankingScore-b.rankingScore)<=3.f&&sameKey(a.key,b.key)&&a.intent==b.intent&&
        a.styles==b.styles&&a.constraints==b.constraints&&close(a.melodyCompatibility.score,b.melodyCompatibility.score)&&
        close(a.matchSimilarity,b.matchSimilarity)&&close(a.subscores.match,b.subscores.match)&&
        close(a.subscores.skeleton,b.subscores.skeleton)&&close(a.subscores.style,b.subscores.style)&&
        close(a.subscores.intent,b.subscores.intent)&&close(a.subscores.rhythm,b.subscores.rhythm)&&
        close(a.subscores.cadence,b.subscores.cadence);
}
}
ColorRankResult ColorPreferenceReranker::rank(std::span<const TimedChord> source,std::span<const KeyCandidate> keys,
    std::optional<KeySignature> selected,std::span<const ContinuationCandidate> candidates,
    std::span<const PathColor> paths,Preference preference) {
    ColorRankResult result;result.order.resize(candidates.size());std::iota(result.order.begin(),result.order.end(),0);
    result.decisions.resize(candidates.size());if(preference==Preference::Off)return result;
    const auto originalPath=HarmonyColorAnalyzer::analyzePath(source);
    const auto original=track(originalPath,0);
    const bool sourceReady=source.size()>=3&&original&&originalPath.temperatureTrend&&originalPath.tensionTrend;
    for(std::size_t i=0;i<candidates.size();++i) {
        auto& decision=result.decisions[i];decision.reason=RankReason::InsufficientColor;
        if(preference==Preference::ContinueSource&&!sourceReady){decision.reason=RankReason::InsufficientSource;continue;}
        if(!eligible(candidates[i],keys,selected)){decision.reason=RankReason::QualityProtected;continue;}
        if(preference==Preference::TensionWarmClose) {
            const auto completion=evaluateIntentCompletion(candidates[i]);
            if(candidates[i].intent!=PhraseIntent::Resolve||
               (completion.reason!=CompletionReason::DominantTonic&&completion.reason!=CompletionReason::StableTonic)) {
                decision.reason=RankReason::FunctionalFallback;continue;
            }
        }
        if(i>=paths.size()||source.empty()||paths[i].recommendationBoundary!=source.size())continue;
        const auto trajectory=track(paths[i],source.size()-1);if(!trajectory)continue;
        decision.cost=fit(*trajectory,originalPath,original,preference);
        decision.reason=decision.cost?RankReason::Matched:RankReason::TargetNotMet;
    }
    sortWindows(candidates,result,comparable);
    return result;
}
PathColor ColorPreferenceReranker::analyzeEnrichment(const Progression& events) {
    auto timed=HarmonyColorAnalyzer::prepare(events);std::vector<bool> missing(events.size());
    for(std::size_t i=0;i<events.size();++i) {
        const auto& event=events[i];bool explicitBass=false;
        if(event.bass||event.bassNoteValue) {
            const auto normalized=normalizeChord(event);
            if(normalized.bass) {
                timed[i].chord.bass=static_cast<int>(*normalized.bass);explicitBass=true;
            }
        } else if(const auto slash=event.name.find('/');slash!=std::string::npos) {
            ChordEvent bass;bass.name=event.name.substr(slash+1);
            if(const auto normalized=normalizeChord(bass);normalized.root) {
                timed[i].chord.bass=static_cast<int>(*normalized.root);explicitBass=true;
            }
        }
        missing[i]=!explicitBass;
        if(missing[i])timed[i].chord.bass=-1; // Prevent the general label materializer's root fallback.
    }
    auto path=HarmonyColorAnalyzer::analyzePath(timed);
    for(std::size_t i=0;i<missing.size();++i)if(missing[i]) {
        path.positions[i].chord.status=Status::Uncertain;
        path.positions[i].chord.reason=Reason::MissingBass;path.status=Status::Uncertain;
    }
    return path;
}
namespace {
// Integrate over overlapping QN intervals: insertions align to the source harmony
// they subdivide, rather than assuming equal chord counts or matching by index.
struct Correspondence { double error{};std::vector<double> sourceW; };
std::optional<Correspondence> correspond(const PathColor& source,const PathColor& candidate) {
    if(source.status!=Status::Known||candidate.status!=Status::Known||source.positions.empty()||candidate.positions.empty())return {};
    const auto end=[](const PositionColor& p){return *p.startQN+*p.durationQN;};
    if(std::abs(*source.positions.front().startQN-*candidate.positions.front().startQN)>1e-6||
       std::abs(end(source.positions.back())-end(candidate.positions.back()))>1e-6)return {};
    Correspondence result;double total{},errorSum{};
    for(const auto& c:candidate.positions) {
        double covered{},w{};
        for(const auto& a:source.positions) {
            const double overlap=std::max(0.,std::min(end(a),end(c))-std::max(*a.startQN,*c.startQN));
            if(overlap<=0)continue;
            covered+=overlap;w+=overlap * *a.chord.W;
            errorSum+=overlap*(.4*unit(std::abs(*c.chord.W-*a.chord.W)/20.)+
                .4*unit(std::abs(*c.chord.T-*a.chord.T)/10.)+.1*unit(std::abs(*c.chord.S-*a.chord.S)/20.));
        }
        if(std::abs(covered-*c.durationQN)>1e-6)return {};
        result.sourceW.push_back(w/covered);total+=covered;
    }
    if(total<=0)return {};result.error=errorSum/total;return result;
}
bool voiceBassMatches(const Progression& progression) {
    for(const auto& event:progression) {
        const auto materialized=chordPitches(event);
        const auto label=chordPitches(event.name,event.quality);
        const auto actual=(event.bass||event.bassNoteValue)?normalizeChord(event).bass:std::optional<PitchClass>{static_cast<PitchClass>(materialized.bass)};
        if(!actual||static_cast<int>(*actual)!=label.bass)return false;
    }
    return true;
}
bool budget(const enrichment::EnrichmentCandidate& c,HarmonicTendency tendency) {
    const auto g=static_cast<std::size_t>(c.group);if(g>=3)return false;
    const auto& limits=tendencyProfile(tendency);
    int inserted{},replaced{};
    for(const auto& op:c.operations) {
        inserted+=op.type==enrichment::OperationType::InsertChord;
        replaced+=op.type==enrichment::OperationType::ReplaceChord;
    }
    return c.operations.size()<=static_cast<std::size_t>(limits.operations[g])&&inserted<=limits.insertions[g]&&
        replaced<=limits.substitutions[g]&&c.skeletonPreservation>=limits.skeletonMinimum;
}
bool supportsClosure(const Progression& source,const HarmonicAnalysisResult& analysis,
                     std::optional<PhraseIntent> intent,const enrichment::EnrichmentCandidate& c) {
    if(source.empty()||c.progression.empty()||!analysis.selectedKey||analysis.full.empty())return false;
    if(intent&&*intent!=PhraseIntent::Neutral&&*intent!=PhraseIntent::Resolve)return false;
    const auto& last=analysis.full.back();
    if(last.function!=HarmonicFunction::Tonic||!last.degree||last.degree->degree!=1||last.degree->alteration!=0)return false;
    const auto final=chordPitches(c.progression.back());
    return final.root==static_cast<int>(analysis.selectedKey->key.tonic)&&
        (final.quality==ChordQuality::Major||final.quality==ChordQuality::Minor||
         final.quality==ChordQuality::Major7||final.quality==ChordQuality::Minor7);
}
}
ColorRankResult ColorPreferenceReranker::rankEnrichment(const Progression& source,const HarmonicAnalysisResult& analysis,
    std::optional<PhraseIntent> intent,HarmonicTendency tendency,
    std::span<const enrichment::EnrichmentCandidate> candidates,std::span<const PathColor> paths,Preference preference) {
    ColorRankResult result;result.order.resize(candidates.size());std::iota(result.order.begin(),result.order.end(),0);
    result.decisions.resize(candidates.size());if(preference==Preference::Off)return result;
    const auto originalPath=analyzeEnrichment(source);const auto original=track(originalPath,0);
    const auto originalVoice=preview::measureVoiceLeading(source);
    std::vector<std::optional<float>> voice(candidates.size());
    for(std::size_t i=0;i<candidates.size();++i) {
        auto& decision=result.decisions[i];decision.reason=RankReason::InsufficientColor;
        const auto& c=candidates[i];
        if(!original){decision.reason=RankReason::InsufficientSource;continue;}
        if(preference==Preference::ContinueSource&&source.size()<3){decision.reason=RankReason::InsufficientSource;continue;}
        if(!std::isfinite(c.score)||c.score<60.f||!c.melodyCompatibility.hardSatisfied||!budget(c,tendency)) {
            decision.reason=RankReason::QualityProtected;continue;
        }
        const auto measured=preview::measureVoiceLeading(c.progression);
        if(!voiceBassMatches(source)||!voiceBassMatches(c.progression)||!originalVoice||!measured||!std::isfinite(measured->score)||
           measured->score+.02f<originalVoice->score) {decision.reason=RankReason::QualityProtected;continue;}
        voice[i]=measured->score;
        if(preference==Preference::TensionWarmClose&&!supportsClosure(source,analysis,intent,c)) {
            decision.reason=RankReason::FunctionalFallback;continue;
        }
        if(i>=paths.size())continue;
        const auto trajectory=track(paths[i],0);const auto aligned=correspond(originalPath,paths[i]);
        if(!trajectory||!aligned)continue;
        if(preference==Preference::ContinueSource) {
            decision.cost=aligned->error+.05*unit(std::abs(trajectory->spatial-original->spatial)/20.)+
                .05*unit(std::abs(trajectory->rotation-original->rotation)/20.);
        } else {
            decision.cost=fit(*trajectory,originalPath,original,preference);
            if(decision.cost&&(preference==Preference::Warmer||preference==Preference::Cooler)) {
                auto delta=*trajectory;
                for(std::size_t j=0;j<delta.w.size();++j)delta.w[j]-=aligned->sourceW[j];
                const double movement=.05*unit(delta.spatial/20.)+.05*unit(std::abs(delta.rotation)/10.);
                // Same total goal weight as dev.6: split between whole and source-relative trajectory.
                decision.cost=.5 * *decision.cost+.5*(.9*error(delta,delta.w,preference==Preference::Warmer?4.:-4.,20.)+movement);
            }
        }
        decision.reason=decision.cost?RankReason::Matched:RankReason::TargetNotMet;
    }
    const auto close=[](float a,float b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=.05f;};
    sortWindows(candidates,result,[&](const auto& a,const auto& b) {
        const auto ai=static_cast<std::size_t>(&a-candidates.data()),bi=static_cast<std::size_t>(&b-candidates.data());
        return a.group==b.group&&a.complexity==b.complexity&&std::floor(a.score/5.f)==std::floor(b.score/5.f)&&
            std::abs(a.score-b.score)<=3.f&&a.constraints==b.constraints&&
            close(a.skeletonPreservation,b.skeletonPreservation)&&close(a.styleCompatibility,b.styleCompatibility)&&
            close(a.complexityScore,b.complexityScore)&&close(a.melodyCompatibility.score,b.melodyCompatibility.score)&&
            voice[ai]&&voice[bi]&&std::abs(*voice[ai]-*voice[bi])<=.02f;
    });return result;
}
const char* preferenceKey(Preference p) noexcept {
    constexpr const char* keys[]{"colorSort.off","colorSort.auto","colorSort.warmer","colorSort.cooler","colorSort.arc","colorSort.warmClose"};
    return keys[static_cast<std::size_t>(p)];
}
const char* rankReasonKey(RankReason r) noexcept {
    constexpr const char* keys[]{"colorSort.reasonOff","colorSort.reasonMatch","colorSort.reasonUnknown","colorSort.reasonSource","colorSort.reasonFunction","colorSort.reasonQuality","colorSort.reasonTarget"};
    return keys[static_cast<std::size_t>(r)];
}
}
