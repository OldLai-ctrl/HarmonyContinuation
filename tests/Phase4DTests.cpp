#include "plugin/DevelopmentMode.h"
#include "plugin/RecommendationWorker.h"
#include "plugin/HostContextAdapter.h"
#include "plugin/VstXmlChordParser.h"
#include "session/ProductServices.h"
#include "midi/MidiWorkflow.h"
#include "midi/MidiImportWorkflow.h"
#include "midi/StandardMidiFileWriter.h"
#include "preview/OfflinePreviewRenderer.h"
#include "snapshot/RecommendationSnapshot.h"
#include "color/ColorPreferenceReranker.h"
#include "color/RelativeColorCurve.h"
#include "ui/EnrichmentProvenance.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#if defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib,"Psapi.lib")
#endif
using namespace harmony;
namespace {
int checks{};
void require(bool condition,const char* why) {++checks;if(!condition)throw std::runtime_error(why);}
bool closeQN(double a,double b){return std::abs(a-b)<0.003;}
Progression phrase(std::vector<std::string> names,std::vector<double> durations,bool open=false) {
    Progression p;double at{};
    for(std::size_t i=0;i<names.size();++i){ChordEvent e;e.name=names[i];e.startQN=at;e.durationQN=durations[i];e.openEnded=false;p.push_back(e);at+=durations[i];}
    if(open){p.back().durationQN.reset();p.back().openEnded=true;}return p;
}
void runCurveChecks() {
    using color::RelativeColorComponent;
    using ui::EnrichmentProvenanceKind;
    constexpr auto baseIndex=static_cast<std::size_t>(RelativeColorComponent::Base);
    constexpr auto structureIndex=static_cast<std::size_t>(RelativeColorComponent::SpecialStructure);
    constexpr auto alteredIndex=static_cast<std::size_t>(RelativeColorComponent::Altered);
    constexpr auto contrastIndex=static_cast<std::size_t>(RelativeColorComponent::Contrast);

    const auto source=phrase({"C","G"},{4.,4.});
    const auto inserted=phrase({"C","Dm","G"},{2.,2.,4.});
    const auto insertionMarks=ui::enrichmentProvenance(source,inserted);
    require(insertionMarks.size()==3&&insertionMarks[0].kind==EnrichmentProvenanceKind::Original&&
        insertionMarks[1].kind==EnrichmentProvenanceKind::Inserted&&!insertionMarks[1].sourceIndex&&
        insertionMarks[2].kind==EnrichmentProvenanceKind::Original&&insertionMarks[2].sourceIndex==1,
        "visual provenance identifies only the internal onset as inserted");

    const auto replaced=ui::enrichmentProvenance(source,phrase({"C","F"},{4.,4.}));
    require(replaced[1].kind==EnrichmentProvenanceKind::Replaced&&replaced[1].sourceIndex==1,
        "visual provenance keeps a same-onset replacement separate from insertion");
    const auto triadChanged=ui::enrichmentProvenance(source,phrase({"Cm","G"},{4.,4.}));
    require(triadChanged[0].kind==EnrichmentProvenanceKind::Replaced,
        "visual provenance detects a defining third change at a stable root");
    const auto extended=ui::enrichmentProvenance(source,phrase({"Cmaj7","G"},{4.,4.}));
    require(extended[0].kind==EnrichmentProvenanceKind::Modified&&extended[0].sourceIndex==0,
        "visual provenance treats a seventh as a modification");
    const auto inversion=ui::enrichmentProvenance(source,phrase({"C/G","G"},{4.,4.}));
    require(inversion[0].kind==EnrichmentProvenanceKind::Modified,
        "visual provenance treats an inversion as a modification");
    const auto incomplete=ui::enrichmentProvenance(source,phrase({"C"},{4.}));
    require(incomplete.size()==1&&incomplete[0].kind==EnrichmentProvenanceKind::Unknown&&
        !incomplete[0].sourceIndex,"visual provenance degrades incomplete onset coverage to unknown");
    Progression malformed=source;
    malformed[0].name="unparsed chord";
    const auto unknownChord=ui::enrichmentProvenance(source,malformed);
    require(unknownChord[0].kind==EnrichmentProvenanceKind::Unknown&&unknownChord[0].sourceIndex==0,
        "visual provenance preserves onset mapping but marks malformed pitch data unknown");

    const KeySignature cMajor{PitchClass::C,Mode::Major};
    const auto plain=color::analyzeRelativeColorCurve(phrase({"C"},{4.}),cMajor);
    const auto seventh=color::analyzeRelativeColorCurve(phrase({"Cmaj7"},{4.}),cMajor);
    const auto extension=color::analyzeRelativeColorCurve(phrase({"C9"},{4.}),cMajor);
    require(plain.size()==1&&seventh.size()==1&&extension.size()==1&&plain[0].value&&
        seventh[0].value&&extension[0].value&&*seventh[0].value>*plain[0].value&&
        *extension[0].value>*plain[0].value,"relative curve raises seven-note and extension structure");
    const auto repeated=color::analyzeRelativeColorCurve(phrase({"C","G7","Am","F"},{2.,2.,2.,2.}),cMajor);
    const auto repeatedAgain=color::analyzeRelativeColorCurve(phrase({"C","G7","Am","F"},{2.,2.,2.,2.}),cMajor);
    const auto transposed=color::analyzeRelativeColorCurve(phrase({"D","A7","Bm","G"},{2.,2.,2.,2.}),
        KeySignature{PitchClass::D,Mode::Major});
    require(repeated.size()==4&&repeatedAgain.size()==4&&transposed.size()==4,
        "relative curve returns one point per resolved event");
    for(std::size_t i=0;i<repeated.size();++i)
        require(repeated[i].value&&repeatedAgain[i].value&&transposed[i].value&&
            std::abs(*repeated[i].value-*repeatedAgain[i].value)<1e-9&&
            std::abs(*repeated[i].value-*transposed[i].value)<1e-9,
            "relative curve is deterministic and transposition/key invariant");
    const auto timed=color::analyzeRelativeColorCurve(inserted,cMajor);
    require(timed.size()==3&&timed[0].startQN==0.&&timed[0].durationQN==2.&&
        timed[1].startQN==2.&&timed[1].durationQN==2.&&timed[2].startQN==4.&&
        timed[2].durationQN==4.,"relative curve preserves event QN and duration");

    const auto special=color::analyzeRelativeColorCurve(phrase({"Cdim","Caug"},{4.,4.}),cMajor);
    require(special.size()==2&&special[0].value&&special[1].value&&
        special[0].contributions[structureIndex]==20.&&special[0].contributions[alteredIndex]==0.&&
        special[1].contributions[structureIndex]==18.&&special[1].contributions[alteredIndex]==0.,
        "diminished and augmented fifths receive structure bonuses without duplicate alteration");

    Progression invalidThenKnown=phrase({"C","G7"},{4.,4.});
    invalidThenKnown[0].name="unparsed chord";
    const auto degraded=color::analyzeRelativeColorCurve(invalidThenKnown,cMajor);
    require(degraded.size()==2&&!degraded[0].value&&degraded[1].value&&
        degraded[1].contributions[baseIndex]==10.&&degraded[1].contributions[contrastIndex]==0.,
        "unknown chord leaves a gap and resets contrast context");
    const auto diminishedSeventh=color::analyzeRelativeColorCurve(phrase({"Cdim7"},{4.}),cMajor);
    require(diminishedSeventh[0].value&&diminishedSeventh[0].contributions[static_cast<std::size_t>(RelativeColorComponent::Seventh)]==12.&&
        diminishedSeventh[0].contributions[static_cast<std::size_t>(RelativeColorComponent::Extensions)]==0.,"dim7 structure is not an added thirteenth");
    const auto pendingTime=color::analyzeRelativeColorCurve(phrase({"C"},{4.},true),cMajor);
    require(!pendingTime[0].value&&!pendingTime[0].durationQN,"unresolved OPEN timing leaves a curve gap");
    const auto noKey=color::analyzeRelativeColorCurve(source,std::nullopt);
    require(noKey.size()==2&&!noKey[0].value&&!noKey[1].value,
        "relative curve does not invent color without a key");
}
Progression symbolic(const Progression& source,bool omitFifth=false) {
    std::ostringstream xml;xml<<"<vst-xml version=\"1.4\"><sourceApp>Phase4DSynthetic</sourceApp>";
    constexpr const char* pitch[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    std::size_t id{};
    for(const auto& e:source){const auto c=chordPitches(e);auto mask=c.intervals;if(omitFifth)mask&=~(1u<<7);
        xml<<"<chord id=\""<<id++<<"\"><name>"<<e.name<<"</name><keyNote>"<<c.root<<"</keyNote><bassNote>"<<c.bass<<"</bassNote><pitches>";
        for(int n=0;n<12;++n)if(mask&(1u<<n))xml<<pitch[(c.root+n)%12]<<';';
        xml<<"</pitches><mask>0x"<<std::hex<<mask<<std::dec<<"</mask><projectTime domain=\"quarterNotes\">"<<e.startQN<<"</projectTime></chord>";
    }
    xml<<"</vst-xml>";auto parsed=plugin::VstXmlChordParser{}.parse(xml.str());require(parsed.ok(),"synthetic Cubase XML parse");
    if(!source.back().openEnded){parsed.chords.back().openEnded=false;parsed.chords.back().durationQN=source.back().durationQN;}
    return parsed.chords;
}
ImportedProgressionSession imported(const Progression& p){ImportedProgressionSession s;require(s.replace(p,TimelineCoordinateMode::RelativeToSelection),"session import");return s;}
void writeInput(const std::filesystem::path& path,const Progression& p,midi::Meter meter={4,4},int octaveShift=0,bool upperInversion=false) {
    // Explicitly emit every tone (including maj7 and 9); no four-voice reduction.
    midi::ExportSequence seq;seq.meter=meter;seq.chordCount=p.size();
    for(const auto& e:p){const auto c=chordPitches(e);const auto d=e.durationQN.value_or(4.);
        seq.notes.push_back({36+c.bass+octaveShift,e.startQN,d,90,0,true});
        for(int n=0;n<12;++n)if(c.intervals&(1u<<n))seq.notes.push_back({60+c.root+n+octaveShift+(upperInversion&&n==0?12:0),e.startQN,d,80,0,false});
        seq.totalQN=std::max(seq.totalQN,e.startQN+d);
    }
    auto bytes=midi::writeToMemory(seq);require(bool(bytes),"input SMF encoding");std::string error;require(midi::writeToFile(bytes.bytes,path,error),"input SMF save");
    const auto loaded=midi::importFile(path,p.back().openEnded);require(bool(loaded)&&loaded.extraction.chords.size()==p.size(),"musical SMF import roundtrip");
    for(std::size_t i=0;i<p.size();++i) {
        const auto a=normalizeChord(p[i]),b=normalizeChord(loaded.extraction.chords[i]);
        require(a.root==b.root && a.quality==b.quality,"MIDI chord core roundtrip");
        require(closeQN(p[i].startQN,loaded.extraction.chords[i].startQN),"MIDI input onset roundtrip");
    }
}
void compareMidi(const preview::Sequence& preview,const midi::ReadResult& file) {
    require(bool(file),"output SMF readback");
    require(!file.file.notes.empty(),"output notes present");
    for(const auto& note:file.file.notes){
        const auto event=std::find_if(preview.events.begin(),preview.events.end(),[&](const auto& e){return closeQN(e.startQN,note.startQN)&&closeQN(e.durationQN,note.durationQN);});
        if(event==preview.events.end()) {
            std::cerr<<"Unexpected MIDI note "<<note.pitch<<" start="<<note.startQN<<" duration="<<note.durationQN<<" expected:";
            for(const auto& e:preview.events)std::cerr<<' '<<e.startQN<<'@'<<e.durationQN;
            std::cerr<<'\n';
        }
        require(event!=preview.events.end(),"MIDI onset/duration must equal preview layout");
    }
    for(const auto& e:preview.events)require(std::any_of(file.file.notes.begin(),file.file.notes.end(),[&](const auto& n){return closeQN(n.startQN,e.startQN)&&closeQN(n.durationQN,e.durationQN);}),"each preview event exported");
}
plugin::WorkerResult wait(plugin::RecommendationWorker& worker) {
    for(int i=0;i<2000;++i){if(auto r=worker.takeLatest()){require(r->error.empty(),"worker error");return std::move(*r);}std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    throw std::runtime_error("worker timeout");
}
constexpr const char* phase5PitchNames[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
std::string phase5SimplifyLabel(const std::string& label) {
    ChordEvent event;event.name=label;const auto n=normalizeChord(event);
    if(!n.root)return label;
    std::string result=phase5PitchNames[static_cast<int>(*n.root)];
    switch(n.quality) {
        case ChordQuality::Minor:case ChordQuality::Minor7:result+="m";break;
        case ChordQuality::Diminished:result+="dim";break;
        case ChordQuality::Diminished7:result+="dim7";break;
        case ChordQuality::HalfDiminished7:result+="m7b5";break;
        case ChordQuality::Sus2:result+="sus2";break;
        case ChordQuality::Sus4:result+="sus4";break;
        case ChordQuality::Augmented:result+="aug";break;
        default:break; // Major7 and dominant7 simplify to major.
    }
    if(const auto slash=label.find('/');slash!=std::string::npos)result+=label.substr(slash);
    return result;
}
std::string phase5QueryKey(Mode mode,std::size_t prefix,bool simplified,const std::vector<std::string>& names,
                           const std::vector<double>& durations) {
    std::ostringstream key;key<<std::setprecision(9)<<int(mode)<<':'<<prefix<<':'<<simplified;
    for(std::size_t i=0;i<names.size();++i)key<<'|'<<names[i]<<'@'<<(i<durations.size()?durations[i]:4.);
    return key.str();
}
struct Phase5PrefixCase {
    std::string key;
    Mode mode{Mode::Major};
    std::size_t prefix{};
    bool simplified{};
    std::vector<std::string> names;
    std::vector<double> durations;
    std::vector<TemplateID> targetIds;
};
std::vector<Phase5PrefixCase> phase5PrefixCases(const std::vector<ProgressionTemplate>& templates) {
    std::map<std::string,Phase5PrefixCase> unique;
    constexpr std::array<std::size_t,3> lengths{2,3,5};
    for(const auto& item:templates)for(const auto length:lengths) {
        if(item.full.size()<=length)continue; // retain a real continuation tail
        const KeySignature key{item.mode==Mode::Major?PitchClass::C:PitchClass::A,item.mode};
        std::vector<std::string> exact,simple;std::vector<double> durations;
        bool valid=true;
        for(std::size_t i=0;i<length;++i) {
            const auto realized=realizeContinuation(item.full[i],key,item.full[i].durationQN.value_or(4.));
            if(realized.label.empty()||realized.label=="?"){valid=false;break;}
            exact.push_back(realized.label);simple.push_back(phase5SimplifyLabel(realized.label));
            durations.push_back(item.full[i].durationQN.value_or(4.));
        }
        if(!valid)continue;
        for(bool simplified:{false,true}) {
            const auto& names=simplified?simple:exact;
            const auto id=phase5QueryKey(item.mode,length,simplified,names,durations);
            auto it=unique.try_emplace(id,Phase5PrefixCase{id,item.mode,length,simplified,names,durations,{}}).first;
            if(std::find(it->second.targetIds.begin(),it->second.targetIds.end(),item.id)==it->second.targetIds.end())
                it->second.targetIds.push_back(item.id);
        }
    }
    std::vector<Phase5PrefixCase> result;result.reserve(unique.size());
    for(auto& [key,value]:unique){(void)key;result.push_back(std::move(value));}
    return result;
}
struct Phase5Lib {
    std::string name;
    std::vector<ProgressionTemplate> templates;
    std::unique_ptr<CandidateIndex> index;
};
struct Phase5TargetOutcome {
    const MatchResult* match{};
    bool eligible{},presented{},top3{},top10{},top20{},top50{};
    std::string group;
    std::size_t sourceCount{};
};
Phase5TargetOutcome phase5Outcome(const RecommendationSet& rec,const TemplateID& id,float minimumScore=60.f) {
    Phase5TargetOutcome out;
    const auto match=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& m){return m.templateId==id;});
    if(match!=rec.matches.end()) {
        out.match=&*match;
        const auto gateScore=rec.retrievalTimingMode==RetrievalTimingMode::HarmonyFirst?
            match->candidateEligibilityScore:match->candidateRankingScore;
        out.eligible=match->hasContinuation&&gateScore>=minimumScore;
    }
    const auto presented=session::presentationIndices(rec,{});
    for(std::size_t g=0;g<rec.groups.size();++g) {
        for(std::size_t rank=0;rank<rec.groups[g].size();++rank) {
            const auto& candidate=rec.groups[g][rank];
            const auto sources=std::count(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),id);
            out.sourceCount+=static_cast<std::size_t>(sources);
            if(sources){
                if(out.group.empty())out.group=g==0?"RESOLVE":g==1?"DEVELOP":g==2?"LOOP":"COLOR";
                if(rank<3)out.top3=true;if(rank<10)out.top10=true;if(rank<20)out.top20=true;if(rank<50)out.top50=true;
            }
        }
        for(const auto rank:presented[g])if(rank<rec.groups[g].size()&&
            std::find(rec.groups[g][rank].supportingTemplates.begin(),rec.groups[g][rank].supportingTemplates.end(),id)!=rec.groups[g][rank].supportingTemplates.end())out.presented=true;
    }
    return out;
}
std::string phase5Styles(StyleFlags flags){std::ostringstream s;s<<"0x"<<std::hex<<flags;return s.str();}
std::string phase5Disposition(const Phase5TargetOutcome& o){return o.match?o.match->disposition:"not_matched";}
std::string phase5Rank(const Phase5TargetOutcome& o){return o.group.empty()?"":o.group;}
std::string phase5ReasonString(const MatchResult& m) {
    MatchReasonFlags flags{};for(const auto& s:m.alignmentTrace)flags|=s.reasons;return reasonNames(flags);
}
struct Phase5SnapshotCheck {
    std::string detail;
};
Phase5SnapshotCheck phase5SnapshotCheck(const std::filesystem::path& out,const Progression& p,
        const ContinuationCandidate& candidate,const std::vector<MatchResult>& matches,midi::Meter meter) {
    Phase5SnapshotCheck check;
    const auto session=imported(p);const auto previewBefore=preview::buildSequence(session,&candidate,90);
    midi::CandidateExportContext ec;ec.imported=session;ec.meter=meter;ec.tempo=90;ec.mode=midi::ArrangementMode::BlockChords;
    const auto payload=midi::candidatePayload(ec,candidate);
    require(bool(previewBefore)&&bool(payload),"Phase5 abstract candidate preview and MIDI payload");
    compareMidi(previewBefore.sequence,midi::readFromMemory(payload.payload.smfBytes));
    compareMidi(previewBefore.sequence,midi::readFromMemory(payload.payload.dawClipBytes));
    std::string error;require(midi::writeToFile(payload.payload.smfBytes,out/"phase5-b11-preview-midi.mid",error),"Phase5 representative SMF write");
    const auto snapshot=snapshot::capture(session,candidate,matches,90,meter.numerator,meter.denominator);
    const auto decoded=snapshot::deserialize(snapshot::serialize(snapshot));require(bool(decoded),"Phase5 candidate snapshot roundtrip");
    const auto& restored=decoded.value.candidate;
    require(restored.continuation.size()==candidate.continuation.size(),"snapshot preserves abstract suffix length");
    for(std::size_t i=0;i<candidate.continuation.size();++i) {
        const auto& before=candidate.continuation[i];const auto& after=restored.continuation[i];
        require(before.label==after.label&&closeQN(before.durationQN,after.durationQN),"snapshot preserves abstract suffix label and QN");
        require(before.quality==after.quality&&bool(before.harmonicData)==bool(after.harmonicData),"snapshot preserves suffix harmonic-data presence");
        if(before.harmonicData) {
            require(before.harmonicData->name==after.harmonicData->name,"snapshot preserves suffix harmonic-data name");
            const auto a=normalizeChord(*before.harmonicData),b=normalizeChord(*after.harmonicData);
            require(a.root==b.root&&a.bass==b.bass&&a.quality==b.quality&&a.explicitBass==b.explicitBass,
                    "snapshot preserves suffix harmonic-data core and bass semantics");
        }
    }
    const auto previewAfter=preview::buildSequence(session,&restored,90);
    require(bool(previewAfter)&&previewAfter.sequence.events.size()==previewBefore.sequence.events.size(),"restored candidate preview rebuild");
    for(std::size_t i=0;i<previewBefore.sequence.events.size();++i) {
        const auto& a=previewBefore.sequence.events[i];const auto& b=previewAfter.sequence.events[i];
        require(a.chord.root==b.chord.root&&a.chord.intervals==b.chord.intervals&&
            closeQN(a.startQN,b.startQN)&&closeQN(a.durationQN,b.durationQN),"snapshot preview chord and timing equivalence");
    }
    compareMidi(previewAfter.sequence,midi::readFromMemory(payload.payload.smfBytes));
    check.detail="suffix labels/QN/harmonicData, preview and SMF preserved; experiment switch/reason intentionally diagnostic-only";
    return check;
}
MatchResult phase5SingleMatch(const std::string& queryName,const std::string& templateName,bool abstract) {
    AnalysisContext context;context.forcedKey=KeySignature{PitchClass::C,Mode::Major};
    context.timeSigNumerator=4;context.timeSigDenominator=4;
    const auto run=[&](std::vector<std::string> queryNames,std::vector<std::string> templateNames) {
        std::vector<double> durations(queryNames.size(),4.);
        const auto query=makeMatchQuery(phrase(std::move(queryNames),durations),context);
        const auto templQuery=makeMatchQuery(phrase(std::move(templateNames),durations),context);
        require(!templQuery.interpretations.empty()&&!templQuery.interpretations.front().full.empty(),"single-chord template query analysis");
        ProgressionTemplate templ;templ.id="PHASE5_SINGLE_"+templateName;templ.mode=Mode::Major;
        templ.full=templQuery.interpretations.front().full;
        for(std::size_t i=0;i<templ.full.size();++i)templ.skeletonIndices.push_back(i);
        templ.loopable=false;prepareTemplate(templ);CandidateIndex one({templ});MatchWeights weights;weights.abstractChordCompatibility=abstract;
        return matchProgression(query,one,weights,1);
    };
    auto matches=run({queryName},{templateName});
    if(matches.empty())matches=run({"Am",queryName,"F"},{"Am",templateName,"F"});
    require(!matches.empty(),"single-chord directional match exists");return matches.front();
}
void phase5CompatibilityExperiments(const std::filesystem::path& out) {
    std::ofstream file(out/"phase5-compatibility.tsv");
    file<<"direction\tquery\ttemplate\tOFF_rawCost\tON_rawCost\tOFF_similarity\tON_similarity\tabstractReason\tresult\treasonNames\n";
    struct Trial{const char* direction;const char* query;const char* templ;bool allow;};
    const std::array<Trial,13> trials{{
        {"query_subset_extension","C","Cmaj7",true},
        {"dominant_context_guard","C","C7",false},
        {"dominant_context_extension","G","G7",true},
        {"query_subset_extension","Cm","Cm7",true},
        {"query_subset_extension","Cmaj7","Cmaj9",true},
        {"root_mismatch","Db","Cmaj7",false},
        {"third_mismatch","C","Cm7",false},
        {"seventh_mismatch","Cmaj7","C7",false},
        {"diminished_augmented","Cdim","Caug",false},
        {"suspended_quality","Csus4","C",false},
        {"reverse_extension","Cmaj7","C",false},
        {"reverse_query_extension","Cmaj9","Cmaj7",false},
        {"explicit_bass_conflict","C/D","C/A",false}
    }};
    for(const auto& trial:trials) {
        const auto off=phase5SingleMatch(trial.query,trial.templ,false);
        const auto on=phase5SingleMatch(trial.query,trial.templ,true);
        const auto reason=std::any_of(on.alignmentTrace.begin(),on.alignmentTrace.end(),[](const auto& step){
            return hasReason(step.reasons,MatchReason::AbstractExtensionCompatibility);
        });
        if(trial.allow) {
            require(reason,"allowed directional extension carries AbstractExtensionCompatibility reason");
            require(on.rawCost+0.0001f<off.rawCost&&on.similarity>off.similarity,
                    "allowed directional extension lowers cost and improves similarity");
        } else {
            require(!reason,"disallowed identity change has no abstract-extension reason");
            require(std::abs(on.rawCost-off.rawCost)<0.0001f&&std::abs(on.similarity-off.similarity)<0.0001f,
                    "disallowed direction or identity gets no abstract discount");
        }
        file<<trial.direction<<'\t'<<trial.query<<'\t'<<trial.templ<<'\t'<<off.rawCost<<'\t'<<on.rawCost<<'\t'
            <<off.similarity<<'\t'<<on.similarity<<'\t'<<reason<<'\t'<<(trial.allow?"PASS_GAIN":"PASS_NO_GAIN")
            <<'\t'<<phase5ReasonString(on)<<'\n';
    }
}
std::string phase5Mode(Mode mode){return mode==Mode::Major?"Major":"Minor";}
struct Phase5Frequency {std::size_t attempts{},eligible{},defaultVisible{},top3{},top10{},top20{},top50{},support{};};
struct Phase5PerfResult {double p50{},p95{},maximum{},cpuMs{};std::size_t rssBefore{},rssAfter{},rssPeak{},shortlist{};};
template<class Evaluate>
Phase5PerfResult phase5Measure(Evaluate&& evaluate) {
    Phase5PerfResult result;std::vector<double> times;times.reserve(80);
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));result.rssBefore=memory.WorkingSetSize;
    FILETIME creation{},exit{},kernelBefore{},userBefore{},kernelAfter{},userAfter{};
    GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernelBefore,&userBefore);
#endif
    for(int i=0;i<90;++i){const auto start=std::chrono::steady_clock::now();result.shortlist=evaluate();
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(i>=10)times.push_back(ms);}
#if defined(_WIN32)
    GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernelAfter,&userAfter);
    const auto ticks=[](FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
    result.cpuMs=(ticks(kernelAfter)+ticks(userAfter)-ticks(kernelBefore)-ticks(userBefore))/10000.;
    GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));result.rssAfter=memory.WorkingSetSize;result.rssPeak=memory.PeakWorkingSetSize;
#endif
    std::sort(times.begin(),times.end());result.p50=times[39];result.p95=times[75];result.maximum=times.back();return result;
}
// Focused Phase 5.1 R02 factorial. The prior Phase 5 probe coupled a 3/5/4
// duration change with OPEN on Am; this mode varies those inputs independently
// while keeping V4, P3, Abstract, thresholds, and retrieval routes explicit.
int runPhase51R02(const std::filesystem::path& out,const std::filesystem::path& factoryPath) {
    std::filesystem::create_directories(out);
    const auto loaded=library::loadFactory(factoryPath);
    require(bool(loaded)&&loaded.templates.size()==657,"Phase51 R02 reads the 657-entry V4 Factory");
    CandidateIndex index(loaded.templates);
    constexpr std::array<TemplateID,5> targets{
        "V3_SHADE_002","V3_SHADE_008","V3_SHADE_019","V3_SHADE_024","V3_SHADE_031"};
    for(const auto& id:targets)require(std::count_if(loaded.templates.begin(),loaded.templates.end(),
        [&](const auto& item){return item.id==id&&item.mode==Mode::Major;})==1,
        "each R02 Color target is a unique Major-mode V4 template");
    struct KeySetting { const char* name; std::optional<KeySignature> forced; };
    const std::array<KeySetting,3> keySettings{{
        {"Auto",std::nullopt},
        {"Cmajor",KeySignature{PitchClass::C,Mode::Major}},
        {"Aminor",KeySignature{PitchClass::A,Mode::Minor}}}};
    const std::array<std::vector<double>,2> durationPatterns{{{4.,4.,4.},{3.,5.,4.}}};
    const std::array<const char*,4> groupNames{"Resolve","Develop","Loop","Color"};
    std::ofstream cases(out/"r02-cases.tsv"),targetRows(out/"r02-targets.tsv"),trace(out/"r02-trace.txt");
    require(bool(cases)&&bool(targetRows)&&bool(trace),"Phase51 R02 diagnostic outputs open");
    cases<<"case\tkeySetting\tdurationsQN\tAmOPEN\tHarmonyFirst\tP3\tAbstract\tselectedAnalysisKey\tqueryHypotheses\tshortlist\taligned\tsimilarityPassed\ttailPassed\tpathPassed\tscorePassed\tworkerOutput\tColorWorkerCount\tColorWorkerCards\tColorTargetAboveActiveGate\tColorTargetIDs\n";
    targetRows<<"case\tkeySetting\tdurationsQN\tAmOPEN\tHarmonyFirst\tselectedAnalysisKey\ttemplateID\ttemplateMode\tqueryModes\tmatchFound\tmatchKey\tsimilarity\trawCost\trankingScore\teligibilityScore\tactiveGateScore\trankingPass60\teligibilityPass60\tactiveGatePass\texitStage\tminimumMatch\tminimumScore\tcontinuationStart\thasContinuation\tsuffixStageReached\tvalidCompleteSuffix\tsuffix\tgroup\tworkerRank\tworkerTop10\tdefaultCard\n";
    cases<<std::setprecision(9);targetRows<<std::setprecision(9);trace<<std::setprecision(17);
    std::size_t caseNumber{},targetRowCount{};
    for(const auto& keySetting:keySettings)for(std::size_t durationIndex=0;durationIndex<durationPatterns.size();++durationIndex)
    for(const bool open:{false,true})for(const bool hf:{false,true}) {
        ++caseNumber;
        const auto& durations=durationPatterns[durationIndex];
        auto progression=phrase({"F","G","Am"},durations,open);
        AnalysisContext context;context.timeSigNumerator=4;context.timeSigDenominator=4;
        context.forcedKey=keySetting.forced;
        context.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        auto weights=plugin::developmentRecommendationWeights();
        weights.diminishedPassingInsertionException=false;weights.abstractChordCompatibility=false;
        require(weights.minimumMatch==0.60f&&weights.minimumScore==60.f&&weights.perGroup==10,
            "R02 preserves the existing match threshold, score threshold and ten-item worker cap");
        const auto query=makeMatchQuery(progression,context);
        require(query.interpretations.size()==(keySetting.forced?1u:24u),"R02 records actual forced or automatic key route");
        const auto rec=recommendContinuations(query,index,{},weights);
        require(rec.retrievalTimingMode==context.retrievalTimingMode&&!rec.phase3aEnabled&&!rec.abstractChordCompatibility,
            "R02 invocation keeps requested HF route and P3/Abstract OFF");
        const auto analysis=analyzeHarmony(progression,context);
        const auto shown=session::presentationIndices(rec,{});
        const auto caseId=std::string(keySetting.name)+"|"+(durationIndex==0?"4-4-4":"3-5-4")+
            "|OPEN"+(open?"1":"0")+"|HF"+(hf?"1":"0");
        const auto durationLabel=durationIndex==0?"4,4,4":"3,5,4";
        std::string colorTargetIds;std::size_t colorTargetMatches{},colorTargetAboveGate{};
        for(const auto& id:targets) {
            const auto match=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& item){return item.templateId==id;});
            if(match!=rec.matches.end()&&(hf?match->candidateEligibilityScore:match->candidateRankingScore)>=weights.minimumScore)++colorTargetAboveGate;
            if(std::any_of(rec.groups[3].begin(),rec.groups[3].end(),[&](const auto& candidate) {
                return std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),id)!=candidate.supportingTemplates.end();
            })) {
                ++colorTargetMatches;
                if(!colorTargetIds.empty())colorTargetIds+=',';
                colorTargetIds+=id;
            }
        }
        const auto selectedKey=analysis.selectedKey?formatKey(analysis.selectedKey->key):"UNKNOWN";
        cases<<caseId<<'\t'<<keySetting.name<<'\t'<<durationLabel<<'\t'<<open<<'\t'<<hf<<"\t0\t0\t"
            <<selectedKey<<'\t'<<query.interpretations.size()<<'\t'<<rec.stats.shortlistSize<<'\t'<<rec.stats.aligned<<'\t'
            <<rec.stats.similarityPassed<<'\t'<<rec.stats.tailPassed<<'\t'<<rec.stats.pathPassed<<'\t'<<rec.stats.scorePassed<<'\t'
            <<rec.stats.output<<'\t'<<rec.groups[3].size()<<'\t'<<colorTargetMatches<<'\t'<<colorTargetAboveGate<<'\t'<<colorTargetIds<<'\n';

        trace<<"\nR02_CASE number="<<caseNumber<<" id="<<caseId<<" library=V4_657 input=phrase({F,G,Am},durations,AmOPEN)"
            <<" durationsQN="<<durationLabel<<" AmOPEN="<<open<<" HarmonyFirst="<<hf
            <<" P3=0 Abstract=0 minimumMatch="<<weights.minimumMatch<<" minimumScore="<<weights.minimumScore
            <<" perGroup="<<weights.perGroup<<" selectedAnalysisKey="<<selectedKey
            <<" ColorWorkerCount="<<rec.groups[3].size()<<" ColorWorkerCards="<<colorTargetMatches
            <<" ColorTargetAboveActiveGate="<<colorTargetAboveGate<<" ColorTargetIDs="<<colorTargetIds
            <<session::recommendationDiagnostics(progression,context,{},rec);

        for(const auto& id:targets) {
            ++targetRowCount;
            const auto itemIt=std::find_if(loaded.templates.begin(),loaded.templates.end(),[&](const auto& item){return item.id==id;});
            require(itemIt!=loaded.templates.end(),"R02 Color template remains present in V4 Factory");
            const auto matchIt=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& match){return match.templateId==id;});
            const MatchResult* match=matchIt==rec.matches.end()?nullptr:&*matchIt;
            const bool modeCompatible=std::any_of(query.interpretations.begin(),query.interpretations.end(),[&](const auto& hypothesis) {
                return hypothesis.key.key.mode==itemIt->mode;
            });
            const ContinuationCandidate* card{};std::size_t group{},rank{};bool defaultCard{};
            for(std::size_t g=0;g<rec.groups.size()&&!card;++g)for(std::size_t r=0;r<rec.groups[g].size();++r) {
                const auto& candidate=rec.groups[g][r];
                if(std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),id)==candidate.supportingTemplates.end())continue;
                card=&candidate;group=g;rank=r;defaultCard=std::find(shown[g].begin(),shown[g].end(),r)!=shown[g].end();break;
            }
            const std::string disposition=match?match->disposition:std::string{};
            bool suffixStageReached=card!=nullptr,validCompleteSuffix=card&&!card->continuation.empty();
            std::string suffix;
            if(card) {
                for(const auto& event:card->continuation)suffix+=event.label+"@"+std::to_string(event.durationQN)+"QN -> ";
            } else if(match) {
                suffixStageReached=disposition=="minimum_score"||disposition=="repeated_current_chord"||
                    disposition=="melody_constraint"||disposition=="candidate_generated"||
                    disposition=="equivalent_path_deduplication"||disposition=="diversity_score"||
                    disposition=="retained_limit"||disposition=="TARGET_RETAINED";
                if(suffixStageReached) {
                    const auto rhythm=estimateRhythmScale(*match,query,*itemIt);
                    std::vector<ConcreteChordEvent> realized;
                    for(std::size_t i=match->continuationStartIndex;i<itemIt->full.size();++i)
                        realized.push_back(realizeContinuation(itemIt->full[i],match->key,
                            itemIt->full[i].durationQN.value_or(4.)*rhythm.scale));
                    if(itemIt->loopable) {
                        const auto wrapEnd=match->templateMatchStart==0
                            ?(match->hasContinuation?0:itemIt->full.size()):match->templateMatchStart;
                        for(std::size_t i=0;i<std::min(wrapEnd,itemIt->full.size());++i)
                            realized.push_back(realizeContinuation(itemIt->full[i],match->key,
                                itemIt->full[i].durationQN.value_or(4.)*rhythm.scale));
                    }
                    validCompleteSuffix=!realized.empty()&&disposition!="repeated_current_chord";
                    for(const auto& event:realized)suffix+=event.label+"@"+std::to_string(event.durationQN)+"QN -> ";
                }
            }
            std::string exitStage;
            if(!match)exitStage=modeCompatible?"no_match_result_after_compatible_mode":"mode_filter_before_alignment";
            else exitStage=disposition;
            std::string queryModes;
            for(const auto mode:{Mode::Major,Mode::Minor})if(std::any_of(query.interpretations.begin(),query.interpretations.end(),[&](const auto& hypothesis) {
                return hypothesis.key.key.mode==mode;
            })) {
                if(!queryModes.empty())queryModes+=',';
                queryModes+=phase5Mode(mode);
            }
            targetRows<<caseId<<'\t'<<keySetting.name<<'\t'<<durationLabel<<'\t'<<open<<'\t'<<hf<<'\t'<<selectedKey<<'\t'
                <<id<<'\t'<<phase5Mode(itemIt->mode)<<'\t'<<queryModes<<'\t'<<bool(match)<<'\t'
                <<(match?formatKey(match->key):"NA")<<'\t'<<(match?match->similarity:-1.f)<<'\t'<<(match?match->rawCost:-1.f)<<'\t'
                <<(match?match->candidateRankingScore:-1.f)<<'\t'<<(match?match->candidateEligibilityScore:-1.f)<<'\t'
                <<(match?(hf?match->candidateEligibilityScore:match->candidateRankingScore):-1.f)<<'\t'
                <<(match&&match->candidateRankingScore>=weights.minimumScore)<<'\t'
                <<(match&&match->candidateEligibilityScore>=weights.minimumScore)<<'\t'
                <<(match&&(hf?match->candidateEligibilityScore:match->candidateRankingScore)>=weights.minimumScore)<<'\t'
                <<exitStage<<'\t'<<weights.minimumMatch<<'\t'<<weights.minimumScore<<'\t'
                <<(match?match->continuationStartIndex:0)<<'\t'<<(match?match->hasContinuation:false)<<'\t'<<suffixStageReached<<'\t'
                <<validCompleteSuffix<<'\t'<<suffix<<'\t'<<(card?groupNames[group]:"NA")<<'\t'<<(card?rank+1:0)<<'\t'
                <<(card&&rank<10)<<'\t'<<(card&&defaultCard)<<'\n';
            trace<<"R02_FIXED_TARGET id="<<id<<" templateMode="<<phase5Mode(itemIt->mode)<<" queryModes="<<queryModes
                <<" matched="<<bool(match)<<" exitStage="<<exitStage;
            if(match)trace<<" matchKey="<<formatKey(match->key)<<" similarity="<<match->similarity<<" rawCost="<<match->rawCost
                <<" rankingScore="<<match->candidateRankingScore<<" eligibilityScore="<<match->candidateEligibilityScore
                <<" activeGateScore="<<(hf?match->candidateEligibilityScore:match->candidateRankingScore)
                <<" continuationStartIndex="<<match->continuationStartIndex<<" hasContinuation="<<match->hasContinuation;
            trace<<" suffixStageReached="<<suffixStageReached<<" validCompleteSuffix="<<validCompleteSuffix
                <<" group="<<(card?groupNames[group]:"NA")<<" workerRank="<<(card?rank+1:0)
                <<" workerTop10="<<(card&&rank<10)<<" defaultCard="<<(card&&defaultCard)<<" suffix=\""<<suffix<<"\"\n";
            if(match) {
                auto matchContext=context;matchContext.forcedKey=match->key;
                const auto interpreted=analyzeHarmony(progression,matchContext);
                trace<<"  MATCH_KEY_INTERPRETATION key="<<formatKey(match->key)<<'\n';
                for(std::size_t i=0;i<interpreted.full.size();++i) {
                    const auto& event=interpreted.full[i];
                    trace<<"  INTERPRETED_EVENT index="<<i<<" chord="<<progression[i].name
                        <<" identity="<<formatDegree(event)<<" function="<<formatFunction(event.function)
                        <<" structuralWeight="<<event.structuralWeight
                        <<" retrievalStructuralWeight="<<event.retrievalStructuralWeight
                        <<" skeleton="<<(std::find(interpreted.skeletonIndices.begin(),interpreted.skeletonIndices.end(),i)!=interpreted.skeletonIndices.end())<<'\n';
                }
            } else if(!modeCompatible) {
                trace<<"  TARGET_EXIT guard=matchProgression_mode_filter templateMode="<<phase5Mode(itemIt->mode)
                    <<" noQueryInterpretationOfSameMode\n";
            } else trace<<"  TARGET_EXIT stage=no_match_result_after_compatible_mode; inspect trace MATCH/ALIGN and stage counts\n";
        }
    }
    require(caseNumber==24&&targetRowCount==120,"Phase51 R02 emits all 24 factorial cells and 120 fixed-target rows");
    trace<<"\nEND R02 cells="<<caseNumber<<" targets="<<targetRowCount<<" uniqueFactorialCells=24\n";
    std::cout<<"Phase51 R02 diagnostics PASS cells="<<caseNumber<<" targets="<<targetRowCount
        <<" Factory=V4_657 P3=OFF Abstract=OFF\n";
    return 0;
}
int runPhase51Music(const std::filesystem::path& out,const std::filesystem::path& candidatePath) {
    const auto blind=out/"blind";const auto r01=out/"r01";
    std::filesystem::create_directories(blind);std::filesystem::create_directories(r01);
    const auto loaded=library::loadFactory(candidatePath);
    require(bool(loaded)&&loaded.templates.size()==658,"Phase51 R01 reads the isolated 658-entry candidate Factory");
    const auto loadedV4=library::loadFactory(HC_FACTORY_DB_PATH);
    require(bool(loadedV4)&&loadedV4.templates.size()==657,"Phase51 Abstract comparison uses the frozen 657-entry V4 Factory");
    CandidateIndex abstractIndex(loadedV4.templates),candidateIndex(loaded.templates);
    const TemplateID candidateId="CANDIDATE_V5_CHROMATIC_MINOR_001";
    const auto candidateTemplate=std::find_if(loaded.templates.begin(),loaded.templates.end(),[&](const auto& item){return item.id==candidateId;});
    require(candidateTemplate!=loaded.templates.end()&&candidateTemplate->full.size()==8,
            "R01 V5 candidate keeps its eight-event typed template");
    const KeySignature aMinor{PitchClass::A,Mode::Minor};
    require(candidateTemplate->mode==Mode::Minor&&candidateTemplate->intent==PhraseIntent::Develop,
            "R01 candidate mode and intent remain Minor/Develop");

    std::ofstream identity(r01/"candidate-typed-events.tsv"),r01Top(r01/"candidate-worker-top3.tsv"),
        lowest(r01/"preview-lowest-notes.tsv"),comparison(out/"abstract-comparison.tsv"),
        rankings(out/"abstract-worker-top10.tsv"),levels(out/"blind-audio-levels.tsv"),midiRows(out/"blind-midi.tsv"),
        answer(out/"answer-key.tsv"),instructions(blind/"listening-instructions.txt");
    require(bool(identity)&&bool(r01Top)&&bool(lowest)&&bool(comparison)&&bool(rankings)&&bool(levels)&&bool(midiRows)&&bool(answer)&&bool(instructions),
            "Phase51 music review output files open");
    identity<<"templateID\tindex\tlabel\tdegree\talteration\tquality\tfunction\trolesHex\tpassingRole\tbassInterval\tbassMeaning\tdurationQN\n";
    r01Top<<"group\tworkerRank\tdefaultPresented\tprimaryID\tsupportingIDs\tintent\tstylesHex\tscore\tsimilarity\tcontinuation\tisR01Candidate\n";
    lowest<<"segment\tindex\tqn\tchord\tpreviewLowestMidi\tpreviewLowestName\tmidiLowestMidi\tmidiLowestName\tmatch\n";
    comparison<<"pair\tquery\tabstract\tmatchedTemplateID\tmatchDisposition\treasonNames\trawCost\tsimilarity\tselectedCardPrimaryID\tsupportingIDs\tgroup\tworkerRank\tdefaultPresented\tintent\tstylesHex\tscore\tinputPath\tcontinuationPath\tfullPath\tfullMidi\tsuffixMidi\n";
    rankings<<"pair\tquery\tabstract\tgroup\tworkerRank\tdefaultPresented\tprimaryID\tsupportingIDs\tintent\tstylesHex\tscore\tsimilarity\tcontinuation\n";
    levels<<"pair\talias\tscope\tabstract\tselectedCardID\tcommonTargetRMS\trawRMS\trawPeak\tlinearGain\tpostGainPeak\tnoClipping\tsampleRate\tframes\twavPath\n";
    midiRows<<"pair\talias\tscope\tabstract\tselectedCardID\tarrangement\tupperVelocity\tbassVelocity\tchannel\tnoteCount\ttempoBPM\tinputOrSuffixQN\tmidiPath\n";
    answer<<"pair\talias\tquery\tabstract\tmatchedTemplateID\tselectedCardPrimaryID\tselectedPath\tscope\twavPath\tmidiPath\n";
    identity<<std::setprecision(10);comparison<<std::setprecision(10);rankings<<std::setprecision(10);
    levels<<std::setprecision(12);midiRows<<std::setprecision(10);r01Top<<std::setprecision(10);lowest<<std::setprecision(10);
    const auto styleText=[](StyleFlags flags){std::ostringstream s;s<<"0x"<<std::hex<<flags;return s.str();};
    const auto idText=[](const std::vector<TemplateID>& ids){std::string out;for(const auto& id:ids){if(!out.empty())out+=',';out+=id;}return out;};
    const auto chordList=[](const std::vector<ConcreteChordEvent>& events){std::string out;for(const auto& e:events){if(!out.empty())out+=" -> ";out+=e.label+"@"+std::to_string(e.durationQN)+"QN";}return out;};
    const auto sourceList=[](const Progression& p){std::string out;for(const auto& e:p){if(!out.empty())out+=" -> ";out+=e.name+"@"+std::to_string(e.durationQN.value_or(4.))+"QN";}return out;};
    const auto pitchName=[](int note){constexpr const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return std::string(names[(note%12+12)%12])+std::to_string(note/12-1);};
    const auto qualityName=[](ChordQuality q){switch(q){case ChordQuality::Major:return "Major";case ChordQuality::Minor:return "Minor";
        case ChordQuality::Diminished:return "Diminished";case ChordQuality::Augmented:return "Augmented";case ChordQuality::Dominant7:return "Dominant7";
        case ChordQuality::Major7:return "Major7";case ChordQuality::Minor7:return "Minor7";case ChordQuality::Diminished7:return "Diminished7";
        case ChordQuality::HalfDiminished7:return "HalfDiminished7";case ChordQuality::Sus2:return "Sus2";case ChordQuality::Sus4:return "Sus4";
        default:return "Unknown";}};
    struct Selected {const ContinuationCandidate* candidate{};std::size_t group{},rank{};bool presented{};};
    const auto selectCard=[](const RecommendationSet& rec,const TemplateID& id) {
        Selected result;const auto shown=session::presentationIndices(rec,{});
        for(int pass=0;pass<2&&!result.candidate;++pass)for(std::size_t g=0;g<rec.groups.size()&&!result.candidate;++g)
            for(std::size_t rank=0;rank<rec.groups[g].size();++rank) {
                const auto& c=rec.groups[g][rank];
                const bool hit=pass==0?c.primaryTemplate==id:
                    std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),id)!=c.supportingTemplates.end();
                if(!hit)continue;result={&c,g,rank,std::find(shown[g].begin(),shown[g].end(),rank)!=shown[g].end()};break;
            }
        return result;
    };
    constexpr const char* groupNames[]{"RESOLVE","DEVELOP","LOOP","COLOR"};
    const auto checkMidiVoicing=[&](const preview::Sequence& whole,const midi::ReadResult& decoded,bool suffixOnly) {
        require(bool(decoded),"decoded SMF is available for full voicing validation");
        require(!whole.events.empty()&&(!suffixOnly||whole.recommendationBoundary<whole.events.size()),
                "full preview has the boundary needed for voicing validation");
        preview::ChordVoicer voicer;const double firstSuffix=whole.events[whole.recommendationBoundary].startQN;
        for(std::size_t i=0;i<whole.events.size();++i) {
            const auto& event=whole.events[i];const auto voiced=voicer.voice(event.chord,event.topVoice);
            if(suffixOnly&&i<whole.recommendationBoundary)continue;
            const double start=event.startQN-(suffixOnly?firstSuffix:0.);
            std::vector<int> expected{voiced.bass};
            for(int v=0;v<voiced.upperCount;++v)expected.push_back(voiced.upper[v]);
            std::vector<int> actual;
            for(const auto& n:decoded.file.notes)if(closeQN(n.startQN,start)&&closeQN(n.durationQN,event.durationQN))actual.push_back(n.pitch);
            std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());
            require(actual==expected,"decoded SMF bass and upper pitch multiset equals the preview voicing at each event");
        }
    };
    struct BlindVersion {
        std::string pair,alias,query,abstractLabel,targetId,primaryId,group,path;
        std::vector<ConcreteChordEvent> continuation;
        preview::Sequence full,suffix;
        preview::AudioBuffer fullAudio,suffixAudio;
        std::vector<std::uint8_t> fullMidi,suffixMidi;
        std::size_t fullNotes{},suffixNotes{};
    };
    std::vector<BlindVersion> blindVersions;std::ostringstream findings;
    findings<<"# Phase 5.1 music findings\n\n"
        <<"The Abstract OFF/ON cases below call the same core `recommendContinuations` path and development weights directly, with `perGroup=10`; `session::presentationIndices(rec,{})` supplies the default-card flag. They do not constitute a new Cubase, installed-plugin, or threaded-worker acceptance run. Existing dev.6 package/SDK gates are separate evidence. The original progression objects are passed through the read-only preview builder; the continuation engine does not rewrite the input.\n\n"
        <<"Full WAV files use a fresh built-in rule-based `PreviewSynth` render at 48 kHz and 120 BPM; each suffix WAV is an exact boundary excerpt from that same full render, preserving the whole-sequence voice-leading context and natural release tail. This is not a recording and does not consume MIDI velocity. MIDI export uses the same preview voicings, VoiceLed full-phrase/continuation-only scopes, and fixed upper/bass velocities 80/90. The SMF has no Program Change; the instructions require one identical GM host patch and host level for every file.\n\n"
        <<"Each WAV pair/scope uses one common target RMS equal to the quieter raw member; only a single linear clip gain (never above 1.0) is applied. Raw RMS/peak and post-gain peak are recorded. No compressor, limiter, or voicing change is applied.\n\n"
        <<"## Abstract complete-path comparison\n\n";

    for(std::size_t i=0;i<candidateTemplate->full.size();++i) {
        const auto& e=candidateTemplate->full[i];const auto realized=realizeContinuation(e,aMinor,e.durationQN.value_or(4.));
        identity<<candidateId<<'\t'<<i<<'\t'<<realized.label<<'\t'
            <<(e.degree?std::to_string(e.degree->degree):"unknown")<<'\t'<<(e.degree?std::to_string(e.degree->alteration):"unknown")<<'\t'
            <<qualityName(e.quality)<<'\t'<<formatFunction(e.function)<<'\t'<<"0x"<<std::hex<<e.roles<<std::dec<<'\t'
            <<hasRole(e.roles,Role::Passing)<<'\t'
            <<(e.bassInterval?std::to_string(*e.bassInterval):"null")<<'\t'
            <<(e.bassInterval?"explicit root-relative semitones":"unspecified; unknown bass, not asserted root-position")<<'\t'
            <<e.durationQN.value_or(4.)<<'\n';
    }
    findings<<"R01 template identity is checked against the isolated database. The machine-readable event table is `r01/candidate-typed-events.tsv`; it records realized labels, degree, quality, function, roles, and optional bass interval. A null interval remains unknown and is never folded to root bass.\n\n"
        <<"`MatchEvent::bassInterval` is an optional root-relative semitone field: D/F# is encoded as 4; null on the other events means unspecified bass, not an asserted root bass. The candidate's G#dim event is checked from its typed degree/quality/role columns above; audio/MIDI realization is a rule-based preview.\n\n";

    struct AbstractCase {std::string pair,name,offId,onId,offAlias,onAlias;Progression input;};
    std::vector<AbstractCase> cases;
    cases.push_back({"Q1","C@4 -> E7@4 -> Am@4","V3_HOME_011","V4_B14","B","A",phrase({"C","E7","Am"},{4,4,4})});
    cases.push_back({"Q2","C@4 -> Am@2 -> A7@1 -> Dm@2 -> G@2","V3_HOME_018","V4_B11","A","B",phrase({"C","Am","A7","Dm","G"},{4,2,1,2,2})});
    for(const auto& test:cases) {
        findings<<"### "<<test.pair<<": "<<test.name<<"\n\n| Abstract | Matched ID | Default card | Group / rank | Full input + continuation | Match reason |\n|---|---|---:|---:|---|---|\n";
        AnalysisContext context;context.forcedKey=KeySignature{PitchClass::C,Mode::Major};context.timeSigNumerator=4;context.timeSigDenominator=4;
        const auto query=makeMatchQuery(test.input,context);require(!query.interpretations.empty(),"Abstract comparison query has a C-major interpretation");
        for(const bool abstract:{false,true}) {
            auto weights=plugin::developmentRecommendationWeights();weights.diminishedPassingInsertionException=false;
            weights.abstractChordCompatibility=abstract;
            require(weights.perGroup==10,"Abstract listening uses the production ten-card worker cap");
            const auto rec=recommendContinuations(query,abstractIndex,{},weights);
            require(rec.abstractChordCompatibility==abstract&&!rec.phase3aEnabled&&rec.retrievalTimingMode==RetrievalTimingMode::Legacy,
                    "Abstract listening records OFF/ON, P3 OFF, Legacy core route");
            const auto target=abstract?test.onId:test.offId;const auto selected=selectCard(rec,target);
            require(selected.candidate!=nullptr,"each requested V3/V4 Abstract candidate is in the ten-card worker output");
            const auto match=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& m){return m.templateId==target;});
            require(match!=rec.matches.end(),"each requested Abstract card has a matched template trace");
            const auto suffix=chordList(selected.candidate->continuation);const auto fullPath=sourceList(test.input)+" -> "+suffix;
            std::string group=groupNames[selected.group];
            const auto shown=session::presentationIndices(rec,{});
            for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t r=0;r<rec.groups[g].size()&&r<10;++r) {
                const auto& c=rec.groups[g][r];const bool visible=std::find(shown[g].begin(),shown[g].end(),r)!=shown[g].end();
                rankings<<test.pair<<'\t'<<test.name<<'\t'<<abstract<<'\t'<<groupNames[g]<<'\t'<<r+1<<'\t'<<visible<<'\t'
                    <<c.primaryTemplate<<'\t'<<idText(c.supportingTemplates)<<'\t'<<intentName(c.intent)<<'\t'<<styleText(c.styles)<<'\t'
                    <<c.rankingScore<<'\t'<<c.matchSimilarity<<'\t'<<chordList(c.continuation)<<'\n';
            }
            const auto session=imported(test.input);const auto built=preview::buildSequence(session,selected.candidate,120.);
            require(bool(built)&&built.sequence.tempoBPM==120.&&built.sequence.recommendationBoundary==test.input.size(),
                    "Abstract full phrase preview preserves input boundary at 120 BPM");
            preview::Sequence suffixSequence;suffixSequence.tempoBPM=built.sequence.tempoBPM;suffixSequence.sourceRecommendation=built.sequence.sourceRecommendation;
            suffixSequence.recommendationBoundary=0;const auto suffixStart=built.sequence.events[built.sequence.recommendationBoundary].startQN;
            for(std::size_t event=built.sequence.recommendationBoundary;event<built.sequence.events.size();++event) {
                auto e=built.sequence.events[event];e.startQN-=suffixStart;e.segment=preview::Segment::Recommended;suffixSequence.events.push_back(std::move(e));
            }
            suffixSequence.totalQN=built.sequence.totalQN-suffixStart;
            require(!suffixSequence.events.empty(),"suffix-only preview contains the complete returned continuation");
            auto fullClip=midi::buildClip(built.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4},selected.candidate->key,selected.candidate->intent);
            auto suffixClip=midi::buildClip(built.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::ContinuationOnly,{4,4},selected.candidate->key,selected.candidate->intent);
            require(bool(fullClip)&&bool(suffixClip),"full and suffix-only MIDI exports build from the preview sequence");
            const auto fullBytes=midi::writeToMemory(fullClip.sequence),suffixBytes=midi::writeToMemory(suffixClip.sequence);
            require(bool(fullBytes)&&bool(suffixBytes),"full and suffix-only MIDI serialize");
            const auto fullDecoded=midi::readFromMemory(fullBytes.bytes),suffixDecoded=midi::readFromMemory(suffixBytes.bytes);
            compareMidi(built.sequence,fullDecoded);compareMidi(suffixSequence,suffixDecoded);
            checkMidiVoicing(built.sequence,fullDecoded,false);checkMidiVoicing(built.sequence,suffixDecoded,true);
            const auto abstractAlias=abstract?test.onAlias:test.offAlias;
            BlindVersion version;version.pair=test.pair;version.alias=abstractAlias;version.query=test.name;version.abstractLabel=abstract?"ON":"OFF";
            version.targetId=target;version.primaryId=selected.candidate->primaryTemplate;version.group=group;
            version.continuation=selected.candidate->continuation;
            version.path=fullPath;version.full=built.sequence;version.suffix=suffixSequence;version.fullMidi=fullBytes.bytes;version.suffixMidi=suffixBytes.bytes;
            version.fullNotes=fullClip.sequence.notes.size();version.suffixNotes=suffixClip.sequence.notes.size();
            version.fullAudio=preview::renderOffline(version.full,48000.);
            require(!version.fullAudio.left.empty()&&version.fullAudio.rms>0,
                    "offline PreviewSynth renders the complete input and continuation");
            const auto suffixFrame=preview::qnToSamples(suffixStart,version.full.tempoBPM,version.fullAudio.sampleRate);
            require(suffixFrame>=0&&static_cast<std::size_t>(suffixFrame)<version.fullAudio.left.size(),
                    "recommendation boundary maps to a sample inside the full WAV");
            version.suffixAudio.sampleRate=version.fullAudio.sampleRate;
            version.suffixAudio.left.assign(version.fullAudio.left.begin()+suffixFrame,version.fullAudio.left.end());
            version.suffixAudio.right.assign(version.fullAudio.right.begin()+suffixFrame,version.fullAudio.right.end());
            long double suffixSquares{};bool suffixFinite=true;
            for(const auto sample:version.suffixAudio.left) {
                suffixFinite=suffixFinite&&std::isfinite(sample);
                version.suffixAudio.peak=std::max(version.suffixAudio.peak,static_cast<double>(std::abs(sample)));
                suffixSquares+=static_cast<long double>(sample)*sample;
            }
            require(suffixFinite,"suffix WAV excerpt samples are finite");
            version.suffixAudio.rms=std::sqrt(static_cast<double>(suffixSquares/version.suffixAudio.left.size()));
            require(!version.suffixAudio.left.empty()&&version.suffixAudio.rms>0,
                    "suffix WAV is a non-empty excerpt from the full render");
            require(version.fullAudio.left==version.fullAudio.right&&version.suffixAudio.left==version.suffixAudio.right,
                    "full-render stereo channels match and suffix excerpt preserves both channels");
            blindVersions.push_back(std::move(version));
            const auto& v=blindVersions.back();
            comparison<<test.pair<<'\t'<<test.name<<'\t'<<abstract<<'\t'<<target<<'\t'<<match->disposition<<'\t'
                <<phase5ReasonString(*match)<<'\t'<<match->rawCost<<'\t'<<match->similarity<<'\t'<<v.primaryId<<'\t'
                <<idText(selected.candidate->supportingTemplates)<<'\t'<<group<<'\t'<<selected.rank+1<<'\t'<<selected.presented<<'\t'
                <<intentName(selected.candidate->intent)<<'\t'<<styleText(selected.candidate->styles)<<'\t'<<selected.candidate->rankingScore<<'\t'
                <<sourceList(test.input)<<'\t'<<suffix<<'\t'<<fullPath<<'\t'<<"blind/"<<test.pair<<'-'<<abstractAlias<<"-full.mid\t"
                <<"blind/"<<test.pair<<'-'<<abstractAlias<<"-suffix.mid\n";
            findings<<"| "<<test.pair<<" "<<(abstract?"ON":"OFF")<<" | `"<<target<<"` | "<<selected.presented
                <<" | "<<group<<" / "<<selected.rank+1<<" | `"<<fullPath
                <<"` | "<<phase5ReasonString(*match)<<" (`"<<match->disposition<<"`) |\n";
        }
    }

    midi::ExportConfig exportConfig;exportConfig.upperVelocity=80;exportConfig.bassVelocity=90;exportConfig.channel=0;
    for(auto& v:blindVersions) {
        const auto fullPath=blind/(v.pair+"-"+v.alias+"-full.mid"),suffixPath=blind/(v.pair+"-"+v.alias+"-suffix.mid");std::string error;
        require(midi::writeToFile(v.fullMidi,fullPath,error),"blind full-phrase MIDI file writes");error.clear();
        require(midi::writeToFile(v.suffixMidi,suffixPath,error),"blind suffix-only MIDI file writes");
        midiRows<<v.pair<<'\t'<<v.alias<<"\tinput+suffix\t"<<v.abstractLabel<<'\t'<<v.primaryId<<"\tVoiceLed\t"
            <<int(exportConfig.upperVelocity)<<'\t'<<int(exportConfig.bassVelocity)<<'\t'<<int(exportConfig.channel)<<'\t'<<v.fullNotes
            <<"\t120\t"<<v.full.totalQN<<'\t'<<fullPath.string()<<'\n';
        midiRows<<v.pair<<'\t'<<v.alias<<"\tsuffix-only\t"<<v.abstractLabel<<'\t'<<v.primaryId<<"\tVoiceLed\t"
            <<int(exportConfig.upperVelocity)<<'\t'<<int(exportConfig.bassVelocity)<<'\t'<<int(exportConfig.channel)<<'\t'<<v.suffixNotes
            <<"\t120\t"<<v.suffix.totalQN<<'\t'<<suffixPath.string()<<'\n';
        answer<<v.pair<<'\t'<<v.alias<<'\t'<<v.query<<'\t'<<v.abstractLabel<<'\t'<<v.targetId<<'\t'<<v.primaryId<<'\t'
            <<v.path<<"\tinput+suffix\t"<<(blind/(v.pair+"-"+v.alias+"-full.wav")).string()<<'\t'<<fullPath.string()<<'\n';
        answer<<v.pair<<'\t'<<v.alias<<'\t'<<v.query<<'\t'<<v.abstractLabel<<'\t'<<v.targetId<<'\t'<<v.primaryId<<'\t'
            <<chordList(v.continuation)<<"\tsuffix-only\t"<<(blind/(v.pair+"-"+v.alias+"-suffix.wav")).string()<<'\t'<<suffixPath.string()<<'\n';
    }
    for(const auto& test:cases)for(const bool suffixOnly:{false,true}) {
        const auto a=std::find_if(blindVersions.begin(),blindVersions.end(),[&](const auto& v){return v.pair==test.pair&&v.alias=="A";});
        const auto b=std::find_if(blindVersions.begin(),blindVersions.end(),[&](const auto& v){return v.pair==test.pair&&v.alias=="B";});
        require(a!=blindVersions.end()&&b!=blindVersions.end(),"each anonymous listening pair has A and B versions");
        const auto& audioA=suffixOnly?a->suffixAudio:a->fullAudio;const auto& audioB=suffixOnly?b->suffixAudio:b->fullAudio;
        const double commonRms=std::min(audioA.rms,audioB.rms);require(commonRms>0,"pair RMS target is positive");
        for(const auto* version:{&*a,&*b}) {
            const auto& raw=suffixOnly?version->suffixAudio:version->fullAudio;const double gain=commonRms/raw.rms;
            require(gain>0&&gain<=1.0000001,"pair normalization never boosts or dynamically compresses a clip");
            const double postPeak=raw.peak*gain;require(postPeak<1.0,"linear-gain WAV remains below full scale");
            auto rendered=raw;for(auto& sample:rendered.left)sample=static_cast<float>(sample*gain);
            for(auto& sample:rendered.right)sample=static_cast<float>(sample*gain);
            rendered.rms=commonRms;rendered.peak=postPeak;
            const auto wavPath=blind/(version->pair+"-"+version->alias+(suffixOnly?"-suffix.wav":"-full.wav"));std::string error;
            require(preview::writeWav16(rendered,wavPath,error),"anonymous 48-kHz PCM16 WAV writes");
            levels<<version->pair<<'\t'<<version->alias<<'\t'<<(suffixOnly?"suffix-only":"input+suffix")<<'\t'
                <<version->abstractLabel<<'\t'<<version->primaryId<<'\t'<<commonRms<<'\t'<<raw.rms<<'\t'<<raw.peak<<'\t'
                <<gain<<'\t'<<postPeak<<'\t'<<(postPeak<1.0)<<'\t'<<raw.sampleRate<<'\t'<<raw.left.size()<<'\t'<<wavPath.string()<<'\n';
        }
    }
    instructions<<"Anonymous Phase 5.1 Abstract comparison. Each Q1/Q2 pair has A and B; full files contain the entire input plus returned continuation, while suffix files contain the complete continuation excerpt. The mapping is in the separate ../answer-key.tsv; keep it closed during blind listening.\n\n"
        <<"WAV: stereo PCM16, 48 kHz, 120 BPM, generated by the same built-in PreviewSynth. Each full WAV is rendered from a fresh synth state. Its suffix WAV is an exact sample excerpt beginning at the recommendation boundary, retaining the full-sequence voice-leading context and natural release tail; it is not a fresh-reset render. Each pair and scope uses one common target RMS (the quieter raw member) with a single linear per-clip gain; no dynamic processing or voicing change is applied. Raw levels, gains, and frame counts are in ../blind-audio-levels.tsv.\n\n"
        <<"MIDI: each file is exported from that same full preview sequence as VoiceLed with 80 upper / 90 bass velocity, channel 1 (zero-based channel 0), 120 BPM, annotated full-phrase or continuation-only scope. Continuation-only MIDI preserves the full-sequence voicing context. No Program Change is written. For MIDI playback, use the same GM patch (e.g. GM Acoustic Grand Piano), host volume, expression, pan, and velocity response for all files. The built-in WAV is a rule-based PreviewSynth render and does not use MIDI velocities; it is not represented as a GM-rendered recording.\n"
        <<"\nDelivered WAV durations in seconds (including the renderer's final release tail):\n| Pair | A full | B full | A suffix | B suffix |\n|---|---:|---:|---:|---:|\n";
    instructions<<std::fixed<<std::setprecision(2);
    for(const auto& test:cases) {
        const auto a=std::find_if(blindVersions.begin(),blindVersions.end(),[&](const auto& v){return v.pair==test.pair&&v.alias=="A";});
        const auto b=std::find_if(blindVersions.begin(),blindVersions.end(),[&](const auto& v){return v.pair==test.pair&&v.alias=="B";});
        require(a!=blindVersions.end()&&b!=blindVersions.end(),"blind duration table has both aliases");
        instructions<<"| "<<test.pair<<" | "<<a->fullAudio.left.size()/a->fullAudio.sampleRate<<" s | "
            <<b->fullAudio.left.size()/b->fullAudio.sampleRate<<" s | "<<a->suffixAudio.left.size()/a->suffixAudio.sampleRate
            <<" s | "<<b->suffixAudio.left.size()/b->suffixAudio.sampleRate<<" s |\n";
    }

    // R01: run the isolated V5 candidate against the original explicit-bass input,
    // then inspect the actual VoiceLed MIDI notes derived from that preview sequence.
    auto r01Input=symbolic(phrase({"Am","G#dim","G","D/F#"},{4,4,4,4},true));const auto r01Original=r01Input;
    AnalysisContext r01Context;r01Context.forcedKey=aMinor;r01Context.timeSigNumerator=4;r01Context.timeSigDenominator=4;
    auto r01Weights=plugin::developmentRecommendationWeights();r01Weights.diminishedPassingInsertionException=false;r01Weights.abstractChordCompatibility=false;
    require(r01Weights.perGroup==10,"R01 candidate route keeps the ten-card worker cap");
    const auto r01Rec=recommendContinuations(makeMatchQuery(r01Input,r01Context),candidateIndex,{},r01Weights);
    const auto r01Card=selectCard(r01Rec,candidateId);require(r01Card.candidate!=nullptr,"V5 R01 candidate is returned by the isolated full-worker core route");
    const auto r01Shown=session::presentationIndices(r01Rec,{});
    for(std::size_t g=0;g<r01Rec.groups.size();++g)for(std::size_t rank=0;rank<std::min<std::size_t>(3,r01Rec.groups[g].size());++rank) {
        const auto& c=r01Rec.groups[g][rank];const bool visible=std::find(r01Shown[g].begin(),r01Shown[g].end(),rank)!=r01Shown[g].end();
        r01Top<<groupNames[g]<<'\t'<<rank+1<<'\t'<<visible<<'\t'<<c.primaryTemplate<<'\t'<<idText(c.supportingTemplates)<<'\t'
            <<intentName(c.intent)<<'\t'<<styleText(c.styles)<<'\t'<<c.rankingScore<<'\t'<<c.matchSimilarity<<'\t'
            <<chordList(c.continuation)<<'\t'<<(c.primaryTemplate==candidateId)<<'\n';
    }
    const auto r01Match=std::find_if(r01Rec.matches.begin(),r01Rec.matches.end(),[&](const auto& m){return m.templateId==candidateId;});
    require(r01Match!=r01Rec.matches.end(),"V5 candidate has an actual R01 MatchResult");
    const auto r01Session=imported(r01Input);const auto r01Preview=preview::buildSequence(r01Session,r01Card.candidate,120.);
    require(bool(r01Preview)&&r01Preview.sequence.events.size()==r01Input.size()+r01Card.candidate->continuation.size()&&
            r01Preview.sequence.recommendationBoundary==r01Input.size(),"R01 preview contains source input followed by returned suffix");
    for(std::size_t i=0;i<r01Input.size();++i) {
        const auto expected=chordPitches(r01Input[i]);const auto& actual=r01Preview.sequence.events[i];
        require(actual.segment==preview::Segment::CurrentOpen||actual.segment==preview::Segment::Existing,
                "R01 source prefix remains on the input side of preview boundary");
        require(expected.root==actual.chord.root&&expected.bass==actual.chord.bass&&expected.intervals==actual.chord.intervals,
                "R01 preview preserves each original source chord without rewriting it");
        require(r01Input[i].name==r01Original[i].name&&r01Input[i].startQN==r01Original[i].startQN&&
                r01Input[i].durationQN==r01Original[i].durationQN&&r01Input[i].openEnded==r01Original[i].openEnded,
                "R01 source progression remains unchanged after preview build");
    }
    auto r01FullClip=midi::buildClip(r01Preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::FullPhrase,{4,4},aMinor,r01Card.candidate->intent,exportConfig);
    auto r01SuffixClip=midi::buildClip(r01Preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::ContinuationOnly,{4,4},aMinor,r01Card.candidate->intent,exportConfig);
    require(bool(r01FullClip)&&bool(r01SuffixClip),"R01 full and suffix-only preview exports build");
    const auto r01FullBytes=midi::writeToMemory(r01FullClip.sequence,exportConfig),r01SuffixBytes=midi::writeToMemory(r01SuffixClip.sequence,exportConfig);
    require(bool(r01FullBytes)&&bool(r01SuffixBytes),"R01 preview exports serialize as MIDI");
    auto decoded=midi::readFromMemory(r01FullBytes.bytes);const auto decodedSuffix=midi::readFromMemory(r01SuffixBytes.bytes);
    compareMidi(r01Preview.sequence,decoded);
    checkMidiVoicing(r01Preview.sequence,decoded,false);checkMidiVoicing(r01Preview.sequence,decodedSuffix,true);
    const auto r01SuffixStart=r01Preview.sequence.events[r01Preview.sequence.recommendationBoundary].startQN;
    preview::Sequence r01SuffixSequence;r01SuffixSequence.tempoBPM=120.;r01SuffixSequence.recommendationBoundary=0;
    for(std::size_t i=r01Preview.sequence.recommendationBoundary;i<r01Preview.sequence.events.size();++i) {
        auto e=r01Preview.sequence.events[i];e.startQN-=r01SuffixStart;r01SuffixSequence.events.push_back(std::move(e));
    }
    r01SuffixSequence.totalQN=r01Preview.sequence.totalQN-r01SuffixStart;
    compareMidi(r01SuffixSequence,decodedSuffix);
    std::string r01Error;const auto r01FullMidi=r01/"R01_V5_rule_based_preview_input-plus-suffix.mid";
    const auto r01SuffixMidi=r01/"R01_V5_rule_based_preview_suffix-only.mid";
    require(midi::writeToFile(r01FullBytes.bytes,r01FullMidi,r01Error),"R01 full preview MIDI writes");r01Error.clear();
    require(midi::writeToFile(r01SuffixBytes.bytes,r01SuffixMidi,r01Error),"R01 suffix-only preview MIDI writes");
    const auto r01Wav=preview::renderOffline(r01Preview.sequence,48000.);require(!r01Wav.left.empty()&&r01Wav.rms>0,"R01 full candidate preview renders at 48 kHz");
    r01Error.clear();require(preview::writeWav16(r01Wav,r01/"R01_V5_rule_based_preview_input-plus-suffix.wav",r01Error),"R01 rule-based preview WAV writes");
    require(bool(decoded),"R01 preview MIDI can be read back");
    std::array<int,5> expectedBass{45,44,43,42,41};bool descent=true;
    preview::ChordVoicer r01Voicer;
    for(std::size_t i=0;i<r01Preview.sequence.events.size();++i) {
        const auto& event=r01Preview.sequence.events[i];const auto voiced=r01Voicer.voice(event.chord,event.topVoice);
        int expectedLowest=voiced.bass;for(int v=0;v<voiced.upperCount;++v)expectedLowest=std::min(expectedLowest,voiced.upper[v]);
        int actual=128;for(const auto& note:decoded.file.notes)
            if(closeQN(note.startQN,event.startQN)&&closeQN(note.durationQN,event.durationQN))actual=std::min(actual,note.pitch);
        const auto expected=chordPitches(r01Input[std::min(i,r01Input.size()-1)]);
        const auto segment=i<r01Preview.sequence.recommendationBoundary?"input":"suffix";
        lowest<<segment<<'\t'<<i<<'\t'<<event.startQN<<'\t'<<event.chord.label<<'\t'<<expectedLowest<<'\t'<<pitchName(expectedLowest)<<'\t'
            <<actual<<'\t'<<pitchName(actual)<<'\t'<<(actual==expectedLowest)<<'\n';
        if(i<expectedBass.size())descent=descent&&actual==expectedBass[i];
        if(i<r01Input.size())require(actual<128&&event.chord.root==expected.root,"R01 input event has emitted MIDI pitches");
    }
    std::ofstream route(r01/"candidate-route.tsv"),r01Levels(r01/"render-level.tsv");require(bool(route)&&bool(r01Levels),"R01 route summary and render levels open");
    route<<"candidateID\tgroup\tworkerRank\tdefaultPresented\tintent\tstylesHex\tscore\tsimilarity\tmatchDisposition\treasonNames\tcontinuation\tpreviewBoundary\tinputQN\ttempoBPM\tfullMidi\tsuffixMidi\tfullWav\tlowBassDescentAtoF\n";
    route<<candidateId<<'\t'<<groupNames[r01Card.group]<<'\t'<<r01Card.rank+1<<'\t'<<r01Card.presented<<'\t'
        <<intentName(r01Card.candidate->intent)<<'\t'<<styleText(r01Card.candidate->styles)<<'\t'<<r01Card.candidate->rankingScore<<'\t'
        <<r01Card.candidate->matchSimilarity<<'\t'<<r01Match->disposition<<'\t'<<phase5ReasonString(*r01Match)<<'\t'
        <<chordList(r01Card.candidate->continuation)<<'\t'<<r01Preview.sequence.recommendationBoundary<<'\t'
        <<r01SuffixStart<<'\t'<<r01Preview.sequence.tempoBPM<<'\t'<<r01FullMidi.string()<<'\t'<<r01SuffixMidi.string()<<'\t'
        <<(r01/"R01_V5_rule_based_preview_input-plus-suffix.wav").string()<<'\t'<<descent<<'\n';
    r01Levels<<"sampleRate\tframes\tchannels\trmsLeft\tpeak\tpeakVoices\tlinearGain\tpostGainPeak\tnoClipping\tpath\n"
        <<r01Wav.sampleRate<<'\t'<<r01Wav.left.size()<<"\t2\t"<<r01Wav.rms<<'\t'<<r01Wav.peak<<'\t'<<r01Wav.peakVoices
        <<"\t1\t"<<r01Wav.peak<<'\t'<<(r01Wav.peak<1.)<<"\t"<<(r01/"R01_V5_rule_based_preview_input-plus-suffix.wav").string()<<'\n';
    findings<<"\nR01 core result: the candidate is `"<<candidateId<<"`, group="<<groupNames[r01Card.group]<<", workerRank="<<r01Card.rank+1
        <<", defaultPresented="<<r01Card.presented<<", intent="<<intentName(r01Card.candidate->intent)<<", styleFlags="<<styleText(r01Card.candidate->styles)
        <<", returned suffix=`"<<chordList(r01Card.candidate->continuation)<<"`. Its full rule-based preview MIDI lows are tabulated by input/suffix segment in `r01/preview-lowest-notes.tsv`; expected A2–G#2–G2–F#2–F2 descent verified="<<descent<<".\n\n"
        <<"R01 outputs are explicitly named rule-based preview MIDI/WAV, not recorded MIDI. MIDI full phrase includes input+suffix; suffix-only is a separate export with relative start at zero. The typed event table preserves null bass declarations independently of the realized preview notes.\n";
    std::ofstream findingsFile(out/"findings.md");require(bool(findingsFile),"Phase51 music findings report opens");findingsFile<<findings.str();
    instructions.flush();findingsFile.flush();
    std::cout<<"Phase51 music diagnostics PASS comparisons="<<blindVersions.size()<<" anonymousWAV=8 anonymousMIDI=8 R01Rank="<<r01Card.rank+1
        <<" R01BassDescent="<<descent<<" output="<<out.string()<<"\n";
    require(descent,"R01 preview MIDI must audibly emit A2-G#2-G2-F#2-F2 as its lowest notes");
    return 0;
}
int runPhase5(const std::filesystem::path& out,const std::filesystem::path& candidatePath) {
    std::filesystem::create_directories(out);
    const auto loadedV4=library::loadFactory(HC_FACTORY_DB_PATH);
    require(bool(loadedV4)&&loadedV4.templates.size()==657,"Phase5 V4/657 baseline loaded");
    const auto loadedCandidate=library::loadFactory(candidatePath);
    require(bool(loadedCandidate)&&loadedCandidate.templates.size()==658,"Phase5 isolated candidate V5/658 loaded");
    for(const auto& t:loadedV4.templates)require(std::count_if(loadedCandidate.templates.begin(),loadedCandidate.templates.end(),[&](const auto& c){return c.id==t.id;})==1,
        "candidate DB keeps every old V4 ID exactly once");
    const TemplateID newId="CANDIDATE_V5_CHROMATIC_MINOR_001";
    require(std::count_if(loadedCandidate.templates.begin(),loadedCandidate.templates.end(),[&](const auto& t){return t.id==newId;})==1,
            "isolated candidate adds exactly the expected V5 ID");
    std::array<Phase5Lib,3> libs{{{"A_V4_657_ABSTRACT_OFF",loadedV4.templates,{}},
        {"B_V4_657_ABSTRACT_ON",loadedV4.templates,{}},{"C_V5_658_ABSTRACT_ON",loadedCandidate.templates,{}}}};
    for(auto& lib:libs)lib.index=std::make_unique<CandidateIndex>(lib.templates);
    phase5CompatibilityExperiments(out);
    std::ofstream summary(out/"phase5-summary.txt"),coverage(out/"phase5-coverage.tsv"),delta(out/"phase5-delta.tsv"),
        support(out/"phase5-support-frequency.tsv"),visible(out/"phase5-visible-candidates.tsv"),
        r02(out/"phase5-r02.tsv"),r02Trace(out/"phase5-r02-trace.txt"),
        b11(out/"phase5-b11-target.tsv"),switches(out/"phase5-switch-matrix.tsv"),perf(out/"phase5-performance.tsv"),
        midiCheck(out/"phase5-preview-midi-check.txt");
    coverage<<"library\tcaseKey\tvariant\tprefixLength\tmode\ttargetId\tintent\tstyles\tshortlist\taligned\tpathPassed\tdistinct\tdedupPaths\tscorePassed\toutput\teligible\tsupportSources\tdefaultPresented\tworkerTop3\tworkerTop10\tworkerTop20\tworkerTop50\tgroup\tsimilarity\trawCost\trankingScore\teligibilityScore\tdisposition\tcolorCount\tabstractON\n";
    delta<<"caseKey\tvariant\tprefixLength\tmode\ttargetV4Id\tA_eligible\tB_eligible\tC_eligible\tA_presented\tB_presented\tC_presented\tA_top3\tB_top3\tC_top3\tA_top10\tB_top10\tC_top10\tA_similarity\tB_similarity\tC_similarity\tA_cost\tB_cost\tC_cost\tC_lost_old_top3\tC_lost_old_top10\n";
    support<<"library\ttargetV4Id\tqueries\teligible\tdefaultPresented\tworkerTop3\tworkerTop10\tworkerTop20\tworkerTop50\tsupportOccurrences\tlowExposureDefault\n";
    visible<<"world\tcaseKey\tvariant\tprefixLength\tprimaryID\tsupportingIDs\tintent\tstyles\tscore\tsimilarity\tcontinuationLabelsQN\tdefaultPresented\tworkerRank\n";
    r02<<"library\tkeySetting\trhythm\tHF\tP3\tabstract\tshortlist\tmatchCount\tpathPassed\tdistinct\toutput\tColor\teligibleMatchCount\tremovalDispositionCounts\thostRootCause\n";
    b11<<"library\tshape\tqueryExtension\tprefixLength\tabstractON\tV4_B11_seen\tsimilarity\trawCost\teligible\tdefaultPresented\tworkerTop3\tworkerTop10\tgroup\tdisposition\treasonNames\n";
    switches<<"library\tquery\tHF\tP3\tabstractON\tactualAbstract\tactualP3\tV4_A08_eligible\tV4_A08_presented\tV4_A08_top3\tV4_A08_top10\tcandidateID_eligible\tcandidateID_presented\tcandidateID_top3\tcandidateID_top10\n";
    perf<<"library\tindexSize\tprefix\tabstractON\tp50ms\tp95ms\tmaxms\tCPUms\tRSSbefore\tRSSafter\tRSSpeak\tshortlist\n";
    const auto prefixCases=phase5PrefixCases(loadedV4.templates);
    std::map<std::pair<std::string,TemplateID>,Phase5Frequency> frequencies;
    std::size_t coverageRows{};
    for(const auto& test:prefixCases) {
        AnalysisContext context;context.forcedKey=KeySignature{test.mode==Mode::Major?PitchClass::C:PitchClass::A,test.mode};
        context.timeSigNumerator=4;context.timeSigDenominator=4;context.retrievalTimingMode=RetrievalTimingMode::Legacy;
        auto queryProgression=phrase(test.names,test.durations);
        const auto query=makeMatchQuery(queryProgression,context);
        std::array<RecommendationSet,3> results;
        std::array<RecommendationWeights,3> recWeights{};
        for(std::size_t lib=0;lib<libs.size();++lib) {
            recWeights[lib]=plugin::developmentRecommendationWeights();
            recWeights[lib].diminishedPassingInsertionException=false;
            recWeights[lib].abstractChordCompatibility=lib!=0;
            results[lib]=recommendContinuations(query,*libs[lib].index,{},recWeights[lib]);
            require(results[lib].retrievalTimingMode==RetrievalTimingMode::Legacy&&
                !results[lib].phase3aEnabled&&results[lib].abstractChordCompatibility==(lib!=0),
                "A/B/C default full-library switch states recorded");
            const auto shown=session::presentationIndices(results[lib],{});
            for(std::size_t group=0;group<results[lib].groups.size();++group)for(std::size_t rank=0;rank<results[lib].groups[group].size();++rank) {
                const auto& candidate=results[lib].groups[group][rank];std::ostringstream sources,tail;
                for(std::size_t i=0;i<candidate.supportingTemplates.size();++i){if(i)sources<<';';sources<<candidate.supportingTemplates[i];}
                for(std::size_t i=0;i<candidate.continuation.size();++i){if(i)tail<<';';tail<<candidate.continuation[i].label<<'@'<<candidate.continuation[i].durationQN;}
                const bool defaultPresented=std::find(shown[group].begin(),shown[group].end(),rank)!=shown[group].end();
                visible<<libs[lib].name<<'\t'<<test.key<<'\t'<<(test.simplified?"simplified":"exact")<<'\t'<<test.prefix<<'\t'
                    <<candidate.primaryTemplate<<'\t'<<sources.str()<<'\t'<<intentName(candidate.intent)<<'\t'<<phase5Styles(candidate.styles)<<'\t'
                    <<candidate.rankingScore<<'\t'<<candidate.matchSimilarity<<'\t'<<tail.str()<<'\t'<<defaultPresented<<'\t'<<rank+1<<'\n';
            }
        }
        for(const auto& target:test.targetIds) {
            const auto item=std::find_if(loadedV4.templates.begin(),loadedV4.templates.end(),[&](const auto& t){return t.id==target;});
            require(item!=loadedV4.templates.end(),"deduplicated prefix retains every expected V4 target ID");
            std::array<Phase5TargetOutcome,3> outcomes;
            for(std::size_t lib=0;lib<libs.size();++lib) {
                const auto& rec=results[lib];auto& outcome=outcomes[lib]=phase5Outcome(rec,target,recWeights[lib].minimumScore);
                auto& f=frequencies[{libs[lib].name,target}];++f.attempts;f.eligible+=outcome.eligible;
                f.defaultVisible+=outcome.presented;f.top3+=outcome.top3;f.top10+=outcome.top10;
                f.top20+=outcome.top20;f.top50+=outcome.top50;f.support+=outcome.sourceCount;
                const auto& m=outcome.match;const auto color=rec.groups[3].size();
                coverage<<libs[lib].name<<'\t'<<test.key<<'\t'<<(test.simplified?"simplified":"exact")<<'\t'
                    <<test.prefix<<'\t'<<phase5Mode(test.mode)<<'\t'<<target<<'\t'<<intentName(item->intent)<<'\t'
                    <<phase5Styles(item->styles)<<'\t'<<rec.stats.shortlistSize<<'\t'<<rec.stats.aligned<<'\t'
                    <<rec.stats.pathPassed<<'\t'<<rec.stats.distinct<<'\t'<<(rec.stats.pathPassed-rec.stats.distinct)<<'\t'
                    <<rec.stats.scorePassed<<'\t'<<rec.stats.output<<'\t'<<outcome.eligible<<'\t'<<outcome.sourceCount<<'\t'
                    <<outcome.presented<<'\t'<<outcome.top3<<'\t'<<outcome.top10<<'\t'<<outcome.top20<<'\t'<<outcome.top50<<'\t'
                    <<phase5Rank(outcome)<<'\t'<<(m?m->similarity:-1.f)<<'\t'<<(m?m->rawCost:-1.f)<<'\t'
                    <<(m?m->candidateRankingScore:-1.f)<<'\t'<<(m?m->candidateEligibilityScore:-1.f)<<'\t'
                    <<phase5Disposition(outcome)<<'\t'<<color<<'\t'<<(lib!=0)<<'\n';++coverageRows;
            }
            const auto score=[](const Phase5TargetOutcome& o,bool similarity){return o.match?(similarity?o.match->similarity:o.match->rawCost):-1.f;};
            const bool lost3=outcomes[0].top3&&!outcomes[2].top3,lost10=outcomes[0].top10&&!outcomes[2].top10;
            delta<<test.key<<'\t'<<(test.simplified?"simplified":"exact")<<'\t'<<test.prefix<<'\t'<<phase5Mode(test.mode)<<'\t'<<target<<'\t'
                <<outcomes[0].eligible<<'\t'<<outcomes[1].eligible<<'\t'<<outcomes[2].eligible<<'\t'
                <<outcomes[0].presented<<'\t'<<outcomes[1].presented<<'\t'<<outcomes[2].presented<<'\t'
                <<outcomes[0].top3<<'\t'<<outcomes[1].top3<<'\t'<<outcomes[2].top3<<'\t'
                <<outcomes[0].top10<<'\t'<<outcomes[1].top10<<'\t'<<outcomes[2].top10<<'\t'
                <<score(outcomes[0],true)<<'\t'<<score(outcomes[1],true)<<'\t'<<score(outcomes[2],true)<<'\t'
                <<score(outcomes[0],false)<<'\t'<<score(outcomes[1],false)<<'\t'<<score(outcomes[2],false)<<'\t'
                <<lost3<<'\t'<<lost10<<'\n';
        }
    }
    for(const auto& [key,f]:frequencies)support<<key.first<<'\t'<<key.second<<'\t'<<f.attempts<<'\t'<<f.eligible<<'\t'
        <<f.defaultVisible<<'\t'<<f.top3<<'\t'<<f.top10<<'\t'<<f.top20<<'\t'<<f.top50<<'\t'<<f.support<<'\t'<<(f.defaultVisible==0)<<'\n';
    summary<<"A/B/C full-library coverage uses the same deduplicated V4-derived exact/simplified prefix corpus; each case keeps all expected V4 template IDs.\n"
        <<"Unique prefix queries="<<prefixCases.size()<<" target coverage rows="<<coverageRows<<"; worker ranks are separated from default presentationIndices visibility.\n"
        <<"A=V4/657 abstract OFF; B=V4/657 abstract ON; C=isolated V5/658 abstract ON. HF=P3=OFF for this comparison.\n"
        <<"R02 host root cause remains UNKNOWN; complete recommendationDiagnostics traces and removal dispositions are exported.\n";

    // Real V4_B11 side-slip input: plain triads and complete maj9 labels, at
    // multiple tails. A short prefix is open; the six-chord case tests a long query.
    const auto b11Prototype=std::find_if(loadedV4.templates.begin(),loadedV4.templates.end(),[](const auto& t){return t.id=="V4_B11";});
    require(b11Prototype!=loadedV4.templates.end()&&b11Prototype->full.size()>6,
            "V4_B11 has the seven-chord cadence and a valid tail after six chords");
    bool snapshotWritten{};
    const std::array<std::size_t,4> b11Lengths{2,3,5,6};
    for(const auto& [shape,extension]:std::array<std::pair<const char*,bool>,2>{{{"plain",false},{"maj9",true}}})
    for(const auto length:b11Lengths) {
        std::vector<std::string> names;std::vector<double> durations;
        for(std::size_t i=0;i<length;++i) {
            const auto& event=b11Prototype->full[i];const auto realized=realizeContinuation(event,{PitchClass::C,Mode::Major},event.durationQN.value_or(4.));
            names.push_back(extension?realized.label:phase5SimplifyLabel(realized.label));durations.push_back(event.durationQN.value_or(4.));
        }
        const auto p=phrase(names,durations,true);
        AnalysisContext context;context.forcedKey=KeySignature{PitchClass::C,Mode::Major};context.timeSigNumerator=4;context.timeSigDenominator=4;
        const auto query=makeMatchQuery(p,context);
        for(std::size_t lib=0;lib<libs.size();++lib) {
            auto w=plugin::developmentRecommendationWeights();w.diminishedPassingInsertionException=false;w.abstractChordCompatibility=lib!=0;
            const auto rec=recommendContinuations(query,*libs[lib].index,{},w);require(rec.abstractChordCompatibility==(lib!=0),"B11 compatibility switch state recorded");
            const auto outcome=phase5Outcome(rec,"V4_B11",w.minimumScore);const auto* m=outcome.match;
            b11<<libs[lib].name<<'\t'<<shape<<'\t'<<extension<<'\t'<<length<<'\t'<<(lib!=0)<<'\t'<<(m!=nullptr)<<'\t'
                <<(m?m->similarity:-1.f)<<'\t'<<(m?m->rawCost:-1.f)<<'\t'<<outcome.eligible<<'\t'<<outcome.presented<<'\t'
                <<outcome.top3<<'\t'<<outcome.top10<<'\t'<<outcome.group<<'\t'<<phase5Disposition(outcome)<<'\t'
                <<(m?phase5ReasonString(*m):std::string(""))<<'\n';
            if(lib==1&&std::string(shape)=="plain"&&length==2) {
                auto offWeights=w;offWeights.abstractChordCompatibility=false;
                const auto aRun=recommendContinuations(query,*libs[0].index,{},offWeights);
                const auto aMatch=std::find_if(aRun.matches.begin(),aRun.matches.end(),[](const auto& candidate){return candidate.templateId=="V4_B11";});
                const auto bMatch=std::find_if(rec.matches.begin(),rec.matches.end(),[](const auto& candidate){return candidate.templateId=="V4_B11";});
                require(aMatch!=aRun.matches.end()&&bMatch!=rec.matches.end(),"plain C-Db B11 prefix matches the real template in A and B");
                if(aMatch!=aRun.matches.end()&&bMatch!=rec.matches.end()) {
                    const bool gained=bMatch->rawCost+0.0001f<aMatch->rawCost&&
                        std::any_of(bMatch->alignmentTrace.begin(),bMatch->alignmentTrace.end(),[](const auto& step){return hasReason(step.reasons,MatchReason::AbstractExtensionCompatibility);});
                    require(gained,"plain C-Db B11 prefix gains real abstract-extension match evidence");
                }
            }
            const bool abstractReason=m&&std::any_of(m->alignmentTrace.begin(),m->alignmentTrace.end(),[](const auto& step){
                return hasReason(step.reasons,MatchReason::AbstractExtensionCompatibility);
            });
            if(lib==1&&std::string(shape)=="plain"&&length==2&&!snapshotWritten&&abstractReason) {
                for(const auto& group:rec.groups) {
                    for(const auto& candidate:group)if(std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),"V4_B11")!=candidate.supportingTemplates.end()) {
                        const auto check=phase5SnapshotCheck(out,p,candidate,rec.matches,{4,4});
                        midiCheck<<"B11 plain length-2 abstract candidate snapshot: "<<check.detail<<"\n";snapshotWritten=true;break;
                    }
                    if(snapshotWritten)break;
                }
            }
        }
    }
    if(!snapshotWritten) {
        for(const auto& test:prefixCases) {
            AnalysisContext context;context.forcedKey=KeySignature{test.mode==Mode::Major?PitchClass::C:PitchClass::A,test.mode};
            context.timeSigNumerator=4;context.timeSigDenominator=4;
            const auto p=phrase(test.names,test.durations);const auto query=makeMatchQuery(p,context);
            auto w=plugin::developmentRecommendationWeights();w.abstractChordCompatibility=true;w.diminishedPassingInsertionException=false;
            const auto rec=recommendContinuations(query,*libs[1].index,{},w);
            for(const auto& group:rec.groups) {
                for(const auto& candidate:group) {
                    const auto source=std::find_if(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),[&](const auto& id){
                        const auto match=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& m){return m.templateId==id;});
                        return match!=rec.matches.end()&&std::any_of(match->alignmentTrace.begin(),match->alignmentTrace.end(),[](const auto& step){
                            return hasReason(step.reasons,MatchReason::AbstractExtensionCompatibility);
                        });
                    });
                    if(source!=candidate.supportingTemplates.end()) {
                        const auto check=phase5SnapshotCheck(out,p,candidate,rec.matches,{4,4});
                        midiCheck<<"Fallback actual B abstract source="<<*source<<" case="<<test.key<<": "<<check.detail<<"\n";
                        snapshotWritten=true;break;
                    }
                }
                if(snapshotWritten)break;
            }
            if(snapshotWritten)break;
        }
    }
    require(snapshotWritten,"at least one actually abstract B recommendation passes snapshot/preview/MIDI roundtrip");

    // The candidate route uses the A08-style chromatic-minor source. Keep an
    // unspecified bass query distinct from slash-bass evidence.
    bool candidateRouteBaseline{};
    for(const std::size_t lib:{0u,2u})for(bool abstract:{false,true})for(bool hf:{false,true})for(bool p3:{false,true})
    for(bool abstractBass:{false,true}) {
        auto p=abstractBass?phrase({"Am","G#dim","G","D"},{4,4,4,4},true):
            symbolic(phrase({"Am","G#dim","G","D/F#"},{4,4,4,4},true));
        AnalysisContext context;context.forcedKey=KeySignature{PitchClass::A,Mode::Minor};context.timeSigNumerator=4;context.timeSigDenominator=4;
        context.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        auto w=plugin::developmentRecommendationWeights();w.diminishedPassingInsertionException=p3;w.abstractChordCompatibility=abstract;
        const auto rec=recommendContinuations(makeMatchQuery(p,context),*libs[lib].index,{},w);
        require(rec.retrievalTimingMode==context.retrievalTimingMode&&rec.phase3aEnabled==p3&&rec.abstractChordCompatibility==abstract,
                "R01 switch matrix actual invocation fields recorded");
        const auto old=phase5Outcome(rec,"V4_A08",w.minimumScore),candidate=phase5Outcome(rec,newId,w.minimumScore);
        if(lib==2&&!abstract&&!hf&&!p3&&!abstractBass)candidateRouteBaseline=candidate.eligible;
        switches<<(lib==0?"V4_657":"V5_658")<<'\t'<<(abstractBass?"unspecified-bass":"explicit-D/F#")<<'\t'<<hf<<'\t'<<p3<<'\t'<<abstract<<'\t'
            <<rec.abstractChordCompatibility<<'\t'<<rec.phase3aEnabled<<'\t'<<old.eligible<<'\t'<<old.presented<<'\t'<<old.top3<<'\t'<<old.top10<<'\t'
            <<candidate.eligible<<'\t'<<candidate.presented<<'\t'<<candidate.top3<<'\t'<<candidate.top10<<'\n';
    }
    require(candidateRouteBaseline,"isolated V5 candidate route remains eligible with compatibility switches OFF");
    auto a08Positive=symbolic(phrase({"Am","G#dim","G","D/F#"},{2,2,2,2},true));
    AnalysisContext a08Context;a08Context.forcedKey=KeySignature{PitchClass::A,Mode::Minor};
    a08Context.timeSigNumerator=4;a08Context.timeSigDenominator=4;a08Context.retrievalTimingMode=RetrievalTimingMode::HarmonyFirst;
    auto a08Weights=plugin::developmentRecommendationWeights();a08Weights.diminishedPassingInsertionException=false;a08Weights.abstractChordCompatibility=false;
    const auto a08Baseline=recommendContinuations(makeMatchQuery(a08Positive,a08Context),*libs[0].index,{},a08Weights);
    const auto a08Outcome=phase5Outcome(a08Baseline,"V4_A08",a08Weights.minimumScore);
    require(a08Outcome.eligible,"original V4_A08 ID remains eligible for verified 2QN OPEN A-minor HF-on baseline");
    switches<<"V4_657\tverified-2QN-OPEN-Aminor-HF-on\t1\t0\t0\t"<<a08Baseline.abstractChordCompatibility<<'\t'
        <<a08Baseline.phase3aEnabled<<'\t'<<a08Outcome.eligible<<'\t'<<a08Outcome.presented<<'\t'<<a08Outcome.top3<<'\t'<<a08Outcome.top10
        <<"\t0\t0\t0\t0\n";

    // R02 factorial diagnostics: Auto/C-major/A-minor, two rhythm patterns,
    // HF/P3/abstract switches, and V4/V5 content. Root cause remains UNKNOWN.
    for(const std::size_t lib:{0u,2u})for(int key=0;key<3;++key)for(int rhythm=0;rhythm<2;++rhythm)
    for(bool hf:{false,true})for(bool p3:{false,true})for(bool abstract:{false,true}) {
        auto p=rhythm?phrase({"F","G","Am"},{3,5,4},true):phrase({"F","G","Am"},{4,4,4});
        AnalysisContext context;context.timeSigNumerator=4;context.timeSigDenominator=4;
        if(key==1)context.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        if(key==2)context.forcedKey=KeySignature{PitchClass::A,Mode::Minor};
        context.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        auto w=plugin::developmentRecommendationWeights();w.diminishedPassingInsertionException=p3;w.abstractChordCompatibility=abstract;
        const auto rec=recommendContinuations(makeMatchQuery(p,context),*libs[lib].index,{},w);
        require(rec.retrievalTimingMode==context.retrievalTimingMode&&rec.phase3aEnabled==p3&&rec.abstractChordCompatibility==abstract,
                "R02 factorial actual invocation fields recorded");
        std::map<std::string,std::size_t> dispositions;std::size_t eligible{};
        for(const auto& m:rec.matches){++dispositions[m.disposition];
            const auto gate=hf?m.candidateEligibilityScore:m.candidateRankingScore;eligible+=m.hasContinuation&&gate>=w.minimumScore;}
        std::ostringstream removed;for(const auto& [name,count]:dispositions)removed<<name<<'='<<count<<';';
        const char* keyName=key==0?"Auto":key==1?"Cmajor":"Aminor";
        r02<<(lib==0?"V4_657":"V5_658")<<'\t'<<keyName<<'\t'<<(rhythm?"3_5_OPEN":"4_4_closed")<<'\t'<<hf<<'\t'<<p3<<'\t'<<abstract<<'\t'
            <<rec.stats.shortlistSize<<'\t'<<rec.matches.size()<<'\t'<<rec.stats.pathPassed<<'\t'<<rec.stats.distinct<<'\t'<<rec.stats.output<<'\t'
            <<rec.groups[3].size()<<'\t'<<eligible<<'\t'<<removed.str()<<"\tUNKNOWN\n";
        r02Trace<<"\nR02 library="<<(lib==0?"V4_657":"V5_658")<<" key="<<keyName<<" rhythm="<<rhythm<<" HF="<<hf<<" P3="<<p3<<" abstract="<<abstract
            <<" hostRootCause=UNKNOWN"<<session::recommendationDiagnostics(p,context,{},rec);
    }

    // Same-process A/B/C short and long query latency, then 2x/4x in-memory
    // index fixtures with unique IDs. Only this optional mode runs the sample.
    AnalysisContext perfContext;perfContext.forcedKey=KeySignature{PitchClass::C,Mode::Major};perfContext.timeSigNumerator=4;perfContext.timeSigDenominator=4;
    for(std::size_t lib=0;lib<libs.size();++lib)for(int length:{2,5}) {
        auto p=length==2?phrase({"C","F"},{4,4},true):phrase({"C","F","G","Am","G"},{4,4,4,4,4},true);
        const auto query=makeMatchQuery(p,perfContext);const bool abstract=lib!=0;auto w=plugin::developmentRecommendationWeights();w.abstractChordCompatibility=abstract;w.diminishedPassingInsertionException=false;
        const auto measured=phase5Measure([&]{return recommendContinuations(query,*libs[lib].index,{},w).stats.shortlistSize;});
        require(measured.shortlist<=800,"A/B/C shortlist remains bounded");
        perf<<libs[lib].name<<'\t'<<libs[lib].templates.size()<<'\t'<<length<<'\t'<<abstract<<'\t'<<measured.p50<<'\t'<<measured.p95<<'\t'<<measured.maximum<<'\t'
            <<measured.cpuMs<<'\t'<<measured.rssBefore<<'\t'<<measured.rssAfter<<'\t'<<measured.rssPeak<<'\t'<<measured.shortlist<<'\n';
    }
    for(const int multiplier:{2,4}) {
        auto expanded=loadedV4.templates;const auto original=loadedV4.templates;
        for(int replica=1;replica<multiplier;++replica)for(const auto& source:original) {
            auto copy=source;copy.id+="_PHASE5_PERF_"+std::to_string(replica);expanded.push_back(std::move(copy));
        }
        CandidateIndex scaleIndex(std::move(expanded));
        for(int length:{2,5}) {
            auto p=length==2?phrase({"C","F"},{4,4},true):phrase({"C","F","G","Am","G"},{4,4,4,4,4},true);
            const auto query=makeMatchQuery(p,perfContext);MatchWeights weights;
            const auto measured=phase5Measure([&]{MatchRunStats stats;matchProgression(query,scaleIndex,weights,800,&stats);return stats.shortlistSize;});
            require(measured.shortlist<=weights.shortlistLimit,"2x/4x in-memory index shortlist remains bounded");
            perf<<"V4_SCALE_"<<multiplier<<'x'<<'\t'<<scaleIndex.templates().size()<<'\t'<<length<<"\t0\t"<<measured.p50<<'\t'<<measured.p95<<'\t'<<measured.maximum<<'\t'
                <<measured.cpuMs<<'\t'<<measured.rssBefore<<'\t'<<measured.rssAfter<<'\t'<<measured.rssPeak<<'\t'<<measured.shortlist<<'\n';
        }
    }
    summary<<"R02 factorial rows="<<2*3*2*2*2*2<<"; A08 switch matrix rows="<<2*2*2*2*2<<" plus one verified baseline; B11 rows="<<3*2*4<<".\n"
        <<"Phase5 checks="<<checks<<"; real Cubase capture/listening, installed plugin behavior and release remain BLOCKED.\n"
        <<"Candidate DB read-only path="<<candidatePath.string()<<"; no source/library DB writes performed by this test mode.\n";
    std::cout<<"Phase5 PASS "<<checks<<" checks; uniqueQueries="<<prefixCases.size()<<" coverageRows="<<coverageRows<<"\n";
    return 0;
}
}
int runPhase6(const std::filesystem::path& out,const std::filesystem::path& factoryPath,
              const std::filesystem::path& oracleRoot,
              const std::filesystem::path& v5FactoryPath={}) {
    std::filesystem::create_directories(out/"midi");
    std::filesystem::create_directories(out/"blind");
    std::filesystem::create_directories(out/"answer-key");
    std::filesystem::remove(out/"blind"/"answer-key.tsv");
    constexpr const char* factoryHash="74c529828f8c4b934bbaf4ada719aa95491ab498b58885522124ff6505671534";
    struct Fixture {std::vector<std::string> names;KeySignature key;};
    const std::map<std::string,Fixture> fixtures{
        {"R01",{{"Am","G#dim","G","D/F#"},{PitchClass::A,Mode::Minor}}},
        {"R01_D",{{"Am","G#dim","G","D"},{PitchClass::A,Mode::Minor}}},
        {"R02_CMAJOR",{{"F","G","Am"},{PitchClass::C,Mode::Major}}},
        {"R02_AMINOR",{{"F","G","Am"},{PitchClass::A,Mode::Minor}}},
        {"B08",{{"C","Cmaj7/B","C7/Bb","F/A"},{PitchClass::C,Mode::Major}}},
        {"B11",{{"Cmaj9","Dbmaj9","Cmaj9"},{PitchClass::C,Mode::Major}}},
        {"DIATONIC",{{"C","Am","F","G"},{PitchClass::C,Mode::Major}}},
        {"DIM_NOT_PASSING",{{"C","Ddim","Em","G"},{PitchClass::C,Mode::Major}}},
        {"BORROWED",{{"C","Fm","G","C"},{PitchClass::C,Mode::Major}}},
        {"COLOR",{{"Cmaj7","Am7","Dm7","G7"},{PitchClass::C,Mode::Major}}},
        {"SECONDARY",{{"Dm","E7","Am"},{PitchClass::A,Mode::Minor}}},
        {"SUS",{{"Gsus4","G","C"},{PitchClass::C,Mode::Major}}},
        {"Q1",{{"C","E7","Am"},{PitchClass::C,Mode::Major}}},
        {"Q2",{{"C","Am","A7","Dm","G"},{PitchClass::C,Mode::Major}}}
    };
    struct Case {
        std::string id,fixture,oracleTrial;
        int numerator{4},denominator{4},semitones{},octaves{};
        double bpm{120.};
        std::vector<double> durations;
        bool open{},midiRoundTrip{true};
        std::optional<PhraseIntent> requestedIntent;
    };
    std::vector<Case> cases;
    const auto add=[&](std::string fixture,int n,int d,std::string variant,std::vector<double> durations,
                       bool open=false,double bpm=120.,int semitones=0,int octaves=0,
                       std::optional<PhraseIntent> intent={},std::string explicitId={}) {
        std::ostringstream trial;trial<<fixture<<'_'<<n<<'-'<<d<<'_'<<variant;
        if(bpm!=120.)trial<<'_'<<static_cast<int>(bpm)<<"bpm";
        if(semitones)trial<<"_transpose"<<semitones;
        if(octaves)trial<<"_octave"<<octaves;
        const auto id=explicitId.empty()?trial.str():std::move(explicitId);
        cases.push_back({id,std::move(fixture),trial.str(),n,d,semitones,octaves,bpm,
                         std::move(durations),open,true,intent});
    };
    const auto fixed=[](std::size_t size,double qn){return std::vector<double>(size,qn);};
    for(const auto& [name,fixture]:fixtures)if(name!="Q1"&&name!="Q2")add(name,4,4,"fixed-4qn",fixed(fixture.names.size(),4.));
    add("R01",3,4,"fixed-4qn",fixed(4,4.));
    add("R01",7,8,"fixed-4qn",fixed(4,4.));
    add("B08",5,4,"fixed-4qn",fixed(4,4.));
    add("B11",7,8,"fixed-4qn",fixed(3,4.));
    add("DIATONIC",6,8,"fixed-4qn",fixed(4,4.));
    add("SUS",9,8,"fixed-4qn",fixed(3,4.));
    add("R01",4,4,"open",fixed(4,4.),true);
    add("R02_CMAJOR",4,4,"R02-3-5-4",{3.,5.,4.});
    add("R02_AMINOR",4,4,"R02-open",{4.,4.,4.},true);
    add("R02_CMAJOR",4,4,"R02-color-intent-fixed",{4.,4.,4.},false,120.,0,0,PhraseIntent::Color);
    cases.push_back({"B11_PLAIN_C_DB_C","B11","",4,4,0,0,120.,{4,4,4},false,false,{}});
    cases.push_back({"R01_2QN_OPEN","R01","",4,4,0,0,120.,{2,2,2,2},true,true,{}});
    add("R01",4,4,"fixed-4qn",fixed(4,4.),false,90.);
    add("R01",4,4,"equal-seconds-2qn",fixed(4,2.),false,90.);
    add("R01",4,4,"transposed",fixed(4,4.),false,120.,2);
    cases.push_back({"R01_OMIT_DIM","R01","",4,4,0,0,120.,{4,4,4},true,false,{}});
    // Two explicit negative controls are intentionally outside the archived exact-input oracle.
    cases.push_back({"R01_CONFLICT_BASS","R01","",4,4,0,0,120.,{4,4,4,4},true,true,{}});
    cases.push_back({"R01_NON_DIM_CHROMATIC","R01","",4,4,0,0,120.,{4,4,4,4},true,true,{}});
    cases.push_back({"INTENT_RESOLVE_R01","R01","",4,4,0,0,120.,{4,4,4,4},true,true,PhraseIntent::Resolve});
    cases.push_back({"INTENT_DEVELOP_B08","B08","",4,4,0,0,120.,{4,4,4,4},false,true,PhraseIntent::Develop});
    cases.push_back({"INTENT_LOOP_R02","R02_CMAJOR","",4,4,0,0,120.,{4,4,4},true,true,PhraseIntent::Loop});
    // Exact Phase 5.1 worker inputs retained for the archived Abstract OFF/ON comparison.
    cases.push_back({"Q1_PHASE51","Q1","",4,4,0,0,120.,{4,4,4},false,false,{}});
    cases.push_back({"Q2_PHASE51","Q2","",4,4,0,0,120.,{4,2,1,2,2},false,false,{}});
    require(cases.size()==35,"Phase6 freezes 35 cases within the requested 24-36 range");

    const auto groupName=[](std::size_t group){constexpr const char* names[]{"RESOLVE","DEVELOP","LOOP","COLOR"};return group<4?names[group]:"UNKNOWN";};
    const auto safe=[](std::string value){for(auto& c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))c='_';return value;};
    const auto joinIds=[](const std::vector<TemplateID>& ids){std::string out;for(const auto& id:ids){if(!out.empty())out+=',';out+=id;}return out;};
    const auto suffixText=[](const std::vector<ConcreteChordEvent>& events){std::ostringstream out;out<<std::fixed<<std::setprecision(6);for(std::size_t i=0;i<events.size();++i){if(i)out<<" | ";out<<events[i].label<<'@'<<events[i].durationQN<<"QN";}return out.str();};
    const auto sourceText=[](const Progression& progression){std::ostringstream out;out<<std::fixed<<std::setprecision(6);for(std::size_t i=0;i<progression.size();++i){if(i)out<<" | ";out<<progression[i].name<<'@'<<progression[i].durationQN.value_or(4.)<<"QN";}return out.str();};
    const auto labelsText=[](const Progression& progression){std::string out;for(const auto& event:progression){if(!out.empty())out+='|';out+=event.name;}return out;};
    const auto splitTabs=[](const std::string& line){std::vector<std::string> fields;std::size_t start{};for(;;){const auto end=line.find('\t',start);fields.push_back(line.substr(start,end==std::string::npos?end:end-start));if(end==std::string::npos)break;start=end+1;}return fields;};
    const auto rankMatch=[&](const RecommendationSet& rec,const TemplateID& id){
        struct Rank {int group{-1},rank{-1};bool visible{};std::string candidateId,primaryId,suffix;};Rank result;
        const auto shown=session::presentationIndices(rec,{});
        for(int pass=0;pass<2;++pass)for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t r=0;r<rec.groups[g].size();++r){const auto& candidate=rec.groups[g][r];
            const bool hit=pass==0?candidate.primaryTemplate==id:std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),id)!=candidate.supportingTemplates.end();
            if(!hit)continue;
            result={static_cast<int>(g),static_cast<int>(r+1),std::find(shown[g].begin(),shown[g].end(),r)!=shown[g].end(),candidate.id,candidate.primaryTemplate,suffixText(candidate.continuation)};return result;
        }return result;
    };
    const auto findMatch=[](const RecommendationSet& rec,const TemplateID& id)->const MatchResult*{const auto it=std::find_if(rec.matches.begin(),rec.matches.end(),[&](const auto& m){return m.templateId==id;});return it==rec.matches.end()?nullptr:&*it;};
    const auto checkVoicing=[&](const preview::Sequence& whole,const midi::ReadResult& decoded,bool suffixOnly){
        require(bool(decoded)&&!decoded.file.notes.empty(),"Phase6 MIDI parses and contains notes");
        require(!whole.events.empty()&&(!suffixOnly||whole.recommendationBoundary<whole.events.size()),"Phase6 MIDI source preview has a continuation boundary");
        preview::ChordVoicer voicer;const double firstSuffix=whole.events[whole.recommendationBoundary].startQN;
        for(std::size_t i=0;i<whole.events.size();++i){const auto& event=whole.events[i];const auto voiced=voicer.voice(event.chord,event.topVoice);if(suffixOnly&&i<whole.recommendationBoundary)continue;
            std::vector<int> expected{voiced.bass};for(int j=0;j<voiced.upperCount;++j)expected.push_back(voiced.upper[j]);std::vector<int> actual;
            const double start=event.startQN-(suffixOnly?firstSuffix:0.);
            for(const auto& note:decoded.file.notes)if(closeQN(note.startQN,start)&&closeQN(note.durationQN,event.durationQN))actual.push_back(note.pitch);
            std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());require(actual==expected,"Phase6 MIDI bass and upper voices equal preview voicing");
        }
    };
    struct Run {Case test;Progression progression;std::array<plugin::WorkerResult,3> results;};
    std::vector<Run> runs;runs.reserve(cases.size());
    plugin::RecommendationWorker worker(factoryPath,out/"isolated-user.db",out/"isolated-factory-store");
    auto db=library::loadFactory(factoryPath);require(bool(db)&&db.templates.size()==657,"Phase6 reads frozen V4/657 Factory");CandidateIndex continuousIndex(db.templates);
    const std::array<plugin::DevelopmentAlgorithmPreset,3> presets{plugin::DevelopmentAlgorithmPreset::Legacy,
        plugin::DevelopmentAlgorithmPreset::HarmonyFirst,plugin::DevelopmentAlgorithmPreset::Abstract};
    std::ofstream caseFile(out/"cases.tsv"),matchFile(out/"matches.tsv"),candidateFile(out/"top3.tsv"),continuousFile(out/"continuous-control.tsv"),performanceFile(out/"performance.tsv"),
        traceFile(out/"traces.txt"),midiFile(out/"midi-index.tsv"),colorFile(out/"color-ranking.tsv"),colorExportFile(out/"color-export.tsv"),
        ablationFile(out/"ablation.tsv"),inputOracleFile(out/"input-oracle-comparison.tsv"),oracleFile(out/"oracle-comparison.tsv"),phase51OracleFile(out/"phase51-oracle-comparison.tsv"),
        blindAnswer(out/"answer-key"/"answer-key.tsv"),blindInstructions(out/"blind"/"listening-instructions.txt"),
        blindMidiCheck(out/"blind-midi-readback.tsv"),r02RuntimeFile(out/"r02-runtime.tsv"),r01V5File(out/"r01-v5-isolated.tsv"),phase51MidiFile(out/"phase51-midi-oracle.tsv");
    require(caseFile&&matchFile&&candidateFile&&continuousFile&&performanceFile&&traceFile&&midiFile&&colorFile&&colorExportFile&&ablationFile&&inputOracleFile&&oracleFile&&phase51OracleFile&&blindAnswer&&blindInstructions&&blindMidiCheck&&r02RuntimeFile&&r01V5File&&phase51MidiFile,
        "Phase6 TSV and review files open");
    caseFile<<"case\toracleTrial\tfixture\tinput\tkey\tmeter\tbpm\tQN\tOPEN\trequestedIntent\timportRoute\n";
    matchFile<<"case\tpreset\tfactoryVersion\tfactorySha256\tFactoryID\tdisposition\treasons\tsimilarity\trawCost\tfullHarmony\tskeletonHarmony\trhythmSimilarity\trankingScore\teligibilityScore\tmatchWindow\tcontinuationStart\tcontinuationLength\tgroup\trank\tdefaultPresented\tcandidateID\tfullSuffix\talignmentTrace\n";
    candidateFile<<"case\tpreset\tfactoryVersion\tfactorySha256\tgroup\trank\tdefaultPresented\tcandidateID\tprimaryID\tsupportingIDs\tintent\tscore\teligibilityScore\tsimilarity\tfullPath\tfullSuffix\tfullMidi\tsuffixMidi\n";
    continuousFile<<"case\tgroup\trank\tcandidateID\tprimaryID\tintent\tscore\tsimilarity\tfullSuffix\tmatchCount\tpathPassed\tscorePassed\n";
    performanceFile<<"case\tpreset\tcacheState\tcomputationMs\tanalysisReused\tfactoryVersion\tfactorySha256\n";
    midiFile<<"case\tpreset\tgroup\trank\tcandidateID\tprimaryID\tscope\tpath\tnoteCount\tvoicingCheck\n";
    colorFile<<"case\tpreset\tgroup\tpreference\tpostDefaultRank\tcandidateID\tpreRank\tdefaultBefore\tdefaultAfter\tcost\treason\tbeforeDefaultIDs\tafterDefaultIDs\n";
    colorExportFile<<"case\tpreset\tgroup\tpreference\tbeforeDefaultIDs\tafterDefaultIDs\tselectedCandidateID\tselectedPrimaryID\tpreviewCandidateID\tfullMidi\tsuffixMidi\tvoicingCheck\n";
    ablationFile<<"case\tparameter\tdefault\tablated\tFactoryID\tdefaultScore\tablatedScore\tdelta\tdefaultDisposition\tablatedDisposition\tdefaultRank\tablatedRank\trankDelta\tminimumMatch\tminimumScore\tstatus\n";
    inputOracleFile<<"case\toracleTrial\toldMeter\tnewMeter\toldBPM\tnewBPM\toldOPEN\tnewOPEN\toldQN\tnewQN\toldInput\tnewInput\toldKey\tnewKey\tstatus\n";
    oracleFile<<"case\tpreset\tcomparisonScope\tbuild\tFactoryID\toldDisposition\tnewDisposition\toldMatchKey\tnewMatchKey\toldSimilarity\tnewSimilarity\toldRankingScore\tnewRankingScore\toldEligibilityScore\tnewEligibilityScore\toldRankGroup\tnewRankGroup\toldRank\tnewRank\toldRetained\tnewRetained\tnewDefaultPresented\toldSuffix\tnewSuffix\tsuffixSemanticEqual\tstatus\n";
    phase51OracleFile<<"case\tpreset\tcomparisonScope\tpair\tFactoryID\toldDisposition\tnewDisposition\toldSimilarity\tnewSimilarity\toldRawCost\tnewRawCost\toldPrimaryID\tnewPrimaryID\toldRank\tnewRank\toldScore\tnewScore\tstatus\n";
    blindAnswer<<"pair\tcase\tscope\talias\tpreset\tcandidateID\tprimaryID\tcontinuation\twav\tfullMidi\tsuffixMidi\n";
    blindMidiCheck<<"pair\talias\tscope\tmidiPath\tnoteCount\tdecodedVoicing\n";
    r02RuntimeFile<<"cell\tkeySetting\tdurationsQN\tOPEN\tpreset\tselectedAnalysisKey\tfactoryVersion\tfactorySha256\tmatchFound\tmatchKey\tsimilarity\trawCost\trankingScore\teligibilityScore\tactiveGateScore\tdisposition\tsuffixStageReached\tvalidCompleteSuffix\tsuffix\tgroup\tworkerRank\tdefaultCard\tminimumMatch\tminimumScore\n";
    r01V5File<<"factoryPath\tfactoryVersion\tfactorySha256\tfactoryCount\tQN\tmeter\tOPEN\tpreset\tselectedAnalysisKey\tinputDurationsQN\ttemplateID\tmatchFound\tmatchKey\tsimilarity\trankingScore\teligibilityScore\tactiveGateScore\tdisposition\teligible\tcardCandidateID\tprimaryID\tgroup\tworkerRank\tdefaultCard\tfullPath\tfullSuffix\tsuffixDurationsQN\tsnapshotSuffixPreserved\tfullMidi\tsuffixMidi\tfullDecodedLowestNotes\tsuffixDecodedLowestNotes\tvoicingCheck\n";
    phase51MidiFile<<"pair\tabstract\tscope\tpreset\tcomparisonScope\tfactoryVersion\tfactorySha256\tmatchedTemplateID\tselectedCardPrimaryID\tworkerRank\toldMidiPath\truntimeMidiPath\tbytesEqual\tbyteComparisonNote\tnoteEventsEqual\ttempoEqual\toldNoteCount\truntimeNoteCount\toldTempoBPM\truntimeTempoBPM\toldVoicing\truntimeVoicing\tstatus\n";
    blindInstructions<<"Phase 6 anonymized preview pairs. Full WAVs and suffix excerpts are rule-based built-in PreviewSynth renders at 48 kHz and the case tempo; they are not recordings or GM-patch renders. Each pair uses the same source progression and renderer. Pair aliases A/B and their files are not mapped to a preset here; the answer key is stored separately under ../answer-key/answer-key.tsv. Automatic scores describe implementation output only and do not rate musical quality.\n";
    caseFile<<std::setprecision(9);matchFile<<std::setprecision(9);candidateFile<<std::setprecision(9);continuousFile<<std::setprecision(9);
    const auto makeSequence=[&](const Case& c,const Fixture& fixture){
        auto rawProgression=phrase(fixture.names,c.durations,c.open);
        if(c.id=="B11_PLAIN_C_DB_C")rawProgression=phrase({"C","Db","C"},c.durations,c.open);
        if(c.id=="R01_CONFLICT_BASS")rawProgression=phrase({"Am","G#dim","G","D/A"},{4,4,4,4},true);
        if(c.id=="R01_NON_DIM_CHROMATIC")rawProgression=phrase({"Am","G#","G","D/F#"},{4,4,4,4},true);
        if(c.id=="R01_OMIT_DIM")rawProgression=phrase({"Am","G","D/F#"},{4,4,4},true);
        if(!c.midiRoundTrip)return rawProgression;
        midi::ExportSequence sequence;sequence.totalQN=rawProgression.back().startQN+rawProgression.back().durationQN.value_or(4.);sequence.chordCount=rawProgression.size();sequence.tempoBPM=c.bpm;sequence.meter={c.numerator,c.denominator};
        for(const auto& event:rawProgression){const auto tones=chordPitches(event);const int root=(tones.root+c.semitones+120)%12,bass=(tones.bass+c.semitones+120)%12,octave=c.octaves*12;
            sequence.notes.push_back({36+bass+octave,event.startQN,event.durationQN.value_or(4.),90,0,true});
            for(int n=0;n<12;++n)if(tones.intervals&(1<<n))sequence.notes.push_back({60+(root+n)%12+octave,event.startQN,event.durationQN.value_or(4.),80,0,false});}
        const auto bytes=midi::writeToMemory(sequence);require(bool(bytes),"Phase6 fixed-case MIDI encodes");const auto raw=midi::readFromMemory(bytes.bytes);require(bool(raw),"Phase6 fixed-case MIDI parses");
        midi::MidiHarmonyExtractionConfig config;config.openEnded=c.open;const auto extraction=midi::extractHarmony(raw.file,config);require(bool(extraction)&&extraction.chords.size()==fixture.names.size(),"Phase6 fixed-case MIDI recognition returns expected chord count");return extraction.chords;
    };
    const auto presetIndex=[](plugin::DevelopmentAlgorithmPreset p){return p==plugin::DevelopmentAlgorithmPreset::Legacy?0u:p==plugin::DevelopmentAlgorithmPreset::HarmonyFirst?1u:2u;};
    std::set<std::string> nonExactCoverageCases;std::size_t performanceRows{};bool coldWorkerSample=true;
    for(const auto& test:cases){
        const auto fit=fixtures.find(test.fixture);require(fit!=fixtures.end(),"Phase6 fixture exists");auto progression=makeSequence(test,fit->second);
        if(test.id=="Q1_PHASE51")progression=phrase({"C","E7","Am"},{4,4,4});
        if(test.id=="Q2_PHASE51")progression=phrase({"C","Am","A7","Dm","G"},{4,2,1,2,2});
        AnalysisContext base;base.forcedKey=fit->second.key;if(test.semitones&&base.forcedKey)base.forcedKey->tonic=static_cast<PitchClass>((static_cast<int>(base.forcedKey->tonic)+test.semitones+12)%12);
        base.timeSigNumerator=test.numerator;base.timeSigDenominator=test.denominator;
        if(test.id=="Q1_PHASE51"||test.id=="Q2_PHASE51")base.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        RecommendationRequest request;request.preferredIntent=test.requestedIntent;
        const auto input=sourceText(progression);caseFile<<test.id<<'\t'<<test.oracleTrial<<'\t'<<test.fixture<<'\t'<<input<<'\t'<<(base.forcedKey?formatKey(*base.forcedKey):"Auto")<<'\t'<<test.numerator<<'/'<<test.denominator<<'\t'<<test.bpm<<'\t';double totalQN{};for(const auto& e:progression)totalQN+=e.durationQN.value_or(4.);caseFile<<totalQN<<'\t'<<test.open<<'\t'<<(test.requestedIntent?intentName(*test.requestedIntent):"neutral")<<'\t'<<(test.midiRoundTrip?"MIDI roundtrip":"direct structured input")<<'\n';
        Run run;run.test=test;run.progression=progression;
        for(const auto preset:presets){const auto pi=presetIndex(preset);worker.submitPreset(progression,base,preset,request,false,pi+1);auto result=wait(worker);
            require(result.algorithmPreset&&*result.algorithmPreset==preset,"Phase6 worker returns the explicitly requested preset");
            require(result.factoryCount==657&&result.factoryVersion==4&&result.factorySha256==factoryHash,"Phase6 worker confirms exact frozen V4/657 Factory metadata");
            require(std::isfinite(result.computationMs)&&result.computationMs>=0.,"Phase6 worker provides a finite computation time");
            performanceFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<(coldWorkerSample?"cold_factory_load_and_sha256":"hot_loaded_index")<<'\t'
                <<std::setprecision(9)<<result.computationMs<<'\t'<<result.analysisReused<<'\t'<<result.factoryVersion<<'\t'<<result.factorySha256<<'\n';
            coldWorkerSample=false;++performanceRows;
            const auto weights=plugin::developmentRecommendationWeights(preset);require(weights.minimumMatch==0.60f&&weights.minimumScore==60.f&&weights.perGroup==10&&!weights.diminishedPassingInsertionException,"Phase6 preset retains existing gates and only the documented preset flags");
            const auto& rec=result.recommendations;require(rec.stats.shortlistSize==657&&rec.groups.size()==4,"Phase6 worker scans the V4 library and emits four intent groups");
            const auto shown=session::presentationIndices(rec,{});
            for(std::size_t m=0;m<rec.matches.size();++m){const auto& match=rec.matches[m];const auto rank=rankMatch(rec,match.templateId);
                if(std::any_of(match.alignmentTrace.begin(),match.alignmentTrace.end(),[](const auto& step){return step.operation!=AlignmentOp::Match;}))nonExactCoverageCases.insert(test.id);
                matchFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<result.factoryVersion<<'\t'<<result.factorySha256<<'\t'<<match.templateId<<'\t'<<match.disposition<<'\t'<<phase5ReasonString(match)<<'\t'<<match.similarity<<'\t'<<match.rawCost<<'\t'<<match.subScores.fullHarmony<<'\t'<<match.subScores.skeletonHarmony<<'\t'<<match.subScores.rhythmSimilarity<<'\t'<<match.candidateRankingScore<<'\t'<<match.candidateEligibilityScore<<'\t'<<match.templateMatchStart<<'-'<<match.templateMatchEnd<<'\t'<<match.continuationStartIndex<<'\t'<<match.continuationLength<<'\t'<<(rank.group<0?"NA":groupName(rank.group))<<'\t'<<rank.rank<<'\t'<<rank.visible<<'\t'<<rank.candidateId<<'\t'<<rank.suffix<<'\t';
                for(std::size_t s=0;s<match.alignmentTrace.size();++s){if(s)matchFile<<'|';const auto& step=match.alignmentTrace[s];matchFile<<(step.queryIndex?std::to_string(*step.queryIndex):"-")<<':'<<(step.templateIndex?std::to_string(*step.templateIndex):"-")<<':'<<alignmentOpName(step.operation);}matchFile<<'\n';}
            traceFile<<"\nCASE="<<test.id<<" PRESET="<<plugin::algorithmPresetName(preset)<<" FACTORY=v"<<result.factoryVersion<<"/"<<result.factorySha256<<session::recommendationDiagnostics(progression,plugin::developmentContext(base,preset),request,rec);
            midi::CandidateExportContext exportContext;exportContext.imported=imported(progression);exportContext.tempo=test.bpm;exportContext.meter={test.numerator,test.denominator};exportContext.mode=midi::ArrangementMode::VoiceLed;
            const auto builtFirst=[&](const ContinuationCandidate& c){return preview::buildSequence(exportContext.imported,&c,test.bpm);};
            for(std::size_t g=0;g<rec.groups.size();++g)for(std::size_t rank=0;rank<std::min<std::size_t>(3,rec.groups[g].size());++rank){const auto& candidate=rec.groups[g][rank];const bool defaultPresented=std::find(shown[g].begin(),shown[g].end(),rank)!=shown[g].end();
                const auto stem=safe(test.id+"_"+plugin::algorithmPresetName(preset)+"_"+groupName(g)+"_r"+std::to_string(rank+1));const auto fullPath=out/"midi"/(stem+"_full.mid"),suffixPath=out/"midi"/(stem+"_suffix.mid");
                auto fullContext=exportContext;fullContext.scope=midi::ExportScope::FullPhrase;auto fullPayload=midi::candidatePayload(fullContext,candidate);require(bool(fullPayload),"Phase6 top3 full-phrase MIDI exports");
                auto suffixContext=exportContext;suffixContext.scope=midi::ExportScope::ContinuationOnly;auto suffixPayload=midi::candidatePayload(suffixContext,candidate);require(bool(suffixPayload),"Phase6 top3 suffix-only MIDI exports");
                const auto previewBuilt=builtFirst(candidate);require(bool(previewBuilt)&&previewBuilt.sequence.recommendationBoundary==progression.size(),"Phase6 top3 preview preserves input boundary");
                const auto fullRead=midi::readFromMemory(fullPayload.payload.smfBytes),suffixRead=midi::readFromMemory(suffixPayload.payload.smfBytes);checkVoicing(previewBuilt.sequence,fullRead,false);checkVoicing(previewBuilt.sequence,suffixRead,true);
                std::string error;require(midi::writeToFile(fullPayload.payload.smfBytes,fullPath,error),"Phase6 top3 full MIDI writes");error.clear();require(midi::writeToFile(suffixPayload.payload.smfBytes,suffixPath,error),"Phase6 top3 suffix MIDI writes");
                const auto suffix=suffixText(candidate.continuation);candidateFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<result.factoryVersion<<'\t'<<result.factorySha256<<'\t'<<groupName(g)<<'\t'<<rank+1<<'\t'<<defaultPresented<<'\t'<<candidate.id<<'\t'<<candidate.primaryTemplate<<'\t'<<joinIds(candidate.supportingTemplates)<<'\t'<<intentName(candidate.intent)<<'\t'<<candidate.rankingScore<<'\t'<<candidate.eligibilityScore<<'\t'<<candidate.matchSimilarity<<'\t'<<input<<" -> "<<suffix<<'\t'<<suffix<<'\t'<<fullPath.string()<<'\t'<<suffixPath.string()<<'\n';
                midiFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<groupName(g)<<'\t'<<rank+1<<'\t'<<candidate.id<<'\t'<<candidate.primaryTemplate<<"\tfull\t"<<fullPath.string()<<'\t'<<fullRead.file.notes.size()<<"\tPASS\n";
                midiFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<groupName(g)<<'\t'<<rank+1<<'\t'<<candidate.id<<'\t'<<candidate.primaryTemplate<<"\tsuffix\t"<<suffixPath.string()<<'\t'<<suffixRead.file.notes.size()<<"\tPASS\n";
            }
            // Mirror the UI: sort the existing default-visible indices inside each intent group,
            // then apply the preference order only to those visible cards.
            const auto sourceTimed=color::HarmonyColorAnalyzer::prepare(progression);
            const std::array<color::Preference,6> preferences{color::Preference::Off,color::Preference::ContinueSource,color::Preference::Warmer,color::Preference::Cooler,color::Preference::TensionArc,color::Preference::TensionWarmClose};
            const bool selectedForMidi=test.id=="R01_4-4_fixed-4qn"||test.id=="B11_4-4_fixed-4qn"||test.id=="R02_CMAJOR_4-4_fixed-4qn";
            for(std::size_t colorGroup=0;colorGroup<rec.groups.size();++colorGroup){const auto& groupCandidates=rec.groups[colorGroup];std::vector<color::PathColor> paths;
                for(const auto& candidate:groupCandidates)paths.push_back(color::HarmonyColorAnalyzer::analyzeContinuation(sourceTimed,candidate));
                const std::set<std::size_t> visibleBefore(shown[colorGroup].begin(),shown[colorGroup].end());std::string beforeDefaultIds;
                for(const auto index:shown[colorGroup]){require(index<groupCandidates.size(),"MainView presentation indices refer to a card in this group");if(!beforeDefaultIds.empty())beforeDefaultIds+=',';beforeDefaultIds+=groupCandidates[index].id;}
                for(const auto preference:preferences){const auto ranked=color::ColorPreferenceReranker::rank(sourceTimed,result.analysis.keyCandidates,
                        result.analysis.selectedKey?std::optional<KeySignature>{result.analysis.selectedKey->key}:std::nullopt,groupCandidates,paths,preference);
                    std::set<std::size_t> allIndices;std::vector<std::size_t> afterVisibleIndices;std::string afterDefaultIds;
                    for(const auto index:ranked.order){require(index<groupCandidates.size()&&allIndices.insert(index).second,"Color reranker returns each valid group index once");
                        if(visibleBefore.count(index)){afterVisibleIndices.push_back(index);if(!afterDefaultIds.empty())afterDefaultIds+=',';afterDefaultIds+=groupCandidates[index].id;}}
                    require(allIndices.size()==groupCandidates.size(),"Color reranker returns every candidate in the current intent group");
                    for(std::size_t position=0;position<ranked.order.size();++position){const auto original=ranked.order[position];const auto visibleRank=std::find(afterVisibleIndices.begin(),afterVisibleIndices.end(),original);
                        const bool visibleAfter=visibleRank!=afterVisibleIndices.end();const auto postRank=visibleAfter?static_cast<std::size_t>(std::distance(afterVisibleIndices.begin(),visibleRank))+1:0;
                        const auto& decision=ranked.decisions[original];colorFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<groupName(colorGroup)<<'\t'
                            <<color::preferenceKey(preference)<<'\t'<<postRank<<'\t'<<groupCandidates[original].id<<'\t'<<original+1<<'\t'
                            <<visibleBefore.count(original)<<'\t'<<visibleAfter<<'\t'<<(decision.cost?std::to_string(*decision.cost):"NA")<<'\t'
                            <<color::rankReasonKey(decision.reason)<<'\t'<<beforeDefaultIds<<'\t'<<afterDefaultIds<<'\n';}
                    if(selectedForMidi){if(!afterVisibleIndices.empty()){const auto& chosen=groupCandidates[afterVisibleIndices.front()];auto fullContext=exportContext;fullContext.scope=midi::ExportScope::FullPhrase;
                            auto fullPayload=midi::candidatePayload(fullContext,chosen);require(bool(fullPayload),"Phase6 preference-selected visible card full MIDI exports");auto suffixContext=exportContext;suffixContext.scope=midi::ExportScope::ContinuationOnly;
                            auto suffixPayload=midi::candidatePayload(suffixContext,chosen);require(bool(suffixPayload),"Phase6 preference-selected visible card suffix MIDI exports");auto built=builtFirst(chosen);require(bool(built),"Phase6 preference-selected visible card previews");
                            const auto fullDecoded=midi::readFromMemory(fullPayload.payload.smfBytes),suffixDecoded=midi::readFromMemory(suffixPayload.payload.smfBytes);checkVoicing(built.sequence,fullDecoded,false);checkVoicing(built.sequence,suffixDecoded,true);
                            const auto stem=safe(test.id+"_"+plugin::algorithmPresetName(preset)+"_"+groupName(colorGroup)+"_"+color::preferenceKey(preference)+"_ranked");
                            const auto fullPath=out/"midi"/(stem+"_full.mid"),suffixPath=out/"midi"/(stem+"_suffix.mid");std::string error;
                            require(midi::writeToFile(fullPayload.payload.smfBytes,fullPath,error),"Phase6 preference-selected full MIDI writes");error.clear();require(midi::writeToFile(suffixPayload.payload.smfBytes,suffixPath,error),"Phase6 preference-selected suffix MIDI writes");
                            colorExportFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<groupName(colorGroup)<<'\t'<<color::preferenceKey(preference)<<'\t'
                                <<beforeDefaultIds<<'\t'<<afterDefaultIds<<'\t'<<chosen.id<<'\t'<<chosen.primaryTemplate<<'\t'<<chosen.id<<'\t'
                                <<fullPath.string()<<'\t'<<suffixPath.string()<<"\tPASS\n";
                        }else colorExportFile<<test.id<<'\t'<<plugin::algorithmPresetName(preset)<<'\t'<<groupName(colorGroup)<<'\t'<<color::preferenceKey(preference)<<'\t'
                            <<beforeDefaultIds<<'\t'<<afterDefaultIds<<"\tNA\tNA\tNA\tNA\tNA\tNO_DEFAULT_VISIBLE_CARD\n";}
                }
            }
            run.results[pi]=std::move(result);
        }
        AnalysisContext continuousContext=base;continuousContext.retrievalTimingMode=RetrievalTimingMode::ContinuousStructure;const auto continuousWeights=plugin::developmentRecommendationWeights(plugin::DevelopmentAlgorithmPreset::Legacy);
        const auto continuous=recommendContinuations(makeMatchQuery(progression,continuousContext),continuousIndex,request,continuousWeights);require(continuous.retrievalTimingMode==RetrievalTimingMode::ContinuousStructure,"ContinuousStructure is exercised through its actual core mode");
        for(std::size_t g=0;g<continuous.groups.size();++g)for(std::size_t r=0;r<std::min<std::size_t>(3,continuous.groups[g].size());++r){const auto& candidate=continuous.groups[g][r];continuousFile<<test.id<<'\t'<<groupName(g)<<'\t'<<r+1<<'\t'<<candidate.id<<'\t'<<candidate.primaryTemplate<<'\t'<<intentName(candidate.intent)<<'\t'<<candidate.rankingScore<<'\t'<<candidate.matchSimilarity<<'\t'<<suffixText(candidate.continuation)<<'\t'<<continuous.matches.size()<<'\t'<<continuous.stats.pathPassed<<'\t'<<continuous.stats.scorePassed<<'\n';}
        runs.push_back(std::move(run));
    }

    // The archived Phase 5.1 R02 target table covered Legacy/HarmonyFirst;
    // this Phase 6 grid uses the production worker for all three presets.
    constexpr std::array<TemplateID,5> r02Targets{
        "V3_SHADE_002","V3_SHADE_008","V3_SHADE_019","V3_SHADE_024","V3_SHADE_031"};
    struct KeySetting {const char* name;std::optional<KeySignature> forced;};
    const std::array<KeySetting,3> keySettings{{
        {"Auto",std::nullopt},{"Cmajor",KeySignature{PitchClass::C,Mode::Major}},
        {"Aminor",KeySignature{PitchClass::A,Mode::Minor}}}};
    const std::array<std::vector<double>,2> r02Durations{{{4.,4.,4.},{3.,5.,4.}}};
    const std::array<const char*,2> r02DurationNames{"4,4,4","3,5,4"};
    std::size_t runtimeRevision=1000,r02CellCount{},r02TargetRows{};
    for(const auto& keySetting:keySettings)for(std::size_t timing=0;timing<r02Durations.size();++timing)
    for(const bool open:{false,true})for(const auto preset:presets) {
        const auto cell=std::string(keySetting.name)+"|"+r02DurationNames[timing]+"|OPEN"+(open?"1":"0")+"|"+plugin::algorithmPresetName(preset);
        const auto progression=phrase({"F","G","Am"},r02Durations[timing],open);
        AnalysisContext context;context.timeSigNumerator=4;context.timeSigDenominator=4;context.forcedKey=keySetting.forced;
        const auto runtimeContext=plugin::developmentContext(context,preset);RecommendationRequest request;
        worker.submitPreset(progression,context,preset,request,false,runtimeRevision++);const auto result=wait(worker);
        require(result.algorithmPreset&&*result.algorithmPreset==preset,"R02 runtime grid returns the requested worker preset");
        require(result.factoryCount==657&&result.factoryVersion==4&&result.factorySha256==factoryHash,"R02 runtime grid confirms the frozen V4 Factory identity");
        const auto weights=plugin::developmentRecommendationWeights(preset);
        require(weights.minimumMatch==0.60f&&weights.minimumScore==60.f,"R02 runtime grid preserves both existing gates");
        const auto& rec=result.recommendations;const auto shown=session::presentationIndices(rec,{});
        const auto query=makeMatchQuery(progression,runtimeContext);
        require(rec.retrievalTimingMode==runtimeContext.retrievalTimingMode,"R02 worker uses the requested preset retrieval route");
        const auto selectedKey=result.analysis.selectedKey?formatKey(result.analysis.selectedKey->key):"UNKNOWN";
        require(result.analysis.selectedKey.has_value(),"R02 runtime cell has a selected analysis key");
        for(const auto& id:r02Targets) {
            ++r02TargetRows;
            const auto item=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& t){return t.id==id;});
            require(item!=db.templates.end()&&item->mode==Mode::Major,"R02 fixed Color target exists as a Major V4 template");
            const auto* match=findMatch(rec,id);
            const ContinuationCandidate* card{};std::size_t cardGroup{},cardRank{};bool defaultCard{};
            for(std::size_t g=0;g<rec.groups.size()&&!card;++g)for(std::size_t r=0;r<rec.groups[g].size();++r) {
                const auto& candidate=rec.groups[g][r];
                if(std::find(candidate.supportingTemplates.begin(),candidate.supportingTemplates.end(),id)==candidate.supportingTemplates.end())continue;
                card=&candidate;cardGroup=g;cardRank=r;defaultCard=std::find(shown[g].begin(),shown[g].end(),r)!=shown[g].end();break;
            }
            std::string suffix;bool suffixStageReached=card!=nullptr;bool validSuffix=card&&!card->continuation.empty();
            if(card)suffix=suffixText(card->continuation);
            else if(match) {
                suffixStageReached=match->disposition=="minimum_score"||match->disposition=="repeated_current_chord"||
                    match->disposition=="melody_constraint"||match->disposition=="candidate_generated"||
                    match->disposition=="equivalent_path_deduplication"||match->disposition=="diversity_score"||
                    match->disposition=="retained_limit"||match->disposition=="TARGET_RETAINED";
                if(suffixStageReached) {
                    const auto rhythm=estimateRhythmScale(*match,query,*item);std::vector<ConcreteChordEvent> realized;
                    for(std::size_t i=match->continuationStartIndex;i<item->full.size();++i)
                        realized.push_back(realizeContinuation(item->full[i],match->key,item->full[i].durationQN.value_or(4.)*rhythm.scale));
                    if(item->loopable) {const auto wrapEnd=match->templateMatchStart==0
                        ?(match->hasContinuation?0:item->full.size()):match->templateMatchStart;
                        for(std::size_t i=0;i<std::min(wrapEnd,item->full.size());++i)
                            realized.push_back(realizeContinuation(item->full[i],match->key,item->full[i].durationQN.value_or(4.)*rhythm.scale));}
                    validSuffix=!realized.empty()&&match->disposition!="repeated_current_chord";suffix=suffixText(realized);
                }
            }
            const float activeScore=match?(rec.retrievalTimingMode==RetrievalTimingMode::HarmonyFirst?match->candidateEligibilityScore:match->candidateRankingScore):-1.f;
            r02RuntimeFile<<cell<<'\t'<<keySetting.name<<'\t'<<r02DurationNames[timing]<<'\t'<<open<<'\t'
                <<plugin::algorithmPresetName(preset)<<'\t'<<selectedKey<<'\t'<<result.factoryVersion<<'\t'<<result.factorySha256<<'\t'
                <<bool(match)<<'\t'<<(match?formatKey(match->key):"NA")<<'\t'<<(match?match->similarity:-1.f)<<'\t'
                <<(match?match->rawCost:-1.f)<<'\t'<<(match?match->candidateRankingScore:-1.f)<<'\t'
                <<(match?match->candidateEligibilityScore:-1.f)<<'\t'<<activeScore<<'\t'
                <<(match?match->disposition:"NO_MATCH_RESULT")<<'\t'<<suffixStageReached<<'\t'<<validSuffix<<'\t'<<(suffix.empty()?"NA":suffix)<<'\t'
                <<(card?groupName(cardGroup):"NA")<<'\t'<<(card?cardRank+1:0)<<'\t'<<defaultCard<<'\t'
                <<weights.minimumMatch<<'\t'<<weights.minimumScore<<'\n';
        }
        ++r02CellCount;
    }
    require(r02CellCount==36&&r02TargetRows==180,"Phase6 R02 runtime covers 36 orthogonal worker cells and five fixed IDs each");
    require(!nonExactCoverageCases.empty(),"Phase6 fixed corpus includes non-exact match alignment coverage");
    require(performanceRows==cases.size()*presets.size(),"Phase6 writes one real WorkerResult performance sample for every case and preset");

    // Historical three-build oracle: exact source cases and original match rows, never regenerated by this implementation.
    struct OldRow {std::vector<std::string> fields;};
    const std::array<std::string,3> oracleFiles{"phase4b-off-matches-selected.tsv","phase4b-hf-on-matches-selected.tsv","phase4b-abstract-matches-selected.tsv"};
    std::array<std::map<std::string,std::map<std::string,OldRow>>,3> oldRows;
    std::map<std::string,std::vector<std::string>> oldCases;
    {std::ifstream input(oracleRoot/"phase4b-off-cases.tsv");if(input){std::string line;std::getline(input,line);while(std::getline(input,line)){auto fields=splitTabs(line);if(fields.size()>=29&&fields[2]=="Legacy")oldCases[fields[0]]=std::move(fields);}}}
    std::size_t inputOracleRows{},inputOracleMismatches{};
    for(const auto& run:runs)if(!run.test.oracleTrial.empty()){
        const auto found=oldCases.find(run.test.oracleTrial);if(found==oldCases.end()){++inputOracleMismatches;inputOracleFile<<run.test.id<<'\t'<<run.test.oracleTrial<<"\tNA\tNA\tNA\t"<<run.test.bpm<<"\tNA\t"<<run.test.open<<"\tNA\tNA\tNA\t"<<labelsText(run.progression)<<"\tNA\tNA\tMISSING_ORACLE_CASE\n";continue;}
        const auto& f=found->second;double qn{};for(const auto& event:run.progression)qn+=event.durationQN.value_or(4.);const auto meter=std::to_string(run.test.numerator)+"/"+std::to_string(run.test.denominator);
        const auto key=run.results[0].analysis.selectedKey?formatKey(run.results[0].analysis.selectedKey->key):"UNKNOWN";const auto intent=run.test.requestedIntent?std::string(intentName(*run.test.requestedIntent)):"neutral";
        const bool same=f[3]==meter&&std::abs(std::stod(f[4])-run.test.bpm)<0.01&&f[5]==intent&&((f[7]=="1")==run.test.open)&&std::abs(std::stod(f[8]))<0.001&&std::abs(std::stod(f[9])-qn)<0.001&&f[11]==labelsText(run.progression)&&f[12]==key;
        if(!same)++inputOracleMismatches;++inputOracleRows;inputOracleFile<<run.test.id<<'\t'<<run.test.oracleTrial<<'\t'<<f[3]<<'\t'<<meter<<'\t'<<f[4]<<'\t'<<run.test.bpm<<'\t'<<f[7]<<'\t'<<run.test.open<<'\t'<<f[9]<<'\t'<<qn<<'\t'<<f[11]<<'\t'<<labelsText(run.progression)<<'\t'<<f[12]<<'\t'<<key<<'\t'<<(same?"MATCH":"DIFF")<<'\n';
    }
    require(inputOracleRows==25,"Phase6 verifies all 25 exact inputs against archived Phase4B cases");
    require(inputOracleMismatches==0,"Phase6 exact historical input, intent, meter, QN, OPEN and key fields agree");
    const std::array<std::string,3> oracleModes{"Legacy","HarmonyFirst","Legacy"};
    const std::array<std::string,26> expectedMatchHeader{"trial","mode","meter","bpm","FactoryID","Factory_intent","Factory_cadence","Factory_path","Factory_harmonic_path","key","start_index","end_index","continuation_index","continuation_length","trace","disposition","match_similarity","full_harmony","skeleton_harmony","rhythm_similarity","eligibility_score","ranking_score","rank_group","rank","visible","continuation"};
    std::map<std::string,std::size_t> matchColumn;
    std::size_t stagedMatchRows{};
    std::array<std::size_t,3> stagedRowsByBuild{};
    for(std::size_t b=0;b<oracleFiles.size();++b){std::ifstream input=std::ifstream(oracleRoot/oracleFiles[b]);require(bool(input),"Phase6 opens each staged historical match oracle");std::string line;require(bool(std::getline(input,line)),"Phase6 historical match oracle has a header");const auto header=splitTabs(line);
        require(header.size()==expectedMatchHeader.size(),"Phase6 historical match oracle has the reviewed 26-column schema");
        for(std::size_t i=0;i<expectedMatchHeader.size()&&i<header.size();++i){require(header[i]==expectedMatchHeader[i],"Phase6 historical oracle column order matches its header semantics");if(b==0)matchColumn[header[i]]=i;}
        while(std::getline(input,line)){auto fields=splitTabs(line);require(fields.size()==expectedMatchHeader.size(),"Phase6 historical match row has all 26 columns");if(fields.size()!=expectedMatchHeader.size())continue;
            if(fields[matchColumn.at("mode")]!=oracleModes[b])continue;
            const auto trialId=fields[matchColumn.at("trial")],factoryId=fields[matchColumn.at("FactoryID")];
            const bool inserted=oldRows[b][trialId].emplace(factoryId,OldRow{std::move(fields)}).second;
            require(inserted,"Phase6 staged historical oracle has no duplicate trial/FactoryID rows");if(inserted){++stagedMatchRows;++stagedRowsByBuild[b];}
        }
    }
    for(const auto rows:stagedRowsByBuild)require(rows==8643,"Phase6 stages all 8,643 selected historical comparisons for each reviewed build");
    // The old Abstract build's Phase4B direct-core rows use default RecommendationWeights (Abstract OFF).
    // Its Legacy rows are a build-consistency oracle; Phase 5.1 below is the actual Abstract ON oracle.
    const std::array<std::size_t,3> oraclePresetForBuild{0,1,0};const std::array<std::string,3> buildNames{"build-off","build-hf-on","build-abstract"};std::size_t oracleRows{},oracleMismatches{};
    const auto directCoreControl=[&](const Run& run,RetrievalTimingMode mode){
        const auto fixture=fixtures.find(run.test.fixture);require(fixture!=fixtures.end(),"Phase6 direct-core historical control fixture exists");
        AnalysisContext context;context.forcedKey=fixture->second.key;
        if(context.forcedKey&&run.test.semitones)context.forcedKey->tonic=static_cast<PitchClass>((static_cast<int>(context.forcedKey->tonic)+run.test.semitones+12)%12);
        context.timeSigNumerator=run.test.numerator;context.timeSigDenominator=run.test.denominator;context.retrievalTimingMode=mode;
        RecommendationRequest request;request.preferredIntent=run.test.requestedIntent;RecommendationWeights weights;
        require(weights.minimumMatch==0.60f&&weights.minimumScore==60.f&&weights.perGroup==3&&
            !weights.diminishedPassingInsertionException&&!weights.abstractChordCompatibility,
            "Historical Phase4B direct-core control retains default cap-3 weights and 0.60/60 gates");
        return recommendContinuations(makeMatchQuery(run.progression,context),continuousIndex,request,weights);
    };
    const auto suffixEquivalent=[](const std::string& archived,const std::string& current){
        const auto trim=[](std::string value){const auto first=value.find_first_not_of(" \t\r\n");if(first==std::string::npos)return std::string{};const auto last=value.find_last_not_of(" \t\r\n");return value.substr(first,last-first+1);};
        const auto parse=[&](const std::string& text,std::vector<std::pair<std::string,double>>& events){
            const auto cleaned=trim(text);if(cleaned.empty())return true;std::istringstream input(cleaned);std::string token;
            while(std::getline(input,token,'|')){token=trim(std::move(token));const auto at=token.rfind('@');if(at==std::string::npos)return false;
                const auto label=trim(token.substr(0,at));auto duration=trim(token.substr(at+1));if(label.empty())return false;
                if(duration.size()>=2&&duration.compare(duration.size()-2,2,"QN")==0)duration.resize(duration.size()-2);
                try{std::size_t used{};const double qn=std::stod(duration,&used);if(used!=duration.size()||!std::isfinite(qn))return false;events.emplace_back(label,qn);}catch(...){return false;}}
            return true;
        };
        std::vector<std::pair<std::string,double>> oldEvents,newEvents;if(!parse(archived,oldEvents)||!parse(current,newEvents)||oldEvents.size()!=newEvents.size())return false;
        for(std::size_t i=0;i<oldEvents.size();++i)if(oldEvents[i].first!=newEvents[i].first||std::abs(oldEvents[i].second-newEvents[i].second)>1e-6)return false;
        return true;
    };
    std::size_t expectedOracleRows{};
    for(const auto& run:runs)if(!run.test.oracleTrial.empty()){
        const auto legacyDirect=directCoreControl(run,RetrievalTimingMode::Legacy);
        const auto harmonyFirstDirect=directCoreControl(run,RetrievalTimingMode::HarmonyFirst);
        for(std::size_t b=0;b<3;++b){const auto p=oraclePresetForBuild[b];const auto& rows=oldRows[b][run.test.oracleTrial];
        require(!rows.empty(),"Phase6 historical oracle includes every exact trial in all three baseline builds");expectedOracleRows+=rows.size();const auto& rec=b==1?harmonyFirstDirect:legacyDirect;
        for(const auto& [id,old]:rows){const auto* now=findMatch(rec,id);const auto rank=rankMatch(rec,id);const auto& f=old.fields;const auto field=[&](const char* name)->const std::string&{return f[matchColumn.at(name)];};
            const auto nearFloat=[&](const char* name,float value,float tol){try{return std::abs(std::stof(field(name))-value)<=tol;}catch(...){return false;}};
            const bool present=now!=nullptr;const int nowRank=rank.rank<0?0:rank.rank;
            const bool disposition=present&&field("disposition")==now->disposition;
            const bool matchKey=present&&field("key")==formatKey(now->key);
            const bool similarity=present&&nearFloat("match_similarity",now->similarity,0.00001f);
            const bool rankingScore=present&&nearFloat("ranking_score",now->candidateRankingScore,0.0001f);
            const bool eligibilityScore=present&&nearFloat("eligibility_score",now->candidateEligibilityScore,0.0001f);
            const bool group=(field("rank_group")=="-"?rank.group<0:(rank.group>=0&&field("rank_group")==groupName(static_cast<std::size_t>(rank.group))));
            const bool retained=(field("visible")=="1")== (rank.group>=0);const bool rankSame=std::stoi(field("rank"))==nowRank;
            const bool suffix=suffixEquivalent(field("continuation"),rank.suffix);
            const bool starts=present&&std::stoi(field("start_index"))==now->templateMatchStart&&std::stoi(field("end_index"))==now->templateMatchEnd&&
                std::stoi(field("continuation_index"))==now->continuationStartIndex&&std::stoi(field("continuation_length"))==now->continuationLength;
            const bool equal=present&&disposition&&matchKey&&similarity&&rankingScore&&eligibilityScore&&group&&retained&&rankSame&&suffix&&starts;
            if(!equal)++oracleMismatches;++oracleRows;
            oracleFile<<run.test.id<<'\t'<<plugin::algorithmPresetName(presets[p])<<"\tdirect_core_cap3\t"<<buildNames[b]<<'\t'<<id<<'\t'<<field("disposition")<<'\t'<<(present?now->disposition:"MISSING")<<'\t'<<field("key")<<'\t'<<(present?formatKey(now->key):"NA")<<'\t'
                <<field("match_similarity")<<'\t'<<(present?std::to_string(now->similarity):"NA")<<'\t'<<field("ranking_score")<<'\t'<<(present?std::to_string(now->candidateRankingScore):"NA")<<'\t'
                <<field("eligibility_score")<<'\t'<<(present?std::to_string(now->candidateEligibilityScore):"NA")<<'\t'<<field("rank_group")<<'\t'<<(rank.group<0?"-":groupName(static_cast<std::size_t>(rank.group)))<<'\t'
                <<field("rank")<<'\t'<<nowRank<<'\t'<<field("visible")<<'\t'<<(rank.group>=0)<<'\t'<<rank.visible<<'\t'<<field("continuation")<<'\t'<<rank.suffix<<'\t'<<suffix<<'\t'<<(equal?"MATCH":"DIFF")<<'\n';
        }
        }
    }
    require(oracleRows==expectedOracleRows&&oracleRows==25929&&stagedMatchRows==25929,
        "Phase6 compares all 25,929 selected historical match rows; missing trials or rows cannot pass as present");
    require(oracleMismatches==0,"Phase6 exact-input direct-core cap-3 controls agree with all three historical build oracles");

    // Phase 5.1 selected Abstract comparisons use the archived Q1/Q2 rows as their independent direct-core oracle.
    const auto abstractFile=oracleRoot/"phase51"/"abstract-comparison.tsv";std::size_t phase51Rows{},phase51Diffs{},phase51MidiRows{},phase51ExactByteRows{};
    {std::ifstream input(abstractFile);require(bool(input),"Phase6 opens archived Phase5.1 selected-card table");std::string line;require(bool(std::getline(input,line)),"Phase5.1 selected-card table has a header");while(std::getline(input,line)){const auto f=splitTabs(line);require(f.size()==21,"Phase5.1 selected-card rows preserve all 21 named fields");const std::string caseId=f[0]=="Q1"?"Q1_PHASE51":f[0]=="Q2"?"Q2_PHASE51":"";require(!caseId.empty(),"Phase5.1 selected-card oracle contains only Q1/Q2 targets");
            const auto runIt=std::find_if(runs.begin(),runs.end(),[&](const auto& r){return r.test.id==caseId;});require(runIt!=runs.end(),"Phase5.1 target exists in the fixed Phase6 corpus");const auto p=std::stoi(f[2])?2u:0u;const auto& rec=runIt->results[p].recommendations;const auto* match=findMatch(rec,f[3]);const auto rank=rankMatch(rec,f[3]);
            const auto closeOld=[&](std::size_t i,float value,float tolerance){try{return std::abs(std::stof(f[i])-value)<=tolerance;}catch(...){return false;}};
            const float currentScore=rank.group>=0&&rank.rank>0?rec.groups[rank.group][rank.rank-1].rankingScore:-1.f;
            const bool same=match&&f[4]==match->disposition&&closeOld(6,match->rawCost,0.0001f)&&closeOld(7,match->similarity,0.0001f)&&
                f[8]==rank.primaryId&&f[10]==(rank.group<0?"NA":groupName(static_cast<std::size_t>(rank.group)))&&
                std::stoi(f[11])==(rank.rank<0?0:rank.rank)&&(f[12]=="1")==rank.visible&&closeOld(15,currentScore,0.0001f);
            if(!same)++phase51Diffs;++phase51Rows;phase51OracleFile<<caseId<<'\t'<<plugin::algorithmPresetName(presets[p])<<"\tproduction_worker_cap10\t"<<f[0]<<'\t'<<f[3]<<'\t'<<f[4]<<'\t'<<(match?match->disposition:"MISSING")<<'\t'<<f[7]<<'\t'<<(match?std::to_string(match->similarity):"NA")<<'\t'<<f[6]<<'\t'<<(match?std::to_string(match->rawCost):"NA")<<'\t'<<f[8]<<'\t'<<(rank.group>=0?rank.primaryId:"NA")<<'\t'<<f[11]<<'\t'<<(rank.rank<0?0:rank.rank)<<'\t'<<f[15]<<'\t'<<currentScore<<'\t'<<(same?"MATCH":"DIFF")<<'\n';
        }}
    require(phase51Rows==4,"Phase6 reads exactly the four Abstract OFF/ON Q1/Q2 historical worker rows");
    require(phase51Diffs==0,"Phase6 Abstract OFF/ON Q1/Q2 results agree with archived Phase5.1 worker rows");

    // Re-export the four archived Phase5.1 selected cards through the actual runtime worker result,
    // then compare both full and suffix SMFs to the staged historical files event by event.
    std::filesystem::create_directories(out/"phase51-runtime");
    const auto fileBytes=[&](const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);require(bool(input),"Phase5.1 historical/runtime MIDI opens");return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),{});};
    const auto noteEventsEqual=[](const midi::ReadResult& oldFile,const midi::ReadResult& current){
        if(!oldFile||!current||oldFile.file.notes.size()!=current.file.notes.size())return false;
        std::vector<bool> used(current.file.notes.size());
        for(const auto& oldNote:oldFile.file.notes){bool found{};for(std::size_t i=0;i<current.file.notes.size();++i){if(used[i])continue;const auto& note=current.file.notes[i];
                if(oldNote.pitch==note.pitch&&oldNote.velocity==note.velocity&&oldNote.channel==note.channel&&
                   std::abs(oldNote.startQN-note.startQN)<0.00001&&std::abs(oldNote.durationQN-note.durationQN)<0.00001){used[i]=true;found=true;break;}}
            if(!found)return false;
        }
        return true;
    };
    const auto tempoEventsEqual=[](const midi::ReadResult& oldFile,const midi::ReadResult& current){
        if(oldFile.file.tempos.size()!=current.file.tempos.size())return false;
        for(std::size_t i=0;i<oldFile.file.tempos.size();++i)if(std::abs(oldFile.file.tempos[i].qn-current.file.tempos[i].qn)>=0.00001||
            std::abs(oldFile.file.tempos[i].bpm-current.file.tempos[i].bpm)>=0.001)return false;
        return true;
    };
    {
        std::ifstream input(abstractFile);require(bool(input),"Phase6 reopens Phase5.1 selected-card table for SMF comparison");std::string line;
        require(bool(std::getline(input,line)),"Phase5.1 SMF oracle header exists");
        while(std::getline(input,line)){
            const auto f=splitTabs(line);require(f.size()==21,"Phase5.1 SMF oracle row retains all fields");
            const std::string caseId=f[0]=="Q1"?"Q1_PHASE51":f[0]=="Q2"?"Q2_PHASE51":"";require(!caseId.empty(),"Phase5.1 MIDI oracle pair is Q1 or Q2");
            const auto runIt=std::find_if(runs.begin(),runs.end(),[&](const auto& r){return r.test.id==caseId;});require(runIt!=runs.end(),"Phase5.1 MIDI oracle query is in the Phase6 run set");
            const auto presetIndexValue=std::stoi(f[2])?2u:0u;const auto preset=presets[presetIndexValue];const auto& rec=runIt->results[presetIndexValue].recommendations;
            const auto* match=findMatch(rec,f[3]);require(match!=nullptr,"Phase5.1 MIDI target retains its actual worker match");
            const auto cardIt=std::find_if(rec.groups.begin(),rec.groups.end(),[&](const auto& group){return std::any_of(group.begin(),group.end(),[&](const auto& c){return c.primaryTemplate==f[8];});});
            require(cardIt!=rec.groups.end(),"Phase5.1 selected primary ID exists in the actual runtime worker result");
            const auto candidateIt=std::find_if(cardIt->begin(),cardIt->end(),[&](const auto& c){return c.primaryTemplate==f[8];});
            require(candidateIt!=cardIt->end(),"Phase5.1 selected primary card is found");
            const auto selectedRank=rankMatch(rec,f[8]);require(selectedRank.primaryId==f[8]&&selectedRank.rank==std::stoi(f[11]),
                "Phase5.1 MIDI export uses the reviewed selected-card rank");
            const auto session=imported(runIt->progression);auto previewResult=preview::buildSequence(session,&*candidateIt,runIt->test.bpm);
            require(bool(previewResult),"Phase5.1 runtime candidate produces a preview sequence");
            midi::CandidateExportContext exportContext;exportContext.imported=session;exportContext.tempo=runIt->test.bpm;
            exportContext.meter={runIt->test.numerator,runIt->test.denominator};exportContext.mode=midi::ArrangementMode::VoiceLed;
            const std::array<std::string,2> scopes{"full","suffix"};const std::array<std::string,2> oldPaths{f[19],f[20]};
            for(std::size_t scopeIndex=0;scopeIndex<scopes.size();++scopeIndex){const bool suffixOnly=scopeIndex==1;const auto oldPath=oracleRoot/"phase51"/oldPaths[scopeIndex];
                require(std::filesystem::is_regular_file(oldPath),"Phase5.1 archived MIDI was staged under oracle/phase51/blind");
                exportContext.scope=suffixOnly?midi::ExportScope::ContinuationOnly:midi::ExportScope::FullPhrase;
                const auto payload=midi::candidatePayload(exportContext,*candidateIt);require(bool(payload),"Phase5.1 actual worker card exports to MIDI");
                const auto runtimePath=out/"phase51-runtime"/(f[0]+"-"+(std::stoi(f[2])?"abstract":"legacy")+"-"+scopes[scopeIndex]+".mid");
                std::string error;require(midi::writeToFile(payload.payload.smfBytes,runtimePath,error),"Phase5.1 runtime full/suffix MIDI is saved for review");
                const auto oldBytes=fileBytes(oldPath),currentBytes=payload.payload.smfBytes;const auto oldDecoded=midi::readFromMemory(oldBytes),currentDecoded=midi::readFromMemory(currentBytes);
                require(bool(oldDecoded)&&bool(currentDecoded),"Phase5.1 historical and current SMFs both decode");
                checkVoicing(previewResult.sequence,oldDecoded,suffixOnly);checkVoicing(previewResult.sequence,currentDecoded,suffixOnly);
                const bool noteEqual=noteEventsEqual(oldDecoded,currentDecoded),tempoEqual=tempoEventsEqual(oldDecoded,currentDecoded),exactBytes=oldBytes==currentBytes;
                require(noteEqual&&tempoEqual,"Phase5.1 old/current MIDI pitch, start, duration, velocity, channel, and tempo events agree");
                if(exactBytes)++phase51ExactByteRows;
                phase51MidiFile<<f[0]<<'\t'<<f[2]<<'\t'<<scopes[scopeIndex]<<'\t'<<plugin::algorithmPresetName(preset)<<"\tproduction_worker_cap10\t"
                    <<runIt->results[presetIndexValue].factoryVersion<<'\t'<<runIt->results[presetIndexValue].factorySha256<<'\t'<<f[3]<<'\t'<<f[8]<<'\t'<<selectedRank.rank<<'\t'
                    <<oldPath.string()<<'\t'<<runtimePath.string()<<'\t'<<exactBytes<<'\t'<<(exactBytes?"identical SMF bytes":"serialized SMF differs; all decoded note and tempo values match")<<'\t'
                    <<noteEqual<<'\t'<<tempoEqual<<'\t'
                    <<oldDecoded.file.notes.size()<<'\t'<<currentDecoded.file.notes.size()<<'\t'
                    <<(oldDecoded.file.tempos.empty()?0.:oldDecoded.file.tempos.front().bpm)<<'\t'
                    <<(currentDecoded.file.tempos.empty()?0.:currentDecoded.file.tempos.front().bpm)<<"\tPASS\tPASS\tPASS\n";
                ++phase51MidiRows;
            }
        }
    }
    require(phase51MidiRows==8,"Phase6 compares all eight historical Phase5.1 Q1/Q2 full/suffix SMFs");

    // These are offline, one-parameter-at-a-time controls over existing weights only.
    // The six fixed cases and both hard gates stay unchanged; no ablated values are adopted.
    std::size_t ablationCases{};
    for(const auto& run:runs){if(run.test.id!="R01_4-4_fixed-4qn"&&run.test.id!="R02_CMAJOR_4-4_fixed-4qn"&&run.test.id!="B08_4-4_fixed-4qn"&&run.test.id!="B11_4-4_fixed-4qn"&&run.test.id!="BORROWED_4-4_fixed-4qn"&&run.test.id!="SUS_4-4_fixed-4qn")continue;
        ++ablationCases;AnalysisContext ctx=plugin::developmentContext(AnalysisContext{},plugin::DevelopmentAlgorithmPreset::Legacy);const auto fixture=fixtures.find(run.test.fixture);ctx.forcedKey=fixture->second.key;ctx.timeSigNumerator=run.test.numerator;ctx.timeSigDenominator=run.test.denominator;
        const auto baselineWeights=plugin::developmentRecommendationWeights(plugin::DevelopmentAlgorithmPreset::Legacy);const auto query=makeMatchQuery(run.progression,ctx);
        const auto before=recommendContinuations(query,continuousIndex,{},baselineWeights);
        for(const std::string parameter:{"intentCompletionTieBreak","unexplainedInsertionPenalty","diversityPenalty"}) {
            auto ablatedWeights=baselineWeights;float defaultValue{};
            if(parameter=="intentCompletionTieBreak"){defaultValue=baselineWeights.intentCompletionTieBreak;ablatedWeights.intentCompletionTieBreak=0.f;}
            else if(parameter=="unexplainedInsertionPenalty"){defaultValue=baselineWeights.unexplainedInsertionPenalty;ablatedWeights.unexplainedInsertionPenalty=0.f;}
            else {defaultValue=baselineWeights.diversityPenalty;ablatedWeights.diversityPenalty=0.f;}
            require(ablatedWeights.minimumMatch==baselineWeights.minimumMatch&&ablatedWeights.minimumScore==baselineWeights.minimumScore&&
                ablatedWeights.abstractChordCompatibility==baselineWeights.abstractChordCompatibility&&
                ablatedWeights.diminishedPassingInsertionException==baselineWeights.diminishedPassingInsertionException,
                "Phase6 one-at-a-time ablation preserves hard gates and feature flags");
            const auto after=recommendContinuations(query,continuousIndex,{},ablatedWeights);
            for(const auto& m:before.matches){const auto* n=findMatch(after,m.templateId);const auto a=rankMatch(before,m.templateId),b=rankMatch(after,m.templateId);
                const float ablatedScore=n?n->candidateRankingScore:-1.f;const int rankDelta=n&&a.rank>=0&&b.rank>=0?b.rank-a.rank:0;
                ablationFile<<run.test.id<<'\t'<<parameter<<'\t'<<defaultValue<<"\t0\t"<<m.templateId<<'\t'<<m.candidateRankingScore<<'\t'
                    <<(n?std::to_string(ablatedScore):"NA")<<'\t'<<(n?std::to_string(ablatedScore-m.candidateRankingScore):"NA")<<'\t'
                    <<m.disposition<<'\t'<<(n?n->disposition:"MISSING")<<'\t'<<a.rank<<'\t'<<(n?b.rank:0)<<'\t'<<rankDelta<<'\t'
                    <<baselineWeights.minimumMatch<<'\t'<<baselineWeights.minimumScore<<'\t'<<(n?"MATCH":"MISSING")<<'\n';
            }
        }
    }
    require(ablationCases==6,"Phase6 controlled ablations use exactly six representative cases");

    // Select at most two differing anonymous pairs; each uses the same actual preview/export path.
    struct BlindPick {std::string caseId,preset,candidateId,primary,suffix;preview::Sequence sequence;std::vector<std::uint8_t> fullMidi,suffixMidi;double bpm{};};std::vector<BlindPick> blindPicks;
    for(const auto& run:runs){if(blindPicks.size()>=4)break;for(std::size_t pair=1;pair<3&&blindPicks.size()<4;++pair){const auto& a=run.results[0].recommendations;const auto& b=run.results[pair].recommendations;
            const ContinuationCandidate *ca{},*cb{};for(const auto& group:a.groups)if(!group.empty()){ca=&group.front();break;}for(const auto& group:b.groups)if(!group.empty()){cb=&group.front();break;}if(!ca||!cb||(ca->id==cb->id&&suffixText(ca->continuation)==suffixText(cb->continuation)))continue;
            const auto session=imported(run.progression);auto ba=preview::buildSequence(session,ca,run.test.bpm),bb=preview::buildSequence(session,cb,run.test.bpm);if(!ba||!bb)continue;
            midi::CandidateExportContext exportContext;exportContext.imported=session;exportContext.tempo=run.test.bpm;exportContext.meter={run.test.numerator,run.test.denominator};exportContext.mode=midi::ArrangementMode::VoiceLed;
            auto fullContext=exportContext;fullContext.scope=midi::ExportScope::FullPhrase;auto suffixContext=exportContext;suffixContext.scope=midi::ExportScope::ContinuationOnly;
            const auto caFull=midi::candidatePayload(fullContext,*ca),caSuffix=midi::candidatePayload(suffixContext,*ca),cbFull=midi::candidatePayload(fullContext,*cb),cbSuffix=midi::candidatePayload(suffixContext,*cb);
            require(bool(caFull)&&bool(caSuffix)&&bool(cbFull)&&bool(cbSuffix),"Phase6 blind anonymous pair exports full and suffix MIDI for both alternatives");
            blindPicks.push_back({run.test.id,"Legacy",ca->id,ca->primaryTemplate,suffixText(ca->continuation),std::move(ba.sequence),caFull.payload.smfBytes,caSuffix.payload.smfBytes,run.test.bpm});
            blindPicks.push_back({run.test.id,pair==1?"HarmonyFirst":"Abstract",cb->id,cb->primaryTemplate,suffixText(cb->continuation),std::move(bb.sequence),cbFull.payload.smfBytes,cbSuffix.payload.smfBytes,run.test.bpm});
        }}
    std::size_t blindPairNumber{};for(std::size_t i=0;i+1<blindPicks.size();i+=2){const auto pair="P"+std::to_string(++blindPairNumber);std::array<preview::AudioBuffer,2> fullRaw,suffixRaw;std::array<std::string,2> aliases{"A","B"};
        for(std::size_t side=0;side<2;++side){const auto& pick=blindPicks[i+side];fullRaw[side]=preview::renderOffline(pick.sequence,48000.);require(!fullRaw[side].left.empty()&&fullRaw[side].rms>0,"Phase6 blind full preview renders non-silent audio");
            const auto boundary=pick.sequence.events[pick.sequence.recommendationBoundary].startQN;const auto frame=preview::qnToSamples(boundary,pick.sequence.tempoBPM,fullRaw[side].sampleRate);require(frame>=0&&static_cast<std::size_t>(frame)<fullRaw[side].left.size(),"Phase6 blind suffix boundary maps into rendered WAV");
            suffixRaw[side].sampleRate=fullRaw[side].sampleRate;suffixRaw[side].left.assign(fullRaw[side].left.begin()+frame,fullRaw[side].left.end());suffixRaw[side].right.assign(fullRaw[side].right.begin()+frame,fullRaw[side].right.end());
            long double squares{};for(const auto sample:suffixRaw[side].left){require(std::isfinite(sample),"Phase6 blind suffix samples remain finite");squares+=static_cast<long double>(sample)*sample;suffixRaw[side].peak=std::max(suffixRaw[side].peak,static_cast<double>(std::abs(sample)));}
            require(!suffixRaw[side].left.empty(),"Phase6 blind suffix excerpt is non-empty");suffixRaw[side].rms=std::sqrt(static_cast<double>(squares/suffixRaw[side].left.size()));require(suffixRaw[side].rms>0,"Phase6 blind suffix excerpt has audible energy");}
        for(const bool suffixOnly:{false,true}){const auto& raw=suffixOnly?suffixRaw:fullRaw;const double common=std::min(raw[0].rms,raw[1].rms);for(std::size_t side=0;side<2;++side){const auto& pick=blindPicks[i+side];auto normalized=raw[side];const double gain=common/raw[side].rms;require(gain>0&&gain<=1.0000001&&raw[side].peak*gain<1.0,"Phase6 blind pair uses non-boosting linear normalization without clipping");for(auto& sample:normalized.left)sample=static_cast<float>(sample*gain);for(auto& sample:normalized.right)sample=static_cast<float>(sample*gain);normalized.rms=common;normalized.peak=raw[side].peak*gain;
                const auto scope=suffixOnly?"suffix":"full";const auto wav=out/"blind"/(pair+"-"+aliases[side]+"-"+scope+".wav");std::string error;require(preview::writeWav16(normalized,wav,error),"Phase6 anonymous preview WAV writes");
                const auto fullMidi=out/"blind"/(pair+"-"+aliases[side]+"-full.mid"),suffixMidi=out/"blind"/(pair+"-"+aliases[side]+"-suffix.mid");
                if(!suffixOnly){error.clear();require(midi::writeToFile(pick.fullMidi,fullMidi,error),"Phase6 blind full MIDI writes");error.clear();require(midi::writeToFile(pick.suffixMidi,suffixMidi,error),"Phase6 blind suffix MIDI writes");
                    const auto fullDecoded=midi::readFromMemory(pick.fullMidi),suffixDecoded=midi::readFromMemory(pick.suffixMidi);checkVoicing(pick.sequence,fullDecoded,false);checkVoicing(pick.sequence,suffixDecoded,true);
                    blindMidiCheck<<pair<<'\t'<<aliases[side]<<"\tfull\t"<<fullMidi.string()<<'\t'<<fullDecoded.file.notes.size()<<"\tPASS\n";
                    blindMidiCheck<<pair<<'\t'<<aliases[side]<<"\tsuffix\t"<<suffixMidi.string()<<'\t'<<suffixDecoded.file.notes.size()<<"\tPASS\n";}
                blindAnswer<<pair<<'\t'<<pick.caseId<<'\t'<<scope<<'\t'<<aliases[side]<<'\t'<<pick.preset<<'\t'<<pick.candidateId<<'\t'<<pick.primary<<'\t'<<pick.suffix<<'\t'<<wav.string()<<'\t'<<fullMidi.string()<<'\t'<<suffixMidi.string()<<'\n';}}
        blindInstructions<<"Pair "<<pair<<": compare A-full/B-full and A-suffix/B-suffix WAVs or MIDI at one fixed host sound and volume. Matching MIDI files use the same aliases and scope.\n";
    }

    require(blindPairNumber>0,"Phase6 produces at least one anonymous A/B preview pair");
    std::size_t r01V5Rows{},r01V5MidiPairs{};
    if(!v5FactoryPath.empty()){
        const auto candidateDb=library::loadFactory(v5FactoryPath);require(bool(candidateDb)&&candidateDb.templates.size()==658,
            "Optional isolated R01 V5 Factory loads read-only with 658 templates");
        const TemplateID v5Target="CANDIDATE_V5_CHROMATIC_MINOR_001";
        const auto targetIt=std::find_if(candidateDb.templates.begin(),candidateDb.templates.end(),[&](const auto& item){return item.id==v5Target;});
        require(targetIt!=candidateDb.templates.end()&&targetIt->full.size()==8&&targetIt->mode==Mode::Minor&&targetIt->intent==PhraseIntent::Develop,
            "Optional R01 target keeps the reviewed eight-event Minor/Develop identity");
        const auto v5UserPath=out/"r01-v5-empty-user-library.db";
        require(!std::filesystem::exists(v5UserPath),"Isolated V5 probe starts without a user database so the worker has nothing to write");
        std::filesystem::create_directories(out/"r01-v5-midi");
        const auto pitchName=[](int note){constexpr const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return std::string(names[(note%12+12)%12])+std::to_string(note/12-1);};
        const auto durationList=[](const std::vector<double>& values){std::ostringstream text;text<<std::fixed<<std::setprecision(6);for(std::size_t i=0;i<values.size();++i){if(i)text<<',';text<<values[i];}return text.str();};
        const auto suffixDurations=[](const std::vector<ConcreteChordEvent>& suffix){std::ostringstream text;text<<std::fixed<<std::setprecision(6);for(std::size_t i=0;i<suffix.size();++i){if(i)text<<',';text<<suffix[i].durationQN;}return text.str();};
        const auto decodedLowest=[&](const preview::Sequence& previewSequence,const midi::ReadResult& decoded,bool suffixOnly){
            std::ostringstream values;bool first=true;const double firstSuffix=previewSequence.events[previewSequence.recommendationBoundary].startQN;
            for(std::size_t i=0;i<previewSequence.events.size();++i){if(suffixOnly&&i<previewSequence.recommendationBoundary)continue;const auto& event=previewSequence.events[i];
                const auto start=event.startQN-(suffixOnly?firstSuffix:0.);int lowest=128;
                for(const auto& note:decoded.file.notes)if(closeQN(note.startQN,start)&&closeQN(note.durationQN,event.durationQN))lowest=std::min(lowest,note.pitch);
                require(lowest<128,"R01 V5 decoded MIDI has a lowest note for every preview event");if(!first)values<<"|";first=false;values<<pitchName(lowest)<<'('<<lowest<<')';}
            return values.str();
        };
        {
            plugin::RecommendationWorker v5Worker(v5FactoryPath,v5UserPath,out/"r01-v5-empty-factory-store");std::uint64_t revision=1;
            for(const double qn:{2.,4.})for(const auto meter:{midi::Meter{4,4},midi::Meter{7,8}})for(const auto preset:presets){
                const auto progression=phrase({"Am","G#dim","G","D/F#"},{qn,qn,qn,qn},true);
                AnalysisContext context;context.forcedKey=KeySignature{PitchClass::A,Mode::Minor};context.timeSigNumerator=meter.numerator;context.timeSigDenominator=meter.denominator;
                v5Worker.submitPreset(progression,context,preset,{},false,revision++);const auto result=wait(v5Worker);
                require(result.algorithmPreset&&*result.algorithmPreset==preset&&result.factoryCount==658&&result.userCount==0&&result.factoryVersion==candidateDb.libraryVersion,
                    "R01 V5 matrix uses the explicit preset and isolated read-only 658-entry Factory");
                const auto& rec=result.recommendations;require(rec.groups.size()==4&&rec.stats.shortlistSize==658,"R01 V5 runtime worker ranks the complete isolated candidate library");
                const auto* match=findMatch(rec,v5Target);const auto rank=rankMatch(rec,v5Target);const auto outcome=phase5Outcome(rec,v5Target,plugin::developmentRecommendationWeights(preset).minimumScore);
                const ContinuationCandidate* card{};for(const auto& group:rec.groups)for(const auto& candidate:group)if(candidate.id==rank.candidateId){card=&candidate;break;}
                const auto selectedKey=result.analysis.selectedKey?formatKey(result.analysis.selectedKey->key):"UNKNOWN";
                const std::vector<double> inputDurations(4,qn);std::string fullPath="NA",fullSuffix="NA",suffixDurationText="NA",snapshotPreserved="NOT_EXPORTED",fullMidiPath="NOT_EXPORTED",suffixMidiPath="NOT_EXPORTED",fullLows="NOT_EXPORTED",suffixLows="NOT_EXPORTED",voicing="NOT_EXPORTED";
                if(card){fullSuffix=suffixText(card->continuation);suffixDurationText=suffixDurations(card->continuation);fullPath=sourceText(progression)+" -> "+fullSuffix;}
                if(preset==plugin::DevelopmentAlgorithmPreset::Legacy&&card){
                    const auto session=imported(progression);const auto captured=snapshot::capture(session,*card,rec.matches,120.,meter.numerator,meter.denominator);
                    const auto stored=snapshot::deserialize(snapshot::serialize(captured));ContinuationCandidate exportCandidate=*card;
                    if(stored){const auto& restored=stored.value.candidate;bool suffixSame=restored.continuation.size()==card->continuation.size();
                        for(std::size_t i=0;suffixSame&&i<card->continuation.size();++i){const auto& before=card->continuation[i];const auto& after=restored.continuation[i];
                            suffixSame=before.label==after.label&&closeQN(before.durationQN,after.durationQN)&&before.quality==after.quality&&bool(before.harmonicData)==bool(after.harmonicData);
                            if(suffixSame&&before.harmonicData){const auto a=normalizeChord(*before.harmonicData),b=normalizeChord(*after.harmonicData);
                                suffixSame=before.harmonicData->name==after.harmonicData->name&&a.root==b.root&&a.bass==b.bass&&a.quality==b.quality&&a.explicitBass==b.explicitBass;}}
                        require(suffixSame,"R01 V5 snapshot route preserves each returned suffix event and chord identity");exportCandidate=restored;snapshotPreserved="PASS";
                    }else snapshotPreserved="NOTSUPPORTED"; // The V5 MIDI check remains independent of V4 snapshot policy.
                    auto previewBuilt=preview::buildSequence(session,&exportCandidate,120.);require(bool(previewBuilt),"R01 V5 selected card previews for full/suffix MIDI export");
                    midi::CandidateExportContext exportContext;exportContext.imported=session;exportContext.tempo=120.;exportContext.meter=meter;exportContext.mode=midi::ArrangementMode::VoiceLed;
                    exportContext.scope=midi::ExportScope::FullPhrase;const auto fullPayload=midi::candidatePayload(exportContext,exportCandidate);require(bool(fullPayload),"R01 V5 selected full phrase exports");
                    exportContext.scope=midi::ExportScope::ContinuationOnly;const auto suffixPayload=midi::candidatePayload(exportContext,exportCandidate);require(bool(suffixPayload),"R01 V5 selected suffix exports");
                    const auto stem="R01_"+std::to_string(static_cast<int>(qn))+"QN_"+std::to_string(meter.numerator)+"-"+std::to_string(meter.denominator)+"_Legacy";
                    const auto fullFile=out/"r01-v5-midi"/(stem+"_full.mid"),suffixFile=out/"r01-v5-midi"/(stem+"_suffix.mid");std::string error;
                    require(midi::writeToFile(fullPayload.payload.smfBytes,fullFile,error),"R01 V5 full MIDI saves after the storage route");error.clear();
                    require(midi::writeToFile(suffixPayload.payload.smfBytes,suffixFile,error),"R01 V5 suffix MIDI saves after the storage route");
                    const auto fullDecoded=midi::readFromMemory(fullPayload.payload.smfBytes),suffixDecoded=midi::readFromMemory(suffixPayload.payload.smfBytes);
                    checkVoicing(previewBuilt.sequence,fullDecoded,false);checkVoicing(previewBuilt.sequence,suffixDecoded,true);
                    compareMidi(previewBuilt.sequence,fullDecoded);fullLows=decodedLowest(previewBuilt.sequence,fullDecoded,false);suffixLows=decodedLowest(previewBuilt.sequence,suffixDecoded,true);
                    fullMidiPath=fullFile.string();suffixMidiPath=suffixFile.string();voicing="PASS";++r01V5MidiPairs;
                }
                r01V5File<<v5FactoryPath.string()<<'\t'<<result.factoryVersion<<'\t'<<result.factorySha256<<'\t'<<result.factoryCount<<'\t'<<qn<<'\t'
                    <<meter.numerator<<'/'<<meter.denominator<<"\t1\t"<<plugin::algorithmPresetName(preset)<<'\t'<<selectedKey<<'\t'<<durationList(inputDurations)<<'\t'<<v5Target<<'\t'
                    <<bool(match)<<'\t'<<(match?formatKey(match->key):"NA")<<'\t'<<(match?match->similarity:-1.f)<<'\t'<<(match?match->candidateRankingScore:-1.f)<<'\t'
                    <<(match?match->candidateEligibilityScore:-1.f)<<'\t'<<(match?(preset==plugin::DevelopmentAlgorithmPreset::HarmonyFirst?match->candidateEligibilityScore:match->candidateRankingScore):-1.f)<<'\t'
                    <<phase5Disposition(outcome)<<'\t'<<outcome.eligible<<'\t'<<(card?card->id:"NA")<<'\t'<<(card?card->primaryTemplate:"NA")<<'\t'
                    <<(rank.group<0?"NA":groupName(static_cast<std::size_t>(rank.group)))<<'\t'<<rank.rank<<'\t'<<rank.visible<<'\t'<<fullPath<<'\t'<<fullSuffix<<'\t'
                    <<suffixDurationText<<'\t'<<snapshotPreserved<<'\t'<<fullMidiPath<<'\t'<<suffixMidiPath<<'\t'<<fullLows<<'\t'<<suffixLows<<'\t'<<voicing<<'\n';
                ++r01V5Rows;
            }
        }
        require(!std::filesystem::exists(v5UserPath),"R01 V5 probe leaves the isolated user database uncreated and writes no database");
        require(r01V5Rows==12&&r01V5MidiPairs==4,"R01 V5 worker grid has twelve rows and four Legacy full/suffix MIDI pairs");
    }
    std::ostringstream nonExactNames;bool firstNonExact=true;for(const auto& id:nonExactCoverageCases){if(!firstNonExact)nonExactNames<<", ";firstNonExact=false;nonExactNames<<id;}
    std::ofstream summary(out/"summary.md");require(bool(summary),"Phase6 summary opens");summary<<"# Phase 6 fixed benchmark\n\n"<<"Cases: "<<cases.size()<<"; presets: Legacy/HarmonyFirst/Abstract; internal ContinuousStructure control: "<<continuousIndex.templates().size()<<" V4 entries. Factory expectation: v4, SHA-256 "<<factoryHash<<".\n\n"
        <<"Historical Phase 4B exact inputs: "<<inputOracleRows<<"; input mismatches: "<<inputOracleMismatches<<". Raw three-build rows checked against the same-input direct-core cap-3 control (default RecommendationWeights, gates 0.60/60): "<<oracleRows<<"; mismatches: "<<oracleMismatches<<". The archived `visible` flag means retained in a recommendation group; it is compared with current group retention, while `newDefaultPresented` separately records `presentationIndices` visibility. This historical direct-core scope is separate from the Phase6 production worker's ten-item retention. Historical Phase 5.1 Q1/Q2 production-worker-cap10 rows: "<<phase51Rows<<"; mismatches: "<<phase51Diffs<<". Phase 5.1 production-worker-cap10 MIDI rows: "<<phase51MidiRows<<"/8 pass; exact SMF bytes: "<<phase51ExactByteRows<<"/8.\n\n"
        <<"R02 production-worker orthogonal grid: "<<r02CellCount<<" cells (Auto/C major/A minor × 4,4,4/3,5,4 QN × closed/OPEN × three presets), with "<<r02TargetRows<<" fixed-target rows. Non-exact alignment coverage cases: "<<nonExactCoverageCases.size()<<" ("<<nonExactNames.str()<<").\n\n"
        <<"Worker performance samples: "<<performanceRows<<" (35 cases × three presets). The first timer sample is cold and includes Factory loading and SHA-256; remaining rows are hot. These values are not a cold-versus-hot preset speed comparison.\n\n"
        <<"Optional isolated V5 R01 probe: "<<(v5FactoryPath.empty()?"not run (optional V5 path was not supplied)":std::to_string(r01V5Rows)+" rows; four Legacy full/suffix MIDI pairs decoded and checked after the existing snapshot storage route")<<". It reads the supplied 658-entry Factory and creates no user database. See `r01-v5-isolated.tsv`.\n\n"
        <<"Phase 5.1 MIDI comparison exports the archived selected Q1/Q2 OFF/ON cards through the current worker, then checks old and new full/suffix SMFs against the same preview voicing and compares decoded pitches, starts, durations, velocities, channels, and tempo. The exact-byte count is reported above; if the bytes differ, the TSV records that the decoded musical event values still match.\n\n"
        <<"Offline one-parameter controls: "<<ablationCases<<" fixed cases × intentCompletionTieBreak, unexplainedInsertionPenalty, and diversityPenalty reset individually to zero; thresholds and feature flags stay unchanged, and no ablated values are adopted.\n\n"
        <<"Existing hard/soft structure behavior is reused: the existing 0.55 skeleton threshold controls ordinary structural selection; Phase 5's FULL/SKELETON checks and this run's actual ContinuousStructure core mode already exercise the full-query/skeleton distinction. No new score or UI mode is added.\n\n"
        <<"Intent request coverage: Resolve, Develop, Loop, and Color. The production Color reranker is checked in all four groups; its order is filtered through `presentationIndices` before default-visible IDs and the first MIDI export are selected. `color-ranking.tsv` records before/after default IDs. The benchmark records machine scores, IDs, groups, dispositions, suffixes, and export/readback consistency; it does not automatically rate musical quality. Anonymous WAV and MIDI full/suffix pairs are in `blind/`; their decode-to-preview voicing checks are in `blind-midi-readback.tsv`. The alias mapping is only in `answer-key/answer-key.tsv`.\n\n"
        <<"Not covered by this offline executable: Cubase-host import/export, actual user MIDI files, physical listening results, and installer behavior. No score threshold or core algorithm was changed.\n";
    std::cout<<"Phase6 PASS cases="<<cases.size()<<" r02Cells="<<r02CellCount<<" r02Targets="<<r02TargetRows<<" nonExactCases="<<nonExactCoverageCases.size()<<" ablationCases="<<ablationCases<<" blindPairs="<<blindPairNumber<<" oracleRows="<<oracleRows<<" oracleMismatches="<<oracleMismatches<<" phase51Rows="<<phase51Rows<<" phase51Mismatches="<<phase51Diffs<<" phase51MidiRows="<<phase51MidiRows<<" r01V5Rows="<<r01V5Rows<<" performanceRows="<<performanceRows<<" output="<<out.string()<<"\n";
    return 0;
}
#include "Phase61Benchmark.h"
namespace {
bool unifiedPathIs(const ContinuationCandidate& c,const std::vector<std::string>& labels) {
    if(c.continuation.size()!=labels.size())return false;
    for(std::size_t i=0;i<labels.size();++i) {
        ChordEvent aEvent;aEvent.name=c.continuation[i].label;aEvent.quality=c.continuation[i].quality;
        ChordEvent bEvent;bEvent.name=labels[i];
        const auto a=normalizeChord(aEvent),b=normalizeChord(bEvent);
        if(a.root!=b.root||a.quality!=b.quality||a.bass!=b.bass||a.intervalMask!=b.intervalMask)return false;
    }
    return true;
}
bool unifiedSourceTraceable(const ContinuationCandidate& c,const RecommendationSet& rec,const TemplateID& id) {
    return std::any_of(c.evidence.begin(),c.evidence.end(),[&](const auto& e) {
        if(e.templateId!=id)return false;
        return std::any_of(rec.matches.begin(),rec.matches.end(),[&](const auto& m) {
            return m.templateId==e.templateId&&m.key.tonic==e.key.tonic&&m.key.mode==e.key.mode&&
                m.templateMatchStart==e.matchStart&&m.templateMatchEnd==e.matchEnd&&
                m.continuationStartIndex==e.continuationStart;
        });
    });
}
int runUnifiedIntegration(const std::filesystem::path& out,const std::filesystem::path& factoryPath) {
    runCurveChecks();
    std::filesystem::create_directories(out);
    std::ofstream log(out/"unified.txt");
    require(bool(log),"unified evidence opens");
    const auto loaded=library::loadFactory(factoryPath);
    require(bool(loaded)&&loaded.libraryVersion==5&&loaded.templates.size()==658,"read-only Factory 5/658");
    for(const TemplateID id:{"POP_014","V5_POP_VARIANT_001"})
        require(std::count_if(loaded.templates.begin(),loaded.templates.end(),
            [&](const auto& t){return t.id==id;})==1,"required long-form source exists exactly once");

    struct Case {std::string name;Progression input;AnalysisContext context;};
    std::vector<Case> cases;
    const auto add=[&](const char* name,Progression p,std::optional<KeySignature> key) {
        AnalysisContext c;c.forcedKey=key;c.timeSigNumerator=4;c.timeSigDenominator=4;
        cases.push_back({name,std::move(p),c});
    };
    add("C-G-Am Auto",phrase({"C","G","Am"},{4,4,4},true),std::nullopt);
    add("C-G-Am-F Auto",phrase({"C","G","Am","F"},{4,4,4,4},true),std::nullopt);
    add("Am-G#dim-G-D/F# Auto",symbolic(phrase({"Am","G#dim","G","D/F#"},{4,4,4,4},true)),std::nullopt);
    add("F-G-Am Auto",phrase({"F","G","Am"},{4,4,4},true),std::nullopt);
    add("F-G-Am forced A minor",phrase({"F","G","Am"},{4,4,4},true),KeySignature{PitchClass::A,Mode::Minor});
    add("C-Fm-G Auto",phrase({"C","Fm","G"},{2,2,2},true),std::nullopt);
    add("C-Fm-G forced C major",phrase({"C","Fm","G"},{2,2,2},true),KeySignature{PitchClass::C,Mode::Major});
    add("F-G-Am Auto 3-5-OPEN",phrase({"F","G","Am"},{3,5,4},true),std::nullopt);
    require(cases.size()==8,"eight targeted input/key/rhythm cases");

    plugin::RecommendationWorker worker(factoryPath,out/"unified-user.db",out/"unified-factory-store");
    constexpr std::array<const char*,4> groups{"Resolve","Develop","Loop","Color"};
    const std::vector<std::string> pop014{"Em","F","C","F","G"},v5pop{"Em","F","C","Dm","G"};
    std::uint64_t revision=1;std::size_t total{},defaultsTotal{},moreTotal{},exploreTotal{};
    for(std::size_t test=0;test<cases.size();++test) {
        const auto& item=cases[test];
        worker.submit(item.input,item.context,{},false,revision++);
        const auto result=wait(worker);const auto& rec=result.recommendations;
        require(result.factoryVersion==5&&result.factoryCount==658&&result.userCount==0,"Worker uses isolated Factory 5/658");
        require(!result.algorithmPreset&&rec.retrievalTimingMode==RetrievalTimingMode::Unified&&
            rec.abstractChordCompatibility&&!rec.phase3aEnabled,"Worker submit runs unified abstract-compatible mode");
        require(rec.stats.shortlistSize==658&&rec.groups.size()==4,"all Factory entries reach the four output groups");
        if(item.context.forcedKey) {
            require(result.analysis.selectedKey&&result.analysis.selectedKey->key.tonic==item.context.forcedKey->tonic&&
                result.analysis.selectedKey->key.mode==item.context.forcedKey->mode,"forced key remains selected");
            for(const auto& m:rec.matches)require(m.key.tonic==item.context.forcedKey->tonic&&
                m.key.mode==item.context.forcedKey->mode,"forced key constrains actual matches");
        }
        const auto visible=session::presentationIndices(rec,{});
        const auto key=result.analysis.selectedKey?formatKey(result.analysis.selectedKey->key):"UNKNOWN";
        auto session=imported(item.input);std::vector<std::string> identities;
        std::size_t caseTotal{},caseDefault{},caseMore{},caseExplore{};
        bool classic{},popFull{},v5Full{},fourLoop{},color{},dmLoop{},gce{},cge{};
        log<<"\nCASE "<<test<<" "<<item.name<<" input=\""<<phase61InputPath(item.input)
           <<"\" selectedKey="<<key<<" ms="<<result.computationMs<<'\n';
        for(std::size_t g=0;g<rec.groups.size();++g) {
            const auto access=session::continuationAccess(rec.groups[g]);
            for(std::size_t n=0;n<rec.groups[g].size();++n) {
                const auto& c=rec.groups[g][n];const auto identity=continuationMusicFingerprint(c);
                require(std::find(identities.begin(),identities.end(),identity)==identities.end(),
                    "identical music occupies one recommendation position");
                identities.push_back(identity);
                require(c.id=="music-v1:"+identity&&std::any_of(rec.matches.begin(),rec.matches.end(),
                    [&](const auto& m){return representativeMatch(c,m);}),"exact identity and real representative match");
                require(c.hardConstraintsPassed&&c.baseQualified&&std::isfinite(c.rankingScore),
                    "every Worker output passed hard and base qualification");
                const bool shown=std::find(visible[g].begin(),visible[g].end(),n)!=visible[g].end();
                const bool inMore=std::find(access.more.begin(),access.more.end(),n)!=access.more.end();
                require(inMore,"retained qualified output is accessible in the qualified list");
                if(c.exploratory)require(!shown,"exploratory candidate is outside Top3 priority positions");
                ++total;++caseTotal;++moreTotal;++caseMore;
                if(shown){++defaultsTotal;++caseDefault;}
                if(c.exploratory){++exploreTotal;++caseExplore;}
                log<<"CARD "<<groups[g]<<" rank="<<n+1<<" id="<<c.id<<" key="<<formatKey(c.key)
                   <<" intent="<<intentName(c.intent)<<" ranking="<<c.rankingScore<<" eligibility="<<c.eligibilityScore
                   <<" hard="<<c.hardConstraintsPassed<<" baseQualified="<<c.baseQualified<<" exploratory="<<c.exploratory
                   <<" default="<<shown<<" more="<<inMore<<" suffix=\""<<phase61ChordPath(c.continuation)
                   <<"\" sourceCount="<<c.supportingTemplates.size()<<" evidence=";
                for(std::size_t i=0;i<std::min<std::size_t>(3,c.evidence.size());++i) {
                    const auto& e=c.evidence[i];if(i)log<<';';
                    log<<e.templateId<<'@'<<formatKey(e.key)<<'['<<e.matchStart<<'-'<<e.matchEnd
                       <<";next="<<e.continuationStart<<']';
                }
                log<<'\n';
                if(test==0) {
                    if(phase61TargetSuffix(c)&&shown)classic=true;
                    if(unifiedPathIs(c,pop014)&&unifiedSourceTraceable(c,rec,"POP_014"))popFull=true;
                    if(unifiedPathIs(c,v5pop)&&unifiedSourceTraceable(c,rec,"V5_POP_VARIANT_001"))v5Full=true;
                }
                if(test==1&&g==2&&c.intent==PhraseIntent::Loop&&phase61CandidateRepeatsInput(item.input,c))fourLoop=true;
                if((test==3||test==4||test==7)&&g==3&&c.intent==PhraseIntent::Color)color=true;
                if((test==5||test==6)&&g==2&&c.intent==PhraseIntent::Loop&&unifiedPathIs(c,{"Dm"})&&shown)dmLoop=true;
                if(test==2) {
                    if(unifiedPathIs(c,{"G","C","Em"})&&c.baseQualified&&c.exploratory&&inMore)gce=true;
                    if(unifiedPathIs(c,{"C","G","Em"})&&c.baseQualified&&c.exploratory&&inMore)cge=true;
                }
                if(n==0) {
                    const auto previewBuilt=preview::buildSequence(session,&c,120.);
                    require(bool(previewBuilt),"first candidate in group builds Preview");
                    const auto snap=snapshot::deserialize(snapshot::serialize(
                        snapshot::capture(session,c,rec.matches,120.,4,4)));
                    require(bool(snap)&&snap.value.match&&representativeMatch(c,*snap.value.match),
                        "first group candidate Snapshot preserves its actual representative");
                    require(session::continuationFingerprint(c)==session::continuationFingerprint(snap.value.candidate),
                        "Snapshot roundtrip preserves exact music identity");
                    midi::CandidateExportContext ec;ec.imported=session;ec.tempo=120.;ec.meter={4,4};ec.key=c.key;
                    for(auto mode:{midi::ArrangementMode::BlockChords,midi::ArrangementMode::VoiceLed})
                    for(auto scope:{midi::ExportScope::FullPhrase,midi::ExportScope::CurrentOnly,midi::ExportScope::ContinuationOnly}) {
                        ec.mode=mode;ec.scope=scope;
                        const auto payload=midi::candidatePayload(ec,c),restored=midi::candidatePayload(ec,snap.value.candidate);
                        require(bool(payload)&&bool(restored)&&payload.payload.smfBytes==restored.payload.smfBytes,
                            "Snapshot preserves Block/VoiceLed MIDI in all three scopes");
                        const auto decoded=midi::readFromMemory(payload.payload.smfBytes);
                        require(bool(decoded)&&!decoded.file.notes.empty(),"actual MIDI output reads back");
                        if(mode==midi::ArrangementMode::VoiceLed&&scope==midi::ExportScope::FullPhrase)
                            compareMidi(previewBuilt.sequence,decoded);
                    }
                }
            }
            require(access.exploratory==static_cast<std::size_t>(std::count_if(rec.groups[g].begin(),
                rec.groups[g].end(),[](const auto& c){return c.exploratory;})),"Qualified-list exploratory count agrees with qualified cards");
        }
        log<<"COUNTS candidates="<<caseTotal<<" defaultVisible="<<caseDefault<<" more="<<caseMore
           <<" exploratory="<<caseExplore<<'\n';
        if(test==0) {
            require(classic,"C-G-Am default reaches Em-F-G-C");
            require(popFull,"full POP_014 suffix Em-F-C-F-G is source-traceable");
            require(v5Full,"full V5_POP_VARIANT_001 suffix Em-F-C-Dm-G is source-traceable");
        }
        if(test==1)require(fourLoop,"C-G-Am-F retains its complete four-chord Loop");
        if(test==2)require(gce&&cge,"G-C-Em and C-G-Em are qualified exploratory list routes");
        if(test==3||test==7)require(color,"F-G-Am Auto retains actual Color continuations");
        // Forced A minor constrains the search. The Auto Color paths above use F major;
        // absence of those paths under a different forced key is not a coverage regression.
        if(test==4)log<<"KEY_CONSTRAINT forced A minor excludes the F-major Auto Color interpretations\n";
        if(test==5||test==6)require(dmLoop,"C-Fm-G retains default Dm Loop for Auto and forced C major");
    }
    phase5CompatibilityExperiments(out);
    log<<"SUMMARY cases="<<cases.size()<<" candidates="<<total<<" defaultVisible="<<defaultsTotal
       <<" more="<<moreTotal<<" exploratory="<<exploreTotal<<" (default counts are observations only)\n";
    log.flush();require(bool(log),"unified evidence flushes");
    std::cout<<"Unified integration PASS cases="<<cases.size()<<" candidates="<<total
       <<" defaultVisible="<<defaultsTotal<<" more="<<moreTotal<<" exploratory="<<exploreTotal
       <<" checks="<<checks<<" output="<<out/"unified.txt"<<'\n';
    return 0;
}
}
int main(int argc,char** argv) {
 try {
    const std::filesystem::path out=argc>1?argv[1]:"phase4d";std::filesystem::create_directories(out);
    if(argc>3&&std::string(argv[3])=="phase51") {
        const auto startChecks=checks;
        runPhase51R02(out/"r02",HC_FACTORY_DB_PATH);
        const auto r02Checks=checks-startChecks;
        runPhase51Music(out/"music",argv[2]);
        std::cout<<"Phase51 PASS totalChecks="<<checks-startChecks<<" R02Checks="<<r02Checks
            <<" musicChecks="<<checks-startChecks-r02Checks<<" output="<<out.string()<<"\n";
        return 0;
    }
    if(argc>3&&std::string(argv[3])=="phase5")return runPhase5(out,argv[2]);
    if(argc>3&&std::string(argv[3])=="phase6")return runPhase6(out,argc>2?argv[2]:HC_FACTORY_DB_PATH,argc>4?argv[4]:out/"oracle",argc>5?std::filesystem::path(argv[5]):std::filesystem::path{});
    if(argc>3&&std::string(argv[3])=="phase61")return runPhase61(out,argc>2?std::filesystem::path(argv[2]):std::filesystem::path(HC_FACTORY_DB_PATH),argc>4&&std::string(argv[4])=="strict");
    if(argc>3&&std::string(argv[3])=="phase62")return runPhase62(out,argv[2],argc>4&&std::string(argv[4])=="strict");
    if(argc>3&&std::string(argv[3])=="consistency")return runDev9Consistency(out,argv[2],argc>4&&std::string(argv[4])=="strict");
    if(argc>3&&std::string(argv[3])=="unified")return runUnifiedIntegration(out,argc>2?std::filesystem::path(argv[2]):std::filesystem::path(HC_FACTORY_DB_PATH));
    if(argc<=3)return runUnifiedIntegration(out,argc>2?std::filesystem::path(argv[2]):std::filesystem::path(HC_FACTORY_DB_PATH));
    if(std::string(argv[3])!="history")throw std::runtime_error("unknown Phase4D mode; use unified or history");
    const auto db=library::loadFactory(HC_FACTORY_DB_PATH);require(bool(db)&&db.templates.size()==657,"Factory V4 baseline");CandidateIndex index(db.templates);
    std::ofstream summary(out/"summary.txt"),trace(out/"candidate-traces.txt"),meters(out/"meters.tsv"),perf(out/"performance.tsv");
    summary<<"SYNTHETIC fixtures from user transcription; no captured Cubase MIDI/XML supplied.\n";
    const auto weights=plugin::developmentRecommendationWeights();
    auto buildContext=plugin::developmentContext({});
    require((buildContext.retrievalTimingMode==RetrievalTimingMode::HarmonyFirst)==plugin::harmonyFirstBuild(),"compiled mode uses submission helper");
    // Optional isolated content gate. The normal plugin and CTest still use
    // the untouched 657-entry V4 database; candidate content is never installed.
    if(argc>2) {
        const auto staged=library::loadFactory(argv[2]);require(bool(staged)&&staged.templates.size()==658,"isolated candidate contains V4 plus exactly one entry");
        const std::string id="CANDIDATE_V5_CHROMATIC_MINOR_001";
        require(std::count_if(staged.templates.begin(),staged.templates.end(),[&](const auto& t){return t.id==id;})==1,"unique candidate ID");
        CandidateIndex stagedIndex(staged.templates);
        std::ofstream content(out/"candidate-content.tsv");content<<"tonic\tmeter\tQN\tOPEN\tabstractBass\tHF\trank\tscore\n";
        const std::array<std::vector<std::string>,3> prefixes{{{"Am","G#dim","G","D/F#"},{"Cm","Bdim","Bb","F/A"},{"Dm","C#dim","C","G/B"}}};
        const std::array<std::vector<std::string>,3> tails{{{"F","G","Am","C"},{"Ab","Bb","Cm","Eb"},{"Bb","C","Dm","F"}}};
        const std::array<PitchClass,3> tonics{PitchClass::A,PitchClass::C,PitchClass::D};
        const auto findCandidate=[&](const RecommendationSet& rec)->const ContinuationCandidate* {
            for(const auto& g:rec.groups)for(const auto& c:g)
                if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),id)!=c.supportingTemplates.end())return &c;
            return nullptr;
        };
        for(std::size_t key=0;key<3;++key)for(const auto meter:{midi::Meter{4,4},{3,4},{6,8},{5,4},{7,8},{9,8}})
        for(double qn:{2.,meter.numerator*4.0/meter.denominator})for(bool open:{false,true})for(bool abstractBass:{false,true})for(bool hf:{false,true}) {
            auto names=prefixes[key];if(abstractBass)names.back().resize(names.back().find('/'));
            const auto p=symbolic(phrase(names,{qn,qn,qn,qn},open));
            AnalysisContext ctx;ctx.forcedKey=KeySignature{tonics[key],Mode::Minor};ctx.timeSigNumerator=meter.numerator;ctx.timeSigDenominator=meter.denominator;
            ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
            const auto rec=recommendContinuations(makeMatchQuery(p,ctx),stagedIndex,{},weights);
            if(key==0 && meter.numerator==4 && meter.denominator==4 && qn==2. && !open && !abstractBass)
                trace<<"\nISOLATED_CONTENT HF="<<hf<<session::recommendationDiagnostics(p,ctx,{},rec);
            const auto* candidate=findCandidate(rec);require(candidate,"isolated route recalled without priority exceptions");
            require(candidate->continuation.size()==4,"candidate has genuine four-chord suffix");
            for(std::size_t i=0;i<4;++i) {
                ChordEvent actual;actual.name=candidate->continuation[i].label;ChordEvent expected;expected.name=tails[key][i];
                const auto a=normalizeChord(actual),b=normalizeChord(expected);
                require(a.root==b.root&&a.quality==b.quality,"candidate preserves independent expected harmony");
                require(closeQN(candidate->continuation[i].durationQN,qn),"candidate output uses input QN");
            }
            const auto session=imported(p);const auto built=preview::buildSequence(session,candidate,90);
            midi::CandidateExportContext ec;ec.imported=session;ec.meter=meter;ec.tempo=90;ec.mode=midi::ArrangementMode::BlockChords;
            const auto payload=midi::candidatePayload(ec,*candidate);require(bool(built)&&bool(payload),"candidate preview and export");
            compareMidi(built.sequence,midi::readFromMemory(payload.payload.smfBytes));compareMidi(built.sequence,midi::readFromMemory(payload.payload.dawClipBytes));
            for(const auto& group:rec.groups)for(std::size_t rank=0;rank<group.size();++rank)if(&group[rank]==candidate)
                content<<int(tonics[key])<<'\t'<<meter.numerator<<'/'<<meter.denominator<<'\t'<<qn<<'\t'<<open<<'\t'<<abstractBass<<'\t'<<hf<<'\t'<<rank+1<<'\t'<<candidate->rankingScore<<'\n';
            if(key==0&&!abstractBass&&!hf&&open&&closeQN(qn,meter.numerator*4.0/meter.denominator)) {
                std::string error;require(midi::writeToFile(payload.payload.smfBytes,out/("CANDIDATE_R01_"+std::to_string(meter.numerator)+"-"+std::to_string(meter.denominator)+"_output.mid"),error),"candidate audition MIDI");
            }
        }
        AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::A,Mode::Minor};ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;
        for(int octave:{-12,0,12})for(bool inversion:{false,true}) {
            const auto path=out/("CANDIDATE_input_octave"+std::to_string(octave)+"_inversion"+std::to_string(inversion)+".mid");
            writeInput(path,phrase(prefixes[0],{4,4,4,4}),{4,4},octave,inversion);
            const auto loaded=midi::importFile(path,true);require(bool(loaded),"candidate voiced input import");
            const auto rec=recommendContinuations(makeMatchQuery(loaded.extraction.chords,ctx),stagedIndex,{},weights);
            require(findCandidate(rec),"candidate octave and upper-voice inversion recall");
        }
        for(const auto& conflict:std::vector<std::vector<std::string>>{{"A","G#dim","G","D/F#"},{"Am","G#","G","D/F#"},{"Am","G#dim","G","Dm/F"},{"Am","G#dim","G","D/A"}}) {
            const auto p=symbolic(phrase(conflict,{4,4,4,4},true));
            for(bool hf:{false,true}) {
                ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
                const auto rec=recommendContinuations(makeMatchQuery(p,ctx),stagedIndex,{},weights);
                if(findCandidate(rec)) {
                    trace<<"\nISOLATED_CONFLICT HF="<<hf<<" input=";for(const auto& name:conflict)trace<<name<<" -> ";
                    trace<<session::recommendationDiagnostics(p,ctx,{},rec);
                    std::cerr<<"Candidate conflict: ";for(const auto& name:conflict)std::cerr<<name<<" -> ";std::cerr<<" HF="<<hf<<'\n';
                }
                require(!findCandidate(rec),"candidate rejects core-quality or defining-bass conflict");
            }
        }
        auto names=prefixes[0];names.insert(names.end(),tails[0].begin(),tails[0].end());
        writeInput(out/"CANDIDATE_R01_full_designed_not_recorded.mid",phrase(names,std::vector<double>(8,4.)));
        std::ofstream candidatePerf(out/"candidate-performance.tsv");
        candidatePerf<<"library\tHF\tp50ms\tp95ms\tmaxms\n";
        for(bool hf:{false,true})for(bool added:{false,true}) {
            ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
            const auto query=makeMatchQuery(symbolic(phrase(prefixes[0],{4,4,4,4},true)),ctx);
            std::vector<double> times;
            for(int i=0;i<90;++i) {
                const auto start=std::chrono::steady_clock::now();
                const auto rec=recommendContinuations(query,added?stagedIndex:index,{},weights);
                if(i>=10)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            }
            std::sort(times.begin(),times.end());candidatePerf<<(added?658:657)<<'\t'<<hf<<'\t'<<times[40]<<'\t'<<times[75]<<'\t'<<times.back()<<'\n';
        }
        summary<<"ISOLATED CONTENT: proposed V5/658 tested; V4/657 remains the plugin baseline; musical listening pending.\n";
    }
    require(weights.perGroup==10,"development recommendation helper retains ten choices per group");
    require(!weights.diminishedPassingInsertionException,"Phase3A remains disabled in development default weights");
    summary<<"Compiled HarmonyFirst="<<plugin::harmonyFirstBuild()<<" Phase3A default="<<weights.diminishedPassingInsertionException
        <<" Abstract default="<<weights.abstractChordCompatibility<<'\n';
    for(double qn:{2.,4.})for(bool open:{false,true})for(int key=0;key<3;++key)for(bool hf:{false,true})for(bool p3:{false,true}) {
        auto p=symbolic(phrase({"Am","G#dim","G","D/F#"},{qn,qn,qn,qn},open));
        AnalysisContext ctx;ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;
        if(key)ctx.forcedKey=KeySignature{PitchClass::A,key==1?Mode::Minor:Mode::Major};
        ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        auto w=weights;w.diminishedPassingInsertionException=p3;
        const auto query=makeMatchQuery(p,ctx);const auto rec=recommendContinuations(query,index,{},w);
        require(rec.retrievalTimingMode==ctx.retrievalTimingMode&&rec.phase3aEnabled==p3,"actual switch state recorded");
        require(rec.stats.shortlistSize==657,"A08 included in initial retrieval");
        trace<<"\nR01 qn="<<qn<<" OPEN="<<open<<" keySetting="<<key<<" HF="<<hf<<" P3="<<p3<<session::recommendationDiagnostics(p,ctx,{},rec);
        if(qn==2&&open&&hf&&!p3){const auto a=analyzeHarmony(p,ctx);summary<<"R01 keySetting="<<key<<" selected="<<formatKey(a.selectedKey->key)<<" skeleton="<<a.skeletonIndices.size()<<" output="<<rec.stats.output<<'\n';}
        if(qn==2&&open&&key==0&&hf&&!p3)writeInput(out/"R01_Am_Gsharpdim_G_DoverFsharp_2QN_OPEN.mid",p);
    }
    for(int key=0;key<3;++key)for(int variant=0;variant<2;++variant)for(bool hf:{false,true}) {
        auto p=symbolic(phrase({"F","G","Am"},variant?std::vector<double>{3,5,4}:std::vector<double>{4,4,4},variant!=0));
        AnalysisContext ctx;ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;
        if(key)ctx.forcedKey=KeySignature{key==1?PitchClass::C:PitchClass::A,key==1?Mode::Major:Mode::Minor};
        ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        const auto rec=recommendContinuations(makeMatchQuery(p,ctx),index,{},weights);
        trace<<"\nR02 keySetting="<<key<<" variant="<<variant<<" HF="<<hf<<session::recommendationDiagnostics(p,ctx,{},rec);
        summary<<"R02 keySetting="<<key<<" variant="<<variant<<" HF="<<hf<<" Color="<<rec.groups[3].size()<<'\n';
        if(key==0&&!hf)writeInput(out/(variant?"R02_F3_G5_AmOPEN.mid":"R02_F_G_Am_4QN.mid"),p);
    }
    // Known input QN takes precedence. A uniform Session meter supplies only
    // the otherwise missing time scale, retaining prototype rhythm ratios.
    {
        const auto& item=*std::find_if(db.templates.begin(),db.templates.end(),[](const auto& t){return t.id=="V4_B11";});
        AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        auto p=phrase({"Cmaj9"},{4},true);
        for(const auto meter:{midi::Meter{4,4},{3,4},{6,8},{5,4},{7,8},{9,8}}) {
            ctx.timeSigNumerator=meter.numerator;ctx.timeSigDenominator=meter.denominator;
            auto query=makeMatchQuery(p,ctx);MatchResult match;match.key=*ctx.forcedKey;
            AlignmentStep step;step.queryIndex=0;step.templateIndex=0;step.operation=AlignmentOp::Match;match.alignmentTrace.push_back(step);
            const auto timing=estimateRhythmScale(match,query,item);
            const auto bar=meter.numerator*4.0/meter.denominator;
            require(closeQN(timing.scale,bar/4.)&&timing.samples==0&&timing.confidence==0,"uniform meter fallback has no invented duration evidence");
            require(std::string(timing.source)=="session-meter-fallback","meter fallback is diagnosed");
            auto special=item;special.full[0].durationQN=2.;
            require(closeQN(*special.full[0].durationQN*estimateRhythmScale(match,query,special).scale,bar/2.),"meter fallback preserves half-bar prototype rhythm");
            const auto rec=recommendContinuations(query,index,{},weights);
            bool checkedOutput{};
            for(const auto& group:rec.groups)for(const auto& c:group) {
                if(c.primaryTemplate!="V4_B11")continue;
                checkedOutput=true;
                require(c.matchEnd<item.full.size()&&c.suggestedCurrentChordDurationQN&&
                    closeQN(*c.suggestedCurrentChordDurationQN,item.full[c.matchEnd].durationQN.value_or(4.)*bar/4.),
                    "single OPEN hold preserves matched prototype bar fraction");
                const auto preview=preview::buildSequence(imported(p),&c,90);
                midi::CandidateExportContext ec;ec.imported=imported(p);ec.meter=meter;ec.tempo=90;ec.mode=midi::ArrangementMode::BlockChords;
                const auto payload=midi::candidatePayload(ec,c);require(bool(preview)&&bool(payload),"meter-only output builds");
                compareMidi(preview.sequence,midi::readFromMemory(payload.payload.smfBytes));
                compareMidi(preview.sequence,midi::readFromMemory(payload.payload.dawClipBytes));
            }
            require(checkedOutput,"meter-only fallback reaches a real Factory recommendation");
            query.timeSigNumerator.reset();query.timeSigDenominator.reset();
            require(closeQN(estimateRhythmScale(match,query,item).scale,1.),"unknown meter and duration preserve prototype");
        }
    }
    meters<<"id\tmeter\tprototypeQN\tfinalQN\tOPENhold\tendQN\n";
    std::vector<const ProgressionTemplate*> sampled;
    for(const auto& id:{"V4_A08","V4_B08","V4_B11","POP_011","FUNCTIONAL_011","V3_SHADE_004"}){
        const auto item=std::find_if(db.templates.begin(),db.templates.end(),[&](const auto& t){return t.id==id;});require(item!=db.templates.end(),"Factory timing sample present");sampled.push_back(&*item);
    }
    for(const auto& item:db.templates)if(item.full.size()>3 && std::any_of(item.full.begin(),item.full.end(),[](const auto& e){return e.durationQN&&*e.durationQN!=4.;})){sampled.push_back(&item);if(sampled.size()==10)break;}
    for(const auto* item:sampled)for(const auto meter:{midi::Meter{4,4},{3,4},{6,8},{5,4},{7,8},{9,8}})for(bool open:{false,true}) {
        KeySignature key{PitchClass::C,item->mode};Progression p;
        const double expected=(meter.numerator*4.0/meter.denominator)/(item->meterNumerator*4.0/item->meterDenominator);
        for(std::size_t i=0;i<2;++i){auto e=realizeContinuation(item->full[i],key,item->full[i].durationQN.value_or(4.)*expected);ChordEvent c=e.harmonicData.value_or(ChordEvent{});c.name=e.label;c.startQN=i==0?0.:p.back().startQN+*p.back().durationQN;c.durationQN=e.durationQN;c.openEnded=false;p.push_back(c);}
        if(open){p.back().durationQN.reset();p.back().openEnded=true;}
        AnalysisContext ctx;ctx.forcedKey=key;ctx.timeSigNumerator=meter.numerator;ctx.timeSigDenominator=meter.denominator;
        auto query=makeMatchQuery(p,ctx);MatchResult match;match.key=key;match.alignmentTrace={{0,0,AlignmentOp::Match},{1,1,AlignmentOp::Match}};
        const auto rhythm=estimateRhythmScale(match,query,*item);
        require(closeQN(rhythm.scale,expected),"input duration estimate incl sparse OPEN evidence");
        auto noMeter=query;noMeter.timeSigNumerator.reset();noMeter.timeSigDenominator.reset();
        auto otherMeter=query;otherMeter.timeSigNumerator=7;otherMeter.timeSigDenominator=8;
        require(closeQN(estimateRhythmScale(match,noMeter,*item).scale,rhythm.scale)&&closeQN(estimateRhythmScale(match,otherMeter,*item).scale,rhythm.scale),"meter alone never changes output rhythm");
        if(item->id=="V4_B11"&&open)for(bool hf:{false,true}) {
            auto actualContext=ctx;actualContext.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
            const auto actual=recommendContinuations(makeMatchQuery(p,actualContext),index,{},weights);
            bool found{};
            for(const auto& group:actual.groups)for(const auto& c:group)
                if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),item->id)!=c.supportingTemplates.end()) {
                    found=true;require(closeQN(c.rhythmScale,expected),"real recommendation uses input rhythm scale");
                    require(c.suggestedCurrentChordDurationQN&&closeQN(*c.suggestedCurrentChordDurationQN,item->full[1].durationQN.value_or(4.)*expected),"real recommendation estimates OPEN hold");
                    require(!c.continuation.empty()&&closeQN(c.continuation.front().durationQN,item->full[2].durationQN.value_or(4.)*expected),"real recommendation suffix timing");
                    const auto actualSession=imported(p);const auto actualPreview=preview::buildSequence(actualSession,&c,120);
                    midi::CandidateExportContext actualExport;actualExport.imported=actualSession;actualExport.meter=meter;actualExport.mode=midi::ArrangementMode::BlockChords;
                    const auto actualPayload=midi::candidatePayload(actualExport,c);
                    require(bool(actualPreview)&&bool(actualPayload),"real recommendation preview and export");
                    compareMidi(actualPreview.sequence,midi::readFromMemory(actualPayload.payload.smfBytes));
                    compareMidi(actualPreview.sequence,midi::readFromMemory(actualPayload.payload.dawClipBytes));
                }
            require(found,"real side-slip candidate survives normal quality gate");
        }
        ContinuationCandidate candidate;candidate.id=item->id;candidate.key=key;candidate.primaryTemplate=item->id;
        candidate.suggestedCurrentChordDurationQN=item->full[1].durationQN.value_or(4.)*rhythm.scale;
        for(std::size_t i=2;i<item->full.size();++i)candidate.continuation.push_back(realizeContinuation(item->full[i],key,item->full[i].durationQN.value_or(4.)*rhythm.scale));
        const auto session=imported(p);auto preview=preview::buildSequence(session,&candidate,120);require(bool(preview),"meter preview build");
        midi::CandidateExportContext ec;ec.imported=session;ec.meter=meter;ec.mode=midi::ArrangementMode::BlockChords;
        const auto payload=midi::candidatePayload(ec,candidate);require(bool(payload),"meter export");
        const auto file=midi::readFromMemory(payload.payload.smfBytes);const auto drag=midi::readFromMemory(payload.payload.dawClipBytes);
        compareMidi(preview.sequence,file);compareMidi(preview.sequence,drag);
        require(!file.file.meters.empty()&&file.file.meters.front().numerator==meter.numerator&&file.file.meters.front().denominator==meter.denominator,"SMF meter meta");
        for(std::size_t i=2;i<item->full.size();++i)require(closeQN(candidate.continuation[i-2].durationQN,item->full[i].durationQN.value_or(4.)*expected),"relative prototype rhythms preserved");
        const auto captured=snapshot::capture(session,candidate,{},120,meter.numerator,meter.denominator);const auto restored=snapshot::deserialize(snapshot::serialize(captured));require(bool(restored),"snapshot decode");
        require(restored.value.candidate.continuation.size()==candidate.continuation.size()&&closeQN(restored.value.candidate.continuation[0].durationQN,candidate.continuation[0].durationQN),"snapshot keeps mapped timing");
        meters<<item->id<<'\t'<<meter.numerator<<'/'<<meter.denominator<<'\t'<<item->full[2].durationQN.value_or(4.)<<'\t'<<candidate.continuation[0].durationQN<<'\t'<<candidate.suggestedCurrentChordDurationQN.value_or(0)<<'\t'<<preview.sequence.totalQN<<'\n';
        if(open){std::string error;require(midi::writeToFile(payload.payload.smfBytes,out/(item->id+"_"+std::to_string(meter.numerator)+"-"+std::to_string(meter.denominator)+"_output.mid"),error),"meter MIDI write");}
    }
    for(bool hf:{false,true}) {
        AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::C,Mode::Major};ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        const auto p=symbolic(phrase({"F","F#dim7","G","C"},{4,.5,3.5,4}));const auto a=analyzeHarmony(p,ctx);
        const auto rec=recommendContinuations(makeMatchQuery(p,ctx),index,{},weights);
        trace<<"\nFULL_SKELETON_REQUESTED HF="<<hf<<session::recommendationDiagnostics(p,ctx,{},rec);
        summary<<"Requested skeleton HF="<<hf<<" size="<<a.skeletonIndices.size()<<" dimWeight="<<a.full[1].structuralWeight<<" retrieval="<<a.full[1].retrievalStructuralWeight<<'\n';
        require(a.full.size()==4,"FULL retains passing diminished chord");
        if(!hf){require(a.skeletonIndices.size()<a.full.size(),"proven FULL/SKELETON difference");writeInput(out/"FULL_SKELETON_F_Fsharpdim7_G_C.mid",p);}
        // In HarmonyFirst, time-independent retrieval can retain the node: explain rather than force deletion.
    }
    {
        const auto p=symbolic(phrase({"Am","G#dim","G","D/F#"},{2,2,2,2},true));
        AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::A,Mode::Minor};ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;ctx.retrievalTimingMode=RetrievalTimingMode::HarmonyFirst;
        const auto a=analyzeHarmony(p,ctx);require(a.skeletonIndices.size()==3&&std::find(a.skeletonIndices.begin(),a.skeletonIndices.end(),1)==a.skeletonIndices.end(),"HarmonyFirst proven skeleton difference in minor context");
        writeInput(out/"FULL_SKELETON_HF_Am_Gsharpdim_G_DoverFsharp_Aminor.mid",p);
    }
    for(int fixture=0;fixture<2;++fixture)for(bool omit:{false,true})for(bool hf:{false,true}) {
        auto full=fixture?phrase({"Cmaj9","Dbmaj9","Cmaj9"},{4,4,4}):phrase({"C","Cmaj7/B","C7/Bb","F/A","Fm/Ab"},{4,4,4,4,4});
        auto p=full;p.resize(fixture?2:3);p=symbolic(p,omit);
        AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::C,Mode::Major};ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        const auto rec=recommendContinuations(makeMatchQuery(p,ctx),index,{},weights);const auto target=fixture?"V4_B11":"V4_B08";bool retained{};
        for(const auto& group:rec.groups)for(const auto& c:group)if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),target)!=c.supportingTemplates.end()) {
            retained=true;require(!c.continuation.empty(),"musical continuation present");summary<<target<<" HF="<<hf<<" omitted5="<<omit<<" score="<<c.rankingScore<<" next="<<c.continuation.front().label<<'\n';
            if(fixture) {const auto n=chordPitches(*c.continuation.front().harmonicData);require((n.intervals&(1<<11))&&!(n.intervals&(1<<10)),"maj9 does not become dominant9");}
        }
        require(retained,"bass/side-slip Factory source retained");
        require(!fixture || normalizeChord(p[0]).quality==ChordQuality::Major7,"Cmaj9 core identity");
        if(!fixture)require(normalizeChord(p[1]).bass==PitchClass::B && normalizeChord(p[2]).bass==PitchClass::Bb,"explicit descending bass survives omitted fifth");
        trace<<"\nMUSICAL fixture="<<fixture<<" omit5="<<omit<<" HF="<<hf<<session::recommendationDiagnostics(p,ctx,{},rec);
        if(!omit&&!hf){writeInput(out/(fixture?"Side_slipping_INPUT_2chords.mid":"Descending_bass_INPUT_3chords.mid"),p);writeInput(out/(fixture?"Side_slipping_FULL_3chords.mid":"Descending_bass_FULL_5chords.mid"),full);}
        if(omit&&!hf) {
            auto midiInput=p;
            // A two-tone C/E dyad has no unique MIDI chord identity. Keep the
            // opening C triad; omit fifths from the following seventh chords.
            if(!fixture)midiInput.front()=full.front();
            writeInput(out/(fixture?"Side_slipping_INPUT_2chords_omitted5.mid":"Descending_bass_INPUT_3chords_omitted5.mid"),midiInput);
        }
    }
    // SDK ProcessContext -> snapshot -> validated meter, including invalid fallback.
    plugin::HostContextAdapter adapter;Steinberg::Vst::ProcessContext pc{};pc.state=Steinberg::Vst::ProcessContext::kTimeSigValid;pc.timeSigNumerator=3;pc.timeSigDenominator=4;adapter.capture(&pc);plugin::HostSnapshot hs{};require(adapter.read(hs),"snapshot read");require(plugin::HostContextAdapter::timeline(hs).timeSignature==std::pair(3,4),"ProcessContext meter");
    pc.state=0;adapter.capture(&pc);require(adapter.read(hs)&&!plugin::HostContextAdapter::timeline(hs).timeSignature,"invalid host meter remains unknown");
    {
        plugin::RecommendationWorker worker(HC_FACTORY_DB_PATH,out/"isolated-user.db");auto p=phrase({"C","F","G"},{4,4,4},true);
        AnalysisContext ctx;ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;
        worker.submit(p,ctx,{},false,1);wait(worker);
        worker.submit(p,ctx,{},true,1);require(wait(worker).analysisReused,"ranking-only cache reuse");
        ctx.timeSigNumerator=3;worker.submit(p,ctx,{},true,1);require(!wait(worker).analysisReused,"meter change invalidates analysis cache");
        ctx.retrievalTimingMode=RetrievalTimingMode::HarmonyFirst;worker.submit(p,ctx,{},true,1);auto r=wait(worker);require(!r.analysisReused&&r.recommendations.retrievalTimingMode==RetrievalTimingMode::HarmonyFirst,"mode change reaches worker");
        const auto configuredContext=plugin::developmentContext({});
        worker.submit(p,configuredContext,{},false,2);const auto configuredResult=wait(worker);
        require(configuredResult.recommendations.retrievalTimingMode==configuredContext.retrievalTimingMode,"worker result follows compiled HarmonyFirst helper");
        require(configuredResult.recommendations.phase3aEnabled==weights.diminishedPassingInsertionException,"worker Phase3A result matches development weights helper");
        require(configuredResult.recommendations.abstractChordCompatibility==weights.abstractChordCompatibility,"worker Abstract result matches development weights helper");
        ctx.forcedKey=KeySignature{PitchClass::C,Mode::Major};worker.submit(p,ctx,{},true,1);require(!wait(worker).analysisReused,"key change invalidates cache");
    }
    perf<<"HarmonyFirst\tmedianMs\tp95Ms\tmaxMs\tCPUms\tworkingSetBefore\tworkingSetAfter\tpeakWorkingSet\n";
    for(bool hf:{false,true}){std::vector<double> times;AnalysisContext ctx;ctx.forcedKey=KeySignature{PitchClass::A,Mode::Minor};ctx.timeSigNumerator=4;ctx.timeSigDenominator=4;ctx.retrievalTimingMode=hf?RetrievalTimingMode::HarmonyFirst:RetrievalTimingMode::Legacy;
        const auto query=makeMatchQuery(phrase({"Am","G#dim","G","D/F#"},{2,2,2,2},true),ctx);
        double cpuMs{};std::size_t rssBefore{},rssAfter{},rssPeak{};
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));rssBefore=memory.WorkingSetSize;
        FILETIME creation{},exit{},kernelBefore{},userBefore{},kernelAfter{},userAfter{};
        GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernelBefore,&userBefore);
#endif
        for(int i=0;i<90;++i){const auto start=std::chrono::steady_clock::now();const auto r=recommendContinuations(query,index,{},weights);const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(i>=10)times.push_back(ms);require(r.stats.shortlistSize==657,"stress candidate count stable");}
#if defined(_WIN32)
        GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernelAfter,&userAfter);
        const auto ticks=[](FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
        cpuMs=(ticks(kernelAfter)+ticks(userAfter)-ticks(kernelBefore)-ticks(userBefore))/10000.;
        GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));rssAfter=memory.WorkingSetSize;rssPeak=memory.PeakWorkingSetSize;
#endif
        std::sort(times.begin(),times.end());perf<<hf<<'\t'<<times[times.size()/2]<<'\t'<<times[75]<<'\t'<<times.back()<<'\t'<<cpuMs<<'\t'<<rssBefore<<'\t'<<rssAfter<<'\t'<<rssPeak<<'\n';
    }
    summary<<"PASS checks="<<checks<<"; Factory=657; synthetic tests only; true R01 installed ON identity and R02 host cause require diagnostic export.\n";
    std::cout<<"Phase4D PASS "<<checks<<" checks\n";return 0;
  }catch(const std::exception& e){const auto phase=argc>3?std::string(argv[3]):std::string{};
    std::cerr<<(phase=="unified"||phase.empty()?"UnifiedIntegration":phase=="phase51"?"Phase51":phase=="phase5"?"Phase5":phase=="phase6"?"Phase6":phase=="phase61"?"Phase61":"Phase4D")<<" FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
