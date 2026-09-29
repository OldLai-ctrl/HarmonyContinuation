#include "benchmark/BenchJson.h"
#include "demo/DemoScenario.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "preview/VoiceLeadingMetrics.h"
#include "product/ProductVersion.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using harmony::benchmark::json::Value;
std::string signature(const harmony::Progression& chords) {
    std::ostringstream out;out.precision(17);
    for(const auto& chord:chords) {
        out<<chord.name<<'@'<<chord.startQN<<':' ;
        if(chord.durationQN)out<<*chord.durationQN;
        else out<<"OPEN";
        out<<';';
    }
    return out.str();
}
}
int main(int argc,char** argv) {
    if(harmony::product::printVersionIfRequested(argc,argv))return 0;
    try {
        if(argc!=3||std::string_view(argv[1])!="--output")
            throw std::runtime_error("usage: enrichment_benchmark_cli --output report.json");
        std::vector<std::filesystem::path> files;
        for(const auto& entry:std::filesystem::directory_iterator(HC_ENRICHMENT_CASE_DIR))
            if(entry.path().extension()==".json")files.push_back(entry.path());
        std::sort(files.begin(),files.end());
        Value report=Value::map();report.object["schemaVersion"]=1;
        report.object["version"]=std::string(harmony::product::version);
        report.object["cases"]=Value::list();
        int candidates{};
        for(const auto& file:files) {
            const auto loaded=harmony::demo::loadScenario(file);
            if(!loaded)throw std::runtime_error(file.string()+": "+loaded.error);
            harmony::AnalysisContext context;context.forcedKey=loaded.scenario.forcedKey;
            const auto analysis=harmony::analyzeHarmony(loaded.scenario.chords,context);
            const auto result=harmony::enrichment::enrichProgression(loaded.scenario.chords,analysis,
                                                                       loaded.scenario.style);
            if(!result.error.empty())throw std::runtime_error(file.string()+": "+result.error);
            Value item=Value::map();item.object["id"]=file.stem().string();
            item.object["groups"]=Value::list();
            for(const auto& group:result.groups) {
                Value cards=Value::list();
                for(const auto& candidate:group) {
                    ++candidates;
                    Value card=Value::map();
                    card.object["fingerprint"]=candidate.fingerprint;
                    card.object["path"]=signature(candidate.progression);
                    card.object["score"]=candidate.score;
                    card.object["skeletonPreservation"]=candidate.skeletonPreservation;
                    card.object["complexityScore"]=candidate.complexityScore;
                    card.object["inversion"]=std::find(candidate.techniques.begin(),candidate.techniques.end(),
                        harmony::enrichment::TechniqueID::Inversion)!=candidate.techniques.end();
                    const auto voice=harmony::preview::measureVoiceLeading(candidate.progression);
                    card.object["voiceLeadingScore"]=voice?Value(voice->score):Value{};
                    card.object["bassMotionSemitones"]=voice?Value(voice->bassMotionSemitones):Value{};
                    cards.array.push_back(std::move(card));
                }
                item.object["groups"].array.push_back(std::move(cards));
            }
            report.object["cases"].array.push_back(std::move(item));
        }
        if(files.size()!=30)throw std::runtime_error("expected 30 enrichment cases");
        const std::filesystem::path output=argv[2];
        if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());
        std::ofstream out(output,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("cannot write report");
        out<<harmony::benchmark::json::dump(report)<<'\n';out.close();
        if(!out)throw std::runtime_error("report write failed");
        std::cout<<"enrichment cases="<<files.size()<<" candidates="<<candidates<<'\n';
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"enrichment benchmark FAIL: "<<e.what()<<'\n';return 1;
    }
}
