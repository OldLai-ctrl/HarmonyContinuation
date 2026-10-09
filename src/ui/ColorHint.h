#pragma once
#include "color/HarmonyColorAnalyzer.h"
#include "localization/Localization.h"

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
        explain(uncertain?"color.whyUncertain":"color.whyUnknown");return out;
    }
    const bool warm=*path.meanW>=1.,cool=*path.meanW<=-1.;
    out.tone=warm?ColorTone::Warm:cool?ColorTone::Cool:ColorTone::Neutral;
    out.label=text(warm?"color.warm":cool?"color.cool":"color.balanced");
    explain(warm?"color.whyWarm":cool?"color.whyCool":"color.whyBalanced");
    const auto add=[&](const char* label,const char* why){out.label+=" · "+text(label);explain(why);};
    if(path.warming)add("color.warming","color.whyWarming");
    else if(path.cooling)add("color.cooling","color.whyCooling");
    else if(path.tensionReleasing)add("color.release","color.whyRelease");
    else if(path.tensionRising)add("color.rising","color.whyRising");
    else if(path.temperatureTurn)add("color.turn","color.whyTurn");
    else if(path.maxTs&&*path.maxTs>=8.)add("color.contrast","color.whyContrast");
    // The last connection is reported separately from the whole-path mean/trend.
    if(path.endingDeltaW&&*path.endingDeltaW>=1.)explain("color.whyEndingWarmer");
    else if(path.endingDeltaW&&*path.endingDeltaW<=-1.)explain("color.whyEndingCooler");
    if(path.tensionReleasing&&path.warming)explain("color.whyRelease");
    explain("color.modelNote");return out;
}
}
