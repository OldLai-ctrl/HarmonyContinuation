#include "core/RecommendationPolicy.h"
#include "demo/DemoScenario.h"
#include "library/LibraryStore.h"
#include "session/ProductServices.h"
#include "plugin/RecommendationWorker.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif

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
        view.locale=session::Locale::EnUS; view.productMode=session::ProductMode::Enrich;
        check(session::recomputeScope(state,view)==session::RecomputeScope::None,"locale and mode do not recompute");
        const auto preferenceRoundtrip=session::deserialize(session::serialize(view));
        check(preferenceRoundtrip && preferenceRoundtrip.state.locale==session::Locale::EnUS &&
              preferenceRoundtrip.state.productMode==session::ProductMode::Enrich,"preferences roundtrip");
        auto resized=state;resized.editorWidth=1800;resized.editorHeight=1000;
        check(session::recomputeScope(state,resized)==session::RecomputeScope::None,"resize does not reanalyze");
        check(view.pin("A") && view.pin("B") && view.pin("C") && !view.pin("D"),"pin max three");
        check(view.unpin("B") && view.pinnedCandidateIds.size()==2 && !view.unpin("B"),"unpin");
        {
            ContinuationCandidate oldRepresentative;
            oldRepresentative.primaryTemplate="source-old";
            oldRepresentative.key={PitchClass::C,Mode::Major};
            oldRepresentative.intent=PhraseIntent::Resolve;
            oldRepresentative.cadence=CadenceType::Authentic;
            oldRepresentative.suggestedCurrentChordDurationQN=2.;
            oldRepresentative.continuation.push_back({"G",2.,{5,0},ChordQuality::Major});
            oldRepresentative.id=session::continuationFingerprint(oldRepresentative);
            auto newRepresentative=oldRepresentative;
            newRepresentative.primaryTemplate="source-new";
            newRepresentative.id=session::continuationFingerprint(newRepresentative);
            RecommendationSet available;available.groups[0].push_back(newRepresentative);

            session::PluginSessionState newMusicPin;newMusicPin.factoryLibraryVersion=2;
            check(newMusicPin.pin(oldRepresentative.id,session::continuationFingerprint(oldRepresentative)),
                  "new music identity pin fixture");
            newMusicPin.resolvePins(available);
            check(newMusicPin.pinnedCandidateIds==std::vector<std::string>{newRepresentative.id}&&
                  newMusicPin.pinnedFingerprints==std::vector<std::string>{session::continuationFingerprint(newRepresentative)},
                  "music pin resolves across a changed representative source");

            session::PluginSessionState legacyPin;legacyPin.factoryLibraryVersion=2;
            check(legacyPin.pin(oldRepresentative.primaryTemplate,
                                session::legacyContinuationFingerprint(oldRepresentative)),
                  "legacy music pin fixture");
            legacyPin.resolvePins(available);
            check(legacyPin.pinnedCandidateIds==std::vector<std::string>{newRepresentative.id}&&
                  legacyPin.pinnedFingerprints==std::vector<std::string>{session::continuationFingerprint(newRepresentative)},
                  "legacy fingerprint migrates to the exact current music identity");

            auto changedDuration=newRepresentative;
            changedDuration.continuation.front().durationQN+=0.01;
            changedDuration.id=session::continuationFingerprint(changedDuration);
            RecommendationSet changedPath;changedPath.groups[0].push_back(changedDuration);
            session::PluginSessionState stalePin;stalePin.factoryLibraryVersion=2;
            check(stalePin.pin(oldRepresentative.primaryTemplate,
                               session::legacyContinuationFingerprint(oldRepresentative)),
                  "duration-migration pin fixture");
            stalePin.resolvePins(changedPath);
            check(std::find(stalePin.pinnedCandidateIds.begin(),stalePin.pinnedCandidateIds.end(),
                            changedDuration.id)==stalePin.pinnedCandidateIds.end(),
                  "legacy pin with changed duration never binds to another music result");

            session::PluginSessionState malformedPin;malformedPin.factoryLibraryVersion=2;
            check(malformedPin.pin("malformed-pin-id","not-a-complete-legacy-fingerprint"),
                  "malformed pin fixture");
            auto matchingIdDifferentMusic=changedDuration;matchingIdDifferentMusic.id="malformed-pin-id";
            RecommendationSet malformedAvailable;malformedAvailable.groups[0].push_back(matchingIdDifferentMusic);
            malformedPin.resolvePins(malformedAvailable);
            check(malformedPin.pinnedCandidateIds.empty()&&
                  !session::continuationReferenceMatches(matchingIdDifferentMusic,
                      "not-a-complete-legacy-fingerprint"),
                  "malformed old reference fails safely without ID-only rebinding");
            auto constrained=newRepresentative;
            MelodyConstraint top;top.pitch=74;top.pitchClass=2;top.role=ConstraintRole::TopVoice;
            constrained.constraints.melody.push_back(top);
            check(!session::continuationReferenceMatches(constrained,session::legacyContinuationFingerprint(oldRepresentative)),
                  "legacy pin cannot invent missing realization constraints");
            session::PluginSessionState longIdentity;longIdentity.factoryLibraryVersion=2;
            const std::string largeReference="music-v1:"+std::string(9000,'x');
            check(longIdentity.pin(largeReference,largeReference),"bounded long realization identity pin");
            const auto restoredLong=session::deserialize(session::serialize(longIdentity));
            check(restoredLong&&restoredLong.state.pinnedFingerprints==longIdentity.pinnedFingerprints,
                  "session5 encodes complete realization references without truncation");
        }
        view.debugExpanded=true; view.factoryLibraryVersion=3;
        auto decoded=session::deserialize(session::serialize(view));
        check(decoded && decoded.state.skeletonView && decoded.state.tab==session::Tab::Library &&
            decoded.state.pinnedCandidateIds.size()==2 && decoded.state.factoryLibraryVersion==3 &&
            decoded.state.imported.events.size()==a.chords.size(),"complete state round trip");
        check(!session::deserialize("HCS1bad"),"corrupt state rejected");
        auto factory=library::loadFactory(HC_FACTORY_DB_PATH);
        check(factory && factory.templates.size()==161,"factory load");
        const int bundledFactoryVersion=factory.libraryVersion;
        const auto bundledFactoryCount=factory.templates.size();
        const auto selectedFactory=library::loadAvailableFactory(HC_FACTORY_DB_PATH);
        check(selectedFactory.library && !selectedFactory.path.empty(),"selected Factory source load");
        const int expectedFactoryVersion=selectedFactory.library.libraryVersion;
        const CandidateIndex index(std::move(factory.templates));
        {
            plugin::RecommendationWorker worker(HC_FACTORY_DB_PATH,
                std::filesystem::temp_directory_path()/"hc-phase35-no-user.db");
            auto waitResult=[](plugin::RecommendationWorker& source) -> plugin::WorkerResult {
                for (int i=0;i<200;++i) {
                    if (auto ready=source.takeLatest()) return std::move(*ready);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                throw std::runtime_error("worker timeout");
            };
            const auto sameRecommendation=[](const RecommendationSet& left,const RecommendationSet& right) {
                if (left.retrievalTimingMode!=right.retrievalTimingMode ||
                    left.phase3aEnabled!=right.phase3aEnabled ||
                    left.abstractChordCompatibility!=right.abstractChordCompatibility ||
                    left.stats.output!=right.stats.output || left.matches.size()!=right.matches.size()) return false;
                for (std::size_t i=0;i<left.matches.size();++i) {
                    if (left.matches[i].templateId!=right.matches[i].templateId ||
                        left.matches[i].similarity!=right.matches[i].similarity ||
                        left.matches[i].candidateRankingScore!=right.matches[i].candidateRankingScore) return false;
                }
                for (std::size_t group=0;group<left.groups.size();++group) {
                    if (left.groups[group].size()!=right.groups[group].size()) return false;
                    for (std::size_t i=0;i<left.groups[group].size();++i) {
                        const auto& a=left.groups[group][i];
                        const auto& b=right.groups[group][i];
                        if (a.id!=b.id || a.eligibilityScore!=b.eligibilityScore ||
                            a.rankingScore!=b.rankingScore) return false;
                    }
                }
                return true;
            };
            const auto unifiedResult=[](const plugin::WorkerResult& result) {
                return result.error.empty() &&
                    result.recommendations.retrievalTimingMode==RetrievalTimingMode::Unified &&
                    result.recommendations.abstractChordCompatibility &&
                    !result.recommendations.phase3aEnabled;
            };
            const auto validFactoryHash=[](const std::string& value) {
#if defined(_WIN32)
                if (value.size()!=64) return false;
                for (const auto ch:value)
                    if (!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f'))) return false;
                return true;
#else
                return value.find("unavailable")!=std::string::npos;
#endif
            };
            const auto allGroupsEmpty=[](const auto& groups) {
                for (const auto& group:groups) if (!group.empty()) return false;
                return true;
            };
            AnalysisContext baseContext;
            baseContext.retrievalTimingMode=RetrievalTimingMode::Legacy;
            const auto policyContext=recommendationContext(baseContext);
            const auto policyWeights=recommendationWeights();
            check(policyContext.retrievalTimingMode==RetrievalTimingMode::Unified &&
                policyWeights.abstractChordCompatibility && !policyWeights.diminishedPassingInsertionException,
                "shared worker policy uses Unified retrieval and keeps Phase3A disabled");
            const auto metadataGeneration=worker.submitPreset({},baseContext,
                plugin::DevelopmentAlgorithmPreset::Legacy,{},false,18);
            const auto metadataOnly=waitResult(worker);
            check(metadataOnly.generation==metadataGeneration &&
                metadataOnly.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Legacy &&
                metadataOnly.error.empty() && !metadataOnly.analysisReused && metadataOnly.analysis.full.empty() &&
                metadataOnly.recommendations.matches.empty() && allGroupsEmpty(metadataOnly.recommendations.groups) &&
                allGroupsEmpty(metadataOnly.enrichments.groups) &&
                metadataOnly.factoryVersion==expectedFactoryVersion && validFactoryHash(metadataOnly.factorySha256),
                "empty metadata-only task reports selected Factory identity without candidates");

            worker.submit(a.chords,{}, {},false,19);
            const auto first=waitResult(worker);
            check(!first.analysisReused && !first.algorithmPreset && unifiedResult(first) &&
                first.factoryVersion==expectedFactoryVersion && validFactoryHash(first.factorySha256),
                "initial worker analysis uses the shared policy and reports selected Factory identity");
            worker.submit(a.chords,{}, {Style::Pop,std::nullopt},true,19);
            const auto ranked=waitResult(worker);
            check(ranked.analysisReused && unifiedResult(ranked),"ranking reuses the unified analysis");

            const auto emptyAfterInputGeneration=worker.submitPreset({},baseContext,
                plugin::DevelopmentAlgorithmPreset::Legacy,{},false,19);
            const auto emptyAfterInput=waitResult(worker);
            check(emptyAfterInput.generation==emptyAfterInputGeneration && emptyAfterInput.error.empty() &&
                emptyAfterInput.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Legacy &&
                allGroupsEmpty(emptyAfterInput.recommendations.groups) && allGroupsEmpty(emptyAfterInput.enrichments.groups),
                "empty metadata-only task clears candidate output after a prior query");
            const auto afterEmptyGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Legacy,{},true,19);
            const auto afterEmpty=waitResult(worker);
            check(afterEmpty.generation==afterEmptyGeneration && !afterEmpty.analysisReused &&
                afterEmpty.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Legacy && unifiedResult(afterEmpty),
                "non-empty task recomputes under the shared policy after empty metadata-only task");

            const auto legacyGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Legacy,{},false,20);
            const auto legacy=waitResult(worker);
            const auto hfGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::HarmonyFirst,{},true,20);
            const auto harmonyFirst=waitResult(worker);
            const auto abstractGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Abstract,{},true,20);
            const auto abstract=waitResult(worker);
            check(legacy.generation==legacyGeneration && !legacy.analysisReused &&
                legacy.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Legacy && unifiedResult(legacy) &&
                harmonyFirst.generation==hfGeneration && harmonyFirst.analysisReused &&
                harmonyFirst.algorithmPreset==plugin::DevelopmentAlgorithmPreset::HarmonyFirst && unifiedResult(harmonyFirst) &&
                abstract.generation==abstractGeneration && abstract.analysisReused &&
                abstract.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Abstract && unifiedResult(abstract) &&
                legacy.factoryVersion==first.factoryVersion && legacy.factorySha256==first.factorySha256 &&
                sameRecommendation(legacy.recommendations,harmonyFirst.recommendations) &&
                sameRecommendation(legacy.recommendations,abstract.recommendations),
                "legacy preset aliases reuse one Unified query and preserve music identity, eligibility, and ranking");

            auto forcedContext=baseContext;
            forcedContext.forcedKey=KeySignature{PitchClass::C,Mode::Major};
            const auto forcedGeneration=worker.submitPreset(a.chords,forcedContext,
                plugin::DevelopmentAlgorithmPreset::Legacy,{},true,20);
            const auto forcedResult=waitResult(worker);
            check(forcedResult.generation==forcedGeneration && !forcedResult.analysisReused && unifiedResult(forcedResult),
                "forced-key change invalidates the same-revision query");
            auto meterContext=forcedContext;
            meterContext.timeSigNumerator=4;meterContext.timeSigDenominator=4;
            const auto meterGeneration=worker.submitPreset(a.chords,meterContext,
                plugin::DevelopmentAlgorithmPreset::HarmonyFirst,{},true,20);
            const auto meterResult=waitResult(worker);
            check(meterResult.generation==meterGeneration && !meterResult.analysisReused && unifiedResult(meterResult),
                "meter change invalidates the same-revision query");
            const auto inputBaseGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Abstract,{},false,21);
            const auto inputBase=waitResult(worker);
            auto changedInput=a.chords;changedInput.front().name="F";
            const auto changedInputGeneration=worker.submitPreset(changedInput,baseContext,
                plugin::DevelopmentAlgorithmPreset::Abstract,{},true,22);
            const auto changedInputResult=waitResult(worker);
            check(inputBase.generation==inputBaseGeneration && !inputBase.analysisReused &&
                changedInputResult.generation==changedInputGeneration && !changedInputResult.analysisReused &&
                unifiedResult(inputBase) && unifiedResult(changedInputResult),
                "changed input revision invalidates the cached query");

            const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            plugin::RecommendationWorker secondWorker(HC_FACTORY_DB_PATH,
                std::filesystem::temp_directory_path()/("hc-phase6-instance-"+unique+".db"));
            const auto instanceLegacyGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Legacy,{},false,23);
            const auto instanceAbstractGeneration=secondWorker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Abstract,{},false,23);
            const auto instanceLegacy=waitResult(worker);
            const auto instanceAbstract=waitResult(secondWorker);
            check(instanceLegacy.generation==instanceLegacyGeneration && !instanceLegacy.analysisReused &&
                instanceAbstract.generation==instanceAbstractGeneration && !instanceAbstract.analysisReused &&
                instanceLegacy.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Legacy && unifiedResult(instanceLegacy) &&
                instanceAbstract.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Abstract && unifiedResult(instanceAbstract) &&
                sameRecommendation(instanceLegacy.recommendations,instanceAbstract.recommendations),
                "worker query caches remain instance-local under the unified policy");

            worker.submitPreset(a.chords,baseContext,plugin::DevelopmentAlgorithmPreset::Legacy,{},false,24);
            worker.submitPreset(a.chords,baseContext,plugin::DevelopmentAlgorithmPreset::HarmonyFirst,{},false,24);
            const auto newestGeneration=worker.submitPreset(a.chords,baseContext,
                plugin::DevelopmentAlgorithmPreset::Abstract,{},false,24);
            const auto newest=waitResult(worker);
            check(newest.generation==newestGeneration &&
                newest.algorithmPreset==plugin::DevelopmentAlgorithmPreset::Abstract && unifiedResult(newest),
                "only the latest preset-alias generation is published");

            const auto isolationId=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            const auto emptyFactoryStore=std::filesystem::temp_directory_path()/
                ("hc-phase6-empty-factory-store-"+isolationId);
            const auto isolatedUserPath=std::filesystem::temp_directory_path()/
                ("hc-phase6-empty-factory-user-"+isolationId+".db");
            std::filesystem::create_directories(emptyFactoryStore);
            {
                plugin::RecommendationWorker isolatedWorker(HC_FACTORY_DB_PATH,isolatedUserPath,emptyFactoryStore);
                const auto isolatedGeneration=isolatedWorker.submitPreset({},baseContext,
                    plugin::DevelopmentAlgorithmPreset::Legacy,{},false,23);
                const auto isolated=waitResult(isolatedWorker);
                check(isolated.generation==isolatedGeneration && isolated.error.empty() &&
                    isolated.factoryVersion==bundledFactoryVersion &&
                    isolated.factoryCount==bundledFactoryCount && validFactoryHash(isolated.factorySha256),
                    "explicit empty Factory store selects the supplied fixture, independent of host active store");
            }
            std::filesystem::remove_all(emptyFactoryStore);
            std::filesystem::remove(isolatedUserPath);
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
        synthetic[2].baseQualified=false;
        session::RecommendationVisibilityPolicy policy;
        policy.minimumDiversityDistance=0;
        check(policy.visibleIndices(synthetic).size()==2,"explicit qualification controls visibility");
        policy.relativeToGroupBest=10;
        check(policy.visibleIndices(synthetic).size()==1,"relative visibility threshold");
        policy.relativeToGroupBest=0; policy.minimumDiversityDistance=0.1f;
        check(policy.visibleIndices(synthetic).size()==1,"minimum diversity distance");
        for (auto& item:synthetic) item.rankingScore=40;
        policy.minimumDiversityDistance=0;
        check(policy.visibleIndices(synthetic).size()==2,"ranking cannot revoke qualification");
        auto accessCandidate=[](std::string id,float score,bool qualified,bool exploratory,std::string label) {
            ContinuationCandidate candidate;candidate.id=std::move(id);candidate.rankingScore=score;
            candidate.baseQualified=qualified;candidate.exploratory=exploratory;
            ConcreteChordEvent chord;chord.label=std::move(label);chord.durationQN=4;
            candidate.continuation.push_back(std::move(chord));return candidate;
        };
        std::vector<ContinuationCandidate> accessGroup{
            accessCandidate("explore",62,true,true,"C"),
            accessCandidate("below-sixty",59,true,false,"Dm"),
            accessCandidate("unqualified",90,false,true,"G")};
        const auto access=session::continuationAccess(accessGroup);
        const auto defaults=session::RecommendationVisibilityPolicy{}.visibleIndices(accessGroup);
        const auto hasIndex=[](const std::vector<std::size_t>& values,std::size_t index) {
            return std::find(values.begin(),values.end(),index)!=values.end();
        };
        check(!hasIndex(defaults,0)&&hasIndex(access.more,0)&&access.exploratory==1,
              "qualified exploration is excluded from defaults but retained in More");
        check(hasIndex(access.more,1),"a retained candidate below 60 remains accessible in More");
        check(!hasIndex(access.more,2),"an unqualified candidate is excluded from More");
        session::RecommendationVisibilityPolicy presentationPolicy;
        presentationPolicy.maxPerGroup=2;
        auto card=[](std::string id,std::string label,float score) {
            ContinuationCandidate item;item.id=std::move(id);item.rankingScore=score;
            ConcreteChordEvent chord;chord.label=std::move(label);chord.durationQN=4;
            item.continuation.push_back(std::move(chord));return item;
        };
        RecommendationSet display;
        display.groups[0]={card("resolve-a","C",95),card("resolve-duplicate","C",94),card("resolve-b","F",90)};
        display.groups[1]={card("develop-a","C",92),card("develop-b","G",83)};
        display.groups[1][0].key.tonic=PitchClass::G; // different analysis, identical audible path
        display.groups[2]={card("loop-a","C",80)};
        display.groups[3]={card("color-a","Cmaj7",75)};
        const auto presented=session::presentationIndices(display,presentationPolicy);
        check(presented[0]==std::vector<std::size_t>({0,2}),"same-group exact path uses one card");
        check(presented[1]==std::vector<std::size_t>({1}),"cross-group duplicate yields to alternative");
        check(presented[2].empty(),"a duplicate does not occupy another group without an alternative");
        check(presented[3]==std::vector<std::size_t>({0}),"similar path remains distinct");
        ChordEvent extraTone;extraTone.name="C";extraTone.extensions.mask="0x891";
        display.groups[0][1].continuation[0].harmonicData=extraTone;
        const auto differentTones=session::presentationIndices(display,presentationPolicy);
        check(differentTones[0]==std::vector<std::size_t>({0,1}),
              "same displayed name with different actual tones stays visible");
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
        updated.tags={"副歌","我的常用"}; updated.note="项目 A"; updated.favorite=true;
        updated.createdAt="2026-09-29T00:00:00Z"; updated.updatedAt="2026-09-29T01:00:00Z";
        check(user.updateProgression(updated,error),"user rename metadata");
        listed=user.listProgressions();
        check(listed && listed.templates.front().name=="Renamed" && listed.templates.front().intent==PhraseIntent::Color &&
            listed.templates.front().tags==updated.tags && listed.templates.front().favorite &&
            listed.templates.front().note==updated.note && listed.templates.front().createdAt==updated.createdAt,
            "user metadata persists");
        library::UserLibrary reopened(db);
        const auto afterReopen=reopened.listProgressions();
        check(afterReopen && afterReopen.templates.size()==1 &&
            afterReopen.templates.front().name==updated.name &&
            afterReopen.templates.front().styles==updated.styles &&
            afterReopen.templates.front().intent==updated.intent &&
            afterReopen.templates.front().tags==updated.tags &&
            afterReopen.templates.front().favorite==updated.favorite &&
            afterReopen.templates.front().note==updated.note,
            "user metadata survives reopen");
        check(library::loadFactory(HC_FACTORY_DB_PATH).templates.size()==161 &&
            reopened.listProgressions().templates.size()==1,
            "factory reload preserves separate user library");
        check(user.removeProgression(updated.id,error) && user.listProgressions().templates.empty(),"user delete");
        std::filesystem::remove(db);
        const auto legacyDb=std::filesystem::temp_directory_path()/"hc-v06-legacy-user.db";
        std::filesystem::remove(legacyDb);
        sqlite3* raw{};
        check(sqlite3_open(legacyDb.string().c_str(),&raw)==SQLITE_OK,"legacy SQLite create");
        const char* legacySql=
            "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
            "CREATE TABLE progressions(id TEXT PRIMARY KEY,payload TEXT NOT NULL);"
            "INSERT INTO metadata VALUES('schema_version','1'),('library_version','1'),('library_type','user');"
            "INSERT INTO progressions VALUES('USER_legacy',"
            "'{\"id\":\"USER_legacy\",\"name\":\"Old saved phrase\",\"mode\":\"major\","
            "\"sequence\":\"I V vi IV\",\"sourceType\":\"user\",\"tags\":\"chorus\"}');";
        const auto legacyCode=sqlite3_exec(raw,legacySql,nullptr,nullptr,nullptr);
        sqlite3_close(raw);
        check(legacyCode==SQLITE_OK,"legacy v1 payload create");
        library::UserLibrary legacyUser(legacyDb);
        const auto oldItems=legacyUser.listProgressions();
        check(oldItems && oldItems.templates.size()==1 && oldItems.templates.front().name=="Old saved phrase" &&
            oldItems.templates.front().tags==std::vector<std::string>{"chorus"},"legacy v1 retained");
        auto promoted=oldItems.templates.front();promoted.favorite=true;promoted.note="kept";
        check(legacyUser.updateProgression(promoted,error),"legacy item update");
        const auto newItems=legacyUser.listProgressions();
        check(newItems && newItems.templates.size()==1 && newItems.templates.front().favorite &&
            newItems.templates.front().note=="kept" && newItems.templates.front().full.size()==4,
            "legacy update retains progression");
        std::filesystem::remove(legacyDb);
        std::cout<<"ProductizationTests PASS "<<checks<<" checks\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"ProductizationTests FAIL: "<<e.what()<<'\n'; return 1; }
}
