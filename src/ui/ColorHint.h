#pragma once
#include "color/HarmonyColorAnalyzer.h"
#include "localization/Localization.h"
#include <algorithm>

namespace harmony::ui {
enum class ColorTone { Neutral, Warm, Cool };
struct ColorHint { ColorTone tone{ColorTone::Neutral};std::string label,explanation;std::vector<std::string> explanations; };
inline ColorHint colorHint(const color::PathColor& path,session::Locale locale,bool enabled=true) {
    ColorHint out;if(!enabled)return out;
    const auto text=[&](const char* key){return std::string(localization::text(locale,key));};
    const auto explain=[&](const char* key){out.explanations.push_back(text(key));if(!out.explanation.empty())out.explanation+=' ';out.explanation+=text(key);};
    if(path.status!=color::Status::Known||!path.meanW) {
        const bool uncertain=path.status==color::Status::Uncertain;
        out.label=text(uncertain?"color.uncertain":"color.unknown");
        const bool missingBass=std::any_of(path.positions.begin(),path.positions.end(),[](const auto& p){return p.chord.reason==color::Reason::MissingBass;});
        explain(missingBass?"color.whyMissingBass":uncertain?"color.whyUncertain":"color.whyUnknown");return out;
    }
    const bool warm=*path.meanW>=1.,cool=*path.meanW<=-1.;
    out.tone=warm?ColorTone::Warm:cool?ColorTone::Cool:ColorTone::Neutral;
    out.label=text(path.warming?"color.warming":path.cooling?"color.cooling":path.temperatureTurn?"color.turn":
        warm?"color.warm":cool?"color.cool":"color.balanced");
    out.explanations.push_back(text(warm?"whyV2.meanWarm":cool?"whyV2.meanCool":"whyV2.meanBalanced"));
    if(path.warming)out.explanations.push_back(text("whyV2.trendWarm"));
    else if(path.cooling)out.explanations.push_back(text("whyV2.trendCool"));
    else if(path.temperatureTurn)out.explanations.push_back(text("whyV2.turn"));
    const auto add=[&](const char* label,const char* why){out.label+=" · "+text(label);out.explanations.push_back(text(why));};
    if(path.tensionReleasing)add("color.release","whyV2.release");
    else if(path.tensionRising)add("color.rising","whyV2.rising");
    else if(path.maxTs&&*path.maxTs>=8.)add("color.contrast","whyV2.contrast");
    out.explanation.clear();
    for(const auto& part:out.explanations){if(!out.explanation.empty())out.explanation+=text("whyV2.clause");out.explanation+=part;}
    out.explanation+=text("whyV2.period");
    return out;
}
}
