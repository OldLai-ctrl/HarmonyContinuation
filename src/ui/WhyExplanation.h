#pragma once
#include "ColorHint.h"
#include "color/ColorPreferenceReranker.h"

namespace harmony::ui {
struct WhyExplanation {std::vector<std::string> sentences;};
inline WhyExplanation explainWhy(std::string functional,const ColorHint& hint,session::Locale locale,
    color::Preference preference,std::optional<color::RankReason> reason={},bool promoted=false) {
    WhyExplanation out;out.sentences.push_back(std::move(functional));
    const auto text=[&](const std::string& key){return std::string(localization::text(locale,key));};
    const bool insufficient=reason&&(*reason==color::RankReason::InsufficientColor||*reason==color::RankReason::InsufficientSource);
    if(!hint.explanation.empty()&&!(preference!=color::Preference::Off&&insufficient))out.sentences.push_back(hint.explanation);
    if(preference!=color::Preference::Off&&reason) {
        if(*reason==color::RankReason::Matched)out.sentences.push_back(text(promoted?"whyV2.rankPromoted":"whyV2.rankCompared")+
            text(color::preferenceKey(preference))+text(promoted?"whyV2.rankPromotedEnd":"whyV2.rankComparedEnd"));
        else out.sentences.push_back(text(color::rankReasonKey(*reason)));
    }
    return out;
}
inline std::string functionalWhy(IntentCompletionResult completion,session::Locale locale) {
    return std::string(localization::text(locale,std::string("whyV2.")+completionReasonKey(completion.reason)));
}
inline std::string functionalWhy(const enrichment::EnrichmentCandidate& candidate,session::Locale locale) {
    const auto text=[&](const std::string& key){return std::string(localization::text(locale,key));};
    std::vector<enrichment::TechniqueID> techniques;
    for(const auto& op:candidate.operations)if(std::find(techniques.begin(),techniques.end(),op.technique)==techniques.end()) {
        techniques.push_back(op.technique);if(techniques.size()==2)break;
    }
    if(techniques.empty())return text("whyV2.enrichmentFallback");
    std::string result=text("whyV2.enrichmentStart")+text("technique."+std::string(enrichment::techniqueName(techniques[0])));
    if(techniques.size()>1)result+=text("whyV2.and")+text("technique."+std::string(enrichment::techniqueName(techniques[1])));
    return result+text("whyV2.enrichmentEnd");
}
}
