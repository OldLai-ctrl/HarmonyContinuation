#include "PhraseGenerator.h"
#include "color/HarmonyColorAnalyzer.h"
#include "preview/VoiceLeadingMetrics.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <map>
#include <set>
#include <sstream>

namespace harmony::generation {
namespace {
int pc(int n){return (n%12+12)%12;}
int step(int a,int b){const int d=pc(b-a);return d>6?d-12:d;}
constexpr const char* notes[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
unsigned tones(const ChordPitchSet& p) {
    unsigned out{};for(int n=0;n<12;++n)if((p.intervals|p.colorMask)&(1u<<n))out|=1u<<pc(p.root+n);
    if(p.bass>=0)out|=1u<<p.bass;return out;
}
unsigned scale(KeySignature key) {
    unsigned out{};for(int n:{0,2,key.mode==Mode::Major?4:3,5,7,key.mode==Mode::Major?9:8,key.mode==Mode::Major?11:10})out|=1u<<pc(int(key.tonic)+n);
    return out;
}
float nearest(unsigned a,unsigned b) {
    float sum{};for(int n=0;n<12;++n)if(a&(1u<<n)){int best=6;for(int m=0;m<12;++m)if(b&(1u<<m))best=std::min(best,std::abs(step(n,m)));sum+=float(best);}
    return sum/float(std::max(1,std::popcount(a)));
}
float tension(unsigned mask) {
    float pairs{};for(int a=0;a<12;++a)if(mask&(1u<<a))for(int b=a+1;b<12;++b)if(mask&(1u<<b)){
        const auto d=std::abs(step(a,b));pairs+=d==1?1.f:d==6?.7f:0.f;}
    return pairs/float(std::max(1,std::popcount(mask)));
}
std::string soundKey(const ChordEvent& event,bool duration=false) {
    const auto p=chordPitches(event);std::ostringstream s;s<<p.bass<<'/'<<tones(p);
    if(duration)s<<'@'<<std::setprecision(17)<<event.durationQN.value_or(0.);return s.str();
}
std::string harmonicCore(const ChordEvent& event) {
    const auto p=chordPitches(event);return std::to_string(p.root)+":"+std::to_string(int(p.quality));
}
bool actualHardNotes(const Progression& events,const HarmonyConstraintSet& constraints) {
    for(const auto& event:events){const auto p=chordPitches(event);const auto sounding=tones(p);
        for(const auto& melody:constraints.melody)if(melody.strictness==ConstraintStrictness::Hard&&
            melody.startQN<event.startQN+event.durationQN.value_or(0.)-1e-7&&melody.startQN+melody.durationQN>event.startQN+1e-7) {
            if(!(sounding&(1u<<melody.pitchClass)))return false;
            if(melody.role==ConstraintRole::TopVoice){ChordVoicer model;if(!model.voice(p,melodyTopPitch(melody)).melodySatisfied)return false;}
        }}return true;
}
ChordEvent chord(int root,ChordQuality quality,int bass=-1,bool ninth=false) {
    root=pc(root);bass=bass<0?root:pc(bass);
    const char* suffix=quality==ChordQuality::Minor?"m":quality==ChordQuality::Major7?(ninth?"maj9":"maj7"):
        quality==ChordQuality::Minor7?(ninth?"m9":"m7"):quality==ChordQuality::Dominant7?(ninth?"9":"7"):
        quality==ChordQuality::Diminished7?"dim7":quality==ChordQuality::HalfDiminished7?"m7b5":quality==ChordQuality::Sus4?"sus4":"";
    ChordEvent e;e.name=std::string(notes[root])+suffix;e.root=PitchClass(root);e.bass=PitchClass(bass);e.quality=quality;
    auto p=chordPitches(e);unsigned mask=p.intervals|p.colorMask;
    if(quality==ChordQuality::Sus4)mask|=1u<<10; // an explicitly authored dominant suspension
    if(ninth)mask|=1u<<2;
    std::ostringstream value;value<<"0x"<<std::hex<<mask;e.extensions.mask=value.str();
    if(bass!=root)e.name+="/"+std::string(notes[bass]);
    e.durationQN=4.;e.openEnded=false;return e;
}
std::vector<ChordEvent> diatonic(KeySignature key,bool color) {
    static constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
    static constexpr ChordQuality maj[]{ChordQuality::Major7,ChordQuality::Minor7,ChordQuality::Minor7,ChordQuality::Major7,ChordQuality::Dominant7,ChordQuality::Minor7,ChordQuality::HalfDiminished7};
    static constexpr ChordQuality min[]{ChordQuality::Minor7,ChordQuality::HalfDiminished7,ChordQuality::Major7,ChordQuality::Minor7,ChordQuality::Dominant7,ChordQuality::Major7,ChordQuality::Dominant7};
    std::vector<ChordEvent> out;for(int i=0;i<7;++i){const auto q=key.mode==Mode::Major?maj[i]:min[i];
        out.push_back(chord(int(key.tonic)+(key.mode==Mode::Major?major[i]:minor[i]),q,-1,color&&q!=ChordQuality::HalfDiminished7));}return out;
}
std::vector<ChordEvent> borrowed(KeySignature key) {
    const int t=int(key.tonic);std::vector<ChordEvent> out;
    if(key.mode==Mode::Major){out={chord(t+5,ChordQuality::Minor7,-1,true),chord(t+8,ChordQuality::Major7,-1,true),
        chord(t+10,ChordQuality::Major7),chord(t+3,ChordQuality::Major7),chord(t,ChordQuality::Minor7)};}
    else out={chord(t+5,ChordQuality::Major7),chord(t+1,ChordQuality::Major7),chord(t,ChordQuality::Major7),chord(t+9,ChordQuality::Minor7)};
    return out;
}
int inferredDirection(const Progression& input) {
    int direction{},count{};const auto begin=input.size()>4?input.size()-4:0;
    for(auto i=begin+1;i<input.size();++i){const auto d=step(chordPitches(input[i-1]).bass,chordPitches(input[i]).bass);
        if(d&&std::abs(d)<=2){direction+=(d>0)-(d<0);++count;}}
    return count>=2&&std::abs(direction)>=2?(direction>0?1:-1):0;
}
std::string hashText(const std::string& text) {
    std::uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}
    std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return out.str();
}
bool sameKey(KeySignature a,KeySignature b){return a.tonic==b.tonic&&a.mode==b.mode;}
float modelMotion(const Voicing& a,const Voicing& b) {
    const auto transition=preview::measureVoiceLeadingTransition(a,b);
    return std::min(1.f,float(transition.upperVoiceTotalMotion)/float(std::max(1,std::min(a.upperCount,b.upperCount))*9)+
        float(std::max(0,transition.maximumVoiceLeap-7))/24.f);
}
double evidence(const StylePrior& prior,int before,int previous,int next) {
    double count{},total{};for(const auto& r:prior.transitions)if(r.previous2==before&&r.previous==previous){total+=r.count;if(r.next==next)count+=r.count;}
    // Prefer supported two-chord contexts; a sparse context backs off to one.
    if(before>=0&&total<8)return evidence(prior,-1,previous,next);
    if(count<=0)return before>=0?evidence(prior,-1,previous,next):0.;
    return total>0&&count>0?(count+.25)/(total+3.):0.;
}
double endingEvidence(const StylePrior& prior,int root) {
    double count{},total{};for(const auto& r:prior.positions)if(r.phase=="end"){total+=r.count;if(r.root==root)count+=r.count;}
    return total>0?(count+.25)/(total+3.):1./12.;
}
std::string family(int degree,bool applied=false){return applied?"dom":degree==2||degree==4||degree==9?"min":"maj";}
double qualityEvidence(const StylePrior& prior,int previous,const std::string& previousFamily,int next,const std::string& nextFamily) {
    double count{},total{};for(const auto& r:prior.qualityTransitions)if(r.previous==previous&&r.previousFamily==previousFamily){total+=r.count;if(r.next==next&&r.nextFamily==nextFamily)count+=r.count;}
    // Missing family observations stay unknown and receive a conservative
    // penalty; they are not silently counted as observed major/minor evidence.
    return total>0&&count>0?(count+.25)/(total+3.):0.;
}
std::string soundingFamily(const ChordPitchSet& p){switch(p.quality){case ChordQuality::Minor:case ChordQuality::Minor7:return "min";case ChordQuality::Dominant7:return "dom";case ChordQuality::Diminished:case ChordQuality::Diminished7:return "dim";case ChordQuality::HalfDiminished7:return "hdim";default:return "maj";}}
std::string functionRole(int degree) {
    return degree==0||degree==9||degree==4?"tonic-family":degree==2||degree==5?"predominant":"dominant";
}
PhrasePlan planPhrase(const StylePrior& prior,const Progression& heard,KeySignature center,VariationLevel variation,double bar,std::size_t& skeletonExpanded,std::size_t& bassExpanded) {
    PhrasePlan plan;plan.priorId=prior.id;plan.variation=variation;plan.outsideBudget=variation==VariationLevel::Bold?1:0;
    if(center.mode!=Mode::Major)throw std::runtime_error("G13.1 corpus planner currently requires an explicitly supported major-context interpretation.");
    double modal=bar,best{};for(const auto& d:prior.durations)if(d.qn>=bar/2&&d.qn<=bar&&d.songs>best){modal=d.qn;best=d.songs;}
    // Input-relative four/eight-bar response; this phrase-length choice is an
    // engineering rule, not an inferred corpus phrase-boundary annotation.
    double inputLength{};for(const auto& e:heard)inputLength+=*e.durationQN;
    const double bars=inputLength>bar*4+1e-6?8.:4.;const double duration=bars*bar;
    std::size_t count=std::clamp<std::size_t>(std::size_t(std::llround(duration/modal)),4,8);
    const double unit=duration/double(count);
    struct Skeleton{std::vector<int> roots;double score{};};std::vector<Skeleton> beam(1);
    const int last=pc(chordPitches(heard.back()).root-int(center.tonic));
    const int before=heard.size()>1?pc(chordPitches(heard[heard.size()-2]).root-int(center.tonic)):-1;
    for(std::size_t depth=0;depth<count;++depth){std::vector<Skeleton> next;
        for(const auto& path:beam)for(int degree:{0,2,4,5,7,9}){
            ++skeletonExpanded;
            const int prev=path.roots.empty()?last:path.roots.back();if(prev==degree)continue;
            const int prev2=path.roots.size()>1?path.roots[path.roots.size()-2]:path.roots.empty()?before:last;
            const double probability=evidence(prior,prev2,prev,degree);if(probability<=0)continue;
            // A sustained response ends at a supported stable family; a V
            // question would need a different explicit ending plan.
            if(depth+1==count&&degree!=0&&degree!=5&&degree!=9)continue;
            auto child=path;child.roots.push_back(degree);child.score+=std::log(probability);
            if(!prior.qualityTransitions.empty()){const auto previousFamily=path.roots.empty()?soundingFamily(chordPitches(heard.back())):family(prev);
                child.score+=.35*std::log(std::max(1e-6,qualityEvidence(prior,prev,previousFamily,degree,family(degree))));}
            if(std::find(path.roots.begin(),path.roots.end(),degree)==path.roots.end())child.score+=.18;
            if(depth+1==count)child.score+=.4*std::log(endingEvidence(prior,degree));
            // Preserve a relationship with D while avoiding exact repetition
            // as the default answer. These are engineering weights.
            if(depth<heard.size()&&degree==pc(chordPitches(heard[depth]).root-int(center.tonic)))child.score-=.22;
            next.push_back(std::move(child));}
        std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.score!=b.score?a.score>b.score:a.roots<b.roots;});
        if(next.size()>24)next.resize(24);beam=std::move(next);
    }
    if(beam.empty())throw std::runtime_error("Corpus has no supported complete response skeleton.");
    // Root plans are selected before chord realization. The richer levels
    // may expand a supported transition; no saved song phrase is retrieved.
    const auto distinct=std::find_if(beam.begin(),beam.end(),[&](const auto& candidate){
        if(candidate.roots.size()!=heard.size())return true;
        for(std::size_t i=0;i<heard.size();++i)
            if(candidate.roots[i]!=pc(chordPitches(heard[i]).root-int(center.tonic)))return true;
        return false;
    });
    if(distinct==beam.end())throw std::runtime_error("No corpus-supported response develops the input skeleton.");
    const auto& roots=distinct->roots;
    for(std::size_t i=0;i<roots.size();++i){const int prev=i?roots[i-1]:last,prev2=i>1?roots[i-2]:i?last:before;
        std::ostringstream why;why<<"Tonic-relative short-context P("<<roots[i]<<"|"<<prev2<<','<<prev<<")="<<evidence(prior,prev2,prev,roots[i]);
        plan.slots.push_back({unit,roots[i],roots[i],false,functionRole(roots[i]),why.str()});}
    if(variation!=VariationLevel::Conventional&&plan.slots.size()<8){
        // Split one anchor into preparation/arrival using observed root edges.
        double bestScore=-1e9;std::size_t at{};int preparation=-1;
        for(std::size_t i=0;i<plan.slots.size();++i){const int prev=i?roots[i-1]:last,target=roots[i];
            for(int degree:{0,2,4,5,7,9})if(degree!=target){
                ++skeletonExpanded;
                const bool applied=variation==VariationLevel::Bold&&degree==pc(target+7)&&target!=0;
                if(variation==VariationLevel::Bold&&!applied)continue;
                if(degree==prev&&!applied)continue;
                const double a=evidence(prior,-1,prev,degree),b=evidence(prior,-1,degree,target);if(a<=0||b<=0)continue;
                // A theory-derived applied dominant is allowed only on a root
                // transition that the actual corpus supports in both directions.
                const double q=qualityEvidence(prior,degree,family(degree,applied),target,family(target));
                if(applied&&q<=0)continue;
                const auto incomingFamily=i?family(prev):soundingFamily(chordPitches(heard.back()));
                if(applied&&qualityEvidence(prior,prev,incomingFamily,degree,"dom")<=0)continue;
                const double value=std::log(a)+std::log(b)+(degree==2&&target==7?.4:0.)+
                    (prior.qualityTransitions.empty()?0.:.35*std::log(std::max(1e-6,q)));
                if(value>bestScore){bestScore=value;at=i;preparation=degree;}}
        }
        if(preparation>=0){auto& anchor=plan.slots[at];anchor.durationQN/=2.;
            const bool applied=variation==VariationLevel::Bold&&preparation==pc(anchor.rootDegree+7)&&anchor.rootDegree!=0;
            PlannedSlot inserted{anchor.durationQN,preparation,anchor.rootDegree,applied,applied?"applied-dominant-preparation":"corpus-supported-preparation",
                "Two observed one-chord root transitions; subdivision and applied-dominant quality are engineering variation rules."};
            plan.slots.insert(plan.slots.begin()+static_cast<std::ptrdiff_t>(at),std::move(inserted));}
    }
    struct BassPath{std::vector<int> notes;double cost{};};std::vector<BassPath> bassBeam(1);
    ChordVoicer bassModel;Voicing dVoice;for(const auto& e:heard)dVoice=bassModel.voice(chordPitches(e));
    for(std::size_t i=0;i<plan.slots.size();++i){std::vector<BassPath> next;const auto& slot=plan.slots[i];
        const bool minor=(slot.rootDegree==2||slot.rootDegree==4||slot.rootDegree==9)&&!slot.appliedDominant;
        for(const auto& path:bassBeam)for(int midi=36;midi<=55;++midi){const int interval=pc(midi-int(center.tonic)-slot.rootDegree);
            ++bassExpanded;
            const bool rootOnly=variation==VariationLevel::Conventional||i+1==plan.slots.size()||slot.appliedDominant;
            if(interval!=0&&(rootOnly||(interval!=(minor?3:4)&&interval!=7)))continue;
            const int previous=path.notes.empty()?dVoice.bass:path.notes.back(),delta=midi-previous;
            if(std::abs(delta)>plan.maximumBassLeap)continue;auto child=path;child.notes.push_back(midi);
            child.cost+=std::abs(delta)+.12*std::abs(midi-43)+(interval?1.:0.);
            if(path.notes.size()>1){const int priorStep=previous-path.notes[path.notes.size()-2];if(delta*priorStep<0)child.cost+=.6;}
            next.push_back(std::move(child));}
        std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.cost!=b.cost?a.cost<b.cost:a.notes<b.notes;});if(next.size()>16)next.resize(16);bassBeam=std::move(next);
    }
    if(bassBeam.empty())throw std::runtime_error("No bounded bass trajectory realizes the corpus skeleton.");
    for(std::size_t i=0;i<plan.slots.size();++i){auto& slot=plan.slots[i];slot.bassMidi=bassBeam.front().notes[i];slot.bassDegree=pc(slot.bassMidi-int(center.tonic));}
    // Keep the cadence in root position; the inner bass target is free to use
    // chord-tone inversions within a bounded leap, then reconnect to the anchor.
    plan.explanation="Input-relative "+std::to_string(int(bars))+"-bar response; most song-supported corpus harmonic duration in half/full-bar range "+std::to_string(modal)+
        " QN; short-context root skeleton and annotated line-ending support. Phrase length, weights, cadence stability and variation limits are engineering choices. Source key/mode conditions are declared in the prior; input major context is an analysis interpretation. The response root skeleton differs from the complete input.";
    std::string identity=prior.id+name(variation);for(const auto& s:plan.slots)identity+=std::to_string(s.rootDegree)+":"+std::to_string(s.durationQN)+":"+std::to_string(s.appliedDominant)+":"+std::to_string(s.bassMidi)+";";
    plan.id="g131-plan:"+hashText(identity);return plan;
}
std::vector<double> rhythm(DevelopmentIntent intent,std::size_t inputCount,std::size_t requested,double bar) {
    const std::size_t count=requested?requested:intent==DevelopmentIntent::Natural?(inputCount>4?8:4):intent==DevelopmentIntent::Color?8:6;
    std::vector<double> units;
    if(count==4)units=intent==DevelopmentIntent::Natural?std::vector<double>{4,4,4,4}:std::vector<double>{2,2,4,8};
    else if(count==6)units=intent==DevelopmentIntent::Open?std::vector<double>{2,2,4,2,2,4}:std::vector<double>{2,2,2,2,4,4};
    else if(count==8)units={4,4,2,2,4,4,4,8};
    else {units.assign(count,2.);units.back()+=16.-2.*double(count);}
    for(auto& n:units)n*=bar/4.;return units;
}
struct Node {
    std::vector<Relation> path;Progression phrase;
    std::vector<int> bassMidi;
    KeySignature center;std::optional<KeySignature> target;
    std::optional<int> expected;int delay{},direction{},motiveStep{},run{},bestRun{};
    std::size_t outside{},outsideRelations{},keyFit{},arrivals{};
    ChordVoicer voicer;Voicing lastVoicing;float utility{},connectionSum{};
};
std::string nodeKey(const Node& n) {
    std::string out;for(const auto& e:n.phrase)out+=soundKey(e,true)+";";
    return out+"k"+std::to_string(int(n.center.tonic))+":"+std::to_string(int(n.center.mode))+":"+std::to_string(n.expected.value_or(-1));
}
bool endpoint(const Node& n,DevelopmentIntent intent,KeySignature origin) {
    if(n.phrase.empty())return false;
    const auto p=chordPitches(n.phrase.back());
    if(p.quality==ChordQuality::Diminished7)return false;
    const int degree=pc(p.root-int(n.center.tonic));
    const bool tonic=degree==0,deceptive=degree==(n.center.mode==Mode::Major?9:8);
    if(intent==DevelopmentIntent::Natural)return !n.expected&&(tonic||deceptive||degree==5);
    if(intent==DevelopmentIntent::Tension)return p.quality==ChordQuality::Sus4||
        (p.quality==ChordQuality::Dominant7&&std::any_of(n.path.begin(),n.path.end(),[](const auto& r){return r.mechanism=="suspension-delay";}));
    if(n.expected)return false;
    if(intent==DevelopmentIntent::Color&&!(n.outside>=2&&n.outsideRelations>=1))return false;
    if(intent==DevelopmentIntent::BassLine&&n.bestRun<3)return false;
    // A complete open phrase can arrive at a non-tonic plateau, rather than
    // cutting a cadence or a still-obligatory leading-tone event in half.
    return !tonic||!sameKey(n.center,origin);
}
}
const char* name(RelationKind kind) noexcept {
    switch(kind){case RelationKind::Functional:return "functional";case RelationKind::VoiceLeading:return "voice-leading";default:return "outside";}
}
const char* name(VariationLevel level) noexcept {
    switch(level){case VariationLevel::Conventional:return "conventional";case VariationLevel::Rich:return "rich";default:return "bold";}
}
StylePrior loadStylePrior(const std::filesystem::path& file) {
    if(std::filesystem::file_size(file)>4*1024*1024)throw std::runtime_error("Style prior exceeds bounded 4 MiB input.");
    std::ifstream in(file);if(!in)throw std::runtime_error("Cannot open style prior.");
    return parseStylePrior(std::string((std::istreambuf_iterator<char>(in)),{}));
}
StylePrior parseStylePrior(std::string_view text) {
    if(text.size()>4*1024*1024)throw std::runtime_error("Style prior exceeds bounded 4 MiB input.");
    std::istringstream in{std::string(text)};StylePrior prior;std::string line;std::size_t rows{};
    while(std::getline(in,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]=='#')continue;
        std::vector<std::string> fields;std::istringstream s(line);std::string field;while(std::getline(s,field,'\t'))fields.push_back(field);
        if(fields.empty())continue;if(++rows>50000)throw std::runtime_error("Style prior row bound exceeded.");
        const auto number=[&](std::size_t i){if(i>=fields.size())throw std::runtime_error("Truncated style prior row.");std::size_t used{};const double n=std::stod(fields[i],&used);if(used!=fields[i].size()||!std::isfinite(n))throw std::runtime_error("Invalid prior numeric field.");return n;};
        if(fields[0]=="meta"&&fields.size()>=3){if(fields[1]=="id"||fields[1]=="priorId")prior.id=fields[2];else if(fields[1]=="source")prior.source=fields[2];else if(fields[1]=="license")prior.license=fields[2];else prior.scope+=fields[1]+"="+fields[2]+"; ";}
        else if(fields[0]=="transition"&&fields.size()>=6){StyleTransition r{int(number(1)),int(number(2)),int(number(3)),number(4),number(5)};
            if(r.previous2 < -1||r.previous2>11||r.previous<0||r.previous>11||r.next<0||r.next>11||r.count<=0||r.songs<=0||r.songs>r.count)throw std::runtime_error("Invalid observed transition evidence.");prior.transitions.push_back(r);}
        else if(fields[0]=="duration"&&fields.size()>=4){StyleDuration d{number(1),number(2),number(3)};if(d.qn<=0||d.qn>4096||d.count<=0||d.songs<=0)throw std::runtime_error("Invalid duration evidence.");prior.durations.push_back(d);}
        else if(fields[0]=="position"&&fields.size()>=5){StylePosition p{fields[1],int(number(2)),number(3),number(4)};if(p.root<0||p.root>11||p.count<=0||p.songs<=0)throw std::runtime_error("Invalid position evidence.");prior.positions.push_back(p);}
        else if(fields[0]=="quality_transition"&&fields.size()>=7){StyleQualityTransition q{int(number(1)),int(number(3)),fields[2],fields[4],number(5),number(6)};
            if(q.previous<0||q.previous>11||q.next<0||q.next>11||q.count<=0||q.songs<=0||q.songs>q.count)throw std::runtime_error("Invalid family transition evidence.");prior.qualityTransitions.push_back(q);}
        // Other explicitly named research rows may be retained without
        // pretending to consume their unsupported annotation dimensions.
    }
    if(prior.id.empty()||prior.source.empty()||prior.license.empty()||prior.transitions.empty()||prior.durations.empty())throw std::runtime_error("Style prior lacks source/license or usable transition/duration evidence.");
    return prior;
}
const char* name(DevelopmentIntent intent) noexcept {
    switch(intent){case DevelopmentIntent::Natural:return "natural";case DevelopmentIntent::Open:return "open";
        case DevelopmentIntent::Tension:return "tension";case DevelopmentIntent::Color:return "color";default:return "bass-line";}
}
std::vector<Relation> successors(const SuccessorContext& context,RelationKind kind) {
    std::vector<Relation> out;if(context.recent.empty()||context.maximum==0)return out;
    const auto previous=chordPitches(context.recent.back());if(previous.root<0||previous.bass<0||!previous.intervals)return out;
    const auto a=tones(previous);auto pool=diatonic(context.center,context.intent!=DevelopmentIntent::Natural);
    if(context.variation){
        // Triads carry the base pop grammar. Extensions are controlled variants
        // of those same relations, rather than a compulsory 7/9 vocabulary.
        for(auto& e:pool){const auto p=chordPitches(e);if(p.quality==ChordQuality::HalfDiminished7)continue;
            const auto q=p.quality==ChordQuality::Minor7?ChordQuality::Minor:ChordQuality::Major;e=chord(p.root,q);}
    }
    const auto add=[&](ChordEvent event,std::string mechanism,std::string explanation,std::optional<int> expected={},std::optional<KeySignature> target={}) {
        const auto p=chordPitches(event);const auto b=tones(p);if(p.root<0||p.bass<0||!b||soundKey(event)==soundKey(context.recent.back()))return;
        if((context.plannedRoot&&p.root!=*context.plannedRoot)||(context.plannedBass&&p.bass!=*context.plannedBass))return;
        const int common=std::popcount(a&b),bass=step(previous.bass,p.bass);
        const float movement=.5f*(nearest(a,b)+nearest(b,a));
        if(!common&&movement>1.75f)return;
        if(context.expectedRoot&&p.root!=*context.expectedRoot&&mechanism!="suspension-delay"&&mechanism!="deceptive-resolution")return;
        const float shared=float(common)/float(std::max(1,std::min(std::popcount(a),std::popcount(b))));
        const float bassFit=std::abs(bass)<=2?1.f:std::abs(bass)==5?.75f:.45f;
        Relation r;r.chord=std::move(event);r.kind=kind;r.mechanism=std::move(mechanism);r.explanation=std::move(explanation);
        r.expectedRoot=expected;r.proposedCenter=target;r.commonPitchClasses=common;r.bassClassStep=bass;
        r.connection=.45f*shared+.35f*std::max(0.f,1.f-movement/4.f)+.2f*bassFit;
        if(r.connection>=.36f)out.push_back(std::move(r));
    };
    if(kind==RelationKind::Functional) {
        const int d=pc(previous.root-int(context.center.tonic));
        const bool tonic=d==0||d==(context.center.mode==Mode::Major?9:3);
        const bool predominant=d==2||d==5;
        const std::vector<int> choices=tonic?std::vector<int>{1,3,2,5}:predominant?std::vector<int>{4,0,5,2}:std::vector<int>{0,5,1,3,4};
        for(int index:choices){const auto& e=pool[index];const auto p=chordPitches(e);
            add(e,context.expectedRoot?"target-resolution":tonic?((index==1||index==3)?"tonic-to-predominant":"tonic-prolongation"):predominant?"predominant-turn":"functional-redirection",
                "Contextual tonic/predominant/dominant-family successor; actual bass and pitch sets retained.",
                p.quality==ChordQuality::Dominant7?std::optional(pc(p.root+5)):std::nullopt);}
        if(context.expectedRoot) {
            auto found=std::find_if(pool.begin(),pool.end(),[&](const auto& e){return chordPitches(e).root==*context.expectedRoot;});
            add(found==pool.end()?chord(*context.expectedRoot,ChordQuality::Major7):*found,"target-resolution","Arrival at the previously stated dominant/leading-tone target.");
            if(*context.expectedRoot==int(context.center.tonic)&&previous.quality==ChordQuality::Dominant7)
                add(pool[5],"deceptive-resolution","Dominant expectation redirected to the relative tonic substitute, not a fabricated tonic cadence.");
            if(previous.quality==ChordQuality::Dominant7)
                add(chord(previous.root,ChordQuality::Sus4),"suspension-delay","Delay the explicit dominant target with a fourth suspension; target remains pending.",context.expectedRoot);
        } else {
            for(int index:{0,1,3,4,5}) {
                const int root=chordPitches(pool[index]).root;
                add(chord(root+7,ChordQuality::Dominant7),"secondary-dominant","Applied dominant with an explicit following root target.",root);
                if(index!=3)add(chord(root-1,ChordQuality::Diminished7),"secondary-leading-tone","Chromatic leading-tone diminished seventh; must resolve to the stated root.",root);
            }
        }
    } else if(kind==RelationKind::VoiceLeading) {
        auto material=pool;const auto modal=borrowed(context.center);material.insert(material.end(),modal.begin(),modal.end());
        for(const auto& base:material) {
            const auto p=chordPitches(base);const auto b=tones(p);
            std::vector<int> movements{-2,-1,1,2,0};if(context.variation)for(int d:{-5,-4,-3,3,4,5,6})movements.push_back(d);
            for(int movement:movements) {
                const int bass=pc(previous.bass+movement);
                const int upperRoot=pc(p.root-bass);
                const bool pedalHarmony=upperRoot==3||upperRoot==4||upperRoot==5||upperRoot==7||upperRoot==8||upperRoot==10;
                const bool pedal=movement==0&&std::popcount(a&b)>=2&&
                    (bass==int(context.center.tonic)||bass==pc(int(context.center.tonic)+7)||context.establishedPedal==bass)&&pedalHarmony;
                const bool structuralBass=(p.intervals&(1u<<pc(bass-p.root)))&&
                    (pc(bass-p.root)==0||pc(bass-p.root)==3||pc(bass-p.root)==4||pc(bass-p.root)==5||pc(bass-p.root)==6||pc(bass-p.root)==7||pc(bass-p.root)>=9);
                if(!structuralBass&&!pedal)continue;
                auto e=base;e.bass=PitchClass(bass);const auto slash=e.name.find('/');if(slash!=e.name.npos)e.name.resize(slash);
                if(bass!=p.root)e.name+="/"+std::string(notes[bass]);
                add(e,pedal?"common-tone-pedal":"directed-bass-inversion",pedal?
                    "Carry the actual bass as a deliberate pedal while the upper harmony changes.":
                    "Chord-tone inversion continues an actual semitone/step bass trajectory; not a relabelled root progression.");
            }
            if(std::abs(step(previous.bass,p.root))==5&&std::popcount(a&b)>=2)
                add(base,"intentional-bass-reorientation","Fourth/fifth bass reorientation with retained upper pitch classes; minimum motion is not the only goal.");
        }
    } else {
        auto material=borrowed(context.center);
        for(int distance:{-4,-3,3,4})material.push_back(chord(previous.root+distance,ChordQuality::Major7,-1,true));
        for(const auto& e:material) {
            add(e,"modal-borrowing-or-mediant","Parallel/modal borrowing or common-tone mediant derived from the active context.");
            const auto p=chordPitches(e);
            if(context.allowCenterChange&&(context.intent==DevelopmentIntent::Color||context.intent==DevelopmentIntent::Open)&&
                std::popcount(a&tones(p))>=1&&p.root!=int(context.center.tonic)) {
                const KeySignature target{PitchClass(p.root),p.quality==ChordQuality::Minor7?Mode::Minor:Mode::Major};
                add(e,"common-tone-center-pivot","Common-tone entry into a proposed center; later chords must provide independent target evidence.",{},target);
            }
        }
        add(chord(int(context.center.tonic)+1,ChordQuality::Dominant7),"tritone-substitution","Chromatic dominant substitute with an explicit semitone tonic target.",int(context.center.tonic));
    }
    if(context.variation&&*context.variation!=VariationLevel::Conventional){
        const auto basic=out;for(const auto& r:basic){const auto p=chordPitches(r.chord);
            if(p.quality==ChordQuality::Major||p.quality==ChordQuality::Minor){const bool dominant=pc(p.root-int(context.center.tonic))==7;
                add(chord(p.root,p.quality==ChordQuality::Minor?ChordQuality::Minor7:dominant?ChordQuality::Dominant7:ChordQuality::Major7,p.bass),
                    r.mechanism,"Controlled seventh variant of the same planned relation. "+r.explanation,r.expectedRoot,r.proposedCenter);}}
    }
    std::stable_sort(out.begin(),out.end(),[&](const auto& x,const auto& y) {
        const auto value=[&](const auto& r){return r.connection+(context.bassDirection&&r.bassClassStep*context.bassDirection>0&&std::abs(r.bassClassStep)<=2?.3f:0.f);};
        if(value(x)!=value(y))return value(x)>value(y);return soundKey(x.chord)+x.mechanism<soundKey(y.chord)+y.mechanism;
    });
    if(out.size()>std::min<std::size_t>(context.maximum,64))out.resize(std::min<std::size_t>(context.maximum,64));return out;
}

ContinuationCandidate previewCandidate(const GeneratedPhrase& phrase) {
    ContinuationCandidate out;out.id=phrase.id;out.key=phrase.originCenter;
    out.intent=phrase.intent==DevelopmentIntent::Natural?PhraseIntent::Neutral:phrase.intent==DevelopmentIntent::Color?PhraseIntent::Color:PhraseIntent::Develop;
    out.baseQualified=false;out.constraints=phrase.constraints;out.suggestedCurrentChordDurationQN=phrase.inputOpenHoldQN;
    out.connectionReason="Algorithm-generated phrase; no Factory template or source-continuation evidence.";
    for(const auto& e:phrase.phrase){ConcreteChordEvent c;c.label=e.name;c.quality=e.quality;c.harmonicData=e;c.durationQN=*e.durationQN;out.continuation.push_back(c);}
    return out;
}
std::string performanceIdentity(const GeneratedPhrase& phrase) {
    auto out=continuationMusicFingerprint(previewCandidate(phrase));out+="|bass-midi:";
    for(int note:phrase.bassMidi)out+=std::to_string(note)+",";return out;
}
preview::BuildResult buildPerformance(const Progression& input,const GeneratedPhrase& phrase,double tempo) {
    if(phrase.phrase.size()!=phrase.bassMidi.size())return {{},"generated bass realization is incomplete"};
    ImportedProgressionSession imported;imported.events=input;const auto candidate=previewCandidate(phrase);
    auto result=preview::buildSequence(imported,&candidate,tempo);if(!result)return result;
    for(auto& event:result.sequence.events)if(event.segment==preview::Segment::Recommended) {
        const double at=event.startQN+input.front().startQN;bool found=false;
        for(std::size_t i=0;i<phrase.phrase.size();++i)if(at>=phrase.phrase[i].startQN-1e-7&&at<phrase.phrase[i].startQN+*phrase.phrase[i].durationQN-1e-7){
            event.bassMidi=phrase.bassMidi[i];found=true;break;}
        if(!found)return {{},"generated event is outside its complete phrase boundary"};
    }
    ChordVoicer voicer;for(const auto& event:result.sequence.events)if(!preview::voiceEvent(event,voicer))return {{},"unplayable generated register/constraint"};
    return result;
}

GenerationResult generate(const GenerationRequest& request) {
    GenerationResult result;const auto started=std::chrono::steady_clock::now();
    const auto finish=[&]{result.elapsedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return result;};
    if(request.input.empty()||request.input.size()>120||request.chordCount>8||(request.chordCount&&request.chordCount<4)||
       !request.beamWidth||request.beamWidth>32||!request.successorLimit||request.successorLimit>32||
       request.maximumExpansions>100000||!request.maximumExpansions||!request.resultsPerIntent||request.resultsPerIntent>4||!validConstraints(request.constraints)) {
        result.error="Invalid input, constraints or bounded-search budget.";return finish();}
    const int numerator=request.context.timeSigNumerator.value_or(4),denominator=request.context.timeSigDenominator.value_or(4);
    if(numerator<1||numerator>12||(denominator!=2&&denominator!=4&&denominator!=8&&denominator!=16)){
        result.error="Unsupported explicit meter.";return finish();}
    const double bar=double(numerator)*4./denominator,hold=progressionOpenDuration(request.input);
    auto heard=request.input;double seam=heard.front().startQN;std::string inputIdentity;
    for(std::size_t i=0;i<heard.size();++i) {
        auto& e=heard[i];const auto p=chordPitches(e);const double duration=e.durationQN&&!e.openEnded?*e.durationQN:
            i+1<heard.size()?heard[i+1].startQN-e.startQN:hold;
        if(p.root<0||p.bass<0||!p.intervals||!std::isfinite(e.startQN)||!std::isfinite(duration)||duration<=0||duration>128||
            (i&&e.startQN<seam-1e-6)) {result.error="Input contains unknown sounding identity, invalid time or overlap.";return finish();}
        e.durationQN=duration;e.openEnded=false;seam=e.startQN+duration;
        inputIdentity+=std::to_string(p.root)+":"+std::to_string(int(p.quality))+":"+soundKey(e,true)+";";
    }
    if(!evaluateMelody(heard,request.constraints,hold).hardSatisfied||!actualHardNotes(heard,request.constraints)){result.error="Input conflicts with an explicit hard melody constraint.";return finish();}
    auto analysis=analyzeHarmony(request.input,request.context);
    if(request.context.forcedKey)result.contextInterpretations.push_back(*request.context.forcedKey);
    else {
        // A minor line-cliche beginning stays an available interpretation even
        // when its chromatic/Dorian tail fits a parallel major center locally.
        const auto first=chordPitches(heard.front());
        if(first.quality==ChordQuality::Minor||first.quality==ChordQuality::Minor7)
            result.contextInterpretations.push_back({PitchClass(first.root),Mode::Minor});
        for(const auto& key:analysis.keyCandidates)if(result.contextInterpretations.size()<2&&
            std::none_of(result.contextInterpretations.begin(),result.contextInterpretations.end(),[&](auto p){return sameKey(p,key.key);}))result.contextInterpretations.push_back(key.key);
    }
    if(result.contextInterpretations.empty()){result.error="No supported context-center interpretation.";return finish();}
    if(request.stylePrior){
        if(int(request.variation)<0||int(request.variation)>2||(!request.intents.empty()&&request.intents!=std::vector<DevelopmentIntent>{DevelopmentIntent::Natural})){
            result.error="Style planning currently supports one complete natural response per variation.";return finish();}
        const auto major=std::find_if(result.contextInterpretations.begin(),result.contextInterpretations.end(),[](auto key){return key.mode==Mode::Major;});
        if(major==result.contextInterpretations.end()){result.error="Style planner has no supported major-context interpretation.";return finish();}
        // Freeze one supported interpretation before planning. The full list
        // remains visible; it is not silently attributed to the source corpus.
    }
    auto intents=request.intents;if(intents.empty())intents=request.stylePrior?std::vector<DevelopmentIntent>{DevelopmentIntent::Natural}:std::vector<DevelopmentIntent>{DevelopmentIntent::Natural,DevelopmentIntent::Open,DevelopmentIntent::Tension,DevelopmentIntent::Color,DevelopmentIntent::BassLine};
    std::sort(intents.begin(),intents.end());intents.erase(std::unique(intents.begin(),intents.end()),intents.end());
    for(auto intent:intents)if(int(intent)<0||int(intent)>4){result.error="Invalid development intent.";return finish();}
    std::vector<GeneratedPhrase> candidates;
    std::optional<int> establishedPedal;
    if(heard.size()>=3){const int lastBass=chordPitches(heard.back()).bass;
        if(chordPitches(heard[heard.size()-2]).bass==lastBass&&chordPitches(heard[heard.size()-3]).bass==lastBass)establishedPedal=lastBass;}
    bool plannedContext{};
    for(auto intent:intents)for(const auto origin:result.contextInterpretations) {
        if(request.stylePrior&&origin.mode!=Mode::Major)continue;
        if(request.stylePrior&&plannedContext)continue;
        if(request.stylePrior)plannedContext=true;
        std::optional<PhrasePlan> phrasePlan;if(request.stylePrior){
            try{phrasePlan=planPhrase(*request.stylePrior,heard,origin,request.variation,bar,result.skeletonExpanded,result.bassExpanded);}catch(const std::exception& e){result.error=e.what();return finish();}}
        std::vector<double> plan;if(phrasePlan){for(const auto& slot:phrasePlan->slots)plan.push_back(slot.durationQN);}else plan=rhythm(intent,heard.size(),request.chordCount,bar);
        Node initial;initial.center=origin;initial.direction=inferredDirection(heard);
        if(intent==DevelopmentIntent::BassLine&&!initial.direction)initial.direction=-1;
        if(heard.size()>1)initial.motiveStep=step(chordPitches(heard[heard.size()-2]).bass,chordPitches(heard.back()).bass);
        if(std::abs(initial.motiveStep)>2||!initial.motiveStep)initial.motiveStep=initial.direction;
        for(const auto& e:heard)initial.lastVoicing=initial.voicer.voice(chordPitches(e));
        std::vector<Node> beam{initial};
        for(std::size_t depth=0;depth<plan.size()&&!beam.empty();++depth) {
            if(request.cancelled&&request.cancelled()){result.error="Generation cancelled.";return finish();}
            std::vector<Node> next;std::set<std::string> identities;
            for(const auto& node:beam) {
                Progression recent=heard;recent.insert(recent.end(),node.phrase.begin(),node.phrase.end());
                if(recent.size()>8)recent.erase(recent.begin(),recent.end()-8);
                SuccessorContext context{recent,node.center,intent,request.tendency,node.direction,node.expected,!node.target&&depth<plan.size()/2,32};
                context.establishedPedal=establishedPedal;
                if(phrasePlan){context.variation=request.variation;context.allowCenterChange=false;context.maximum=64;
                    context.plannedRoot=pc(int(origin.tonic)+phrasePlan->slots[depth].rootDegree);context.plannedBass=pc(int(origin.tonic)+phrasePlan->slots[depth].bassDegree);}
                std::vector<Relation> options;for(auto kind:{RelationKind::Functional,RelationKind::VoiceLeading,RelationKind::Outside}) {
                    auto group=successors(context,kind);options.insert(options.end(),group.begin(),group.end());}
                if(phrasePlan){const auto& slot=phrasePlan->slots[depth];
                    std::erase_if(options,[&](const Relation& relation){const auto p=chordPitches(relation.chord);
                        if(pc(p.root-int(origin.tonic))!=slot.rootDegree||pc(p.bass-int(origin.tonic))!=slot.bassDegree)return true;
                        if(slot.appliedDominant)return p.quality!=ChordQuality::Dominant7||relation.expectedRoot!=std::optional(pc(int(origin.tonic)+slot.anchorDegree));
                        if(tones(p)&~scale(origin))return true;
                        if(request.variation==VariationLevel::Conventional&&p.quality!=ChordQuality::Major&&p.quality!=ChordQuality::Minor)return true;
                        return false;});}
                const auto relationValue=[&](const Relation& r) {
                    float score=r.connection;
                    if(intent==DevelopmentIntent::BassLine){const auto delta=r.bassClassStep;score+=delta*node.direction>0&&std::abs(delta)<=2?.65f:-.35f;
                        if(depth<3&&delta==node.motiveStep)score+=.2f;}
                    if(intent==DevelopmentIntent::Color)score+=r.kind==RelationKind::Outside?(node.outside<2?.25f:.04f):.06f;
                    if(intent==DevelopmentIntent::Natural)score+=r.kind==RelationKind::Functional?.18f:0.f;
                    if(intent==DevelopmentIntent::Tension)score+=r.expectedRoot?.2f:0.f;
                    if(phrasePlan&&request.variation!=VariationLevel::Conventional){const auto p=chordPitches(r.chord);
                        // At most two sevenths are a realization choice. The
                        // short-context root/rhythm plan already exists here.
                        if(p.quality==ChordQuality::Major7||p.quality==ChordQuality::Minor7)score+=.12f;}
                    score+=tendencyProfile(request.tendency).colorBias*(r.kind==RelationKind::Outside?.0125f:0.f);
                    return score;
                };
                std::stable_sort(options.begin(),options.end(),[&](const auto& a,const auto& b){if(relationValue(a)!=relationValue(b))return relationValue(a)>relationValue(b);return soundKey(a.chord)+a.mechanism<soundKey(b.chord)+b.mechanism;});
                std::set<std::string> optionKeys;std::size_t tried{};
                for(auto relation:options) {
                    const auto optionKey=soundKey(relation.chord)+":"+std::to_string(relation.expectedRoot.value_or(-1))+":"+(relation.proposedCenter?formatKey(*relation.proposedCenter):"");
                    if(!optionKeys.insert(optionKey).second)continue;if(tried++>=request.successorLimit)break;
                    if(result.expanded>=request.maximumExpansions){result.budgetExhausted=true;break;}++result.expanded;
                    if(relation.mechanism=="suspension-delay"&&node.delay>=1)continue;
                    Node child=node;relation.chord.startQN=node.phrase.empty()?seam:node.phrase.back().startQN+*node.phrase.back().durationQN;
                    relation.chord.durationQN=plan[depth];child.phrase.push_back(relation.chord);child.path.push_back(relation);
                    std::size_t coreRun{};for(auto i=child.phrase.rbegin();i!=child.phrase.rend()&&harmonicCore(*i)==harmonicCore(relation.chord);++i)++coreRun;
                    if(coreRun>(intent==DevelopmentIntent::BassLine?3u:2u))continue;
                    child.expected=relation.expectedRoot;child.delay=relation.mechanism=="suspension-delay"?node.delay+1:0;
                    if(relation.proposedCenter){child.center=*relation.proposedCenter;child.target=relation.proposedCenter;child.keyFit=0;}
                    const auto pitch=chordPitches(relation.chord),previous=chordPitches(recent.back());
                    const auto outside=tones(pitch)&~scale(origin);child.outside+=outside!=0;child.outsideRelations+=relation.kind==RelationKind::Outside;
                    if(phrasePlan&&child.outside>phrasePlan->outsideBudget)continue;
                    if(phrasePlan){std::size_t extensions{};for(const auto& e:child.phrase){const auto p=chordPitches(e);extensions+=p.quality==ChordQuality::Major7||p.quality==ChordQuality::Minor7;}
                        if(extensions>(request.variation==VariationLevel::Conventional?0u:2u))continue;}
                    child.keyFit+=child.target&&!(tones(pitch)&~scale(child.center));
                    child.arrivals+=child.target&&pitch.root==int(child.center.tonic)&&previous.quality==ChordQuality::Dominant7&&pc(previous.root+5)==pitch.root;
                    const int delta=step(previous.bass,pitch.bass);
                    float repeated{};for(const auto& e:node.phrase)if(soundKey(e)==soundKey(relation.chord))repeated+=.25f;
                    // Preserve a cross-chord bass motive across the phrase, not
                    // just a smooth seam; deliberate reprise remains possible.
                    int bassMidi=node.lastVoicing.bass+delta;
                    if(phrasePlan)bassMidi=phrasePlan->slots[depth].bassMidi;
                    if(bassMidi<24||bassMidi>60){if(intent==DevelopmentIntent::BassLine)continue;
                        while(bassMidi<24)bassMidi+=12;while(bassMidi>60)bassMidi-=12;child.path.back().explanation+=" Explicit bass register reset within C1-C4.";}
                    preview::Event performance{pitch,relation.chord.startQN,plan[depth],preview::Segment::Recommended,{},bassMidi};
                    const auto realized=preview::voiceEvent(performance,child.voicer);if(!realized||!realized->upperCount)continue;
                    const auto& voicing=*realized;child.bassMidi.push_back(bassMidi);
                    const int actualDelta=bassMidi-node.lastVoicing.bass;
                    child.run=actualDelta*child.direction>0&&std::abs(actualDelta)<=2?node.run+1:0;child.bestRun=std::max(child.bestRun,child.run);
                    if(intent==DevelopmentIntent::BassLine&&depth<3&&child.run!=int(depth+1))continue;
                    float move=modelMotion(node.lastVoicing,voicing);child.lastVoicing=voicing;child.connectionSum+=relation.connection;
                    child.utility+=relationValue(relation)-.18f*move-repeated;
                    if(std::none_of(node.phrase.begin(),node.phrase.end(),[&](const auto& e){return harmonicCore(e)==harmonicCore(relation.chord);}))child.utility+=.12f;
                    if(intent==DevelopmentIntent::BassLine)child.utility+=.15f*child.run;
                    if(intent==DevelopmentIntent::Open&&depth+1<plan.size()&&pitch.root==int(origin.tonic))child.utility-=.12f;
                    if(intent==DevelopmentIntent::Color&&child.outside>plan.size()-2)child.utility-=.2f;
                    if(!request.constraints.melody.empty()) {
                        auto whole=heard;whole.insert(whole.end(),child.phrase.begin(),child.phrase.end());
                        if(!evaluateMelody(whole,request.constraints).hardSatisfied||!actualHardNotes({relation.chord},request.constraints)){++result.constraintRejected;continue;}
                        bool playable=true;for(const auto& m:request.constraints.melody)if(m.role==ConstraintRole::TopVoice&&m.strictness==ConstraintStrictness::Hard&&
                            m.startQN<relation.chord.startQN+plan[depth]-1e-7&&m.startQN+m.durationQN>relation.chord.startQN+1e-7) {
                            ChordVoicer check;performance.topVoice=melodyTopPitch(m);if(!preview::voiceEvent(performance,check))playable=false;}
                        if(!playable){++result.constraintRejected;continue;}
                    }
                    if(depth+1==plan.size()&&!(phrasePlan?!child.expected:endpoint(child,intent,origin))){++result.structureRejected;continue;}
                    if(depth+1==plan.size()){std::set<std::string> cores;for(const auto& e:child.phrase)cores.insert(harmonicCore(e));
                        if(cores.size()<3){++result.structureRejected;continue;}}
                    const auto identity=nodeKey(child);if(identities.insert(identity).second)next.push_back(std::move(child));
                }
                if(result.budgetExhausted)break;
            }
            std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){if(a.utility!=b.utility)return a.utility>b.utility;return nodeKey(a)<nodeKey(b);});
            if(next.size()>request.beamWidth)next.resize(request.beamWidth);beam=std::move(next);
            if(result.budgetExhausted)break;
        }
        for(const auto& node:beam) {
            if(node.phrase.size()!=plan.size())continue;++result.completeExamined;
            std::set<std::string> harmonies;for(const auto& e:node.phrase)harmonies.insert(soundKey(e));
            std::set<std::string> cores;for(const auto& e:node.phrase)cores.insert(harmonicCore(e));
            if(harmonies.size()<3||cores.size()<3||!(phrasePlan?!node.expected:endpoint(node,intent,origin))){++result.structureRejected;continue;}
            GeneratedPhrase phrase;phrase.intent=intent;phrase.originCenter=origin;phrase.targetCenter=node.target;
            phrase.contextInterpretations=result.contextInterpretations;phrase.phrase=node.phrase;phrase.path=node.path;phrase.constraints=request.constraints;
            phrase.bassMidi=node.bassMidi;
            phrase.seamQN=seam;phrase.inputOpenHoldQN=hold;phrase.inputIdentity=inputIdentity;
            for(const auto& e:node.phrase)phrase.durationQN+=*e.durationQN;
            phrase.complete=true;phrase.hardConstraintsPassed=true;
            phrase.plan=phrasePlan;
            phrase.ending=intent==DevelopmentIntent::Tension?PhraseEnding::Suspended:intent==DevelopmentIntent::Natural?PhraseEnding::InterimPause:
                node.target&&node.arrivals?PhraseEnding::CenterTransition:PhraseEnding::OpenUnresolved;
            phrase.structureExplanation=std::to_string(plan.size())+" complete events, "+std::to_string(phrase.durationQN/bar)+
                " bars of planned grouping; final hold "+std::to_string(plan.back())+" QN. No source phrase was truncated.";
            if(phrasePlan)phrase.structureExplanation=phrasePlan->explanation;
            phrase.endingExplanation=intent==DevelopmentIntent::Tension?"Whole-phrase dominant/suspension question; resolution is deliberately deferred, not claimed complete.":
                intent==DevelopmentIntent::Natural?"Prepared stage arrival at a tonic-family or subdominant harmony.":
                "Complete rhythmic group ending on an open harmonic plateau; future continuation remains unproven.";
            phrase.centerEvidence=node.target?(node.arrivals&&node.keyFit>=3?
                "Prepared dominant arrival plus subsequent target-collection support; proposed center migration, not unique key recovery.":
                "Common-tone pivot/temporary center interpretation; no established modulation is claimed."):
                "Context center retained as one supported interpretation; borrowed chords do not establish a new key.";
            auto& e=phrase.evaluation;e.connection=node.connectionSum/float(plan.size());e.structure=1.f;e.uniqueHarmonies=harmonies.size();
            e.outsideChords=node.outside;e.directedBassSteps=node.bestRun;e.bassContinuity=float(node.bestRun)/float(plan.size());
            float minimum=1.f,maximum{};for(const auto& c:node.phrase){const auto t=tension(tones(chordPitches(c)));minimum=std::min(minimum,t);maximum=std::max(maximum,t);}e.tension=maximum-minimum;
            e.development=intent==DevelopmentIntent::BassLine?e.bassContinuity:intent==DevelopmentIntent::Color?
                std::min(1.f,float(node.outside)/3.f):intent==DevelopmentIntent::Tension?.7f+std::min(.3f,e.tension):.5f+.5f*float(harmonies.size())/float(plan.size());
            phrase.id="g13:"+hashText(inputIdentity+performanceIdentity(phrase));candidates.push_back(std::move(phrase));
        }
        if(result.budgetExhausted)break;
    }
    RecommendationSet profiles;for(const auto& p:candidates)profiles.groups[1].push_back(previewCandidate(p));
    MatchQuery query;query.source=request.input;prepareContextualDiversity(query,profiles);prepareHarmonicDevelopment(query,profiles);
    for(std::size_t i=0;i<candidates.size();++i) {
        auto& p=candidates[i];const auto& profile=profiles.groups[1][i].routeProfile;const auto& evidence=profiles.groups[1][i].harmonicDevelopment;
        p.evaluation.novelty=evidence.novelty;p.evaluation.recentSimilarity=profile.recentSimilarity;
        p.evaluation.historySimilarity=std::max(profile.historySimilarity,profile.developmentHistorySimilarity);
        const auto colorPath=color::HarmonyColorAnalyzer::analyzeContinuation(color::HarmonyColorAnalyzer::prepare(heard),profiles.groups[1][i]);
        if(colorPath.status==color::Status::Known)p.evaluation.colorTensionTrend=colorPath.tensionTrend;
    }
    const auto utility=[](const GeneratedPhrase& p){const auto& e=p.evaluation;return .35f*e.connection+.4f*e.development+.15f*e.novelty+.1f*e.tension-.12f*e.historySimilarity;};
    std::vector<std::size_t> ranked;for(std::size_t i=0;i<candidates.size();++i)ranked.push_back(i);
    std::stable_sort(ranked.begin(),ranked.end(),[&](auto a,auto b){if(utility(candidates[a])!=utility(candidates[b]))return utility(candidates[a])>utility(candidates[b]);return candidates[a].id<candidates[b].id;});
    std::set<std::string> identities;std::map<DevelopmentIntent,std::vector<std::size_t>> chosen;
    for(auto i:ranked) {
        const auto& p=candidates[i];auto& selected=chosen[p.intent];if(selected.size()>=request.resultsPerIntent)continue;
        std::string identity;for(const auto& e:p.phrase)identity+=soundKey(e,true)+";";
        if(identities.contains(identity))continue;
        if(std::any_of(selected.begin(),selected.end(),[&](auto old){return creativeRouteSimilarity(profiles.groups[1][old],profiles.groups[1][i])>=.91f;}))continue;
        identities.insert(identity);selected.push_back(i);
    }
    for(auto intent:intents)for(auto i:chosen[intent])result.phrases.push_back(std::move(candidates[i]));
    return finish();
}
}
