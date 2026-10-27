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
    for(int i=0;i<12;++i)if(c.intervals&(1u<<i))mask|=1u<<mod(c.root+i);
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
    if(a.root==b.root&&a.bass==b.bass&&a.intervals==b.intervals)return {};
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
    return t.sourceType=="factory"&&t.version==7&&t.id.starts_with("V7_")&&
        std::find(t.tags.begin(),t.tags.end(),"V7-editorial")!=t.tags.end();
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
    const auto destination=c.targetCenter.value_or(c.key);
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
            if(degree==5)for(const auto quality:{ChordQuality::Sus4,ChordQuality::Dominant7}) {
                terminal.quality=quality;
                const auto suspended=boundary(pitches(realizeContinuation(terminal,k,4)),start);
                if(suspended.passed)result.push_back({i,0,terminal,suspended.reason});
            }
        }
    }
    return result;
}

void appendContextualContinuations(const MatchQuery& query,const CandidateIndex& index,
    const RecommendationRequest& request,RecommendationSet& output) {
    const auto begin=std::chrono::steady_clock::now();
    if(query.source.size()<2||query.source.size()>64||query.interpretations.empty())return;
    const auto origin=query.interpretations.front().key.key;
    std::vector<KeySignature> centers;
    const auto addCenter=[&](KeySignature k) {
        if(std::none_of(centers.begin(),centers.end(),[&](auto x){return sameCenter(x,k);}))centers.push_back(k);
    };
    for(std::size_t i=0;i<std::min<std::size_t>(3,query.interpretations.size());++i) {
        const auto k=query.interpretations[i].key.key;addCenter(k);
        addCenter({PitchClass(mod(int(k.tonic)+(k.mode==Mode::Major?9:3))),k.mode==Mode::Major?Mode::Minor:Mode::Major});
    }
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
                for(const auto n:{query.source.size(),std::size_t(3),std::size_t(2)}) {
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
                RecommendationChannel::ContextFull:length==3?RecommendationChannel::ContextThree:RecommendationChannel::ContextTwo;
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
            if(c.continuation.empty()||c.continuation.size()+query.source.size()>64)continue;
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
            c.suggestedCurrentChordDurationQN=progressionOpenDuration(query.source);
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
                if(finalCadence&&!centerShift)continue;
                if(!open&&!centerShift)continue;
                c.completion={.8f,centerShift?CompletionReason::FunctionalPivot:CompletionReason::DevelopedPath};
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
            c.connectionReason=(transition?"algorithm-assessed seam; V7 editorial phrase, no independent seam review; ":
                "direct-source suffix; contiguous "+std::to_string(length)+"-chord context; ")+seam.reason;
            if(modulation)c.connectionReason+="; shared diatonic pivot and destination V-I";
            else if(relativeCenter(origin,target))c.connectionReason+="; relative major/minor interpretation";
            if(authoredCenterEstablished)c.connectionReason+="; source-authored predominant/V7/I establishes "+formatKey(*c.targetCenter);
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
            if(best==group.size()||utility>bestUtility+.001f||
               (std::abs(utility-bestUtility)<.001f&&c.channel==group[best].channel&&c.rankingScore>group[best].rankingScore)) {
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
        case RecommendationChannel::PhraseTransition:return "phrase-transition";
        default:return "exact";
    }
}
int recommendationQualityTier(const ContinuationCandidate& c) noexcept { return qualityTier(c); }
CreativeDirection inspirationDirection(const ContinuationCandidate& c) {
    if(c.channel!=RecommendationChannel::Exact)return c.direction;
    auto allowed=scaleMask(c.key);
    if(c.key.mode==Mode::Minor)allowed|=1u<<mod(int(c.key.tonic)+11); // ordinary minor dominant is not a bold modulation
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
        // A reviewed phrase does not confer an independently reviewed seam.
        return c.provenance==ConnectionProvenance::AlgorithmAssessed?1:c.creativeQuality.tier;
    };
    std::array<std::size_t,3> directions{},sources{};
    std::array<std::map<std::string,int>,7> seen;
    std::vector<bool> used(pool.size());
    std::vector<RecommendationPosition> order;order.reserve(pool.size());
    constexpr float directionTargets[]{.2f,.5f,.3f},sourceTargets[]{.25f,.4f,.35f};
    constexpr float diversityWeights[]{6,5,8,4,3,2,2};
    for(std::size_t slot=0;slot<pool.size();++slot) {
        std::size_t best=pool.size();float bestUtility=-1e9f;
        for(std::size_t i=0;i<pool.size();++i)if(!used[i]) {
            const auto& c=candidate(i);
            const auto d=std::size_t(c.direction),s=std::size_t(sourceClass(c));
            float penalty{};
            for(std::size_t f=0;f<7;++f) {
                const auto found=seen[f].find(pool[i].features[f]);
                if(found!=seen[f].end())penalty+=diversityWeights[f]*float(found->second);
            }
            const float directionDeficit=directionTargets[d]*float(slot+1)-float(directions[d]);
            const float sourceDeficit=sourceTargets[s]*float(slot+1)-float(sources[s]);
            const float utility=qualityBand(c)*6.f+8.f*(directionDeficit+sourceDeficit)-penalty;
            const bool better=best==pool.size()||(!c.exploratory&&candidate(best).exploratory)||
                (c.exploratory==candidate(best).exploratory&&(utility>bestUtility+.001f||
                    (std::abs(utility-bestUtility)<.001f&&c.channel==candidate(best).channel&&c.rankingScore>candidate(best).rankingScore)));
            if(better){best=i;bestUtility=utility;}
        }
        used[best]=true;const auto& c=candidate(best);
        ++directions[std::size_t(c.direction)];++sources[std::size_t(sourceClass(c))];
        for(std::size_t f=0;f<7;++f)++seen[f][pool[best].features[f]];
        order.push_back(pool[best].position);
    }
    return order;
}
}
