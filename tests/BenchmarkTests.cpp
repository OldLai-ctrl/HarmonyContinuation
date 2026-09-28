#include "benchmark/Benchmark.h"
#include "library/ProgressionLibrary.h"
#include "session/ProductServices.h"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
int checks{};
void check(bool condition,const char* what){if(!condition)throw std::runtime_error(what);++checks;}
}
int main(){
    try{
        namespace hc=harmony;
        const auto files=hc::benchmark::caseFiles(HC_BENCH_CASE_DIR);
        check(files.size()>=30&&files.size()<=50,"benchmark count");
        std::set<std::string> ids,categories;
        for(const auto& file:files){const auto item=hc::benchmark::loadCase(file);
            check(ids.insert(item.id).second,"unique case ID");
            check(item.scenario.chords.size()>=2,"case has phrase");
            categories.insert(item.category);
        }
        check(categories.size()>=15,"category breadth");
        const auto factory=hc::library::loadFactory(HC_FACTORY_DB_PATH);
        check(factory&&factory.templates.size()==161,"factory frozen at 161");
        const hc::CandidateIndex index(factory.templates);
        const auto example=hc::benchmark::loadCase(files.front());
        hc::AnalysisContext context;context.forcedKey=example.scenario.forcedKey;
        const auto query=hc::makeMatchQuery(example.scenario.chords,context);
        const auto rec=hc::recommendContinuations(query,index);
        const hc::ContinuationCandidate* candidate{};
        for(const auto& group:rec.groups)if(!group.empty()){candidate=&group.front();break;}
        check(candidate!=nullptr,"candidate for rating");
        hc::ImportedProgressionSession imported;
        check(imported.replace(example.scenario.chords,hc::TimelineCoordinateMode::RelativeToSelection),"rating input");
        hc::benchmark::Rating rating;
        rating.benchmarkId=example.id;
        rating.candidateFingerprint=hc::session::continuationFingerprint(*candidate);
        rating.recommendation=hc::snapshot::capture(imported,*candidate,rec.matches,
            example.scenario.tempo,example.scenario.meterNumerator,example.scenario.meterDenominator);
        rating.naturalness=4;rating.usability=5;rating.verdict="KEEP";rating.issueCategory="Other";
        rating.note="Human review fixture";
        const auto output=std::filesystem::path(HC_TEST_OUTPUT_DIR)/"phase5-rating-test";
        std::string error;check(hc::benchmark::saveRating(rating,output,error),"save rating");
        const auto loaded=hc::benchmark::loadRating(output/hc::benchmark::ratingFilename(rating));
        check(loaded.candidateFingerprint==rating.candidateFingerprint&&loaded.usability==5&&
            loaded.verdict=="KEEP"&&loaded.note==rating.note,"rating roundtrip");
        rating.candidateFingerprint="wrong";
        check(!hc::benchmark::saveRating(rating,output,error),"reject mismatched fingerprint");
        std::cout<<checks<<" benchmark checks passed\n";
    }catch(const std::exception& e){std::cerr<<"BenchmarkTests: "<<e.what()<<'\n';return 1;}
}
