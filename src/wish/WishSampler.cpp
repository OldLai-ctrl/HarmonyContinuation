#include "WishSampler.h"
#include "core/RecommendationPolicy.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace harmony::wish {
namespace {
int pc(int n) {return (n%12+12)%12;}
bool validFilter(const WishFilter& filter,std::string& error) {
    if(filter.fixedTonic && (int(*filter.fixedTonic)<0 || int(*filter.fixedTonic)>11))error="invalid wish tonic";
    for(const auto mode:filter.modes)if(int(mode)<0 || int(mode)>int(MusicalMode::ClassicalMelodicMinor))error="invalid wish mode";
    if(filter.strict && filter.modes.empty())error="strict wish filter requires a selected mode";
    return error.empty();
}
void countState(WishPool& pool,AnnotationState state) {
    if(state==AnnotationState::SourceMismatch)++pool.sourceMismatches;
    else if(state==AnnotationState::Unannotated)++pool.unannotatedSources;
    else if(state==AnnotationState::Unreviewed)++pool.unreviewedSources;
    else if(state==AnnotationState::Unconfirmed)++pool.unconfirmedSources;
}
// A coarse connection is a soft diversity bucket, never a duplicate verdict.
std::string familyIdentity(const preview::Sequence& sequence) {
    const int anchor=sequence.events.front().chord.root;
    std::ostringstream out;
    for(const auto& event:sequence.events) {
        auto quality=event.chord.quality;
        if(quality==ChordQuality::Major7)quality=ChordQuality::Major;
        if(quality==ChordQuality::Minor7)quality=ChordQuality::Minor;
        out<<pc(event.chord.root-anchor)<<':'<<pc(event.chord.bass-anchor)<<':'<<int(quality)<<'|';
    }
    return out.str();
}
std::string variantIdentity(preview::Sequence sequence,int numerator,int denominator) {
    const int anchor=sequence.events.front().chord.root;
    for(auto& e:sequence.events) {
        e.chord.root=pc(e.chord.root-anchor);e.chord.bass=pc(e.chord.bass-anchor);
        if(e.topVoice)e.topVoice=*e.topVoice-anchor;
    }
    return performanceFingerprint(sequence,numerator,denominator);
}
struct SourceInfo {std::string fingerprint;AnnotationResolution annotation;};
std::vector<RealizedMembership> acceptedMemberships(const ProgressionTemplate& source,const SourceInfo& info,
    const preview::Sequence& music,KeySignature key,const WishFilter& filter) {
    std::vector<RealizedMembership> accepted;
    for(const auto& m:info.annotation.memberships) {
        if(m.review!=ReviewStatus::Reviewed)continue;
        const auto tonic=pc(int(key.tonic)+m.tonicOffset);
        if(!filter.modes.empty() && std::find(filter.modes.begin(),filter.modes.end(),m.mode)==filter.modes.end())continue;
        if(!filter.modes.empty() && filter.fixedTonic && tonic!=int(*filter.fixedTonic))continue;
        if(filter.strict && !strictlyCompatible(music,m.mode,tonic))continue;
        accepted.push_back({source.id,m,tonic});
    }
    return accepted;
}
void addRoute(std::map<std::string,WishItem>& unique,const ProgressionTemplate& source,const SourceInfo& info,
    preview::SourceRouteResult route,const WishFilter& filter,const MatchResult* match=nullptr) {
    const auto music=preview::buildSequence(route.imported,&route.candidate,preview::defaultTempoBPM);
    if(!music)throw std::invalid_argument(music.error);
    if(filter.modes.empty() && filter.fixedTonic && route.candidate.key.tonic!=*filter.fixedTonic)return;
    auto memberships=acceptedMemberships(source,info,music.sequence,route.candidate.key,filter);
    if(!filter.modes.empty() && memberships.empty())return;
    const auto identity=performanceFingerprint(music.sequence,route.meterNumerator,route.meterDenominator);
    WishSource ref{source.id,info.fingerprint,info.annotation.state};
    if(auto found=unique.find(identity);found!=unique.end()) {
        auto& old=found->second;
        if(std::none_of(old.sources.begin(),old.sources.end(),[&](const auto& s){return s.id==ref.id;})) {
            old.sources.push_back(std::move(ref));
            old.memberships.insert(old.memberships.end(),memberships.begin(),memberships.end());
        }
        return;
    }
    WishItem value;value.route=std::move(route);value.musicIdentity=identity;
    value.familyIdentity=familyIdentity(music.sequence);
    value.variantIdentity=variantIdentity(music.sequence,value.route.meterNumerator,value.route.meterDenominator);
    value.route.candidate.id="wish-v1:"+identity;
    value.sources.push_back(std::move(ref));value.memberships=std::move(memberships);
    if(match)value.match=*match;
    unique.emplace(identity,std::move(value));
}
std::map<TemplateID,SourceInfo> sourceInfo(std::span<const ProgressionTemplate> sources,const AnnotationCatalog& annotations,WishPool& result) {
    std::map<TemplateID,SourceInfo> info;
    for(const auto& source:sources) {
        if(source.sourceType!="factory")continue;
        if(info.contains(source.id))throw std::invalid_argument("duplicate Factory ID in wish catalog");
        try {
            auto resolution=resolveAnnotations(source,annotations);
            if(resolution.state==AnnotationState::Invalid)++result.invalidAnnotations;
            countState(result,resolution.state);
            info.emplace(source.id,SourceInfo{sourceFingerprint(source),std::move(resolution)});
        } catch(const std::exception&) {++result.invalidSources;}
    }
    return info;
}
void finish(std::map<std::string,WishItem>& unique,WishPool& result) {
    for(auto& [identity,item]:unique)result.items.push_back(std::move(item));
}
}
WishPool makeFreePool(std::span<const ProgressionTemplate> sources,const AnnotationCatalog& annotations,const WishFilter& filter) {
    WishPool result;
    if(annotations.version!=1){result.error="unsupported annotation catalog version";return result;}
    if(!validFilter(filter,result.error))return result;
    try {
        const auto info=sourceInfo(sources,annotations,result);
        std::map<std::string,WishItem> unique;
        // Stable source order chooses a reproducible representative, unrelated to scores or history.
        std::map<TemplateID,const ProgressionTemplate*> ordered;
        for(const auto& source:sources)if(info.contains(source.id))ordered.emplace(source.id,&source);
        for(const auto& [id,source]:ordered)for(int tonic=0;tonic<12;++tonic) {
            if(filter.modes.empty() && filter.fixedTonic && tonic!=int(*filter.fixedTonic))continue;
            auto route=preview::templateRoute(*source,{static_cast<PitchClass>(tonic),source->mode});
            if(!route){++result.invalidSources;continue;}
            addRoute(unique,*source,info.at(id),std::move(route),filter);
        }
        finish(unique,result);
    } catch(const std::exception& e){result.error=e.what();result.items.clear();}
    return result;
}
WishPool makeInputPool(std::span<const ProgressionTemplate> sources,const AnnotationCatalog& annotations,
    const WishInput& input,const WishFilter& filter) {
    WishPool result;
    if(annotations.version!=1){result.error="unsupported annotation catalog version";return result;}
    if(!validFilter(filter,result.error))return result;
    if(input.imported.events.empty())return result;
    if(input.imported.events.size()>64 || input.imported.coordinateMode==TimelineCoordinateMode::Unknown) {
        result.error="invalid wish input timeline";return result;
    }
    try {
        const auto info=sourceInfo(sources,annotations,result);
        std::vector<ProgressionTemplate> valid;
        for(const auto& source:sources)if(info.contains(source.id))valid.push_back(source);
        const auto query=makeMatchQuery(input.imported.events,recommendationContext(input.analysis));
        const CandidateIndex index(std::move(valid));
        MatchWeights matchWeights;matchWeights.abstractChordCompatibility=true;matchWeights.keyPriorWeight=0.f;
        const auto shortlist=index.shortlist(query,matchWeights);
        result.unifiedStats.imported=query.source.size();
        result.unifiedStats.analyzed=query.interpretations.empty()?0:query.interpretations.front().full.size();
        result.unifiedStats.keys=query.interpretations.size();result.unifiedStats.shortlistSize=shortlist.size();
        std::map<std::string,WishItem> unique;
        // Run unchanged Unified qualification per globally shortlisted source. Its normal
        // suffix aggregation would otherwise discard different complete source prefixes.
        std::vector<std::size_t> ordered=shortlist;
        std::sort(ordered.begin(),ordered.end(),[&](auto a,auto b){return index.templates()[a].id<index.templates()[b].id;});
        for(const auto sourceIndex:ordered) {
            const auto& source=index.templates()[sourceIndex];
            const auto set=recommendContinuations(query,CandidateIndex({source}),input.request,recommendationWeights());
            result.unifiedStats.aligned+=set.stats.aligned;
            result.unifiedStats.similarityPassed+=set.stats.similarityPassed;
            result.unifiedStats.scorePassed+=set.stats.scorePassed;
            result.unifiedStats.totalMs+=set.stats.totalMs;
            std::vector<const ContinuationCandidate*> candidates;
            for(const auto& group:set.groups)for(const auto& candidate:group)
                if(candidate.baseQualified&&candidate.hardConstraintsPassed&&std::isfinite(candidate.rankingScore))candidates.push_back(&candidate);
            std::sort(candidates.begin(),candidates.end(),[](const auto* a,const auto* b) {
                return std::tie(a->key.tonic,a->key.mode,a->matchStart,a->matchEnd,a->id)<
                       std::tie(b->key.tonic,b->key.mode,b->matchStart,b->matchEnd,b->id);
            });
            for(const auto* candidate:candidates) {
                auto route=preview::sourceRoute(input.imported,*candidate,source);
                if(!route){++result.invalidSources;continue;}
                const auto match=std::find_if(set.matches.begin(),set.matches.end(),[&](const auto& m){return representativeMatch(*candidate,m);});
                if(match==set.matches.end()){++result.invalidSources;continue;}
                addRoute(unique,source,info.at(source.id),std::move(route),filter,&*match);
            }
        }
        finish(unique,result);
        result.unifiedStats.distinct=result.items.size();result.unifiedStats.output=result.items.size();
    } catch(const std::exception& e){result.error=e.what();result.items.clear();}
    return result;
}
WishRound::WishRound(WishPool pool,std::uint64_t seed):random_(seed) {
    if(!pool)throw std::invalid_argument(pool.error);
    std::map<std::string,std::map<std::string,std::map<std::string,WishItem>>> grouped;
    for(auto& item:pool.items)grouped[item.familyIdentity][item.variantIdentity].try_emplace(item.musicIdentity,std::move(item));
    for(auto& [family,units]:grouped) {
        Family f;f.identity=family;
        for(auto& [variant,items]:units) {
            Unit u;u.identity=variant;
            for(auto& [identity,item]:items){u.items.push_back(std::move(item));++remaining_;}
            f.units.push_back(std::move(u));
        }
        families_.push_back(std::move(f));
    }
}
std::uint64_t WishRound::bounded(std::uint64_t bound) {
    const auto threshold=(std::uint64_t(0)-bound)%bound;
    for(;;){const auto value=random_();if(value>=threshold)return value%bound;}
}
std::vector<WishItem> WishRound::draw(std::size_t count) {
    if(count>10)throw std::invalid_argument("wish draw batch exceeds ten");
    std::vector<WishItem> result;
    const auto choose=[&](const auto& values,const auto& available) {
        std::uint64_t total{};
        for(const auto& value:values)if(available(value))total+=std::max<std::uint64_t>(1,1024/(1+value.drawn));
        auto ticket=bounded(total);
        for(std::size_t i=0;i<values.size();++i)if(available(values[i])) {
            const auto weight=std::max<std::uint64_t>(1,1024/(1+values[i].drawn));
            if(ticket<weight)return i;ticket-=weight;
        }
        throw std::logic_error("wish round population mismatch");
    };
    const auto hasUnit=[](const auto& f){return std::any_of(f.units.begin(),f.units.end(),[](const auto& u){return !u.items.empty();});};
    while(result.size()<count && remaining_) {
        auto& f=families_[choose(families_,hasUnit)];
        auto& u=f.units[choose(f.units,[](const auto& value){return !value.items.empty();})];
        const auto index=static_cast<std::size_t>(bounded(u.items.size()));
        result.push_back(std::move(u.items[index]));u.items.erase(u.items.begin()+index);
        ++u.drawn;++f.drawn;--remaining_;
    }
    return result;
}
std::size_t WishRound::remaining() const noexcept {return remaining_;}
}
