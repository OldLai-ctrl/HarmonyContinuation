#include "color/HarmonyColorAnalyzer.h"
#include "ui/ColorHint.h"
#include "ui/MainView.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/vstguiinit.h"
#include <windows.h>
#include <objbase.h>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace harmony;
using color::HarmonyColorAnalyzer;
void require(bool value,const char* name){if(!value)throw std::runtime_error(name);}
bool approxEqual(double a,double b){return std::abs(a-b)<1.e-6;}
int main() {
    try {
        // 1. Ordinary chord: independent angle and trigonometric reference.
        const auto c=HarmonyColorAnalyzer::analyze(chordPitches("C",ChordQuality::Major));
        require(c.status==color::Status::Known&&approxEqual(*c.r,10.)&&approxEqual(*c.theta,55.*std::numbers::pi/180.),"C static coordinates");
        require(approxEqual(*c.W,7.660444431)&&approxEqual(*c.S,6.427876097)&&approxEqual(*c.T,0.),"C W/S/T");
        const std::vector<color::TimedChord> single{{chordPitches("C"),0.,4.}};
        require(!HarmonyColorAnalyzer::analyzePath(single).positions.front().fromPrevious,"first chord has no dynamic metrics");
        std::cout<<"1 ordinary chord PASS\n";
        // 2. Inversions, unequal durations and the original/recommended seam.
        const auto inversion=HarmonyColorAnalyzer::analyze(chordPitches("C/E",ChordQuality::Major));
        const auto change=HarmonyColorAnalyzer::transition(c,inversion);
        require(change&&approxEqual(change->Ts,0.)&&approxEqual(change->Ws,0.)&&approxEqual(*inversion.W,-9.396926208),"inversion changes W, not r/theta");
        const std::vector<color::TimedChord> source{{chordPitches("C/E"),0.,6.},{chordPitches("C/G"),6.,1.}};
        ContinuationCandidate candidate;ConcreteChordEvent end;end.label="C";end.quality=ChordQuality::Major;end.durationQN=1.;candidate.continuation.push_back(end);
        const auto path=HarmonyColorAnalyzer::analyzeContinuation(source,candidate);
        require(path.status==color::Status::Known&&path.recommendationBoundary==2&&path.positions[2].fromPrevious&&
                approxEqual(*path.positions[2].startQN,7.)&&path.meanW&&*path.meanW< -1.&&path.warming,"whole path cool, gradually warmer; seam/timing retained");
        require(ui::colorHint(path,session::Locale::ZhCN).tone==ui::ColorTone::Cool,"ending alone must not color whole path warm");
        std::cout<<"2 inversion / complete path / seam PASS\n";
        // 3. Angular wrap and a genuinely multi-directional sonority.
        const auto d=HarmonyColorAnalyzer::analyze(chordPitches("D",ChordQuality::Major));
        const auto g=HarmonyColorAnalyzer::analyze(chordPitches("G",ChordQuality::Major));
        const auto crossing=HarmonyColorAnalyzer::transition(d,g);
        require(crossing&&approxEqual(crossing->Ws,-5.)&&approxEqual(crossing->Ts,5.176380902),"355 -> 25 degrees: radians and signed Ws");
        const auto ambiguous=HarmonyColorAnalyzer::analyze(chordPitches("Caug",ChordQuality::Augmented));
        require(ambiguous.status==color::Status::Uncertain&&ambiguous.directions.size()==3&&!ambiguous.W&&
                !HarmonyColorAnalyzer::transition(c,ambiguous),"multi-direction structure has no invented score");
        std::cout<<"3 wrap / multi-direction PASS\n";
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);VSTGUI::init(GetModuleHandleW(nullptr));
        {
            const auto canvas=VSTGUI::COffscreenContext::create({1100,900});require(static_cast<bool>(canvas),"UI offscreen context");
            auto view=VSTGUI::owned(new ui::MainView({0,0,1100,900},{}));
            require(view->runColorHintSmoke(canvas.get()),"UI / Why / toggle / editor preference smoke");
        }
        VSTGUI::exit();CoUninitialize();std::cout<<"UI / Why zh-CN+en-US / toggle PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
