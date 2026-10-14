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
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
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
                        const auto wrapEnd=match->templateMatchStart==0?1:match->templateMatchStart;
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
    std::cerr<<(phase=="phase51"?"Phase51":phase=="phase5"?"Phase5":"Phase4D")<<" FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
