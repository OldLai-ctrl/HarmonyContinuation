#include "ProductServices.h"
#include "persistence/ChordData.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace harmony::session {
std::string recommendationDiagnostics(const Progression& p,const AnalysisContext& ctx,
    const RecommendationRequest& request,const RecommendationSet& set) {
    std::ostringstream out;out<<std::setprecision(17);
    out<<"\nRECOMMENDATION_DIAGNOSTICS_V1\nOrigin: current instance input; synthetic status is not inferred from XML sourceApp.\n";
    out<<"ACTUAL retrievalMode="<<int(set.retrievalTimingMode)<<" HarmonyFirst="
        <<(set.retrievalTimingMode==RetrievalTimingMode::HarmonyFirst)<<" Phase3A="<<set.phase3aEnabled
        <<" Abstract="<<set.abstractChordCompatibility
        <<" contextMode="<<int(ctx.retrievalTimingMode)<<" meter="<<ctx.timeSigNumerator.value_or(0)<<'/'<<ctx.timeSigDenominator.value_or(0)
        <<" timingPolicy=matched input QN, then uniform session meter when no QN evidence, then prototype; live host meter is reference only; beat grouping unknown\n";
    const auto analysis=analyzeHarmony(p,ctx);
    out<<"SELECTED_KEY "<<(analysis.selectedKey?formatKey(analysis.selectedKey->key):"UNKNOWN")<<'\n';
    for(const auto& k:analysis.keyCandidates)out<<"KEY "<<formatKey(k.key)<<" score="<<k.score<<" confidence="<<k.confidence<<'\n';
    for(std::size_t i=0;i<analysis.full.size();++i) {
        const auto& e=analysis.full[i];
        out<<"STRUCTURE "<<i<<" weight="<<e.structuralWeight<<" retrievalWeight="<<e.retrievalStructuralWeight
            <<" roles="<<e.roles<<" retrievalRoles="<<e.retrievalRoles<<" skeleton="
            <<(std::find(analysis.skeletonIndices.begin(),analysis.skeletonIndices.end(),i)!=analysis.skeletonIndices.end())<<'\n';
    }
    out<<"Key="<<(ctx.forcedKey?formatKey(*ctx.forcedKey):"AUTO")<<" Style="<<(request.style?std::to_string(int(*request.style)):"AUTO")
        <<" Intent="<<(request.preferredIntent?std::to_string(int(*request.preferredIntent)):"AUTO")<<" Tendency="<<int(request.tendency)<<'\n';
    for(std::size_t i=0;i<p.size();++i) {
        const auto& e=p[i];const auto n=normalizeChord(e);
        out<<"INPUT "<<i<<" source="<<int(e.source)<<" name="<<std::quoted(e.name)<<" rawName="<<std::quoted(e.rawName.value_or(""))
            <<" keyNote="<<std::quoted(e.rawKeyNote.value_or(""))<<" bassNote="<<std::quoted(e.rawBassNote.value_or(""))
            <<" pitches="<<std::quoted(e.extensions.pitches.value_or(""))<<" mask="<<std::quoted(e.extensions.mask.value_or(""))
            <<" projectTime="<<std::quoted(e.rawProjectTime.value_or(""))<<" domain="<<std::quoted(e.rawTimeDomain.value_or(""))
            <<" root="<<(n.root?int(*n.root):-1)<<" quality="<<int(n.quality)<<" bass="<<(n.bass?int(*n.bass):-1)
            <<" intervalMask="<<n.intervalMask<<" colorMask="<<n.colorMask<<" matchingMask="<<n.matchingIntervalMask
            <<" startQN="<<e.startQN<<" durationQN="<<e.durationQN.value_or(-1)<<" OPEN="<<e.openEnded
            <<" symbolic="<<n.symbolicHarmony<<" rootSource="<<int(n.rootSource)<<" qualitySource="<<int(n.qualitySource)
            <<" bassSource="<<int(n.bassSource)<<" performedBass="<<(n.performedBass?int(*n.performedBass):-1)
            <<" confidence="<<n.confidence<<" uncertaintyCost="<<n.compatibilityUncertainty<<" conflict="<<n.fieldConflict<<'\n';
        for(const auto& d:n.diagnostics)out<<"  warning="<<std::quoted(d)<<'\n';
    }
    const auto& s=set.stats;
    out<<"STAGES imported="<<s.imported<<" analyzed="<<s.analyzed<<" keys="<<s.keys<<" shortlist="<<s.shortlistSize
       <<" aligned="<<s.aligned<<" similarity="<<s.similarityPassed<<" tail="<<s.tailPassed<<" realization="<<s.realizationPassed
       <<" path="<<s.pathPassed<<" distinct="<<s.distinct<<" score="<<s.scorePassed<<" workerOutput="<<s.output<<'\n';
    for(const auto& m:set.matches) {
        out<<"MATCH "<<m.templateId<<" key="<<formatKey(m.key)<<" similarity="<<m.similarity<<" rawCost="<<m.rawCost
           <<" rhythmScore="<<m.subScores.rhythmSimilarity<<" continuationStartIndex="<<m.continuationStartIndex
           <<" candidateScore="<<m.candidateRankingScore<<" eligibility="<<m.candidateEligibilityScore
           <<" full="<<m.subScores.fullHarmony<<" skeleton="<<m.subScores.skeletonHarmony<<" disposition="<<m.disposition
           <<" timingSource="<<m.timingSource<<'\n';
        for(const auto& a:m.alignmentTrace)out<<"  ALIGN q="<<(a.queryIndex?int(*a.queryIndex):-1)<<" t="<<(a.templateIndex?int(*a.templateIndex):-1)
            <<" op="<<alignmentOpName(a.operation)<<" cost="<<a.cost<<" reasons="<<reasonNames(a.reasons)<<'\n';
    }
    const auto shown=presentationIndices(set,{});
    for(std::size_t g=0;g<set.groups.size();++g)for(std::size_t i=0;i<set.groups[g].size();++i) {
        const auto& c=set.groups[g][i];out<<"CANDIDATE group="<<g<<" rank="<<i+1<<" primary="<<c.primaryTemplate
          <<" score="<<c.rankingScore<<" rhythm="<<c.subscores.rhythm<<" DEFAULT_VISIBLE="
          <<(std::find(shown[g].begin(),shown[g].end(),i)!=shown[g].end())
          // Show More lists every retained worker candidate (MainView), without
          // the default presentation's score/diversity/path filtering.
          <<" MORE_ACCESSIBLE=1 sources=";
        for(const auto& id:c.supportingTemplates)out<<id<<',';
        out<<" rhythmScale="<<c.rhythmScale<<" rhythmConfidence="<<c.rhythmConfidence<<" OPENhold="<<c.suggestedCurrentChordDurationQN.value_or(-1)
            <<" path=";for(const auto& e:c.continuation)out<<e.label<<'@'<<e.durationQN<<"QN -> ";out<<'\n';
    }
    return out.str();
}
std::string continuationFingerprint(const ContinuationCandidate& c) {
    std::ostringstream out;
    const auto sized=[&](const std::string& value){out << value.size() << ':' << value;};
    out << static_cast<int>(c.key.tonic) << ':' << static_cast<int>(c.key.mode) << ':'
        << static_cast<int>(c.intent) << ':' << static_cast<int>(c.cadence) << ':';
    sized(c.primaryTemplate); out << ':';
    if (c.suggestedCurrentChordDurationQN) out << std::bit_cast<std::uint64_t>(*c.suggestedCurrentChordDurationQN);
    else out << '-';
    out << ':' << c.continuation.size() << ':';
    for (const auto& event : c.continuation) {
        out << event.degree.degree << ':' << event.degree.alteration << ':';
        sized(event.label);
        out << ':' << static_cast<int>(event.quality) << ':'
            << std::bit_cast<std::uint64_t>(event.durationQN) << ';';
        if(event.harmonicData)sized(persistence::encodeChords({*event.harmonicData}));
    }
    return out.str();
}
namespace {
float pathDistance(const ContinuationCandidate& a,const ContinuationCandidate& b) {
    const auto count=std::max(a.continuation.size(),b.continuation.size());
    if (!count) return 0.f;
    float distance{};
    for (std::size_t i=0;i<count;++i) {
        if (i>=a.continuation.size() || i>=b.continuation.size()) { distance+=1.f; continue; }
        const auto& x=a.continuation[i]; const auto& y=b.continuation[i];
        float diff{};
        if (x.degree.degree!=y.degree.degree || x.degree.alteration!=y.degree.alteration) diff+=0.65f;
        if (x.quality!=y.quality) diff+=0.2f;
        const auto ratio=std::min(x.durationQN,y.durationQN)/std::max(0.001,std::max(x.durationQN,y.durationQN));
        diff+=static_cast<float>((1.0-ratio)*0.15);
        distance+=diff;
    }
    return distance/static_cast<float>(count);
}
}
std::vector<std::size_t> RecommendationVisibilityPolicy::visibleIndices(const std::vector<ContinuationCandidate>& group) const {
    std::vector<std::size_t> visible;
    if (group.empty() || maxPerGroup==0) return visible;
    float best=group.front().rankingScore;
    for (const auto& candidate:group) best=std::max(best,candidate.rankingScore);
    for (std::size_t i=0;i<group.size() && visible.size()<maxPerGroup;++i) {
        const auto& c=group[i];
        // Constraint penalties describe melody fit, not the underlying harmony
        // gate. Soft conflicts must remain visible rather than becoming empty UI.
        const float harmonyScore=c.rankingScore+(c.constraints.melody.empty()?0.f:
            melodySoftPenaltyPoints*(1.f-c.melodyCompatibility.score));
        if (!std::isfinite(c.rankingScore) || harmonyScore<absoluteMinimumScore ||
            (relativeToGroupBest>0 && c.rankingScore<best-relativeToGroupBest)) continue;
        if (minimumDiversityDistance>0 && std::any_of(visible.begin(),visible.end(),[&](std::size_t chosen){
            return pathDistance(c,group[chosen])<minimumDiversityDistance; })) continue;
        visible.push_back(i);
    }
    return visible;
}
namespace {
bool sameContinuationPath(const ContinuationCandidate& a,const ContinuationCandidate& b) {
    if(a.suggestedCurrentChordDurationQN!=b.suggestedCurrentChordDurationQN||
       a.continuation.size()!=b.continuation.size())return false;
    for(std::size_t i=0;i<a.continuation.size();++i) {
        const auto& x=a.continuation[i];const auto& y=b.continuation[i];
        if(x.label!=y.label||x.quality!=y.quality||x.durationQN!=y.durationQN)return false;
    }
    return true;
}
}
RecommendationPresentation presentationIndices(const RecommendationSet& set,
    const RecommendationVisibilityPolicy& policy) {
    RecommendationPresentation shown;
    std::array<std::size_t,4> order{0,1,2,3};
    std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b) {
        const auto score=[&](std::size_t group) {
            return set.groups[group].empty()?-1.f:set.groups[group].front().rankingScore;
        };
        return score(a)>score(b);
    });
    auto all=policy;
    all.maxPerGroup=100;
    for(const auto group:order) {
        const auto eligible=all.visibleIndices(set.groups[group]);
        for(const auto index:eligible) {
            const auto& candidate=set.groups[group][index];
            const bool sameGroup=std::any_of(shown[group].begin(),shown[group].end(),[&](std::size_t chosen) {
                return sameContinuationPath(candidate,set.groups[group][chosen]);
            });
            if(sameGroup)continue;
            bool inOtherGroup{};
            for(std::size_t other=0;other<shown.size();++other)if(other!=group)
                for(const auto chosen:shown[other])
                    if(sameContinuationPath(candidate,set.groups[other][chosen]))inOtherGroup=true;
            if(inOtherGroup)continue;
            shown[group].push_back(index);
            if(shown[group].size()>=policy.maxPerGroup)break;
        }
        // Keep one valid result when a group has no distinct alternative.
        if(shown[group].empty()&&!eligible.empty()&&policy.maxPerGroup>0)
            shown[group].push_back(eligible.front());
    }
    return shown;
}
SaveResult makeUserProgression(const ImportedProgressionSession& session,
                               const ContinuationCandidate& candidate, const SaveMetadata& metadata) {
    SaveResult result;
    if (session.events.empty() || candidate.continuation.empty() || metadata.name.empty() || metadata.name.size()>128 ||
        session.events.size()+candidate.continuation.size()>64) { result.error="invalid recommendation or name"; return result; }
    try {
        Progression phrase=session.events;
        const auto anchor=phrase.front().startQN;
        for (auto& event:phrase) event.startQN-=anchor;
        auto& open=phrase.back();
        double hold=(!open.openEnded&&open.durationQN)?*open.durationQN:
            candidate.suggestedCurrentChordDurationQN.value_or(4.0);
        if (!std::isfinite(hold) || hold<=0 || hold>128) { result.error="invalid OPEN hold"; return result; }
        open.durationQN=hold; open.openEnded=false;
        double cursor=open.startQN+hold;
        for (const auto& suggested:candidate.continuation) {
            if (!std::isfinite(suggested.durationQN) || suggested.durationQN<=0 || suggested.durationQN>128) {
                result.error="invalid continuation duration"; return result;
            }
            ChordEvent event=suggested.harmonicData.value_or(ChordEvent{}); event.name=suggested.label; event.startQN=cursor;
            event.durationQN=suggested.durationQN; event.openEnded=false;
            event.quality=suggested.quality; phrase.push_back(std::move(event)); cursor+=suggested.durationQN;
        }
        AnalysisContext context; context.forcedKey=candidate.key;
        const auto query=makeMatchQuery(phrase,context);
        if (query.interpretations.empty()) { result.error="cannot analyze saved phrase"; return result; }
        static std::atomic<std::uint64_t> sequence{0};
        const auto ticks=std::chrono::steady_clock::now().time_since_epoch().count();
        auto& item=result.item;
        item.id="USER_"+std::to_string(ticks)+"_"+std::to_string(++sequence);
        item.name=metadata.name; item.sourceType="user"; item.mode=candidate.key.mode;
        item.full=query.interpretations.front().full;
        item.skeletonIndices=query.interpretations.front().skeletonIndices;
        item.phraseLength=item.full.size(); item.intent=metadata.intent;
        item.cadence=candidate.cadence; item.loopable=candidate.intent==PhraseIntent::Loop;
        if (metadata.style) { item.styles=static_cast<StyleFlags>(*metadata.style); item.styleWeights.emplace_back(*metadata.style,1.f); }
        item.tags=metadata.tags;
        item.note=metadata.note;
        item.rawChords=phrase; item.rawChordKey=candidate.key;
        for (std::size_t i=0;i<item.full.size();++i) {
            auto& event=item.full[i];event.durationQN=phrase[i].durationQN;
            const auto chord=chordPitches(phrase[i]);
            if(chord.root<0||chord.bass<0||!chord.intervals)throw std::runtime_error("unsupported saved chord");
            event.bassInterval=(chord.bass-chord.root+12)%12;event.intervalMask=chord.intervals;event.colorMask=chord.colorMask;
            auto name=phrase[i].name.substr(0,phrase[i].name.find('/'));
            std::size_t rootLength=1;if(name.size()>1&&(name[1]=='#'||name[1]=='b'))rootLength=2;
            if(name.size()>=rootLength)event.displaySuffix=name.substr(rootLength);
        }
        prepareTemplate(item);
    } catch (const std::exception& e) { result.error=e.what(); result.item={}; }
    return result;
}
bool saveRecommendation(library::UserLibrary& library, const ImportedProgressionSession& session,
                        const ContinuationCandidate& candidate, const SaveMetadata& metadata, std::string& error) {
    auto converted=makeUserProgression(session,candidate,metadata);
    if (!converted) { error=converted.error; return false; }
    return library.addProgression(converted.item,error);
}
} // namespace harmony::session
