#pragma once
#include "ColorHint.h"
#include "color/ColorPreferenceReranker.h"
#include <string>

namespace harmony::ui {
struct WhyExplanation {std::vector<std::string> sentences;};
inline std::string generatedVariationLabel(const ContinuationCandidate& candidate,session::Locale locale) {
    if(!candidate.generated)return {};
    const bool zh=locale==session::Locale::ZhCN;
    const auto& variation=candidate.generated->variation;
    if(variation=="conventional")return zh?"常规":"Conventional";
    if(variation=="rich")return zh?"丰富":"Rich";
    if(variation=="bold")return zh?"大胆":"Bold";
    return variation;
}
inline std::string generatedCandidateLabel(const ContinuationCandidate& candidate,session::Locale locale) {
    if(!candidate.generated)return {};
    return std::string(locale==session::Locale::ZhCN?"算法生成 · ":"Algorithm-generated · ")+generatedVariationLabel(candidate,locale);
}
inline std::string generatedWhy(const ContinuationCandidate& candidate,session::Locale locale) {
    if(!candidate.generated)return {};
    const bool zh=locale==session::Locale::ZhCN;
    std::string out=generatedCandidateLabel(candidate,locale);
    if(!candidate.generated->explanation.empty())out+=" · "+candidate.generated->explanation;
    if(!candidate.generated->priorId.empty()||!candidate.generated->priorLicense.empty()) {
        out+=zh?"。先验：":". Prior: ";out+=candidate.generated->priorId;
        if(!candidate.generated->priorLicense.empty())out+=" · "+candidate.generated->priorLicense;
    }
    return out;
}
inline std::string realizationWhy(const ContinuationCandidate& candidate,session::Locale locale) {
    bool bass=false,tones=false;
    for(const auto& event:candidate.continuation)if(event.harmonicData){
        const auto pitches=chordPitches(*event.harmonicData);
        bass|=pitches.bass!=pitches.root;tones|=pitches.exactIntervals;
    }
    std::string out;
    if(bass)out+=std::string(localization::text(locale,"whyV4.bass"));
    if(tones){if(!out.empty())out+=' ';out+=std::string(localization::text(locale,"whyV4.pitchSet"));}
    return out;
}
inline WhyExplanation explainWhy(std::string functional,const ColorHint& hint,session::Locale locale,
    color::Preference preference,std::optional<color::RankReason> reason={},bool promoted=false) {
    WhyExplanation out;out.sentences.push_back(std::move(functional));
    const auto text=[&](const std::string& key){return std::string(localization::text(locale,key));};
    const bool insufficient=reason&&(*reason==color::RankReason::InsufficientColor||*reason==color::RankReason::InsufficientSource);
    if(!hint.explanation.empty()&&!(preference!=color::Preference::Off&&insufficient&&!hint.staticOnly))out.sentences.push_back(hint.explanation);
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
