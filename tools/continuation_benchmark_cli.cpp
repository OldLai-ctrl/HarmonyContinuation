#include "benchmark/Benchmark.h"
#include "benchmark/BenchJson.h"
#include "product/ProductVersion.h"
#include "core/HarmonyAnalysis.h"
#include "library/ProgressionLibrary.h"
#include "midi/StandardMidiFileWriter.h"
#include "preview/OfflinePreviewRenderer.h"
#include "session/ProductServices.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using namespace harmony;
using harmony::benchmark::json::Value;
constexpr std::array<const char*,4> groups{"resolve","develop","loop","color"};
Value object(){return Value::map();}
Value list(){return Value::list();}
std::string pathSignature(const ContinuationCandidate& candidate) {
    std::ostringstream out;out.precision(17);
    if(candidate.suggestedCurrentChordDurationQN)out<<*candidate.suggestedCurrentChordDurationQN;
    out<<'|';for(const auto& chord:candidate.continuation)out<<chord.label<<':'<<chord.durationQN<<';';
    return out.str();
}
void writeFile(const std::filesystem::path& path,const std::string& data) {
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out)throw std::runtime_error("cannot write "+path.string());
    out<<data;out.close();if(!out)throw std::runtime_error("write failed: "+path.string());
}
struct Totals {
    int caseCount{},candidateCount{},previewSuccess{},midiSuccess{},snapshotSuccess{},offlineSuccess{};
    int sameGroupDuplicates{},crossGroupDuplicates{},completeCount{},lowScoreCount{};
    std::array<int,4> present{},empty{},candidatePerGroup{};
    std::array<int,4> lengths{}; // 1, 2-3, 4-6, 7+
    std::map<std::string,int> categories;
    int errors{},warnings{};
};
Value runCase(const benchmark::Case& input,const CandidateIndex& index,Totals& total) {
    ++total.caseCount;++total.categories[input.category];
    Value result=object();
    result.object["id"]=input.id;result.object["name"]=input.name;
    result.object["category"]=input.category;result.object["notes"]=input.notes;
    Value expectations=list();for(const auto& e:input.expectedCharacteristics)expectations.array.emplace_back(e);
    result.object["expectedCharacteristics"]=std::move(expectations);
    Value source=list();
    for(const auto& chord:input.scenario.chords) {
        auto value=object();value.object["name"]=chord.name;value.object["startQN"]=chord.startQN;
        if(chord.durationQN)value.object["durationQN"]=*chord.durationQN;
        else value.object["durationQN"]={};
        source.array.push_back(std::move(value));
    }
    result.object["input"]=std::move(source);
    Value errors=list(),warnings=list(),keyCandidates=list(),groupData=object();
    const auto addError=[&](std::string text){errors.array.emplace_back(std::move(text));++total.errors;};
    const auto addWarning=[&](std::string text){warnings.array.emplace_back(std::move(text));++total.warnings;};
    AnalysisContext context;context.forcedKey=input.scenario.forcedKey;
    context.timeSigNumerator=input.scenario.meterNumerator;
    context.timeSigDenominator=input.scenario.meterDenominator;
    const auto analysis=analyzeHarmony(input.scenario.chords,context);
    const auto query=makeMatchQuery(input.scenario.chords,context);
    for(const auto& key:analysis.keyCandidates) {
        auto item=object();item.object["key"]=formatKey(key.key);
        item.object["score"]=key.score;item.object["confidence"]=key.confidence;
        keyCandidates.array.push_back(std::move(item));
    }
    if(analysis.full.size()!=input.scenario.chords.size()||query.interpretations.empty())
        addError("analysis or match query unavailable");
    const RecommendationRequest request{input.scenario.style,input.scenario.intent};
    const auto recommendations=recommendContinuations(query,index,request);
    ImportedProgressionSession imported;
    if(!imported.replace(input.scenario.chords,TimelineCoordinateMode::RelativeToSelection))
        addError("cannot prepare progression session");
    std::set<std::string> ids;
    std::map<std::string,std::string> pathOwner;
    bool offlineDone{};int caseCandidates{};
    for(std::size_t g=0;g<groups.size();++g) {
        Value candidates=list();
        const auto& sourceGroup=recommendations.groups[g];
        if(sourceGroup.empty())++total.empty[g];else ++total.present[g];
        total.candidatePerGroup[g]+=static_cast<int>(sourceGroup.size());
        std::set<std::string> groupPaths;
        for(std::size_t rank=0;rank<sourceGroup.size();++rank) {
            const auto& candidate=sourceGroup[rank];++total.candidateCount;++caseCandidates;
            const auto signature=pathSignature(candidate);
            if(rank<2&&!groupPaths.insert(signature).second){++total.sameGroupDuplicates;addError(std::string(groups[g])+" Top 2 identical path");}
            else groupPaths.insert(signature);
            const auto [prior,itInserted]=pathOwner.emplace(signature,groups[g]);
            if(!itInserted&&prior->second!=groups[g]){
                ++total.crossGroupDuplicates;addWarning("CrossGroupDuplicate: "+prior->second+" / "+groups[g]);
            }
            if(!ids.insert(candidate.id).second)addError("duplicate candidate ID: "+candidate.id);
            if(candidate.continuation.empty())addError("empty continuation");
            if(!std::isfinite(candidate.rankingScore)||!std::isfinite(candidate.matchSimilarity))
                addError("nonfinite candidate score");
            if(candidate.rankingScore<60)++total.lowScoreCount;
            if(candidate.continuation.size()>=2)++total.completeCount;
            const auto length=candidate.continuation.size();
            ++total.lengths[length<=1?0:length<=3?1:length<=6?2:3];
            if(input.scenario.chords.back().openEnded&&
               (!candidate.suggestedCurrentChordDurationQN||
                !std::isfinite(*candidate.suggestedCurrentChordDurationQN)||*candidate.suggestedCurrentChordDurationQN<=0))
                addError("invalid OPEN hold");
            Value row=object();row.object["rank"]=static_cast<int>(rank+1);
            row.object["id"]=candidate.id;
            row.object["fingerprint"]=session::continuationFingerprint(candidate);
            row.object["pathSignature"]=signature;
            row.object["score"]=candidate.rankingScore;
            row.object["matchSimilarity"]=candidate.matchSimilarity;
            row.object["intent"]=std::string(intentName(candidate.intent));
            row.object["key"]=formatKey(candidate.key);
            row.object["cadence"]=std::string(cadenceName(candidate.cadence));
            row.object["primaryTemplate"]=candidate.primaryTemplate;
            if(candidate.suggestedCurrentChordDurationQN)
                row.object["openHoldQN"]=*candidate.suggestedCurrentChordDurationQN;
            else row.object["openHoldQN"]={};
            Value path=list(),sources=list();
            for(const auto& chord:candidate.continuation) {
                auto event=object();event.object["label"]=chord.label;event.object["durationQN"]=chord.durationQN;
                event.object["roles"]=static_cast<double>(chord.roles);
                if(chord.label.empty()||!std::isfinite(chord.durationQN)||chord.durationQN<=0)
                    addError("invalid continuation chord");
                path.array.push_back(std::move(event));
            }
            for(const auto& id:candidate.supportingTemplates)sources.array.emplace_back(id);
            row.object["path"]=std::move(path);row.object["sourceTemplates"]=std::move(sources);
            if(g==2&&candidate.cadence!=CadenceType::LoopClosure)
                addWarning("Loop candidate without LoopClosure: "+candidate.id);
            if(g==3&&std::none_of(candidate.continuation.begin(),candidate.continuation.end(),[](const auto& e){
                return hasRole(e.roles,Role::Borrowed)||hasRole(e.roles,Role::SecondaryDominant)||
                    hasRole(e.roles,Role::SecondaryLeadingTone);}))
                addWarning("Color candidate lacks explicit color role: "+candidate.id);
            const auto preview=preview::buildSequence(imported,&candidate,input.scenario.tempo);
            if(preview) {
                ++total.previewSuccess;row.object["previewSuccess"]=true;
                const auto clip=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,
                    midi::ExportScope::FullPhrase,{input.scenario.meterNumerator,input.scenario.meterDenominator},
                    candidate.key,candidate.intent);
                const auto written=clip?midi::writeToMemory(clip.sequence):midi::WriteResult{};
                if(clip&&written&&!written.bytes.empty()){
                    ++total.midiSuccess;row.object["midiSuccess"]=true;
                }else {row.object["midiSuccess"]=false;addError("MIDI export failed: "+candidate.id);}
                if(!offlineDone) {
                    const auto audio=preview::renderOffline(preview.sequence,16000);
                    if(!audio.left.empty()&&audio.left.size()==audio.right.size()){
                        ++total.offlineSuccess;row.object["offlineRenderSuccess"]=true;
                    }else {row.object["offlineRenderSuccess"]=false;addError("offline render failed");}
                    offlineDone=true;
                }
            }else {row.object["previewSuccess"]=false;row.object["midiSuccess"]=false;
                addError("PreviewSequence failed: "+candidate.id+" "+preview.error);}
            try {
                const auto snap=snapshot::capture(imported,candidate,recommendations.matches,input.scenario.tempo,
                    input.scenario.meterNumerator,input.scenario.meterDenominator,
                    input.scenario.forcedKey?input.scenario.forcedKey:std::optional(candidate.key),
                    input.scenario.style,input.scenario.intent);
                const auto decoded=snapshot::deserialize(snapshot::serialize(snap));
                if(!decoded||session::continuationFingerprint(decoded.value.candidate)!=
                    session::continuationFingerprint(candidate))throw std::runtime_error("snapshot readback mismatch");
                ++total.snapshotSuccess;row.object["snapshotSuccess"]=true;
            }catch(const std::exception& e){row.object["snapshotSuccess"]=false;
                addError("snapshot failed: "+candidate.id+" "+e.what());}
            candidates.array.push_back(std::move(row));
        }
        groupData.object[groups[g]]=std::move(candidates);
    }
    if(caseCandidates==0||!offlineDone)addError("no auditionable continuation");
    result.object["keyCandidates"]=std::move(keyCandidates);
    result.object["groups"]=std::move(groupData);
    result.object["warnings"]=std::move(warnings);
    result.object["errors"]=std::move(errors);
    return result;
}
Value summary(const Totals& t) {
    auto result=object();result.object["caseCount"]=t.caseCount;
    result.object["candidateCount"]=t.candidateCount;
    result.object["previewSuccess"]=t.previewSuccess;
    result.object["midiSuccess"]=t.midiSuccess;
    result.object["snapshotSuccess"]=t.snapshotSuccess;
    result.object["offlineRenderSuccess"]=t.offlineSuccess;
    result.object["sameGroupDuplicates"]=t.sameGroupDuplicates;
    result.object["crossGroupDuplicates"]=t.crossGroupDuplicates;
    result.object["completeContinuationRatio"]=t.candidateCount?
        static_cast<double>(t.completeCount)/t.candidateCount:0.;
    result.object["lowScoreRatio"]=t.candidateCount?static_cast<double>(t.lowScoreCount)/t.candidateCount:0.;
    result.object["structuralErrors"]=t.errors;result.object["warnings"]=t.warnings;
    auto length=object();for(int i=0;i<4;++i)length.object[std::array<const char*,4>{"one","twoToThree","fourToSix","sevenPlus"}[i]]=t.lengths[i];
    result.object["continuationLengths"]=std::move(length);
    auto category=object();for(const auto& [name,count]:t.categories)category.object[name]=count;
    result.object["categories"]=std::move(category);
    auto group=object();for(std::size_t i=0;i<groups.size();++i){auto detail=object();
        detail.object["casesWithCandidates"]=t.present[i];detail.object["emptyCases"]=t.empty[i];
        detail.object["emptyRatio"]=t.caseCount?static_cast<double>(t.empty[i])/t.caseCount:0.;
        detail.object["averageCandidates"]=t.caseCount?static_cast<double>(t.candidatePerGroup[i])/t.caseCount:0.;
        group.object[groups[i]]=std::move(detail);}
    result.object["groups"]=std::move(group);return result;
}
std::string markdown(const Value& report) {
    const auto& s=report.at("summary");std::ostringstream out;
    out<<"# Continuation benchmark baseline\n\n";
    out<<"Cases: "<<s.at("caseCount").integer(0,100000)<<"; candidates: "
       <<s.at("candidateCount").integer(0,100000)<<".\n\n";
    out<<"Preview / MIDI / Snapshot successes: "<<s.at("previewSuccess").integer(0,100000)<<" / "
       <<s.at("midiSuccess").integer(0,100000)<<" / "<<s.at("snapshotSuccess").integer(0,100000)<<".\n\n";
    out<<"Complete continuation ratio (at least two events): "
       <<s.at("completeContinuationRatio").finite()<<". Structural errors: "
       <<s.at("structuralErrors").integer(0,100000)<<".\n\n";
    out<<"Same-group Top 2 duplicate paths: "<<s.at("sameGroupDuplicates").integer(0,100000)
       <<"; cross-group duplicate occurrences: "<<s.at("crossGroupDuplicates").integer(0,100000)<<".\n\n";
    const auto& lengths=s.at("continuationLengths");
    out<<"Path lengths: one chord "<<lengths.at("one").integer(0,100000)
       <<", 2–3 chords "<<lengths.at("twoToThree").integer(0,100000)
       <<", 4–6 chords "<<lengths.at("fourToSix").integer(0,100000)
       <<", 7+ chords "<<lengths.at("sevenPlus").integer(0,100000)<<".\n\n";
    out<<"| Group | Cases with options | Empty cases | Average candidates |\n| --- | ---: | ---: | ---: |\n";
    for(const auto name:groups){const auto& g=s.at("groups").at(name);
        out<<"| "<<name<<" | "<<g.at("casesWithCandidates").integer(0,100000)<<" | "
           <<g.at("emptyCases").integer(0,100000)<<" | "<<g.at("averageCandidates").finite()<<" |\n";}
    out<<"\nThis is a machine baseline, not a quality verdict. Listen and rate candidates before changing weights.\n";
    return out.str();
}
} // namespace
int main(int argc,char** argv) {
    if (harmony::product::printVersionIfRequested(argc, argv)) return 0;
    try {
        std::filesystem::path caseDir=HC_BENCH_CASE_DIR;
        std::filesystem::path factoryPath=std::filesystem::path(argv[0]).parent_path()/"factory.db";
        std::filesystem::path output="reports/phase5_baseline.json";
        std::string oneCase,commit="unknown";bool all=false;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];
            if(arg=="--all")all=true;
            else if(i+1<argc&&arg=="--case")oneCase=argv[++i];
            else if(i+1<argc&&arg=="--output")output=argv[++i];
            else if(i+1<argc&&arg=="--cases")caseDir=argv[++i];
            else if(i+1<argc&&arg=="--factory")factoryPath=argv[++i];
            else if(i+1<argc&&arg=="--commit")commit=argv[++i];
            else throw std::runtime_error("usage: continuation_benchmark_cli (--all | --case bench_001) [--output report-dir-or-json] [--factory factory.db]");
        }
        if(all==!oneCase.empty())throw std::runtime_error("choose --all or --case ID");
        const auto loaded=library::loadFactory(factoryPath);
        if(!loaded)throw std::runtime_error("factory: "+loaded.error);
        if(loaded.templates.size()!=161)throw std::runtime_error("factory template count changed from 161");
        const CandidateIndex index(loaded.templates);
        std::vector<std::filesystem::path> files;
        if(all)files=benchmark::caseFiles(caseDir);
        else files={caseDir/(oneCase+".json")};
        if(files.empty())throw std::runtime_error("no benchmark cases");
        auto report=object();report.object["schemaVersion"]=1;
        report.object["productVersion"]=std::string(product::version);
        report.object["sourceCommit"]=commit;
        report.object["factoryTemplateCount"]=static_cast<int>(loaded.templates.size());
        report.object["matchingConfigVersion"]=benchmark::matchingConfigVersion;
        report.object["recommendationConfigVersion"]=benchmark::recommendationConfigVersion;
        report.object["sessionSchemaVersion"]=static_cast<int>(session::PluginSessionState::currentSchemaVersion);
        Value caseResults=list();Totals total;std::set<std::string> names;
        for(const auto& path:files){const auto item=benchmark::loadCase(path);
            if(!names.insert(item.name).second)throw std::runtime_error("duplicate benchmark name");
            caseResults.array.push_back(runCase(item,index,total));}
        report.object["cases"]=std::move(caseResults);
        report.object["summary"]=summary(total);
        if(output.extension()!=".json")output/="continuation_report.json";
        writeFile(output,benchmark::json::dump(report)+"\n");
        auto brief=output;brief.replace_extension(".md");writeFile(brief,markdown(report));
        std::cout<<"benchmark "<<total.caseCount<<" cases, "<<total.candidateCount<<" candidates, "
                 <<total.errors<<" structural errors; report="<<output.string()<<'\n';
        return total.errors?1:0;
    }catch(const std::exception& e){std::cerr<<"benchmark FAIL: "<<e.what()<<'\n';return 2;}
}
