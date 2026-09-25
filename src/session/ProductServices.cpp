#include "ProductServices.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <sstream>

namespace harmony::session {
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
        if (!std::isfinite(c.rankingScore) || c.rankingScore<absoluteMinimumScore ||
            (relativeToGroupBest>0 && c.rankingScore<best-relativeToGroupBest)) continue;
        if (minimumDiversityDistance>0 && std::any_of(visible.begin(),visible.end(),[&](std::size_t chosen){
            return pathDistance(c,group[chosen])<minimumDiversityDistance; })) continue;
        visible.push_back(i);
    }
    return visible;
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
        double hold=candidate.suggestedCurrentChordDurationQN.value_or(4.0);
        if (!std::isfinite(hold) || hold<=0 || hold>128) { result.error="invalid OPEN hold"; return result; }
        open.durationQN=hold; open.openEnded=false;
        double cursor=open.startQN+hold;
        for (const auto& suggested:candidate.continuation) {
            if (!std::isfinite(suggested.durationQN) || suggested.durationQN<=0 || suggested.durationQN>128) {
                result.error="invalid continuation duration"; return result;
            }
            ChordEvent event; event.name=suggested.label; event.startQN=cursor;
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
        for (std::size_t i=0;i<item.full.size();++i) item.full[i].durationQN=phrase[i].durationQN;
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
