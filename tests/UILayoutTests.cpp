#include "ui/UILayout.h"
#include "ui/OverlayPolicy.h"
#include "ui/ProgressionTimeline.h"
#include "ui/RecommendationAlignment.h"
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
void recommendationAlignmentChecks() {
    harmony::ImportedProgressionSession imported;
    for (const auto* name : {"Am", "G#dim", "G", "D/F#"}) {
        harmony::ChordEvent event;
        event.name = name;
        event.startQN = static_cast<double>(imported.events.size()) * 4.;
        event.durationQN = 4.;
        event.openEnded = false;
        imported.events.push_back(std::move(event));
    }
    imported.events.back().durationQN.reset();
    imported.events.back().openEnded = true;

    harmony::MatchResult match;
    match.templateId = "r01";
    match.key = {harmony::PitchClass::E, harmony::Mode::Minor};
    match.templateMatchStart = 1;
    match.templateMatchEnd = 4;
    match.continuationStartIndex = 5;
    match.hasContinuation = true;
    match.continuationLength = 1;
    match.templateLabels = {"V", "iv", "G#dim", "III", "VII", "i"};
    match.alignmentTrace = {
        {0, 1, harmony::AlignmentOp::Match},
        {std::nullopt, 2, harmony::AlignmentOp::TemplateDeletion},
        {1, std::nullopt, harmony::AlignmentOp::QueryInsertion},
        {2, 3, harmony::AlignmentOp::Match},
        {3, 4, harmony::AlignmentOp::Substitute}
    };

    harmony::ContinuationCandidate candidate;
    candidate.primaryTemplate = match.templateId;
    candidate.key = match.key;
    candidate.matchStart = match.templateMatchStart;
    candidate.matchEnd = match.templateMatchEnd;
    candidate.continuationStart = match.continuationStartIndex;
    candidate.continuation.push_back({"Em"});

    const auto cells = recommendationAlignment(imported, candidate, match);
    check(cells.size() == 6, "R01 alignment rows omit source prefix");
    check(cells[0].kind == AlignmentCellKind::Aligned &&
        cells[0].queryIndex == 0 && cells[0].templateIndex == 1,
        "R01 Am follows matched iv trace");
    check(cells[1].kind == AlignmentCellKind::TemplateSkip &&
        !cells[1].queryIndex && cells[1].templateIndex == 2,
        "R01 skipped template event remains a gap");
    check(cells[2].kind == AlignmentCellKind::InputInsertion &&
        cells[2].queryIndex == 1 && !cells[2].templateIndex,
        "R01 G sharp diminished stays an input insertion");
    check(cells[3].kind == AlignmentCellKind::Aligned &&
        cells[3].queryIndex == 2 && cells[3].templateIndex == 3,
        "R01 G follows matched III trace");
    check(cells[4].kind == AlignmentCellKind::Aligned &&
        cells[4].queryIndex == 3 && cells[4].templateIndex == 4,
        "R01 D over F sharp follows matched VII trace");
    check(cells[4].queryIndex && imported.events[*cells[4].queryIndex].openEnded,
        "OPEN input event remains reachable by query index");
    check(cells[5].kind == AlignmentCellKind::Continuation &&
        !cells[5].queryIndex && cells[5].templateIndex == 5 &&
        cells[5].continuationBoundary,
        "R01 continuation begins at actual template i");
    std::array<int, 4> queryCoverage{};
    for (const auto& cell : cells) if (cell.queryIndex) ++queryCoverage[*cell.queryIndex];
    for (const auto count : queryCoverage)
        check(count == 1, "every imported query event appears exactly once");

    auto missingTrace = match;
    missingTrace.alignmentTrace.clear();
    check(recommendationAlignment(imported, candidate, missingTrace).empty(),
        "alignment without trace is empty");
    auto incompleteTrace = match;
    incompleteTrace.alignmentTrace.pop_back();
    check(recommendationAlignment(imported, candidate, incompleteTrace).empty(),
        "incomplete query coverage is empty");
    auto outOfRange = match;
    outOfRange.alignmentTrace[0].queryIndex = imported.events.size();
    check(recommendationAlignment(imported, candidate, outOfRange).empty(),
        "out of range query index is empty");
    outOfRange = match;
    outOfRange.alignmentTrace[0].templateIndex = match.templateLabels.size();
    check(recommendationAlignment(imported, candidate, outOfRange).empty(),
        "out of range template index is empty");
    check(recommendationAlignment(imported, candidate, match, true).empty(),
        "snapshot alignment does not reuse lost query indices");

    harmony::ImportedProgressionSession wrapInput;
    for (const auto* name : {"Am", "G#dim", "G"}) {
        harmony::ChordEvent event;
        event.name = name;
        event.startQN = static_cast<double>(wrapInput.events.size()) * 4.;
        event.durationQN = 4.;
        event.openEnded = false;
        wrapInput.events.push_back(std::move(event));
    }
    harmony::MatchResult wrapMatch;
    wrapMatch.templateId = "wrap";
    wrapMatch.key = match.key;
    wrapMatch.templateMatchStart = 2;
    wrapMatch.templateMatchEnd = 3;
    wrapMatch.continuationStartIndex = 4;
    wrapMatch.hasContinuation = true;
    wrapMatch.continuationLength = 1;
    wrapMatch.templateLabels = {"a", "b", "c", "d", "e"};
    wrapMatch.alignmentTrace = {
        {0, 2, harmony::AlignmentOp::Match},
        {1, std::nullopt, harmony::AlignmentOp::QueryInsertion},
        {2, 3, harmony::AlignmentOp::Substitute}
    };
    harmony::ContinuationCandidate wrapCandidate;
    wrapCandidate.primaryTemplate = wrapMatch.templateId;
    wrapCandidate.key = wrapMatch.key;
    wrapCandidate.matchStart = wrapMatch.templateMatchStart;
    wrapCandidate.matchEnd = wrapMatch.templateMatchEnd;
    wrapCandidate.continuationStart = wrapMatch.continuationStartIndex;
    wrapCandidate.continuation.resize(3);
    const auto wrapped = recommendationAlignment(wrapInput, wrapCandidate, wrapMatch);
    check(wrapped.size() == 6 && wrapped[3].templateIndex == 4 &&
        wrapped[4].templateIndex == 0 && wrapped[5].templateIndex == 1,
        "continuation follows finite suffix and one template wrap");
    check(wrapped[3].continuationBoundary && !wrapped[4].continuationBoundary &&
        !wrapped[5].continuationBoundary,
        "only first continuation row marks the boundary");
    wrapCandidate.continuation.resize(4);
    check(recommendationAlignment(wrapInput, wrapCandidate, wrapMatch).empty(),
        "continuation rejects repeated wraps beyond engine output");
}
int main(){
    try {
        recommendationAlignmentChecks();
        check(UILayoutResult::minimumWidth==800&&UILayoutResult::minimumHeight==640,"global minimum size");
        check(UILayoutResult::defaultWidth==800&&UILayoutResult::defaultHeight==780,"global default size");
        const auto defaultLayout=computeLayout({});
        check(std::abs(defaultLayout.viewport.width()-800.)<0.01&&
            std::abs(defaultLayout.viewport.height()-780.)<0.01,"default layout size");
        const auto clampedLayout=computeLayout({100,100});
        check(std::abs(clampedLayout.viewport.width()-800.)<0.01&&
            std::abs(clampedLayout.viewport.height()-640.)<0.01,"minimum layout size");
        const auto overlay=computeLayout({1100,900,1,harmony::session::Tab::Recommend,true,false}).inspector;
        check(!dismissOnOutsideClick(OverlayKind::Transient,overlay,overlay.left+12,overlay.top+12),"inside overlay remains");
        check(dismissOnOutsideClick(OverlayKind::Transient,overlay,overlay.left-12,overlay.top+12),"outside dismisses transient");
        check(!dismissOnOutsideClick(OverlayKind::Persistent,overlay,overlay.left-12,overlay.top+12),"persistent remains");
        check(!dismissOnOutsideClick(OverlayKind::Modal,overlay,overlay.left-12,overlay.top+12),"modal remains");
        check(dismissOnEscape(OverlayKind::Transient)&&!dismissOnEscape(OverlayKind::Persistent)&&
            !dismissOnEscape(OverlayKind::Modal),"escape policy");
        constexpr std::array<int,20> widths{800,801,899,900,901,1049,1050,1051,1099,1100,
            1599,1600,1601,1701,1702,1703,1800,2105,2106,2200};
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
            for(const auto lane:l.lanes)for(int row=0;row<2;++row)for(bool exportControls:{false,true}) {
                const auto card=candidateRowGeometry(lane,row,exportControls);
                check(inside(card.row,lane)&&inside(card.timeline,card.row)&&inside(card.score,card.row)&&
                    inside(card.summary,card.row)&&inside(card.why,card.row),"candidate row geometry");
                check(inside(card.pin,card.row)&&inside(card.audition,card.row)&&
                    !overlap(card.pin,card.audition)&&!overlap(card.timeline,card.score),"candidate action geometry");
                check(card.row.height()>=70&&inside(card.intent,card.row)&&
                    !overlap(card.timeline,card.intent)&&!overlap(card.timeline,card.pin)&&
                    !overlap(card.summary,card.why)&&!overlap(card.summary,card.score),
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
        for(const auto [w,h]:std::array<std::pair<int,int>,3>{{{900,640},{1100,900},{1800,1000}}})
            for(const auto zoom:{1.,1.25,1.5})for(const auto dpi:{1.,1.5}) {
                const auto l=computeLayout({static_cast<double>(w),static_cast<double>(h),dpi,
                    harmony::session::Tab::Recommend,true,true,zoom});
                check(std::abs(l.viewport.width()*zoom-w)<0.01,"zoom independent from resize");
                for(auto control:l.constraintControls)check(inside(control,l.topBar),"constraint control bounds");
                check(inside(l.inspector,l.viewport)&&l.inspector.height()>180,"zoom overlay bounds");
                const auto card=candidateRowGeometry(l.lanes[0],0,true);
                check(card.row.width()>=410&&inside(card.midi,card.row)&&inside(card.why,card.row),"zoom card controls");
                const auto library=computeLayout({static_cast<double>(w),static_cast<double>(h),dpi,
                    harmony::session::Tab::Library,false,false,zoom});
                check(library.content.height()>=165&&library.libraryRows>=1,"zoom library usable");
            }

        const auto compact=computeCompactLayout(800,780,1.);
        check(compact.candidateColumns==1&&!compact.narrow,"A3 is single-column at default size");
        check(compact.header.height()==48&&compact.input.height()==140&&compact.footer.height()==110,
            "A3 base region sizes");
        check(inside(compact.header,compact.viewport)&&inside(compact.input,compact.viewport)&&
            inside(compact.recommendationHeading,compact.viewport)&&inside(compact.intentToolbar,compact.viewport)&&
            inside(compact.cardsViewport,compact.viewport)&&inside(compact.footer,compact.viewport),
            "A3 region bounds");
        check(!overlap(compact.header,compact.input)&&!overlap(compact.input,compact.recommendationHeading)&&
            !overlap(compact.recommendationHeading,compact.intentToolbar)&&
            !overlap(compact.intentToolbar,compact.cardsViewport)&&!overlap(compact.cardsViewport,compact.footer),
            "A3 region separation");
        for(const auto toolbar:{compact.intentToolbar,computeCompactLayout(800,780,1.5).intentToolbar}) {
            std::array<UiRect,5> tabs{};
            for(std::size_t i=0;i<tabs.size();++i){
                tabs[i]=compactIntentTabGeometry(toolbar,i);
                check(inside(tabs[i],toolbar)&&tabs[i].width()>70,"A3 five intent tabs remain readable");
                for(std::size_t j=0;j<i;++j)check(!overlap(tabs[j],tabs[i]),"A3 intent tabs do not overlap");
            }
            check(compactIntentTabGeometry(toolbar,5).width()==0,"A3 invalid intent tab is empty");
        }

        const auto wideCard=compactCandidateGeometry({12,270,788,664},270,false);
        check(!wideCard.actionWrapped&&wideCard.row.height()==100&&
            inside(wideCard.row,{12,270,788,664}),"A3 mini card size");
        check(wideCard.select.width()==28&&wideCard.select.height()==28&&
            inside(wideCard.select,wideCard.row),"A3 circular selection bounds");
        const std::array<UiRect,8> wideControls{wideCard.select,wideCard.name,wideCard.facts,
            wideCard.location,wideCard.match,wideCard.audition,wideCard.midi,wideCard.details};
        for(const auto control:wideControls)check(inside(control,wideCard.row),"A3 mini control bounds");
        for(std::size_t a=0;a<wideControls.size();++a)for(std::size_t b=a+1;b<wideControls.size();++b)
            check(!overlap(wideControls[a],wideControls[b]),"A3 mini control separation");
        check(wideCard.timeBand.height()==34&&inside(wideCard.timeBand,wideCard.row)&&
            wideCard.audition.width()==56&&wideCard.midi.width()==66&&wideCard.details.width()==52&&
            wideCard.audition.height()==36&&wideCard.midi.height()==36&&wideCard.details.height()==36,
            "A3 readable mini action and ribbon sizes");
        const auto thirdCard=compactCandidateGeometry({12,200,788,800},400,false);
        check(wideCard.timeBand.top==326&&thirdCard.row.top==400&&thirdCard.timeBand.top==456&&
            inside(thirdCard.timeBand,thirdCard.row),"A3 time band follows candidate row offset");
        check(!overlap(wideCard.timeBand,wideCard.audition)&&
            !overlap(wideCard.timeBand,wideCard.midi)&&!overlap(wideCard.timeBand,wideCard.details),
            "A3 mini time band retained");

        const auto narrow=computeCompactLayout(800,780,1.5);
        check(narrow.candidateColumns==1&&narrow.narrow&&narrow.logicalWidth<620&&
            narrow.footer.height()==110,"A3 narrow layout and wrapped footer");
        check(computeCompactLayout(929,780,1.5).narrow&&
            !computeCompactLayout(930,780,1.5).narrow,"A3 logical width breakpoint");
        check(compactCandidateGeometry({12,0,607,240},0,false).actionWrapped&&
            !compactCandidateGeometry({12,0,608,240},0,false).actionWrapped,
            "A3 card action wrap breakpoint");
        const UiRect narrowArea{12,270,521,500};
        const auto narrowCard=compactCandidateGeometry(narrowArea,270,false);
        check(narrowCard.actionWrapped&&narrowCard.row.height()==148&&
            narrowCard.select.width()==28&&narrowCard.select.height()==28,"A3 narrow mini geometry");
        const std::array<UiRect,8> narrowControls{narrowCard.select,narrowCard.name,narrowCard.facts,
            narrowCard.location,narrowCard.match,narrowCard.audition,narrowCard.midi,narrowCard.details};
        for(const auto control:narrowControls)check(inside(control,narrowCard.row),"A3 narrow control bounds");
        for(std::size_t a=0;a<narrowControls.size();++a)for(std::size_t b=a+1;b<narrowControls.size();++b)
            check(!overlap(narrowControls[a],narrowControls[b]),"A3 narrow control separation");
        check(narrowCard.timeBand.height()==34&&narrowCard.audition.top==narrowCard.row.top+56&&
            narrowCard.timeBand.top==narrowCard.row.top+104&&
            inside(narrowCard.timeBand,narrowCard.row)&&!overlap(narrowCard.timeBand,narrowCard.audition)&&
            !overlap(narrowCard.timeBand,narrowCard.midi)&&!overlap(narrowCard.timeBand,narrowCard.details),
            "A3 narrow third action row and time band retained");

        const auto detailedCard=compactCandidateGeometry(narrowArea,270,true);
        check(detailedCard.row.height()==190&&detailedCard.timeBand.height()==34&&
            detailedCard.inputTimes.height()==34&&inside(detailedCard.inputTimes,detailedCard.row)&&
            inside(detailedCard.sourceLocation,detailedCard.row)&&
            detailedCard.timeBand.top==detailedCard.row.top+100,
            "A3 detailed dual time bands and source geometry");
        check(!overlap(detailedCard.timeBand,detailedCard.sourceLocation)&&
            !overlap(detailedCard.sourceLocation,detailedCard.inputTimes)&&
            !overlap(detailedCard.details,detailedCard.inputTimes)&&
            !overlap(detailedCard.details,detailedCard.timeBand)&&
            !overlap(detailedCard.audition,detailedCard.sourceLocation)&&
            !overlap(detailedCard.midi,detailedCard.sourceLocation)&&
            !overlap(detailedCard.audition,detailedCard.midi)&&
            !overlap(detailedCard.audition,detailedCard.details)&&
            !overlap(detailedCard.midi,detailedCard.details),"A3 detailed action and information separation");

        const auto minimumZoom=computeCompactLayout(800,640,1.5);
        check(minimumZoom.cardsViewport.height()>=40&&inside(minimumZoom.cardsViewport,minimumZoom.viewport),
            "A3 minimum zoom keeps scrollable candidate area");
        std::cout<<checks<<" layout checks passed\n";
    }catch(const std::exception& e){std::cerr<<"UILayoutTests: "<<e.what()<<'\n';return 1;}
}
