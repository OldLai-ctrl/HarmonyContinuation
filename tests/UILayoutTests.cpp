#include "ui/UILayout.h"
#include "ui/ProgressionTimeline.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace harmony::ui;
namespace {
int checks{};
void check(bool okay,const char* name){if(!okay)throw std::runtime_error(name);++checks;}
bool inside(UiRect r,UiRect p){return r.left>=p.left&&r.top>=p.top&&r.right<=p.right&&r.bottom<=p.bottom&&r.width()>=0&&r.height()>=0;}
bool overlap(UiRect a,UiRect b){return a.left<b.right&&a.right>b.left&&a.top<b.bottom&&a.bottom>b.top;}
}
int main(){
    try {
        constexpr std::array<int,18> widths{900,901,1049,1050,1051,1099,1100,1599,1600,1601,
            1701,1702,1703,1800,2105,2106,2107,2200};
        constexpr std::array<int,6> heights{640,641,700,850,900,1400};
        constexpr std::array<double,4> scales{1,1.25,1.5,2};
        for(auto w:widths)for(auto h:heights)for(auto scale:scales)for(bool inspector:{false,true})for(bool compare:{false,true}){
            auto l=computeLayout({static_cast<double>(w),static_cast<double>(h),scale,
                harmony::session::Tab::Recommend,inspector,compare});
            check(inside(l.topBar,l.viewport)&&inside(l.phrase,l.viewport)&&inside(l.content,l.viewport),"primary bounds");
            check(!overlap(l.topBar,l.phrase)&&!overlap(l.phrase,l.content),"primary separation");
            check(inside(l.timeline,l.phrase),"timeline bounds");
            for(auto control:l.topControls)check(inside(control,l.topBar),"top control bounds");
            check(l.laneColumns==1||l.laneColumns==2||l.laneColumns==4,"column count");
            check(l.lanes[0].width()>=410,"card minimum width");
            for(int a=0;a<4;++a)for(int b=a+1;b<4;++b)check(!overlap(l.lanes[a],l.lanes[b]),"lane separation");
            for(const auto lane:l.lanes)for(int row=0;row<3;++row)for(bool exportControls:{false,true}) {
                const auto card=candidateRowGeometry(lane,row,exportControls);
                check(inside(card.row,lane)&&inside(card.timeline,card.row)&&inside(card.score,card.row),"candidate row geometry");
                check(inside(card.pin,card.row)&&inside(card.audition,card.row)&&
                    !overlap(card.pin,card.audition)&&!overlap(card.timeline,card.score),"candidate action geometry");
                check(card.row.height()>=48&&inside(card.intent,card.row)&&
                    !overlap(card.timeline,card.intent)&&!overlap(card.timeline,card.pin),
                    "readable candidate rows");
                if(exportControls)check(inside(card.midi,card.row)&&inside(card.snapshot,card.row)&&
                    !overlap(card.midi,card.snapshot),"candidate export geometry");
            }
            check(l.laneScrollMax>=0&&l.libraryRows>=1,"scroll and rows");
            if(inspector)check(inside(l.inspector,l.viewport)&&l.inspectorMode!=InspectorMode::None,"inspector bounds");
            if(compare)check(inside(l.compare,l.viewport)&&!overlap(l.compare,l.content),"compare bounds");
            if(w<1050)check(l.mode==LayoutMode::Compact&&l.laneColumns==1,"compact breakpoint");
            else if(w<1600)check(l.mode==LayoutMode::Standard&&l.laneColumns==2,"standard breakpoint");
            else check(l.mode==LayoutMode::Wide,"wide breakpoint");
        }
        for(char letter='A';letter<='H';++letter)for(auto w:{900,1100,1800}){
            const auto l=computeLayout({static_cast<double>(w),900,1,harmony::session::Tab::Recommend,false,false});
            for(int candidates:{0,1,3,10})check(candidates==0||l.lanes[0].width()>=410,"fixture card fit");
        }
        for(const int count:{16,32,64}) {
            harmony::Progression chords;
            for(int i=0;i<count;++i){harmony::ChordEvent event;event.name=i%2?"F#m7b5":"Cmaj7";
                event.startQN=i*4.0;event.durationQN=4.0;chords.push_back(std::move(event));}
            const auto timeline=layoutScrollableTimeline(chords,852);
            check(timeline.blocks.size()==static_cast<std::size_t>(count)&&timeline.contentWidth>=count*56.0,
                "long progression scroll width");
            for(const auto& block:timeline.blocks)check(block.width>=56,"long progression tile width");
            check(timeline.blocks.back().x+timeline.blocks.back().width<=timeline.contentWidth+0.01,
                "long progression final bound");
        }
        std::cout<<checks<<" layout checks passed\n";
    }catch(const std::exception& e){std::cerr<<"UILayoutTests: "<<e.what()<<'\n';return 1;}
}
