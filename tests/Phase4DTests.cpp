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
#include <sstream>
#include <thread>
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
}
int main(int argc,char** argv) {
 try {
    const std::filesystem::path out=argc>1?argv[1]:"phase4d";std::filesystem::create_directories(out);
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
    summary<<"Compiled HarmonyFirst="<<plugin::harmonyFirstBuild()<<" Phase3A default="<<weights.diminishedPassingInsertionException<<'\n';
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
 }catch(const std::exception& e){std::cerr<<"Phase4D FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
