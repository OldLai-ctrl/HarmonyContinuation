#include "ContextualContinuation.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace harmony {
namespace {
int mod(int n) { return (n%12+12)%12; }
ChordQuality core(ChordQuality q) {
    if(q==ChordQuality::Major7||q==ChordQuality::Dominant7)return ChordQuality::Major;
    if(q==ChordQuality::Minor7)return ChordQuality::Minor;
    if(q==ChordQuality::Diminished7||q==ChordQuality::HalfDiminished7)return ChordQuality::Diminished;
    return q;
}
ChordPitchSet pitches(const ConcreteChordEvent& e) {
    return e.harmonicData?chordPitches(*e.harmonicData):chordPitches(e.label,e.quality);
}
ConcreteChordEvent sourceEvent(const ProgressionTemplate& item,std::size_t i,KeySignature key) {
    auto concrete=realizeContinuation(item.full[i],key,item.full[i].durationQN.value_or(4));
    if(item.rawChords.empty())return concrete;
    if(!item.rawChordKey||item.rawChords.size()!=item.full.size()) {concrete.label="?";concrete.harmonicData.reset();return concrete;}
    auto raw=item.rawChords[i];const auto p=chordPitches(raw);
    if(p.root<0||p.bass<0){concrete.label="?";concrete.harmonicData.reset();return concrete;}
    const int shift=mod(int(key.tonic)-int(item.rawChordKey->tonic));
    if(shift) {
        constexpr const char* notes[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
        const auto slash=raw.name.find('/');std::size_t rootEnd=raw.name.empty()?0:1;
        while(rootEnd<raw.name.size()&&(raw.name[rootEnd]=='#'||raw.name[rootEnd]=='b'))++rootEnd;
        const auto suffix=raw.name.substr(rootEnd,slash==std::string::npos?slash:slash-rootEnd);
        raw.root=PitchClass(mod(p.root+shift));raw.bass=PitchClass(mod(p.bass+shift));raw.quality=p.quality;
        raw.name=notes[int(*raw.root)]+suffix;
        if(p.bass!=p.root||slash!=std::string::npos)raw.name+="/"+std::string(notes[int(*raw.bass)]);
        raw.keyNoteValue.reset();raw.bassNoteValue.reset();raw.extensions.pitches.reset();
        std::ostringstream mask;mask<<"0x"<<std::hex<<(p.intervals|p.colorMask);raw.extensions.mask=mask.str();
    }
    concrete.durationQN=raw.durationQN.value_or(concrete.durationQN);raw.durationQN=concrete.durationQN;raw.openEnded=false;
    concrete.label=raw.name;concrete.quality=raw.quality;concrete.harmonicData=std::move(raw);return concrete;
}
unsigned absoluteMask(const ChordPitchSet& c) {
    unsigned mask{};
    for(int i=0;i<12;++i)if((c.intervals|c.colorMask)&(1u<<i))mask|=1u<<mod(c.root+i);
    // A slash pedal is an actual sounding tone even outside the upper chord.
    if(c.bass>=0)mask|=1u<<c.bass;
    return mask;
}
int common(const ChordPitchSet& a,const ChordPitchSet& b) {
    return std::popcount(absoluteMask(a)&absoluteMask(b));
}
int distance(int a,int b) { const auto d=mod(a-b);return std::min(d,12-d); }
bool sameCenter(KeySignature a,KeySignature b) { return a.tonic==b.tonic&&a.mode==b.mode; }
bool relativeCenter(KeySignature a,KeySignature b) {
    return a.mode!=b.mode&&mod(int(a.tonic)+(a.mode==Mode::Major?9:3))==int(b.tonic);
}
unsigned scaleMask(KeySignature k) {
    constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
    unsigned m{};for(int i=0;i<7;++i)m|=1u<<mod(int(k.tonic)+(k.mode==Mode::Major?major:minor)[i]);return m;
}
bool compatible(const ChordEvent& input,const ConcreteChordEvent& expected) {
    const auto a=chordPitches(input),b=pitches(expected);
    if(a.root<0||b.root<0||a.root!=b.root||core(a.quality)!=core(b.quality))return false;
    if((input.bass||a.bass!=a.root)&&a.bass!=b.bass)return false;
    // A triad can retrieve a superset extension. Named sevenths, alterations
    // and explicit performed tones are binding even without an exact-mask flag.
    if((absoluteMask(a)&absoluteMask(b))!=absoluteMask(a))return false;
    return true;
}
struct Boundary { bool passed{}; float score{}; std::string reason; };
Boundary boundary(const ChordPitchSet& a,const ChordPitchSet& b) {
    if(a.root<0||a.bass<0||b.root<0||b.bass<0||!a.intervals||!b.intervals)return {};
    if(a.root==b.root&&a.bass==b.bass&&(a.intervals|a.colorMask)==(b.intervals|b.colorMask))return {};
    const int bass=distance(a.bass,b.bass),shared=common(a,b),root=mod(b.root-a.root);
    const bool dominant=(a.quality==ChordQuality::Dominant7||core(a.quality)==ChordQuality::Major)&&root==5;
    // The seam needs audible continuity or a functional fifth. Novelty never
    // substitutes for this gate. The exact bass and extended pitch set are used.
    if(!dominant&&!(shared>=2&&bass<=5)&&!(shared>=1&&bass<=2))return {};
    // Avoid an unresolved dominant third/tritone at an unrelated destination.
    if(a.quality==ChordQuality::Dominant7&&!dominant&&shared<2)return {};
    float movement{};
    const auto am=absoluteMask(a),bm=absoluteMask(b);
    for(int p=0;p<12;++p)if(am&(1u<<p)) {
        int best=6;for(int q=0;q<12;++q)if(bm&(1u<<q))best=std::min(best,distance(p,q));
        movement+=float(best);
    }
    movement/=float(std::max(1,std::popcount(am)));
    if(movement>1.5f)return {};
    return {true,std::clamp(.72f+.04f*shared-.025f*bass-.03f*movement+(dominant?.12f:0),0.f,1.f),
        dominant?"functional-fifth; actual bass and guide-tone movement checked":
        "common-tone/stepwise seam; actual bass and extended tones checked"};
}
bool reviewed(const ProgressionTemplate& t) {
    return t.sourceType=="factory"&&
        ((t.version==7&&t.id.starts_with("V7_")&&std::find(t.tags.begin(),t.tags.end(),"V7-editorial")!=t.tags.end())||
         (t.version==8&&t.id.starts_with("V8_")&&std::find(t.tags.begin(),t.tags.end(),"G8-editorial")!=t.tags.end()));
}
PhraseEnding editorialEnding(const ProgressionTemplate& t) {
    if(!reviewed(t)||std::find(t.tags.begin(),t.tags.end(),"G8-complete")==t.tags.end())return PhraseEnding::Unknown;
    constexpr std::pair<const char*,PhraseEnding> values[]{
        {"closed",PhraseEnding::Closed},{"modulated_closure",PhraseEnding::ModulatedClosure},
        {"interim_pause",PhraseEnding::InterimPause},{"open_unresolved",PhraseEnding::OpenUnresolved},
        {"suspended",PhraseEnding::Suspended},{"loop_return",PhraseEnding::LoopReturn},
        {"modal_development",PhraseEnding::ModalDevelopment},{"center_transition",PhraseEnding::CenterTransition}};
    for(const auto& [name,value]:values)
        if(std::find(t.tags.begin(),t.tags.end(),std::string("G8-ending-")+name)!=t.tags.end())return value;
    return PhraseEnding::Unknown;
}
std::optional<KeySignature> authoredDestination(const ContinuationCandidate& c) {
    if(c.continuation.size()<3)return {};
    const auto n=c.continuation.size();
    const auto p=pitches(c.continuation[n-3]),v=pitches(c.continuation[n-2]),i=pitches(c.continuation[n-1]);
    const bool major=i.quality==ChordQuality::Major||i.quality==ChordQuality::Major7;
    const bool minor=i.quality==ChordQuality::Minor||i.quality==ChordQuality::Minor7;
    if((!major&&!minor)||v.quality!=ChordQuality::Dominant7||mod(v.root-i.root)!=7)return {};
    const auto predominant=mod(p.root-i.root);
    if(predominant!=2&&predominant!=5)return {};
    return KeySignature{PitchClass(i.root),major?Mode::Major:Mode::Minor};
}
std::size_t groupFor(PhraseIntent i) {
    return i==PhraseIntent::Resolve?0:i==PhraseIntent::Loop?2:i==PhraseIntent::Color?3:1;
}
std::string familyFor(const ProgressionTemplate& t) {
    for(const auto& tag:t.tags)if(tag!="V7-editorial")return tag;
    std::string s;
    for(const auto& e:t.full)if(e.degree)s+=std::to_string(e.degree->degree)+":"+std::to_string(e.degree->alteration)+",";
    return s;
}
int qualityTier(const ContinuationCandidate& c) {
    return c.channel==RecommendationChannel::Exact?(c.exploratory?0:c.eligibilityScore>=75?2:1):c.creativeQuality.tier;
}
std::array<std::string,7> features(const ContinuationCandidate& c) {
    const auto first=pitches(c.continuation.front());
    const auto destination=c.targetCenter.value_or(c.originCenter.value_or(c.key));
    std::string bass,melody,color;
    int previous=first.bass,top=-1;
    for(const auto& e:c.continuation) {
        const auto p=pitches(e);
        bass+=std::to_string(mod(p.bass-previous))+",";previous=p.bass;
        int high=p.root;
        for(int n=0;n<12;++n)if(p.intervals&(1u<<n)) {
            const auto tone=mod(p.root+n);
            if(top>=0&&distance(tone,top)<distance(high,top))high=tone;
        }
        if(top>=0)melody+=std::to_string(mod(high-top))+",";top=high;
        if(e.degree.alteration||e.roles&static_cast<RoleFlags>(Role::Borrowed))color+="b";
        if(std::popcount(unsigned(p.intervals))>3)color+="e";
    }
    return {std::to_string(first.root)+":"+std::to_string(int(core(first.quality))),
        bass,c.family,std::to_string(int(destination.tonic))+":"+std::to_string(int(destination.mode)),
        std::to_string(int(c.intent)),color,melody};
}
bool near(double a,double b) {return std::abs(a-b)<1e-6;}
unsigned rotated(unsigned mask,int shift) {
    unsigned out{};for(int n=0;n<12;++n)if(mask&(1u<<n))out|=1u<<mod(n+shift);return out;
}
std::string mechanism(const ProgressionTemplate& t) {
    const auto family=familyFor(t);
    // These source families share a tonal plateau/relative-minor vocabulary.
    // Treating their labels as separate ideas caused G8's apparent diversity.
    if(family=="PlateauLoop"||family=="AeolianLift"||family=="SubdominantPlane")return "tonal-open-plane";
    if(family=="AbsentTonic")return "absent-tonic";
    if(family=="ModalPedal") {
        const auto key=t.rawChordKey.value_or(KeySignature{PitchClass::C,t.mode});
        for(std::size_t i=0;i<t.full.size();++i)
            if(mod(pitches(sourceEvent(t,i,key)).root-int(key.tonic))==1)return "phrygian-pedal";
        return "dorian-pedal";
    }
    if(family=="ChangingPedal")return "changing-upper-pedal";
    if(family=="MinorLine")return "chromatic-bass-line";
    if(family=="AscendingBass")return "ascending-bass";
    if(family=="LydianExpansion")return "lydian-expansion";
    if(family=="ChromaticMediantBridge"||family=="MinorMediantMotion")return "chromatic-center";
    if(family=="SuspendedSequence")return "suspended-sequence";
    if(family=="BorrowedPlagal")return "borrowed-minor-plane";
    return family.empty()?"unclassified-source":family;
}
struct HistoryEvidence {
    std::set<std::string> families,mechanisms;
    bool tailSourceMatched{};
    std::optional<double> sourceOpenHold;
    std::optional<double> reviewedTailStart;
    std::string tailGrouping;
};
HistoryEvidence historyEvidence(const MatchQuery& query,const CandidateIndex& index) {
    HistoryEvidence result;
    std::set<double> tailHolds;
    std::vector<ChordPitchSet> input;for(const auto& e:query.source)input.push_back(chordPitches(e));
    for(const auto& t:index.templates()) {
        const auto n=t.full.size();if(n<3||n>input.size())continue;
        if(query.timeSigNumerator&&query.timeSigDenominator&&
            (*query.timeSigNumerator!=t.meterNumerator||*query.timeSigDenominator!=t.meterDenominator))continue;
        const KeySignature key{PitchClass::C,t.mode};
        std::vector<ChordPitchSet> pattern;std::vector<double> durations;
        bool valid=true;
        for(std::size_t i=0;i<n;++i) {
            const auto event=sourceEvent(t,i,key);pattern.push_back(pitches(event));durations.push_back(event.durationQN);
            valid&=pattern.back().bass>=0&&std::isfinite(event.durationQN)&&event.durationQN>0;
        }
        if(!valid)continue;
        for(std::size_t begin=0;begin+n<=input.size();++begin) {
            const int shift=mod(input[begin].bass-pattern[0].bass);
            bool match=true;
            for(std::size_t j=0;j<n&&match;++j) {
                const auto& actual=query.source[begin+j];
                match=input[begin+j].bass==mod(pattern[j].bass+shift)&&
                    absoluteMask(input[begin+j])==rotated(absoluteMask(pattern[j]),shift);
                if(j+1<n)match&=near(query.source[begin+j+1].startQN-actual.startQN,durations[j]);
                if(actual.durationQN&&!actual.openEnded)match&=near(*actual.durationQN,durations[j]);
            }
            if(!match)continue;
            // A music/timing match is evidence of material reuse, not recovery
            // of a lost source ID from a MIDI chord symbol.
            result.families.insert(familyFor(t));result.mechanisms.insert(mechanism(t));
            if(begin+n==input.size()) {
                result.tailSourceMatched=true;
                tailHolds.insert(durations.back());
                if(t.version==8&&reviewed(t)&&n==8&&
                    std::all_of(durations.begin(),durations.end(),[](double d){return near(d,4);})) {
                    result.reviewedTailStart=query.source[begin].startQN;
                    result.tailGrouping="4+4-bar source grouping";
                }
                // Focused G9 review of the reported V6 counterexample: two
                // bars of tonic preparation precede two four-bar groups. Bind
                // the review to exact sounding music and authored durations,
                // not just an ID or a convenient total-length multiple.
                if(t.id=="V6_B3_02_054"&&t.version==6&&n==8) {
                    constexpr int roots[]{0,10,5,3,5,0,5,0},basses[]{0,2,9,7,8,7,5,0};
                    constexpr unsigned masks[]{145,1060,545,1160,289,145,545,145};
                    bool intact=true;
                    for(std::size_t j=0;j<n;++j)intact&=pattern[j].root==roots[j]&&
                        pattern[j].bass==basses[j]&&absoluteMask(pattern[j])==masks[j]&&
                        near(durations[j],j==0||j==7?8.:4.);
                    if(intact) {
                        result.reviewedTailStart=query.source[begin].startQN+8;
                        result.tailGrouping="reviewed two-bar tonic preparation + 4+4-bar groups; asymmetric whole source retained";
                    }
                }
            }
        }
    }
    // Conflicting source endings do not authorize choosing a convenient hold.
    if(tailHolds.size()==1)result.sourceOpenHold=*tailHolds.begin();
    return result;
}
float materialSimilarity(const Progression& input,const ContinuationCandidate& c) {
    if(input.size()<4||c.continuation.size()<4)return 0;
    float best{};
    for(std::size_t a=0;a+4<=input.size();++a)for(std::size_t b=0;b+4<=c.continuation.size();++b) {
        const auto first=chordPitches(input[a]);const auto next=pitches(c.continuation[b]);
        const int shift=mod(first.bass-next.bass);int count{};
        for(std::size_t j=0;j<4;++j) {
            const auto x=chordPitches(input[a+j]),y=pitches(c.continuation[b+j]);
            // Transposition-neutral skeleton and real bass, not inferred IDs.
            count+=x.bass==mod(y.bass+shift)&&x.root==mod(y.root+shift)&&core(x.quality)==core(y.quality);
        }
        best=std::max(best,float(count)/4.f);
    }
    return best;
}
}

std::vector<ContextAnchor> buildContextAnchors(const std::vector<ProgressionTemplate>& templates) {
    std::vector<ContextAnchor> result;
    for(std::size_t i=0;i<templates.size();++i) {
        const auto& t=templates[i];
        if(t.full.size()<2||t.full.size()>64)continue;
        for(std::size_t next=2;next<t.full.size();++next)
            result.push_back({i,next,{},"direct-source internal anchor"});
        if(!reviewed(t)||t.full.size()<3||t.intent==PhraseIntent::Neutral)continue;
        const KeySignature k{PitchClass::C,t.mode};
        const auto start=pitches(realizeContinuation(t.full.front(),k,4));
        constexpr ChordQuality major[]{ChordQuality::Major,ChordQuality::Minor,ChordQuality::Minor,ChordQuality::Major,ChordQuality::Major,ChordQuality::Minor,ChordQuality::Diminished};
        constexpr ChordQuality minor[]{ChordQuality::Minor,ChordQuality::Diminished,ChordQuality::Major,ChordQuality::Minor,ChordQuality::Minor,ChordQuality::Major,ChordQuality::Major};
        for(int degree=1;degree<=7;++degree) {
            MatchEvent terminal;terminal.degree=ScaleDegree{degree,0};
            terminal.quality=(t.mode==Mode::Major?major:minor)[degree-1];
            const auto seam=boundary(pitches(realizeContinuation(terminal,k,4)),start);
            if(seam.passed)result.push_back({i,0,terminal,seam.reason});
            else {
                // A seventh can supply the common tone missing from a triad
                // prototype. Admission still rechecks the user's actual bass/tones.
                auto extended=terminal;
                extended.quality=terminal.quality==ChordQuality::Minor?ChordQuality::Minor7:
                    terminal.quality==ChordQuality::Diminished?ChordQuality::HalfDiminished7:
                    degree==5?ChordQuality::Dominant7:ChordQuality::Major7;
                const auto richer=boundary(pitches(realizeContinuation(extended,k,4)),start);
                if(richer.passed)result.push_back({i,0,terminal,"seventh-prototype entry; "+richer.reason});
            }
            // A suspended sonority need not be the dominant of a global key.
            // The index can expose its other diatonic contexts; the user's
            // actual bass, tones, center and phrase structure still gate entry.
            for(const auto quality:{ChordQuality::Sus2,ChordQuality::Sus4,ChordQuality::Dominant7}) {
                if(quality==ChordQuality::Dominant7&&degree!=5)continue;
                terminal.quality=quality;
                const auto suspended=boundary(pitches(realizeContinuation(terminal,k,4)),start);
                if(suspended.passed)result.push_back({i,0,terminal,suspended.reason});
            }
        }
    }
    return result;
}

std::vector<PhraseLink> buildDevelopmentLinks(const std::vector<ProgressionTemplate>& templates) {
    // Only explicitly reviewed complete open phrases participate. At most four
    // different entry/family choices per endpoint; never combine the full catalogue.
    std::vector<std::size_t> eligible;
    for(std::size_t i=0;i<templates.size();++i)
        if(unterminatedEnding(editorialEnding(templates[i]))&&templates[i].full.size()>=4)eligible.push_back(i);
    std::vector<PhraseLink> result;
    for(const auto from:eligible) {
        const auto& a=templates[from];const KeySignature center{PitchClass::C,a.mode};
        const auto terminal=pitches(sourceEvent(a,a.full.size()-1,center));
        std::vector<PhraseLink> choices;
        for(const auto to:eligible) {
            const auto& b=templates[to];
            if(from==to||a.mode!=b.mode||familyFor(a)==familyFor(b)||
                a.meterNumerator!=b.meterNumerator||a.meterDenominator!=b.meterDenominator)continue;
            const auto seam=boundary(terminal,pitches(sourceEvent(b,0,center)));
            if(seam.passed)choices.push_back({from,to,seam.score,seam.reason});
        }
        std::sort(choices.begin(),choices.end(),[&](const auto& a,const auto& b){
            return a.boundaryScore!=b.boundaryScore?a.boundaryScore>b.boundaryScore:
                templates[a.nextTemplate].id<templates[b.nextTemplate].id;
        });
        std::set<std::string> entries;std::size_t admitted{};
        for(const auto& link:choices) {
            const auto& b=templates[link.nextTemplate];const auto first=pitches(sourceEvent(b,0,center));
            const auto signature=std::to_string(first.root)+":"+std::to_string(first.bass)+":"+familyFor(b);
            if(entries.insert(signature).second) {result.push_back(link);if(++admitted==4)break;}
        }
    }
    return result;
}

bool unterminatedEnding(PhraseEnding e) noexcept {
    return e==PhraseEnding::OpenUnresolved||e==PhraseEnding::Suspended||e==PhraseEnding::LoopReturn||
        e==PhraseEnding::ModalDevelopment||e==PhraseEnding::CenterTransition;
}
const char* phraseEndingName(PhraseEnding e) noexcept {
    switch(e) {
        case PhraseEnding::Closed:return "closed";
        case PhraseEnding::ModulatedClosure:return "modulated-closure";
        case PhraseEnding::InterimPause:return "interim-pause";
        case PhraseEnding::OpenUnresolved:return "open-unresolved";
        case PhraseEnding::Suspended:return "suspended";
        case PhraseEnding::LoopReturn:return "loop-return";
        case PhraseEnding::ModalDevelopment:return "modal-development";
        case PhraseEnding::CenterTransition:return "center-transition";
        default:return "unknown";
    }
}
void annotatePhraseDevelopment(ContinuationCandidate& c,const ProgressionTemplate& item,
    const CandidateIndex& index,KeySignature inputCenter) {
    if(c.continuation.empty())return;
    const bool wholeCycle=c.channel==RecommendationChannel::Exact&&item.loopable&&
        c.continuationStart==item.full.size()&&c.continuation.size()==item.full.size();
    c.sourceEndPreserved=c.continuationStart+c.continuation.size()==item.full.size()||wholeCycle;
    c.completeSourcePhrase=c.sourceEndPreserved&&(c.continuationStart==0||wholeCycle);
    const auto editorial=c.sourceEndPreserved?editorialEnding(item):PhraseEnding::Unknown;
    const auto last=pitches(c.continuation.back());
    const auto authored=authoredDestination(c);
    const auto center=authored.value_or(c.targetCenter.value_or(inputCenter));
    bool cadence{};
    if(c.continuation.size()>=2) {
        const auto before=pitches(c.continuation[c.continuation.size()-2]);
        cadence=last.root==int(center.tonic)&&last.bass==last.root&&
            (core(last.quality)==ChordQuality::Major||core(last.quality)==ChordQuality::Minor)&&
            (mod(before.root-last.root)==7||mod(before.root-last.root)==5);
    }
    // Actual cadential evidence takes precedence over an editorial open tag.
    if(cadence)c.ending=sameCenter(inputCenter,center)?PhraseEnding::Closed:PhraseEnding::ModulatedClosure;
    else if(editorial!=PhraseEnding::Unknown){c.ending=editorial;c.reviewedEnding=true;}
    else if(core(last.quality)==ChordQuality::Sus2||core(last.quality)==ChordQuality::Sus4||last.quality==ChordQuality::Dominant7)
        c.ending=PhraseEnding::Suspended;
    else if(last.root==int(center.tonic))c.ending=PhraseEnding::InterimPause;
    else c.ending=PhraseEnding::Unknown; // no final I does not certify development
    c.endingEvidence=c.reviewedEnding?"complete phrase editorial review; no user audition claim":"observed ending; development unverified";
    if(c.ending==PhraseEnding::CenterTransition) {
        c.targetCenter.reset();c.centerRelation=CenterRelation::Undetermined;
        c.endingEvidence+="; center in transition, no established destination claimed";
    }
    if(!c.sourceEndPreserved||!unterminatedEnding(c.ending))return;
    const auto from=std::find_if(index.templates().begin(),index.templates().end(),[&](const auto& t){return t.id==item.id;});
    if(from==index.templates().end())return;
    const auto fromIndex=std::size_t(from-index.templates().begin());
    for(const auto& link:index.developmentLinks())if(link.sourceTemplate==fromIndex) {
        const auto& next=index.templates()[link.nextTemplate];
        if(boundary(last,pitches(sourceEvent(next,0,c.key))).passed)c.nextPhraseSources.push_back(next.id);
    }
}

void appendContextualContinuations(const MatchQuery& query,const CandidateIndex& index,
    const RecommendationRequest& request,RecommendationSet& output) {
    const auto begin=std::chrono::steady_clock::now();
    if(query.source.size()<2||query.source.size()>maxImportedChordEvents||query.interpretations.empty())return;
    // Bounded recent phrase (up to eight events) plus 4/3/2-event anchors.
    // Historical source remains untouched for playback, durations and hard constraints.
    const auto recentSize=std::min<std::size_t>(8,query.source.size());
    AnalysisContext recentContext;recentContext.timeSigNumerator=query.timeSigNumerator;
    recentContext.timeSigDenominator=query.timeSigDenominator;
    if(query.forcedKey)recentContext.forcedKey=query.interpretations.front().key.key;
    const Progression recent(query.source.end()-recentSize,query.source.end());
    const auto history=historyEvidence(query,index);
    const auto local=makeMatchQuery(recent,recentContext);
    if(local.interpretations.empty())return;
    const auto origin=local.interpretations.front().key.key;
    std::vector<KeySignature> centers;
    const auto addCenter=[&](KeySignature k) {
        if(std::none_of(centers.begin(),centers.end(),[&](auto x){return sameCenter(x,k);}))centers.push_back(k);
    };
    for(std::size_t i=0;i<std::min<std::size_t>(3,local.interpretations.size());++i) {
        const auto k=local.interpretations[i].key.key;addCenter(k);
        addCenter({PitchClass(mod(int(k.tonic)+(k.mode==Mode::Major?9:3))),k.mode==Mode::Major?Mode::Minor:Mode::Major});
    }
    // Earlier context is an additional explanation, never a prerequisite.
    if(!query.forcedKey)for(std::size_t i=0;i<std::min<std::size_t>(2,query.interpretations.size());++i)
        addCenter(query.interpretations[i].key.key);
    const auto current=chordPitches(query.source.back());
    // A new center must share the actual pivot chord with the current center.
    // Its catalogue phrase must then establish that destination (checked below).
    if(!query.forcedKey&&(absoluteMask(current)&scaleMask(origin))==absoluteMask(current))
        for(int tonic=0;tonic<12;++tonic) {
            const KeySignature k{PitchClass(tonic),Mode::Major};
            if(distance(tonic,int(origin.tonic))!=5)continue; // adjacent circle-of-fifths destinations only
            if((absoluteMask(current)&scaleMask(k))==absoluteMask(current))addCenter(k);
        }
    std::map<std::string,ContinuationCandidate> distinct;
    std::set<std::string> exact;
    for(const auto& g:output.groups)for(const auto& c:g)exact.insert(continuationMusicFingerprint(c));
    for(const auto& anchor:index.contextAnchors()) {
        const auto& item=index.templates()[anchor.templateIndex];
        if(query.timeSigNumerator&&query.timeSigDenominator&&
            (*query.timeSigNumerator!=item.meterNumerator||*query.timeSigDenominator!=item.meterDenominator))continue;
        const bool transition=anchor.terminalPrototype.has_value();
        if(transition)++output.transitionExamined;else ++output.contextualExamined;
        for(const auto target:centers) {
            if(target.mode!=item.mode)continue;
            const bool modulation=!sameCenter(origin,target)&&!relativeCenter(origin,target);
            if(modulation&&!transition)continue;
            if(modulation&&((absoluteMask(current)&scaleMask(origin))!=absoluteMask(current)||
                (absoluteMask(current)&scaleMask(target))!=absoluteMask(current)))continue;
            std::size_t length{};
            if(transition) {
                const auto terminal=pitches(realizeContinuation(*anchor.terminalPrototype,target,4));
                // Prototype indexes a harmonic terminal class, not a substitute
                // for the query's voicing. Its actual bass/tones are checked below.
                if(current.root!=terminal.root||core(current.quality)!=core(terminal.quality))continue;
            } else {
                for(const auto n:{recentSize,std::size_t(4),std::size_t(3),std::size_t(2)}) {
                    if(n<2||n>query.source.size()||n>anchor.continuationStart)continue;
                    bool match=true;
                    for(std::size_t j=0;j<n&&match;++j)
                        match=compatible(query.source[query.source.size()-n+j],
                            sourceEvent(item,anchor.continuationStart-n+j,target));
                    if(match){length=n;break;}
                }
                if(!length)continue;
            }
            ContinuationCandidate c;
            c.channel=transition?RecommendationChannel::PhraseTransition:length==query.source.size()?
                RecommendationChannel::ContextFull:length>4?RecommendationChannel::ContextPhrase:length==4?
                RecommendationChannel::ContextFour:length==3?RecommendationChannel::ContextThree:RecommendationChannel::ContextTwo;
            c.provenance=transition?ConnectionProvenance::AlgorithmAssessed:ConnectionProvenance::DirectSource;
            c.primaryTemplate=item.id;c.supportingTemplates={item.id};c.key=target;c.originCenter=origin;c.targetCenter=target;
            c.centerRelation=modulation?CenterRelation::Modulation:relativeCenter(origin,target)?CenterRelation::Relative:CenterRelation::Same;
            c.sourceCoordinateCenter=item.rawChordKey.value_or(KeySignature{PitchClass::C,item.mode});
            c.contextLength=length;c.matchStart=transition?0:anchor.continuationStart-length;
            c.matchEnd=transition?0:anchor.continuationStart-1;c.continuationStart=anchor.continuationStart;
            c.sourceAnchorQN=query.source.front().startQN;c.styles=item.styles;c.family=familyFor(item);
            // Source suffix and authored rhythm remain intact. This projection is
            // also the one consumed by playback/MIDI/Snapshot; no later rebuilding.
            for(std::size_t j=anchor.continuationStart;j<item.full.size();++j)
                c.continuation.push_back(sourceEvent(item,j,target));
            if(c.continuation.empty()||c.continuation.size()+query.source.size()>maxImportedChordEvents)continue;
            bool authoredCenterEstablished{};
            if(reviewed(item))if(const auto destination=authoredDestination(c);destination&&!sameCenter(*destination,target)) {
                c.targetCenter=*destination;
                c.centerRelation=sameCenter(origin,*destination)?CenterRelation::Same:
                    relativeCenter(origin,*destination)?CenterRelation::Relative:CenterRelation::Modulation;
                authoredCenterEstablished=true;
            }
            const auto seam=boundary(current,pitches(c.continuation.front()));
            if(!seam.passed)continue;
            if(modulation) {
                bool established{};
                for(std::size_t j=1;j<c.continuation.size();++j) {
                    const auto a=pitches(c.continuation[j-1]),b=pitches(c.continuation[j]);
                    established|=mod(a.root-int(target.tonic))==7&&b.root==int(target.tonic)&&
                        (a.quality==ChordQuality::Dominant7||core(a.quality)==ChordQuality::Major)&&core(b.quality)==ChordQuality::Major;
                }
                if(!established)continue;
            }
            bool playable=true;std::set<int> roots;int colors{};
            for(const auto& e:c.continuation) {
                const auto p=pitches(e);roots.insert(p.root);
                playable&=p.root>=0&&p.bass>=0&&p.intervals&&std::isfinite(e.durationQN)&&e.durationQN>0&&e.durationQN<=128;
                colors+=e.degree.alteration!=0||std::popcount(unsigned(p.intervals))>3||p.bass!=p.root;
            }
            if(!playable)continue;
            c.suggestedCurrentChordDurationQN=history.sourceOpenHold.value_or(progressionOpenDuration(recent));
            c.constraints=request.constraints;
            c.melodyCompatibility=evaluateMelody(continuationProgression(query.source,c),request.constraints);
            c.hardConstraintsPassed=c.melodyCompatibility.hardSatisfied;
            if(!c.hardConstraintsPassed)continue;
            const auto last=pitches(c.continuation.back()),first=chordPitches(query.source.front());
            c.actualPhraseReturn=last.root==first.root&&last.bass==first.bass&&last.intervals==first.intervals;
            c.pathContextReliable=!transition;c.preservedLoop=false;
            c.intent=item.intent==PhraseIntent::Neutral?PhraseIntent::Develop:item.intent;
            if(c.intent==PhraseIntent::Resolve)continue; // Resolve retains the Unified-only qualification contract
            c.completion=evaluateIntentCompletion(c);
            // Loop requires the actual wrap seam, not the source's loopable flag.
            if(c.intent==PhraseIntent::Loop&&!boundary(last,first).passed&&!c.actualPhraseReturn)c.intent=PhraseIntent::Develop;
            const auto endCenter=c.targetCenter.value_or(target);
            const bool centerShift=c.centerRelation==CenterRelation::Modulation;
            const bool open=last.root!=int(endCenter.tonic)||core(last.quality)==ChordQuality::Sus4||core(last.quality)==ChordQuality::Sus2;
            if(c.intent==PhraseIntent::Develop) {
                if(c.continuation.size()<2||roots.size()<2)continue;
                if(!open&&!modulation&&c.completion.reason==CompletionReason::DominantTonic)continue;
            } else if(c.intent==PhraseIntent::Color&&colors==0)continue;
            c.completion=evaluateIntentCompletion(c);
            if(c.intent==PhraseIntent::Develop) {
                const auto previous=c.continuation.size()>1?pitches(c.continuation[c.continuation.size()-2]):current;
                const bool finalCadence=last.root==int(endCenter.tonic)&&
                    (mod(previous.root-last.root)==7||mod(previous.root-last.root)==5);
                // Retain a sourced modulating cadence as a completed route, not
                // as evidence of open development. Ending drives inspiration exposure.
                if(finalCadence)c.completion={.8f,CompletionReason::DominantTonic};
                else if(!open)c.completion={.6f,CompletionReason::StableTonic};
                else c.completion={.8f,centerShift?CompletionReason::FunctionalPivot:CompletionReason::DevelopedPath};
            }
            if(c.intent==PhraseIntent::Loop)c.completion={.8f,CompletionReason::LoopClosure};
            if(c.intent==PhraseIntent::Color&&colors)c.completion={.8f,CompletionReason::AudibleColor};
            c.exploratory=false;
            auto& q=c.creativeQuality;
            q.boundary=seam.score;q.context=transition?.75f:length>=3?1.f:.85f;
            q.center=modulation?.8f:1.f;q.source=reviewed(item)?.9f:.7f;q.playability=1.f;
            q.phrase=roots.size()>=3?.95f:c.continuation.size()>=2?.8f:.55f;
            // Independent conjunctive gates. There is no relaxed Unified cutoff.
            q.qualified=q.boundary>=.65f&&q.context>=.75f&&q.center>=.8f&&q.source>=.7f&&q.phrase>=.55f;
            if(!q.qualified)continue;
            // A larger count of shared tones does not make a repeated tonic
            // extension a better quality band than a functional new direction.
            q.tier=q.phrase>=.8f&&(q.source>=.9f||length>=3)?2:1;
            c.baseQualified=true;c.eligibilityScore=0; // never a Unified score
            c.rankingScore=100.f*(.3f*q.boundary+.2f*q.context+.15f*q.center+.15f*q.source+.2f*q.phrase);
            c.matchSimilarity=transition?0.f:1.f;
            const bool borrowed=std::any_of(c.continuation.begin(),c.continuation.end(),[&](const auto& e) {
                return e.degree.alteration!=0||(absoluteMask(pitches(e))&~scaleMask(target))!=0;
            });
            c.direction=centerShift||borrowed?CreativeDirection::Bold:CreativeDirection::Rich;
            annotatePhraseDevelopment(c,item,index,origin);
            c.connectionReason=(transition?"algorithm-assessed seam; complete editorial phrase, no independent seam review; ":
                "direct-source suffix; contiguous "+std::to_string(length)+"-chord context; ")+seam.reason;
            if(modulation)c.connectionReason+="; shared diatonic pivot and destination V-I";
            else if(relativeCenter(origin,target))c.connectionReason+="; relative major/minor interpretation";
            if(authoredCenterEstablished)c.connectionReason+="; source-authored predominant/V7/I establishes "+formatKey(*c.targetCenter);
            c.connectionReason+="; ending="+std::string(phraseEndingName(c.ending));
            if(!c.nextPhraseSources.empty())c.connectionReason+="; "+std::to_string(c.nextPhraseSources.size())+" indexed next-phrase seams (constraints rechecked on next import)";
            if(transition)++output.transitionQualified;else ++output.contextualQualified;
            const auto identity=continuationMusicFingerprint(c);
            if(exact.contains(identity))continue;
            c.id="g6:"+std::string(recommendationChannelName(c.channel))+":"+identity;
            auto found=distinct.find(identity);
            const auto better=[&](const auto& old) {
                if(old.provenance!=c.provenance)return c.provenance==ConnectionProvenance::DirectSource;
                if(old.contextLength!=c.contextLength)return c.contextLength>old.contextLength;
                return std::tuple(-q.tier,-c.rankingScore,c.primaryTemplate)<
                    std::tuple(-old.creativeQuality.tier,-old.rankingScore,old.primaryTemplate);
            };
            if(found==distinct.end())distinct.emplace(identity,std::move(c));
            else if(better(found->second))found->second=std::move(c);
        }
    }
    for(auto& [id,c]:distinct)output.groups[groupFor(c.intent)].push_back(std::move(c));
    output.contextualMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
}

void assessInspirationStructure(const MatchQuery& query,const CandidateIndex& index,RecommendationSet& set) {
    if(query.source.empty())return;
    const auto history=historyEvidence(query,index);
    const bool meterKnown=query.timeSigNumerator&&query.timeSigDenominator&&
        *query.timeSigNumerator>0&&*query.timeSigDenominator>0;
    const double bar=meterKnown?4.0*(*query.timeSigNumerator)/(*query.timeSigDenominator):0;
    const double fallback=progressionOpenDuration(query.source);
    const auto aligned=[](double value,double unit){return unit>0&&near(value/unit,std::round(value/unit));};
    for(auto& group:set.groups)for(auto& c:group) {
        c.structureQualified=false;c.structurePriority=0;
        const auto source=std::find_if(index.templates().begin(),index.templates().end(),
            [&](const auto& t){return t.id==c.primaryTemplate;});
        if(source==index.templates().end()){c.structureReason="unproven source structure";continue;}
        c.developmentMechanism=mechanism(*source);
        c.historyFamilyPresent=history.families.contains(familyFor(*source));
        c.historyMechanismPresent=history.mechanisms.contains(c.developmentMechanism);
        c.historySimilarity=materialSimilarity(query.source,c);
        double end=query.source.front().startQN;bool contiguous=true;
        for(std::size_t i=0;i<query.source.size();++i) {
            const auto& e=query.source[i];
            const auto duration=e.durationQN&&!e.openEnded?*e.durationQN:
                i+1==query.source.size()?c.suggestedCurrentChordDurationQN.value_or(fallback):0.;
            contiguous&=std::isfinite(duration)&&duration>0&&near(e.startQN,end);
            end=e.startQN+duration;
        }
        c.seamQN=end-query.source.front().startQN;
        if(!contiguous){c.structureReason="unproven discontinuous input timeline";continue;}
        if(!meterKnown){c.structureReason="unproven meter and phrase downbeat";continue;}
        if(source->meterNumerator!=*query.timeSigNumerator||source->meterDenominator!=*query.timeSigDenominator) {
            c.structureReason="unproven meter change";continue;
        }
        if(!c.sourceEndPreserved){c.structureReason="unproven original source end";continue;}
        const bool wholeCycle=c.channel==RecommendationChannel::Exact&&source->loopable&&
            c.continuationStart==source->full.size()&&c.completeSourcePhrase;
        if(!c.completeSourcePhrase) {
            // A local catalogue relationship is valid retrieval evidence. It
            // does not prove a new phrase began inside an already complete input.
            const auto length=c.channel==RecommendationChannel::Exact?query.source.size():c.contextLength;
            const bool fromSelectionStart=length==query.source.size()&&c.matchStart==0&&
                c.continuationStart==query.source.size();
            if(!fromSelectionStart){c.structureReason="unproven shared-material overlap; local tail is not an independent phrase start";continue;}
            bool rhythm=true;
            for(std::size_t i=0;i<query.source.size();++i) {
                const auto sourceChord=sourceEvent(*source,i,c.key);
                const auto expected=sourceChord.durationQN;
                const auto actual=i+1==query.source.size()?end-query.source[i].startQN:
                    query.source[i+1].startQN-query.source[i].startQN;
                rhythm&=near(actual,expected)&&compatible(query.source[i],sourceChord);
            }
            if(!rhythm){c.structureReason="unproven source-prefix rhythm alignment";continue;}
            c.structureQualified=true;c.structurePriority=1;
            c.structureReason="contiguous source prefix from selection start to original ending; no shared events emitted twice";
            continue;
        }
        if(!aligned(c.seamQN,bar)){c.structureReason="unproven off-bar seam; no pickup/delay evidence";continue;}
        if(c.continuation.empty()||!boundary(chordPitches(query.source.back()),pitches(c.continuation.front())).passed) {
            c.structureReason="unproven actual-note harmonic seam";continue;
        }
        double total{};bool authored=true;
        for(const auto& e:c.continuation){total+=e.durationQN;authored&=std::isfinite(e.durationQN)&&e.durationQN>0;}
        const bool reviewedGroups=source->version==8&&reviewed(*source)&&source->full.size()==8&&
            c.continuation.size()==8&&near(total,32)&&
            std::all_of(c.continuation.begin(),c.continuation.end(),[](const auto& e){return near(e.durationQN,4);});
        const bool sourceMetric=authored&&aligned(total,bar)&&
            (aligned(total,4*bar)||aligned(total,8*bar));
        const bool knownTail=history.reviewedTailStart&&near(end-*history.reviewedTailStart,32);
        const bool inputMetric=history.tailSourceMatched&&c.seamQN>=4*bar-1e-6&&aligned(c.seamQN,4*bar);
        if(!(sourceMetric&&(inputMetric||knownTail))) {
            c.structureReason="unproven asymmetric phrase grouping; authored timing retained, no automatic padding or overlap";continue;
        }
        c.structureQualified=true;c.structurePriority=reviewedGroups?2:1;
        c.structureReason=wholeCycle?"complete cycle appended once after a supported phrase boundary":
            reviewedGroups?"whole source, 4+4-bar grouping; independent append after metric/source-supported boundary; algorithm-assessed seam, no independent seam review":
            "whole source and regular metric boundary; inferred grouping, no independent seam review";
        if(query.source.back().openEnded&&history.sourceOpenHold&&
            near(end-query.source.back().startQN,*history.sourceOpenHold))
            c.structureReason+="; OPEN hold supported by full actual music/timing match, source ID not recovered";
        if(knownTail&&!history.tailGrouping.empty())c.structureReason+="; input="+history.tailGrouping;
    }
}
void rankCreativeContinuations(std::vector<ContinuationCandidate>& group) {
    // Preserve every legal route. Diversity changes only display order. Start
    // from deterministic identity order and compare raw scores within a channel.
    std::sort(group.begin(),group.end(),[](const auto& a,const auto& b){
        return std::tuple(a.channel,a.primaryTemplate,a.key.tonic,a.key.mode,a.matchStart,a.id)<
            std::tuple(b.channel,b.primaryTemplate,b.key.tonic,b.key.mode,b.matchStart,b.id);
    });
    std::vector<ContinuationCandidate> ranked;ranked.reserve(group.size());
    std::array<std::size_t,3> selected{};
    std::array<std::map<std::string,int>,7> seen;
    std::vector<std::array<std::string,7>> fs;for(const auto& c:group)fs.push_back(features(c));
    std::vector<bool> used(group.size());
    constexpr float ratios[]{.2f,.5f,.3f};
    for(std::size_t slot=0;slot<group.size();++slot) {
        std::size_t best=group.size();float bestUtility=-1e9f;
        for(std::size_t i=0;i<group.size();++i)if(!used[i]) {
            const auto& c=group[i];const auto d=std::size_t(c.direction);
            float penalty{};constexpr float weights[]{8,5,10,3,4,2,2};
            for(std::size_t f=0;f<7;++f) {
                const auto it=seen[f].find(fs[i][f]);if(it!=seen[f].end())penalty+=weights[f]*float(it->second);
            }
            const float deficit=ratios[d]*float(slot+1)-float(selected[d]);
            const float utility=qualityTier(c)*40.f+deficit*6.f-penalty;
            if(best==group.size()||(c.structureQualified&&!group[best].structureQualified)||
               (c.structureQualified==group[best].structureQualified&&(utility>bestUtility+.001f||
               (std::abs(utility-bestUtility)<.001f&&c.channel==group[best].channel&&c.rankingScore>group[best].rankingScore)))) {
                best=i;bestUtility=utility;
            }
        }
        used[best]=true;++selected[std::size_t(group[best].direction)];
        for(std::size_t f=0;f<7;++f)++seen[f][fs[best][f]];
        ranked.push_back(std::move(group[best]));
    }
    group=std::move(ranked);
}
const char* recommendationChannelName(RecommendationChannel c) noexcept {
    switch(c) {
        case RecommendationChannel::ContextFull:return "context-full";
        case RecommendationChannel::ContextThree:return "context-three";
        case RecommendationChannel::ContextTwo:return "context-two";
        case RecommendationChannel::ContextFour:return "context-four";
        case RecommendationChannel::ContextPhrase:return "context-phrase";
        case RecommendationChannel::PhraseTransition:return "phrase-transition";
        default:return "exact";
    }
}
int recommendationQualityTier(const ContinuationCandidate& c) noexcept { return qualityTier(c); }
CreativeDirection inspirationDirection(const ContinuationCandidate& c,std::optional<KeySignature> inputCenter) {
    if(c.channel!=RecommendationChannel::Exact)return c.direction;
    const auto context=inputCenter.value_or(c.key);
    auto allowed=scaleMask(context);
    if(context.mode==Mode::Minor)allowed|=1u<<mod(int(context.tonic)+11); // ordinary minor dominant is not a bold modulation
    bool rich=c.continuation.size()>8;
    for(const auto& e:c.continuation) {
        const auto p=pitches(e);
        if((absoluteMask(p)&~allowed)!=0)return CreativeDirection::Bold;
        rich|=p.bass!=p.root||(p.intervals&((1u<<2)|(1u<<5)|(1u<<9)))!=0;
    }
    return rich?CreativeDirection::Rich:CreativeDirection::Stable;
}
std::vector<RecommendationPosition> mixedInspirationOrder(const RecommendationSet& set) {
    if(!set.inspirationOrder.empty())return set.inspirationOrder;
    struct Item { RecommendationPosition position; std::array<std::string,7> features; };
    std::vector<Item> pool;
    std::set<std::string> identities;
    for(std::size_t g=0;g<set.groups.size();++g)for(std::size_t i=0;i<set.groups[g].size();++i) {
        const auto& c=set.groups[g][i];
        if(!c.baseQualified||!c.hardConstraintsPassed||!std::isfinite(c.rankingScore)||c.continuation.empty())continue;
        if(identities.insert(continuationMusicFingerprint(c)).second)pool.push_back({{g,i},features(c)});
    }
    const auto candidate=[&](std::size_t i)->const ContinuationCandidate& {const auto p=pool[i].position;return set.groups[p.group][p.index];};
    const auto sourceClass=[](const ContinuationCandidate& c) {
        return c.channel==RecommendationChannel::Exact?0:c.channel==RecommendationChannel::PhraseTransition?2:1;
    };
    const auto qualityBand=[](const ContinuationCandidate& c) {
        if(c.exploratory)return 0;
        if(c.channel==RecommendationChannel::Exact)return c.eligibilityScore>=70?2:1;
        if(c.continuation.size()<2)return 0; // a color fragment remains pageable; prefer independent multi-chord routes on the homepage
        // A reviewed phrase does not confer an independently reviewed seam.
        return c.provenance==ConnectionProvenance::AlgorithmAssessed?1:c.creativeQuality.tier;
    };
    std::array<std::size_t,3> directions{},sources{};
    std::map<int,std::size_t> endings;
    std::map<std::string,int> mechanismUse,familyUse;
    std::array<std::map<std::string,int>,7> seen;
    std::vector<bool> used(pool.size());
    std::vector<RecommendationPosition> order;order.reserve(pool.size());
    constexpr float directionTargets[]{.2f,.5f,.3f},sourceTargets[]{.25f,.4f,.35f};
    constexpr float diversityWeights[]{6,5,8,4,3,2,2};
    for(std::size_t slot=0;slot<pool.size();++slot) {
        std::size_t best=pool.size();float bestUtility=-1e9f;
        const auto independent=[](const ContinuationCandidate& c) {
            return c.structureQualified&&!c.exploratory&&c.completeSourcePhrase&&c.reviewedEnding&&unterminatedEnding(c.ending);
        };
        const auto coverage=[&](const ContinuationCandidate& c) {
            return std::tuple(int(c.historyMechanismPresent||mechanismUse[c.developmentMechanism]>0),
                int(c.historyFamilyPresent||familyUse[c.family]>0),int(c.historySimilarity>=.75f));
        };
        std::optional<std::tuple<int,int,int>> availableCoverage;
        // First select nonredundant, independently qualified development
        // mechanisms; only then compare utility. No musical score is weakened
        // and no legal candidate is deleted from later pages.
        for(std::size_t i=0;i<pool.size();++i)if(!used[i]&&independent(candidate(i))) {
            const auto key=coverage(candidate(i));
            if(!availableCoverage||key<*availableCoverage)availableCoverage=key;
        }
        for(std::size_t i=0;i<pool.size();++i)if(!used[i]) {
            const auto& c=candidate(i);
            if(independent(c)&&availableCoverage&&coverage(c)>*availableCoverage)continue;
            const auto d=std::size_t(c.direction),s=std::size_t(sourceClass(c));
            float penalty{};
            for(std::size_t f=0;f<7;++f) {
                const auto found=seen[f].find(pool[i].features[f]);
                if(found!=seen[f].end())penalty+=diversityWeights[f]*float(found->second);
            }
            const float directionDeficit=directionTargets[d]*float(slot+1)-float(directions[d]);
            const float sourceDeficit=sourceTargets[s]*float(slot+1)-float(sources[s]);
            // Reviewed full phrases with actual next-entry edges get exposure;
            // a new key or a non-tonic last chord alone earns no such benefit.
            const bool developing=c.sourceEndPreserved&&c.reviewedEnding&&unterminatedEnding(c.ending);
            const float development=developing?(c.nextPhraseSources.empty()?2.f:
                c.completeSourcePhrase?7.f:c.continuation.size()>=4?4.f:1.f):0.f;
            const bool closed=c.ending==PhraseEnding::Closed||c.ending==PhraseEnding::ModulatedClosure;
            const float endingPenalty=closed?3.f*float(endings[int(PhraseEnding::Closed)]):0.f;
            const float utility=qualityBand(c)*6.f+8.f*(directionDeficit+sourceDeficit)+development-penalty-endingPenalty;
            const bool better=best==pool.size()||(c.structureQualified&&!candidate(best).structureQualified)||
                (c.structureQualified==candidate(best).structureQualified&&
                 ((!c.exploratory&&candidate(best).exploratory)||
                 (c.exploratory==candidate(best).exploratory&&(utility>bestUtility+.001f||
                    (std::abs(utility-bestUtility)<.001f&&c.channel==candidate(best).channel&&c.rankingScore>candidate(best).rankingScore)))));
            if(better){best=i;bestUtility=utility;}
        }
        used[best]=true;const auto& c=candidate(best);
        ++directions[std::size_t(c.direction)];++sources[std::size_t(sourceClass(c))];
        ++mechanismUse[c.developmentMechanism];++familyUse[c.family];
        if(c.ending==PhraseEnding::Closed||c.ending==PhraseEnding::ModulatedClosure)++endings[int(PhraseEnding::Closed)];
        for(std::size_t f=0;f<7;++f)++seen[f][pool[best].features[f]];
        order.push_back(pool[best].position);
    }
    return order;
}
}
