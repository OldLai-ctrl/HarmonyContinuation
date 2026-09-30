#include "VoiceLeadingMetrics.h"
#include "ChordVoicer.h"
#include "PreviewSequence.h"
#include <algorithm>
#include <cmath>

namespace harmony::preview {
namespace {
VoiceLeadingTransition transition(const Voicing& previous, const Voicing& current) {
    VoiceLeadingTransition out;
    const int count=std::min(previous.upperCount,current.upperCount);
    for(int i=0;i<current.upperCount;++i) {
        if(std::find(previous.upper.begin(),previous.upper.begin()+previous.upperCount,current.upper[i])!=
           previous.upper.begin()+previous.upperCount) ++out.commonToneCount;
        if((i && current.upper[i]<=current.upper[i-1]) || current.upper[i]<=current.bass)
            ++out.voiceCrossingPenalty;
    }
    out.commonToneRatio=static_cast<float>(out.commonToneCount)/
        static_cast<float>(std::max(1,std::min(previous.upperCount,current.upperCount)));
    const int bassDelta=current.bass-previous.bass;
    out.bassMotionSemitones=std::abs(bassDelta);
    out.bassDirection=(bassDelta>0)-(bassDelta<0);
    for(int i=0;i<count;++i) {
        const int leap=std::abs(current.upper[i]-previous.upper[i]);
        out.upperVoiceTotalMotion+=leap;
        out.maximumVoiceLeap=std::max(out.maximumVoiceLeap,leap);
    }
    const float bassSmooth=1.f-std::min(out.bassMotionSemitones,12)/12.f;
    const float upperSmooth=1.f-std::min(out.upperVoiceTotalMotion,12*std::max(1,count))/
        static_cast<float>(12*std::max(1,count));
    const float largeLeap=std::min(1.f,std::max(0,out.maximumVoiceLeap-7)/12.f);
    out.score=std::clamp(0.50f+0.18f*out.commonToneRatio+0.15f*bassSmooth+
        0.17f*upperSmooth-0.10f*std::min(1,out.voiceCrossingPenalty)-0.05f*largeLeap,0.f,1.f);
    return out;
}
} // namespace

std::optional<VoiceLeadingMetrics> measureVoiceLeading(const Progression& progression) {
    if(progression.empty()||progression.size()>64)return std::nullopt;
    VoiceLeadingMetrics result;
    ChordVoicer voicer;
    std::optional<Voicing> previous;
    float scoreSum{};
    for(const auto& event:progression) {
        const auto chord=chordFromLabel(event.name,event.quality);
        if(chord.root<0||chord.bass<0||chord.intervals==0)return std::nullopt;
        const auto current=voicer.voice(chord);
        result.bassMidi.push_back(current.bass);
        if(previous) {
            auto edge=transition(*previous,current);
            result.commonToneCount+=edge.commonToneCount;
            result.commonToneRatio+=edge.commonToneRatio;
            result.bassMotionSemitones+=edge.bassMotionSemitones;
            result.upperVoiceTotalMotion+=edge.upperVoiceTotalMotion;
            result.maximumVoiceLeap=std::max(result.maximumVoiceLeap,edge.maximumVoiceLeap);
            result.voiceCrossingPenalty+=edge.voiceCrossingPenalty;
            scoreSum+=edge.score;
            result.transitions.push_back(edge);
        }
        previous=current;
    }
    if(!result.transitions.empty()) {
        result.commonToneRatio/=static_cast<float>(result.transitions.size());
        result.score=scoreSum/static_cast<float>(result.transitions.size());
        const int delta=result.bassMidi.back()-result.bassMidi.front();
        result.bassDirection=(delta>0)-(delta<0);
    }
    return result;
}
} // namespace harmony::preview
