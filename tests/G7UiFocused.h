#pragma once
namespace {
inline int runG7UiHarness(const char* factoryArg,const char* outputArg) {
        const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        VSTGUI::init(GetModuleHandleW(nullptr));
        auto* platform=VSTGUI::getPlatformFactory().asWin32Factory();
        platform->disableDirectComposition();platform->useD2DHardwareRenderer(false);
        Window window;VSTGUI::CFrame* frame{};bool opened{},passed{};
        try {
            const auto factoryPath=std::filesystem::absolute(factoryArg);
            const auto output=std::filesystem::absolute(outputArg);
            std::filesystem::create_directories(output);
            auto factory=library::loadFactory(factoryPath);
            require(static_cast<bool>(factory),"G7 UI Factory load: "+factory.error);
            require(factory.libraryVersion==7,"G7 UI requires a read-only V7 Factory database");
            const auto factoryCount=factory.templates.size();

            Progression phrase;
            for(const auto& chord:std::array<std::pair<const char*,double>,4>{{{"F",0},{"G",4},{"Em",8},{"Am",12}}}) {
                ChordEvent event;event.name=chord.first;event.startQN=chord.second;
                phrase.push_back(std::move(event));
            }
            require(sortAndInferDurations(phrase),"G7 FGEmAm progression normalization");
            require(phrase.size()==4&&phrase.back().name=="Am"&&phrase.back().openEnded&&
                    !phrase.back().durationQN,"G7 FGEmAm retains final OPEN chord");

            session::PluginSessionState state;
            state.editorWidth=800;state.editorHeight=780;state.uiZoomPercent=100;
            state.productMode=session::ProductMode::Continue;state.tab=session::Tab::Recommend;
            state.intent.reset();state.factoryLibraryVersion=factory.libraryVersion;
            require(state.imported.replace(std::move(phrase),TimelineCoordinateMode::RelativeToSelection),
                    "G7 FGEmAm progression installation");
            const auto phraseRevision=state.imported.revision;

            const auto query=makeMatchQuery(state.imported.events,
                recommendationContext(state.analysisContext()));
            RecommendationRequest request;
            request.style=state.style;request.preferredIntent=state.intent;
            request.constraints=state.constraints;request.tendency=state.tendency;
            request.inspirationPriority=state.inspirationPriority;
            request.prepareBothViews=true;

            frame=new VSTGUI::CFrame({0,0,800,780},nullptr);
            ui::MainView* view{};
            const RecommendationSet* mixedBank{};
            const RecommendationSet* exactBank{};
            std::size_t stateCallbacks{},workerSubmissions{},paintCount{},messageCount{},idleCount{};
            std::size_t g1Paints{},g1Messages{},g2Paints{},g2Messages{};
            std::uint64_t lastAppliedGeneration{};
            std::size_t appliedResults{};
            ui::MainView::Actions actions;
            actions.stateChanged=[&](const session::PluginSessionState& next) {
                ++stateCallbacks;state=next;
                if(!view)return;
                view->setSessionState(next);
                if(mixedBank&&exactBank) {
                    const auto& bank=next.inspirationPriority?*mixedBank:*exactBank;
                    view->setMatches(bank.matches,"Ready");
                    view->setRecommendations(bank,true);
                }
            };
            auto* createdView=new ui::MainView({0,0,800,780},std::move(actions));
            view=createdView;frame->addView(createdView);
            opened=frame->open(window.handle,VSTGUI::PlatformType::kHWND);
            require(opened,"G7 UI VSTGUI frame at 800x780");
            view->setSessionState(state);view->setLibrary(factory.templates,{});
            view->setWorkerStatus(0,0,false,factoryCount,0,false);
            const auto context=VSTGUI::COffscreenContext::create({800,780});
            require(static_cast<bool>(context),"G7 UI offscreen context at 800x780");
            const auto paintAndPump=[&] {
                MSG message{};
                while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                    ++messageCount;
                    if(message.message==WM_QUIT) {
                        PostQuitMessage(static_cast<int>(message.wParam));break;
                    }
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                frame->idle();++idleCount;
                context->beginDraw();view->drawRect(context,{0,0,800,780});context->endDraw();
                ++paintCount;
            };
            std::set<std::string> screenshots;
            const auto capture=[&](const std::string& name) {
                auto filename=std::filesystem::path(name);
                if(filename.extension().empty())filename+=".png";
                context->beginDraw();view->drawRect(context,{0,0,800,780});context->endDraw();
                const auto png=VSTGUI::getPlatformFactory().createBitmapMemoryPNGRepresentation(
                    context->getBitmap()->getPlatformBitmap());
                std::ofstream file(output/filename,std::ios::binary);
                file.write(reinterpret_cast<const char*>(png.data()),static_cast<std::streamsize>(png.size()));
                file.close();
                require(file.good()&&!png.empty(),"G7 UI screenshot capture: "+filename.string());
                screenshots.insert(filename.filename().string());
            };

            plugin::RecommendationWorker worker(factoryPath,output/"isolated-user.db",
                output/"isolated-factory-store");
            const auto submit=[&] {
                ++workerSubmissions;
                return worker.submit(state.imported.events,state.analysisContext(),request,false,phraseRevision);
            };
            const auto await=[&](std::uint64_t expected,double& waitMs) {
                const auto started=std::chrono::steady_clock::now();
                const auto deadline=started+std::chrono::seconds(30);
                std::optional<plugin::WorkerResult> result;
                while(std::chrono::steady_clock::now()<deadline) {
                    result=worker.takeLatest();
                    if(result)break;
                    paintAndPump();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                waitMs=std::chrono::duration<double,std::milli>(
                    std::chrono::steady_clock::now()-started).count();
                require(result.has_value(),"G7 real recommendation worker timed out");
                require(result->generation==expected,"G7 worker returned another generation");
                require(result->error.empty(),"G7 worker failed: "+result->error);
                return result;
            };
            const auto applyWorkerResult=[&](const plugin::WorkerResult& result) {
                if(!view->acceptsRecommendationGeneration(result.generation))return false;
                view->setAnalysis(result.analysis);
                view->setMatches(result.recommendations.matches,"Ready");
                view->setRecommendations(result.recommendations);
                view->setEnrichments(result.enrichments);
                view->setWorkerStatus(result.generation,result.computationMs,false,
                    result.factoryCount,result.userCount,true);
                lastAppliedGeneration=result.generation;++appliedResults;
                return true;
            };

            double g1WaitMs{},g2WaitMs{};
            const auto g1Generation=submit();
            require(g1Generation!=0,"G7 G1 has a real worker generation");
            const auto g1PaintStart=paintCount,g1MessageStart=messageCount;
            view->setWorkerStatus(g1Generation,0,true,factoryCount,0,false);
            paintAndPump();
            auto g1=await(g1Generation,g1WaitMs);
            g1Paints=paintCount-g1PaintStart;g1Messages=messageCount-g1MessageStart;
            require(g1->factoryVersion==factory.libraryVersion&&g1->factoryCount==factoryCount,
                    "G7 G1 worker reads the supplied V7 Factory");

            const auto g2Generation=submit();
            require(g2Generation!=g1Generation,"G7 G2 is a newer real worker generation");
            const auto g2PaintStart=paintCount,g2MessageStart=messageCount;
            view->setWorkerStatus(g2Generation,0,true,factoryCount,0,false);
            paintAndPump();
            require(!view->acceptsRecommendationGeneration(g1->generation),
                    "G7 UI gate rejects delayed G1 after G2 becomes busy");
            require(!applyWorkerResult(*g1)&&lastAppliedGeneration==0&&appliedResults==0,
                    "G7 stale G1 result is actually prevented from applying");
            auto g2=await(g2Generation,g2WaitMs);
            g2Paints=paintCount-g2PaintStart;g2Messages=messageCount-g2MessageStart;
            require(g2->factoryVersion==factory.libraryVersion&&g2->factoryCount==factoryCount,
                    "G7 G2 worker reads the supplied V7 Factory");
            require(g2->userCount==0,"G7 isolated User Library starts empty");
            mixedBank=&g2->recommendations;exactBank=&g2->exactRecommendations;
            require(view->acceptsRecommendationGeneration(g2->generation),
                    "G7 UI gate accepts the current G2 generation");
            require(applyWorkerResult(*g2)&&lastAppliedGeneration==g2Generation&&appliedResults==1,
                    "G7 current G2 result applies both-view mixed recommendations");

            const auto& mixed=*mixedBank;
            const auto& exact=*exactBank;
            auto uncachedMixed=mixed;uncachedMixed.inspirationOrder.clear();
            const auto canonicalOrder=mixedInspirationOrder(uncachedMixed);
            require(!mixed.inspirationOrder.empty(),"G7 mixed inspiration homepage has legal candidates");
            require(mixed.inspirationOrder.size()==canonicalOrder.size(),
                    "G7 cached inspiration order retains every legal unique candidate");
            std::set<std::pair<std::size_t,std::size_t>> positions;
            std::set<std::string> musicIdentities;
            for(std::size_t n=0;n<mixed.inspirationOrder.size();++n) {
                const auto position=mixed.inspirationOrder[n];
                require(position.group<mixed.groups.size()&&position.index<mixed.groups[position.group].size(),
                        "G7 inspiration row position resolves into the original mixed bank");
                require(position.group==canonicalOrder[n].group&&position.index==canonicalOrder[n].index,
                        "G7 cached inspiration position matches the complete canonical order");
                require(positions.emplace(position.group,position.index).second,
                        "G7 inspiration positions are unique");
                const auto& candidate=mixed.groups[position.group][position.index];
                require(candidate.baseQualified&&candidate.hardConstraintsPassed&&
                        std::isfinite(candidate.rankingScore)&&!candidate.continuation.empty(),
                        "G7 inspiration position is a legal continuation candidate");
                require(musicIdentities.insert(continuationMusicFingerprint(candidate)).second,
                        "G7 inspiration candidates have unique musical identities");
            }

            std::ofstream homepage(output/"g7-homepage.tsv",std::ios::binary);
            require(static_cast<bool>(homepage),"G7 homepage TSV output open");
            homepage<<"rank\tsource\tchannel\tprovenance\tdirection\tfollowup\tfingerprint\torigin\ttarget\tmatch_key\n";
            const auto provenanceName=[](ConnectionProvenance value)->const char* {
                switch(value) {
                    case ConnectionProvenance::DirectSource:return "direct-source";
                    case ConnectionProvenance::AlgorithmAssessed:return "algorithm-assessed";
                    case ConnectionProvenance::EditorialReviewed:return "editorial-reviewed";
                }
                return "unknown";
            };
            const auto directionName=[](CreativeDirection value)->const char* {
                switch(value) {
                    case CreativeDirection::Stable:return "stable";
                    case CreativeDirection::Rich:return "rich";
                    case CreativeDirection::Bold:return "bold";
                }
                return "unknown";
            };
            const auto cleanCell=[](std::string value) {
                for(auto& ch:value)if(ch=='\t'||ch=='\r'||ch=='\n')ch=' ';
                return value;
            };
            const auto homepageRows=std::min<std::size_t>(10,mixed.inspirationOrder.size());
            for(std::size_t rank=0;rank<homepageRows;++rank) {
                const auto position=mixed.inspirationOrder[rank];
                const auto& candidate=mixed.groups[position.group][position.index];
                std::string followup;
                for(const auto& chord:candidate.continuation) {
                    if(!followup.empty())followup+=" > ";
                    followup+=chord.label;
                }
                homepage<<rank+1<<'\t'<<cleanCell(candidate.primaryTemplate)<<'\t'
                    <<recommendationChannelName(candidate.channel)<<'\t'<<provenanceName(candidate.provenance)<<'\t'
                    <<directionName(candidate.direction)<<'\t'<<cleanCell(followup)<<'\t'
                    <<cleanCell(session::continuationFingerprint(candidate))<<'\t'
                    <<cleanCell(formatKey(candidate.originCenter.value_or(candidate.key)))<<'\t'
                    <<cleanCell(candidate.targetCenter?formatKey(*candidate.targetCenter):"not inferred")<<'\t'
                    <<cleanCell(formatKey(candidate.key))<<'\n';
            }
            homepage.close();
            require(homepage.good(),"G7 homepage TSV output write");

            auto exactRequest=request;exactRequest.inspirationPriority=false;exactRequest.prepareBothViews=false;
            const auto independentExact=recommendContinuations(query,CandidateIndex(factory.templates),
                exactRequest,recommendationWeights());
            std::size_t exactCandidates{};
            for(std::size_t group=0;group<exact.groups.size();++group) {
                require(exact.groups[group].size()==independentExact.groups[group].size(),
                        "G7 Exact bank retains independent core group results");
                exactCandidates+=exact.groups[group].size();
                for(std::size_t index=0;index<exact.groups[group].size();++index) {
                    const auto& actual=exact.groups[group][index];
                    const auto& expected=independentExact.groups[group][index];
                    require(actual.id==expected.id&&actual.primaryTemplate==expected.primaryTemplate&&
                            actual.supportingTemplates==expected.supportingTemplates&&actual.channel==expected.channel&&
                            actual.provenance==expected.provenance&&actual.direction==expected.direction&&
                            actual.matchStart==expected.matchStart&&actual.matchEnd==expected.matchEnd&&
                            actual.continuationStart==expected.continuationStart&&
                            actual.rankingScore==expected.rankingScore&&actual.eligibilityScore==expected.eligibilityScore&&
                            actual.matchSimilarity==expected.matchSimilarity&&
                            actual.subscores.match==expected.subscores.match&&
                            actual.subscores.skeleton==expected.subscores.skeleton&&
                            actual.subscores.style==expected.subscores.style&&
                            actual.subscores.intent==expected.subscores.intent&&
                            actual.subscores.cadence==expected.subscores.cadence&&
                            actual.subscores.continuation==expected.subscores.continuation&&
                            actual.subscores.prior==expected.subscores.prior&&
                            actual.subscores.rhythm==expected.subscores.rhythm&&
                            actual.subscores.support==expected.subscores.support&&
                            session::continuationFingerprint(actual)==session::continuationFingerprint(expected),
                            "G7 Exact bank preserves old core raw scores, source, and music identity");
                    require(actual.evidence.size()==expected.evidence.size(),"G7 Exact source evidence count");
                    for(std::size_t e=0;e<actual.evidence.size();++e) {
                        const auto& left=actual.evidence[e];const auto& right=expected.evidence[e];
                        require(left.templateId==right.templateId&&left.key.tonic==right.key.tonic&&
                                left.key.mode==right.key.mode&&left.matchStart==right.matchStart&&
                                left.matchEnd==right.matchEnd&&left.continuationStart==right.continuationStart,
                                "G7 Exact source evidence identity");
                    }
                }
            }

            require(view->runG7UiSmoke(context.get(),capture),"G7 four-entry UI/navigation smoke");
            require(stateCallbacks>0,"G7 UI navigation reaches the preserved stateChanged callback");
            require(workerSubmissions==2,"G7 navigation reuses both prepared banks without another worker submission");
            require(state.imported.revision==phraseRevision&&state.imported.events.size()==4&&
                    state.imported.events.back().name=="Am"&&state.imported.events.back().openEnded,
                    "G7 navigation preserves FGEmAm input and its final OPEN chord");
            require(screenshots.contains("g7-zh.png")&&screenshots.contains("g7-en.png"),
                    "G7 UI smoke captures Chinese and English screens");

            frame->close();frame=nullptr;opened=false;passed=true;
            std::cout<<"G7 UI PASS: Factory V7; G1="<<g1Generation<<" ("<<g1WaitMs<<" ms), G2="
                <<g2Generation<<" ("<<g2WaitMs<<" ms); stale G1 rejected, G2 applied; wait messages="
                <<g1Messages<<'/'<<g2Messages<<", wait paints="<<g1Paints<<'/'<<g2Paints
                <<", total frame-idles="<<idleCount<<", total paints="<<paintCount<<"; Exact/core candidates="
                <<exactCandidates<<", homepage TSV rows="<<homepageRows<<"; zh/en screenshots saved under "
                <<output.string()<<'\n';
        } catch(const std::exception& error) {
            std::cerr<<"G7 UI smoke failure: "<<error.what()<<'\n';
        }
        if(frame){if(opened)frame->close();else frame->forget();}
        VSTGUI::exit();if(SUCCEEDED(com))CoUninitialize();return passed?0:1;
}
}
