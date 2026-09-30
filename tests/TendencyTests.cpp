#include "demo/DemoScenario.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "library/ProgressionLibrary.h"
#include <iostream>
#include <stdexcept>
using namespace harmony;
namespace {
int chromatic(const enrichment::EnrichmentResult& result) {
    int count{};for(const auto& group:result.groups)for(const auto& c:group)
        for(const auto t:c.techniques)if(t==enrichment::TechniqueID::BorrowedChord||t==enrichment::TechniqueID::SecondaryDominant||
            t==enrichment::TechniqueID::SecondaryLeadingTone||t==enrichment::TechniqueID::PassingDiminished)++count;
    return count;
}
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
}
int main(){try {
    const auto factory=library::loadFactory(HC_FACTORY_DB_PATH);check(static_cast<bool>(factory),"factory load");
    const CandidateIndex index(factory.templates);
    int cases{};for(const char* id:{"001_major_pop_1564","005_jazz_251","007_minor_cadence","010_secondary_target",
        "012_borrowed_four","016_inversion","021_rnb_cadence","027_rock_simple"}) {
        const auto scenario=demo::loadScenario(std::filesystem::path(HC_ENRICHMENT_CASE_DIR)/(std::string(id)+".json"));
        check(static_cast<bool>(scenario),"load tendency case");const auto& p=scenario.scenario.chords;
        const auto analysis=analyzeHarmony(p);const auto baseline=enrichment::enrichProgression(p,analysis,scenario.scenario.style);
        enrichment::EnrichmentResult results[3];
        for(int i=0;i<3;++i) {
            enrichment::EnrichmentConfig config;config.tendency=static_cast<HarmonicTendency>(i);
            results[i]=enrichment::enrichProgression(p,analysis,scenario.scenario.style,config);
            for(const auto& group:results[i].groups)for(const auto& c:group) {
                check(c.skeletonPreservation>=tendencyProfile(config.tendency).skeletonMinimum,"skeleton minimum");
                check(c.styleCompatibility>=0&&c.styleCompatibility<=1,"style validity");
                for(const auto& chord:c.progression)check(normalizeChord(chord).root.has_value(),"harmony validity");
            }
        }
        for(int g=0;g<3;++g) {
            check(baseline.groups[g].size()==results[1].groups[g].size(),"balanced count unchanged");
            for(std::size_t i=0;i<baseline.groups[g].size();++i)check(baseline.groups[g][i].fingerprint==
                results[1].groups[g][i].fingerprint&&baseline.groups[g][i].score==results[1].groups[g][i].score,"balanced exact behavior");
        }
        check(chromatic(results[0])<=chromatic(results[2]),"conservative no more chromatic than bold");++cases;
        AnalysisContext context;context.forcedKey=scenario.scenario.forcedKey;
        const auto query=makeMatchQuery(p,context);
        for(int tendency=0;tendency<3;++tendency) {
            RecommendationRequest request;request.style=scenario.scenario.style;request.tendency=static_cast<HarmonicTendency>(tendency);
            const auto continuation=recommendContinuations(query,index,request);
            for(const auto& group:continuation.groups)for(const auto& c:group) {
                check(c.rankingScore>=0&&c.rankingScore<=100,"continuation bounded score");
                for(const auto& chord:continuationProgression(p,c))check(normalizeChord(chord).root.has_value(),"continuation harmony valid");
            }
        }
    }
    std::cout<<"tendency progressions="<<cases<<"/8, profiles=24, both engines PASS\n";
}catch(const std::exception& e){std::cerr<<"TendencyTests: "<<e.what()<<'\n';return 1;}}
