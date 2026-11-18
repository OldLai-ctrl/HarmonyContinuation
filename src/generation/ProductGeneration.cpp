#include "ProductGeneration.h"
#include "GenerationPrior.h"
#include "core/ContextualContinuation.h"
#include <set>
#include <sstream>
namespace harmony::generation {
const StylePrior& productStylePrior(){static const auto prior=parseStylePrior(hcGenerationPrior);return prior;}
void appendProductGeneration(RecommendationSet& output,const Progression& input,const AnalysisContext& context,
    const RecommendationRequest& request,const std::function<bool()>& cancelled){
    if(input.empty()||input.size()>120)return;
    if(output.inspirationOrder.empty())output.inspirationOrder=mixedInspirationOrder(output);
    std::vector<RecommendationPosition> positions;
    std::set<std::string> seen;
    for(const auto& group:output.groups)for(const auto& candidate:group)seen.insert(continuationMusicFingerprint(candidate));
    for(auto level:{VariationLevel::Conventional,VariationLevel::Rich,VariationLevel::Bold}){
        if(cancelled&&cancelled())return;
        GenerationRequest generation;generation.input=input;generation.context=context;generation.constraints=request.constraints;
        generation.tendency=request.tendency;generation.stylePrior=productStylePrior();generation.variation=level;
        generation.resultsPerIntent=1;generation.cancelled=cancelled;
        const auto result=generate(generation);if(!result||result.phrases.empty())continue;
        const auto& phrase=result.phrases.front();auto candidate=previewCandidate(phrase);
        std::ostringstream why;
        why<<"依据流行音乐标注中的短上下文连接统计，先规划"<<phrase.durationQN/4.
            <<"小节后句、和弦内低音与句尾暂歇。当前调中心采用"<<formatKey(phrase.originCenter)<<"解释。";
        if(level==VariationLevel::Conventional)why<<"使用调内三和弦保持功能方向。";
        else if(level==VariationLevel::Rich)why<<"在功能骨架内细分准备与抵达，加入有限七和弦及转位。";
        else why<<"允许一次有前后连接统计支持的次属准备，并立即抵达目标：";
        if(level==VariationLevel::Bold&&phrase.plan)for(std::size_t i=0;i+1<phrase.plan->slots.size();++i)
            if(phrase.plan->slots[i].appliedDominant)why<<phrase.phrase[i].name<<" → "<<phrase.phrase[i+1].name<<"。";
        why<<"具体句长、变奏范围和配音为生成规则，完整后句由程序组合。";
        candidate.generated=GeneratedCandidateMetadata{"g131-corpus-plan-v1",productStylePrior().id,
            productStylePrior().license,name(level),why.str(),phrase.bassMidi};
        candidate.baseQualified=true;candidate.hardConstraintsPassed=phrase.hardConstraintsPassed;
        candidate.structureQualified=true;candidate.structurePriority=1;candidate.structureReason=phrase.structureExplanation;
        candidate.connectionReason=why.str();candidate.ending=phrase.ending;candidate.endingEvidence=phrase.endingExplanation;
        candidate.originCenter=phrase.originCenter;candidate.direction=CreativeDirection::Stable;
        candidate.family="generated-"+std::string(name(level));candidate.developmentMechanism=candidate.family;
        candidate.seamQN=phrase.seamQN;candidate.sourceAnchorQN=input.front().startQN;
        if(!seen.insert(continuationMusicFingerprint(candidate)).second)continue;
        positions.push_back({1,output.groups[1].size()});output.groups[1].push_back(std::move(candidate));
    }
    output.inspirationOrder.insert(output.inspirationOrder.begin()+std::min<std::size_t>(3,output.inspirationOrder.size()),positions.begin(),positions.end());
}
}
