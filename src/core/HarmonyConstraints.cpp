#include "HarmonyConstraints.h"
#include "ChordPitchSet.h"
#include "ChordVoicer.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>
#include <stdexcept>

namespace harmony {
namespace {
int pc(int value){return (value%12+12)%12;}
}
bool validConstraints(const HarmonyConstraintSet& set) noexcept {
    if(set.melody.size()>128)return false;
    for(const auto& e:set.melody)
        if(!std::isfinite(e.startQN)||std::abs(e.startQN)>1e9||!std::isfinite(e.durationQN)||e.durationQN<=0||e.durationQN>512||
           !std::isfinite(e.startQN+e.durationQN)||e.pitchClass<0||e.pitchClass>11||
           (e.pitch&&(*e.pitch<0||*e.pitch>127||*e.pitch%12!=e.pitchClass))||
           static_cast<unsigned>(e.role)>1||static_cast<unsigned>(e.strictness)>1)return false;
    return true;
}
MelodyRelation classifyMelody(const ChordEvent& chord,int pitchClass) {
    return classifyMelody(chordPitches(chord),pitchClass);
}
MelodyRelation classifyMelody(const ChordPitchSet& tones,int pitchClass) {
    if(tones.root<0||!tones.intervals)return MelodyRelation::Uncertain;
    const int interval=pc(pitchClass-tones.root);
    if(tones.intervals&(1u<<interval))
        return tones.colorMask&(1u<<interval)?MelodyRelation::AvailableTension:MelodyRelation::ChordTone;
    switch(tones.quality) {
    case ChordQuality::Major:case ChordQuality::Major7:
        if(interval==2||interval==9||interval==11)return MelodyRelation::AvailableTension;
        if(interval==5||interval==6)return MelodyRelation::Uncertain;
        break;
    case ChordQuality::Minor:case ChordQuality::Minor7:
        if(interval==2||interval==5||interval==10)return MelodyRelation::AvailableTension;
        if(interval==8||interval==9)return MelodyRelation::Uncertain;
        break;
    case ChordQuality::Dominant7:
        if(interval==2||interval==9)return MelodyRelation::AvailableTension;
        if(interval==1||interval==3||interval==5||interval==6||interval==8)return MelodyRelation::Uncertain;
        break;
    case ChordQuality::Unknown:case ChordQuality::Diminished:case ChordQuality::Diminished7:
    case ChordQuality::HalfDiminished7:case ChordQuality::Augmented:
        return MelodyRelation::Uncertain;
    default:break;
    }
    return MelodyRelation::Conflict;
}
std::optional<int> melodyTopPitch(const MelodyConstraint& e) noexcept {
    if(e.role!=ConstraintRole::TopVoice)return {};
    return e.pitch?e.pitch:std::optional<int>(72+e.pitchClass);
}
double progressionOpenDuration(const Progression& source) {
    const auto analysis=analyzeHarmony(source);std::vector<double> known,structural;
    for(std::size_t i=0;i<source.size();++i)if(source[i].durationQN&&!source[i].openEnded) {
        known.push_back(*source[i].durationQN);
        if(i<analysis.full.size()&&hasRole(analysis.full[i].roles,Role::Structural))structural.push_back(*source[i].durationQN);
    }
    if(structural.empty())structural=std::move(known);if(structural.empty())return 4.;
    std::sort(structural.begin(),structural.end());const auto mid=structural.size()/2;
    return structural.size()%2?structural[mid]:(structural[mid-1]+structural[mid])/2;
}
MelodyCompatibility evaluateMelody(const Progression& path,const HarmonyConstraintSet& set,double openDuration) {
    MelodyCompatibility result;
    if(!validConstraints(set)){result.score=0;result.hardSatisfied=false;return result;}
    if(!(openDuration>0))openDuration=progressionOpenDuration(path);
    double total{},weighted{};
    for(std::size_t i=0;i<path.size();++i) {
        const auto& chord=path[i];
        const double end=chord.startQN+chord.durationQN.value_or(
            i+1<path.size()?path[i+1].startQN-chord.startQN:openDuration);
        for(const auto& e:set.melody) {
            const double overlap=std::min(end,e.startQN+e.durationQN)-std::max(chord.startQN,e.startQN);
            if(overlap<=1e-7)continue;
            const auto relation=classifyMelody(chord,e.pitchClass);
            bool topSatisfied=true;
            if(const auto top=melodyTopPitch(e)) {
                ChordVoicer voicer;
                topSatisfied=relation!=MelodyRelation::Conflict&&voicer.voice(chordPitches(chord),top).melodySatisfied;
                for(const auto& other:set.melody)
                    if(other.role==ConstraintRole::TopVoice&&other.strictness==ConstraintStrictness::Hard&&
                       std::min({end,e.startQN+e.durationQN,other.startQN+other.durationQN})>
                       std::max({chord.startQN,e.startQN,other.startQN})+1e-7&&
                       melodyTopPitch(other)!=top)topSatisfied=false;
            }
            const float compatibility=relation==MelodyRelation::ChordTone?1.f:
                relation==MelodyRelation::AvailableTension?0.9f:relation==MelodyRelation::Uncertain?0.65f:0.f;
            weighted+=overlap*(topSatisfied?compatibility:0.f);total+=overlap;
            if(e.strictness==ConstraintStrictness::Hard&&
               (relation==MelodyRelation::Conflict||!topSatisfied))result.hardSatisfied=false;
            const auto normalized=chordPitches(chord);
            result.observations.push_back({std::max(chord.startQN,e.startQN),chord.name,e.pitchClass,
                normalized.root<0?0:pc(e.pitchClass-normalized.root),relation,topSatisfied});
        }
    }
    if(total>0)result.score=static_cast<float>(weighted/total);
    return result;
}
std::optional<MelodyConstraint> parseMelodyNote(std::string_view text) {
    if(text.empty())return {};
    const auto letter=text.front();const std::string_view letters="C D EF G A B";
    const auto found=letters.find(letter);if(found==std::string_view::npos||letter==' ')return {};
    int pitchClass=static_cast<int>(found);std::size_t at=1;
    if(at<text.size()&&(text[at]=='#'||text[at]=='b'))pitchClass+=text[at++]=='#'?1:-1;
    MelodyConstraint e;e.pitchClass=pc(pitchClass);
    if(at<text.size()) {
        int octave{};const auto parsed=std::from_chars(text.data()+at,text.data()+text.size(),octave);
        if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())return {};
        const int note=(octave+1)*12+pitchClass;
        if(note<0||note>127)return {};e.pitch=note;e.pitchClass=note%12;
    }
    return e;
}
std::string melodyNoteName(const MelodyConstraint& e) {
    constexpr const char* names[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    return std::string(names[pc(e.pitchClass)])+(e.pitch?std::to_string(*e.pitch/12-1):std::string{});
}
std::string encodeConstraints(const HarmonyConstraintSet& set) {
    if(!validConstraints(set))throw std::runtime_error("invalid melody constraints");
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    for(const auto& e:set.melody)out<<e.startQN<<' '<<e.durationQN<<' '<<e.pitch.value_or(-1)<<' '
        <<e.pitchClass<<' '<<static_cast<int>(e.role)<<' '<<static_cast<int>(e.strictness)<<';';
    return out.str();
}
HarmonyConstraintSet decodeConstraints(std::string_view input) {
    if(input.size()>32768)throw std::runtime_error("melody constraint data too large");
    HarmonyConstraintSet set;std::istringstream all{std::string(input)};all.imbue(std::locale::classic());
    std::string item;
    while(std::getline(all,item,';')) {
        std::istringstream in(item);in.imbue(std::locale::classic());MelodyConstraint e;int note{},role{},strictness{};
        if(!(in>>e.startQN>>e.durationQN>>note>>e.pitchClass>>role>>strictness))throw std::runtime_error("invalid melody constraint data");
        in>>std::ws;if(!in.eof()||role<0||role>1||strictness<0||strictness>1||note<-1)throw std::runtime_error("invalid melody constraint data");
        if(note>=0)e.pitch=note;e.role=static_cast<ConstraintRole>(role);e.strictness=static_cast<ConstraintStrictness>(strictness);
        set.melody.push_back(e);
    }
    if(!validConstraints(set))throw std::runtime_error("invalid melody constraint data");return set;
}
}
