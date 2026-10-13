#include "plugin/DevelopmentMode.h"
#include "plugin/RecommendationWorker.h"
#include "plugin/HostContextAdapter.h"
#include "plugin/VstXmlChordParser.h"
#include "session/ProductServices.h"
#include "midi/MidiWorkflow.h"
#include "midi/MidiImportWorkflow.h"
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
  }catch(const std::exception& e){std::cerr<<(argc>3&&std::string(argv[3])=="phase5"?"Phase5":"Phase4D")<<" FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
