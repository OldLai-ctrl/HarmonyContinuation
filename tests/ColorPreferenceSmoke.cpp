#include "color/ColorPreferenceReranker.h"
#include "session/ProductServices.h"
#include "ui/MainView.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/vstguiinit.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <stdexcept>
using namespace harmony;
using namespace harmony::color;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
ContinuationCandidate candidate(const char* id,bool arc,float score) {
    ContinuationCandidate c;c.id=id;c.intent=PhraseIntent::Resolve;c.key={PitchClass::C,Mode::Major};c.rankingScore=score;
    for(int i=0;i<2;++i) {
        ConcreteChordEvent e;e.label=arc&&i==0?"Cmaj7":"C";e.quality=arc&&i==0?ChordQuality::Major7:ChordQuality::Major;
        e.durationQN=4.;e.degree={1,0};c.continuation.push_back(e);
    }
    return c;
}
int main() {
    try {
        ChordEvent c;c.name="C";c.root=PitchClass::C;c.bass=PitchClass::C;c.quality=ChordQuality::Major;c.durationQN=4.;c.openEnded=false;
        Progression progression{c};const auto source=HarmonyColorAnalyzer::prepare(progression);
        RecommendationSet set;set.groups[0]={candidate("flat",false,89),candidate("arc",true,88),candidate("low-quality",true,65)};
        const auto& group=set.groups[0];std::vector<PathColor> paths;
        std::vector<std::string> identities;for(const auto& item:group){paths.push_back(HarmonyColorAnalyzer::analyzeContinuation(source,item));identities.push_back(session::continuationFingerprint(item));}
        const std::vector<std::size_t> original{0,1,2};
        auto result=ColorPreferenceReranker::rank(source,{}, {},group,{},Preference::Off);
        require(result.order==original,"Off preserves original IDs/order without color data");
        std::cout<<"1 Off / original candidate order PASS\n";
        result=ColorPreferenceReranker::rank(source,{}, {},group,paths,Preference::TensionArc);
        require(result.order==std::vector<std::size_t>({1,0,2}),"complete tension arc advances within quality window only");
        require(result.decisions[1].cost&&result.decisions[0].cost&&*result.decisions[1].cost<*result.decisions[0].cost,"whole trajectory accumulation/release beats flat tension");
        for(std::size_t i=0;i<group.size();++i)require(identities[i]==session::continuationFingerprint(group[i]),"candidate musical content unchanged");
        std::cout<<"2 complete tension goal / original quality protection PASS\n";
        auto unknownPaths=paths;unknownPaths[1].status=Status::Unknown;
        result=ColorPreferenceReranker::rank(source,{}, {},group,unknownPaths,Preference::TensionArc);
        require(result.order==original&&!result.decisions[1].cost,"Unknown anchors retain original positions");
        unknownPaths[1].status=Status::Uncertain;
        result=ColorPreferenceReranker::rank(source,{}, {},group,unknownPaths,Preference::TensionArc);
        require(result.order==original&&!result.decisions[1].cost,"Uncertain does not select favorable direction");
        result=ColorPreferenceReranker::rank(source,{}, {},group,paths,Preference::ContinueSource);
        require(result.order==original&&result.decisions[0].reason==RankReason::InsufficientSource,"short prefix auto fallback");
        result=ColorPreferenceReranker::rank(source,{}, {},group,paths,Preference::Off);
        require(result.order==original,"Off restores original order after active reranking");
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);VSTGUI::init(GetModuleHandleW(nullptr));
        {
            auto canvas=VSTGUI::COffscreenContext::create({1100,900});require(static_cast<bool>(canvas),"UI canvas");
            auto view=VSTGUI::owned(new ui::MainView({0,0,1100,900},{}));
            require(view->runColorSortSmoke(canvas.get(),progression,set),"UI toggle / Why / no regeneration / original index actions");
        }
        VSTGUI::exit();CoUninitialize();std::cout<<"3 Unknown / Uncertain / Off restore / same-scenario UI+Why PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
