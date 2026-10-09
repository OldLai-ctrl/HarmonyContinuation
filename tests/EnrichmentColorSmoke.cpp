#include "color/ColorPreferenceReranker.h"
#include "ui/MainView.h"
#include "preview/VoiceLeadingMetrics.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/vstguiinit.h"
#include <windows.h>
#include <objbase.h>
#include <iostream>
#include <stdexcept>
using namespace harmony;
using namespace harmony::color;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
Progression path(bool arc) {
    Progression p;
    for(int i=0;i<16;++i) {
        const bool seventh=arc?(i==7||i==8):(i==0||i==15);
        ChordEvent e;e.name=seventh?"Cmaj7":"C";e.root=PitchClass::C;e.bass=PitchClass::C;
        e.quality=seventh?ChordQuality::Major7:ChordQuality::Major;e.startQN=i*4.;e.durationQN=4.;e.openEnded=false;p.push_back(e);
    }
    return p;
}
int main(int argc,char** argv) {
    try {
        auto source=path(false);for(auto& event:source){event.name="C";event.quality=ChordQuality::Major;}
        enrichment::EnrichmentResult candidates;
        for(int i=0;i<2;++i) {
            enrichment::EnrichmentCandidate c;c.id=i?"arc-upgrade":"edge-tension";c.fingerprint=c.id;
            c.progression=path(i==1);c.score=89.f-i;c.skeletonPreservation=.985f;c.styleCompatibility=.8f;c.complexityScore=.44f;
            c.techniques={enrichment::TechniqueID::SeventhColor};
            for(std::size_t index=0;index<c.progression.size();++index)if(c.progression[index].quality==ChordQuality::Major7) {
                enrichment::EnrichmentOperation op;op.type=enrichment::OperationType::UpgradeQuality;
                op.technique=enrichment::TechniqueID::SeventhColor;op.sourceIndex=index;op.before="C";op.after="Cmaj7";c.operations.push_back(op);
            }
            candidates.groups[0].push_back(c);
        }
        const auto& group=candidates.groups[0];std::vector<PathColor> colors;
        for(const auto& c:group)colors.push_back(ColorPreferenceReranker::analyzeEnrichment(c.progression));
        const std::vector<std::size_t> original{0,1};
        auto ranked=ColorPreferenceReranker::rankEnrichment(source,{}, {},HarmonicTendency::Balanced,group,colors,Preference::TensionArc);
        if(ranked.order!=std::vector<std::size_t>({1,0})) {
            for(std::size_t i=0;i<group.size();++i)std::cerr<<group[i].id<<" reason="<<rankReasonKey(ranked.decisions[i].reason)
                <<" cost="<<ranked.decisions[i].cost.value_or(-1.)<<" voice="<<preview::measureVoiceLeading(group[i].progression)->score<<'\n';
        }
        require(ranked.order==std::vector<std::size_t>({1,0}),"qualified complete enrichment tension arc must advance");
        auto bassFixture=source;bassFixture[0].name="C";bassFixture[0].quality=ChordQuality::Major;
        bassFixture[0].bass=PitchClass::E;
        const auto inverted=ColorPreferenceReranker::analyzeEnrichment(bassFixture);
        require(inverted.positions[0].chord.W&&*inverted.positions[0].chord.W< -9.,"typed bass overrides abstract label for model W");
        bassFixture[0].bass.reset();
        const auto missing=ColorPreferenceReranker::analyzeEnrichment(bassFixture);
        require(missing.status==Status::Uncertain&&!missing.positions[0].chord.W,"missing bass is uncertain, no fabricated root bass score");
        std::cout<<"Enrichment tension goal / explicit inversion bass PASS\n";
        if(argc>1&&std::string(argv[1])=="--target-only")return 0;
        ranked=ColorPreferenceReranker::rankEnrichment(source,{}, {},HarmonicTendency::Balanced,group,{},Preference::Off);
        require(ranked.order==original,"Off restores original indices without color work");
        std::cout<<"Off restores original order PASS\n";
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);VSTGUI::init(GetModuleHandleW(nullptr));
        {
            auto canvas=VSTGUI::COffscreenContext::create({1100,900});require(static_cast<bool>(canvas),"UI canvas");
            auto view=VSTGUI::owned(new ui::MainView({0,0,1100,900},{}));
            require(view->runEnrichmentColorSmoke(canvas.get(),source,candidates),"reordered card / Why / preview / MIDI / comparison / snapshot identity");
        }
        VSTGUI::exit();CoUninitialize();
        std::cout<<"Reordered card / Why / Preview / MIDI / comparison / snapshot identity PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
