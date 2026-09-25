#include "demo/DemoScenario.h"
#include "session/ProductServices.h"
#include "plugin/RecommendationWorker.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace harmony;
int checks{};
void check(bool yes,const char* name) { if (!yes) throw std::runtime_error(name); ++checks; }
demo::Scenario caseFile(char letter) {
    const auto path=std::filesystem::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+static_cast<char>(letter-'A'+'a')+".json");
    const auto loaded=demo::loadScenario(path);
    if (!loaded) throw std::runtime_error(loaded.error);
    return loaded.scenario;
}
}
int main() {
    try {
        session::PluginSessionState state;
        const auto a=caseFile('A');
        check(state.imported.replace(a.chords,TimelineCoordinateMode::AbsoluteProjectQN),"progression import");
        state.meterNumerator=4; state.meterDenominator=4;
        auto forced=state; forced.forcedKey=KeySignature{PitchClass::C,Mode::Major};
        check(session::recomputeScope(state,forced)==session::RecomputeScope::Analysis,"force key invalidation");
        const auto result=analyzeHarmony(forced.imported.events,forced.analysisContext());
        check(result.selectedKey && result.selectedKey->key.tonic==PitchClass::C && result.selectedKey->key.mode==Mode::Major,"forced C Major");
        check(session::recomputeScope(forced,state)==session::RecomputeScope::Analysis,"auto key invalidation");
        auto style=state; style.style=Style::Pop;
        check(session::recomputeScope(state,style)==session::RecomputeScope::Ranking,"style ranking invalidation");
        style.style=Style::Jazz;
        check(session::deserialize(session::serialize(style)).state.style==Style::Jazz,"style round trip");
        auto intent=state; intent.intent=PhraseIntent::Resolve;
        check(session::recomputeScope(state,intent)==session::RecomputeScope::Ranking,"intent ranking invalidation");
        check(session::deserialize(session::serialize(intent)).state.intent==PhraseIntent::Resolve,"intent round trip");
        auto view=state; view.skeletonView=true; view.tab=session::Tab::Library;
        check(session::recomputeScope(state,view)==session::RecomputeScope::None,"view does not reanalyze");
        auto resized=state;resized.editorWidth=1800;resized.editorHeight=1000;
        check(session::recomputeScope(state,resized)==session::RecomputeScope::None,"resize does not reanalyze");
        check(view.pin("A") && view.pin("B") && view.pin("C") && !view.pin("D"),"pin max three");
        check(view.unpin("B") && view.pinnedCandidateIds.size()==2 && !view.unpin("B"),"unpin");
        view.debugExpanded=true; view.factoryLibraryVersion=3;
        auto decoded=session::deserialize(session::serialize(view));
        check(decoded && decoded.state.skeletonView && decoded.state.tab==session::Tab::Library &&
            decoded.state.pinnedCandidateIds.size()==2 && decoded.state.factoryLibraryVersion==3 &&
            decoded.state.imported.events.size()==a.chords.size(),"complete state round trip");
        check(!session::deserialize("HCS1bad"),"corrupt state rejected");
        auto factory=library::loadFactory(HC_FACTORY_DB_PATH);
        check(factory && factory.templates.size()==161,"factory load");
        const CandidateIndex index(std::move(factory.templates));
        {
            plugin::RecommendationWorker worker(HC_FACTORY_DB_PATH,
                std::filesystem::temp_directory_path()/"hc-phase35-no-user.db");
            auto waitResult=[&]() -> plugin::WorkerResult {
                for (int i=0;i<200;++i) {
                    if (auto ready=worker.takeLatest()) return std::move(*ready);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                throw std::runtime_error("worker timeout");
            };
            worker.submit(a.chords,{}, {},false,19);
            const auto first=waitResult();
            check(!first.analysisReused && first.error.empty(),"initial worker analysis");
            worker.submit(a.chords,{}, {Style::Pop,std::nullopt},true,19);
            const auto ranked=waitResult();
            check(ranked.analysisReused && ranked.error.empty(),"ranking reuses analysis");
        }
        RecommendationWeights weights; weights.perGroup=10;
        for (char letter='A';letter<='H';++letter) {
            const auto scenario=caseFile(letter);
            AnalysisContext context; context.forcedKey=scenario.forcedKey;
            context.timeSigNumerator=scenario.meterNumerator; context.timeSigDenominator=scenario.meterDenominator;
            const auto analysis=analyzeHarmony(scenario.chords,context);
            const auto query=makeMatchQuery(scenario.chords,context);
            const auto recommendations=recommendContinuations(query,index,{scenario.style,scenario.intent},weights);
            check(!analysis.full.empty() && !query.interpretations.empty() && recommendations.stats.totalMs>=0,
                "demo A-H full pipeline");
            for (const auto& group:recommendations.groups) if (!group.empty()) {
                ImportedProgressionSession imported;
                check(imported.replace(scenario.chords,TimelineCoordinateMode::RelativeToSelection),"demo save input");
                const auto converted=session::makeUserProgression(imported,group.front(),
                    {"Demo Save",scenario.style,group.front().intent,{}});
                check(converted && converted.item.full.size()==scenario.chords.size()+group.front().continuation.size(),
                    "demo candidate save conversion");
                break;
            }
        }
        auto mismatched=decoded.state; mismatched.factoryLibraryVersion=999;
        auto restored=session::deserialize(session::serialize(mismatched));
        check(restored && !recommendContinuations(makeMatchQuery(restored.state.imported.events,restored.state.analysisContext()),index).matches.empty(),
            "library version mismatch recomputes");
        const auto h=caseFile('H');
        check(analyzeHarmony(h.chords).keyCandidates.size()>=2,"ambiguous key candidates retained");
        std::vector<ContinuationCandidate> synthetic(3);
        synthetic[0].rankingScore=90; synthetic[1].rankingScore=70; synthetic[2].rankingScore=50;
        session::RecommendationVisibilityPolicy policy;
        check(policy.visibleIndices(synthetic).size()==2,"absolute visibility threshold");
        policy.relativeToGroupBest=10;
        check(policy.visibleIndices(synthetic).size()==1,"relative visibility threshold");
        policy.relativeToGroupBest=0; policy.minimumDiversityDistance=0.1f;
        check(policy.visibleIndices(synthetic).size()==1,"minimum diversity distance");
        for (auto& item:synthetic) item.rankingScore=40;
        check(policy.visibleIndices(synthetic).empty(),"no strong option is empty");
        const auto query=makeMatchQuery(a.chords);
        const auto rec=recommendContinuations(query,index,{},weights);
        const ContinuationCandidate* choice{};
        for (const auto& group:rec.groups) if (!group.empty() && !choice) choice=&group.front();
        check(choice!=nullptr,"save candidate available");
        auto absolute=state.imported;
        for (auto& chord:absolute.events) chord.startQN+=32;
        const auto saved=session::makeUserProgression(absolute,*choice,{"Saved Test",Style::Pop,choice->intent,{"test"}});
        check(saved && saved.item.sourceType=="user" && saved.item.full.size()==absolute.events.size()+choice->continuation.size(),"save combined phrase");
        check(saved.item.full.front().durationQN==absolute.events.front().durationQN &&
            saved.item.full[absolute.events.size()-1].durationQN==choice->suggestedCurrentChordDurationQN,"relative durations and OPEN hold");
        const auto db=std::filesystem::temp_directory_path()/"hc-phase35-test-user.db";
        std::filesystem::remove(db);
        library::UserLibrary user(db); std::string error;
        check(user.addProgression(saved.item,error),"user save");
        auto listed=user.listProgressions();
        check(listed && listed.templates.size()==1 && listed.templates.front().name=="Saved Test","user list");
        auto updated=listed.templates.front(); updated.name="Renamed"; updated.intent=PhraseIntent::Color;
        updated.styles=static_cast<StyleFlags>(Style::Jazz); updated.styleWeights={{Style::Jazz,1.f}};
        check(user.updateProgression(updated,error),"user rename metadata");
        listed=user.listProgressions();
        check(listed && listed.templates.front().name=="Renamed" && listed.templates.front().intent==PhraseIntent::Color,"user metadata persists");
        check(user.removeProgression(updated.id,error) && user.listProgressions().templates.empty(),"user delete");
        std::filesystem::remove(db);
        std::cout<<"ProductizationTests PASS "<<checks<<" checks\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"ProductizationTests FAIL: "<<e.what()<<'\n'; return 1; }
}
