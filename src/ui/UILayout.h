#pragma once
#include "session/PluginSessionState.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace harmony::ui {
struct UiRect {
    double left{}, top{}, right{}, bottom{};
    double width() const { return right-left; }
    double height() const { return bottom-top; }
    bool contains(double x,double y) const { return x>=left&&x<right&&y>=top&&y<bottom; }
};
enum class LayoutMode { Compact, Standard, Wide };
enum class InspectorMode { None, Overlay, Side };
struct UILayoutInput {
    double width{800}, height{780}, contentScaleFactor{1};
    session::Tab tab{session::Tab::Recommend};
    bool inspectorOpen{}, compareOpen{};
    double userZoom{1};
};
struct UILayoutResult {
    static constexpr int minimumWidth=800, minimumHeight=640;
    static constexpr int defaultWidth=800, defaultHeight=780;
    static constexpr int maximumWidth=2200, maximumHeight=1400;
    LayoutMode mode{LayoutMode::Standard};
    InspectorMode inspectorMode{InspectorMode::None};
    int laneColumns{2}, libraryRows{1};
    double contentScaleFactor{1};
    UiRect viewport,topBar,phrase,timeline,content,inspector,compare,library;
    std::array<UiRect,4> lanes{};
    std::array<UiRect,6> topControls{}; // key, style, intent, advanced, recommend, library
    std::array<UiRect,4> constraintControls{};
    double laneHeight{}, laneScrollMax{};
};
struct CandidateRowGeometry {
    UiRect row,timeline,summary,why,score,intent,pin,audition,midi,snapshot;
};
inline CandidateRowGeometry candidateRowGeometry(UiRect lane,int row,bool exportControls) {
    CandidateRowGeometry out;
    const double rowHeight=(lane.height()-30)/2;
    const double y=lane.top+27+row*rowHeight,right=lane.right-6;
    const double actionWidth=exportControls?148:77,pin=right-actionWidth;
    const bool roomy=rowHeight>44;
    const double actionY=roomy?y+rowHeight-26:y+4;
    const double scoreLeft=pin-37;
    out.row={lane.left+6,y,right,y+rowHeight-2};
    out.timeline={lane.left+11,y+5,roomy?right-8:std::max(lane.left+115,scoreLeft-4),
        y+std::min(25.,rowHeight-5)};
    out.summary={lane.left+12,y+29,right-56,y+51};
    out.why={right-53,y+29,right-5,y+51};
    out.score={scoreLeft,actionY,scoreLeft+33,actionY+22};
    out.intent={lane.left+12,actionY,scoreLeft-2,actionY+22};
    out.pin={pin,actionY,pin+35,actionY+22};
    out.audition={pin+36,actionY,exportControls?pin+64:right,actionY+22};
    if(exportControls) {
        out.midi={pin+65,actionY,pin+108,actionY+22};
        out.snapshot={pin+109,actionY,right,actionY+22};
    }
    return out;
}

struct CompactLayoutResult {
    UiRect viewport,header,input,recommendationHeading,intentToolbar,cardsViewport,footer;
    double logicalWidth{},logicalHeight{},userZoom{1};
    int candidateColumns{1};
    bool narrow{};
};

struct CompactCandidateGeometry {
    UiRect row,select,name,facts,location,match,timeBand,inputTimes,sourceLocation;
    UiRect audition,midi,details;
    bool actionWrapped{};
};

inline CompactLayoutResult computeCompactLayout(double width,double height,double userZoom=1.) {
    CompactLayoutResult out;
    const double zoom=std::clamp(std::isfinite(userZoom)?userZoom:1.,1.,1.5);
    const double w=std::clamp(std::isfinite(width)?width:static_cast<double>(UILayoutResult::defaultWidth),
        static_cast<double>(UILayoutResult::minimumWidth),static_cast<double>(UILayoutResult::maximumWidth))/zoom;
    const double h=std::clamp(std::isfinite(height)?height:static_cast<double>(UILayoutResult::defaultHeight),
        static_cast<double>(UILayoutResult::minimumHeight),static_cast<double>(UILayoutResult::maximumHeight))/zoom;
    out.viewport={0,0,w,h};
    out.logicalWidth=w;
    out.logicalHeight=h;
    out.userZoom=zoom;
    out.narrow=w<620.;

    out.header={0,0,w,48};
    out.input={12,52,w-12,192};
    out.recommendationHeading={12,196,w-12,224};
    out.intentToolbar={12,228,w-12,264};
    const double footerHeight=110.;
    out.footer={0,h-footerHeight,w,h};
    const double cardsTop=270.;
    out.cardsViewport={12,cardsTop,w-12,std::max(cardsTop,out.footer.top-6)};
    return out;
}

inline UiRect compactIntentTabGeometry(UiRect toolbar,std::size_t index,std::size_t count=5) noexcept {
    if(count==0||index>=count)return {};
    constexpr double gap=4.;
    const double width=(toolbar.width()-gap*(count-1))/count;
    const double left=toolbar.left+index*(width+gap);
    return {left,toolbar.top,left+width,toolbar.bottom};
}

inline CompactCandidateGeometry compactCandidateGeometry(UiRect area,double y,bool detailed) {
    CompactCandidateGeometry out;
    // cardsViewport uses 12 logical pixels of horizontal inset on each side.
    out.actionWrapped=area.width()<596.;
    const double height=detailed?190.:(out.actionWrapped?148.:100.);
    out.row={area.left,y,area.right,y+height};

    const double inset=8.,gap=8.;
    constexpr double auditionWidth=56.,midiWidth=66.,detailsWidth=52.,buttonHeight=36.;
    const double right=area.right-inset;
    out.select={area.left+inset,y+8,area.left+inset+28,y+36};
    const double contentLeft=area.left+48.;
    const double actionsWidth=auditionWidth+midiWidth+detailsWidth+2*gap;
    const double actionLeft=right-actionsWidth;
    const double contentRight=out.actionWrapped?right:actionLeft-8.;
    const double textWidth=std::max(0.,contentRight-contentLeft);
    const double factsWidth=std::min(190.,std::max(132.,textWidth*0.36));
    const double factsLeft=contentRight-factsWidth;
    out.name={contentLeft,y+7,factsLeft-8,y+28};
    out.facts={factsLeft,y+7,contentRight,y+28};
    const double locationWidth=std::max(0.,(contentRight-contentLeft)*0.58);
    out.location={contentLeft,y+31,contentLeft+locationWidth,y+49};
    out.match={contentLeft+locationWidth+6, y+31,contentRight,y+49};

    const double actionY=y+(out.actionWrapped?56.:8.);
    out.audition={actionLeft,actionY,actionLeft+auditionWidth,actionY+buttonHeight};
    out.midi={actionLeft+auditionWidth+gap,actionY,
        actionLeft+auditionWidth+gap+midiWidth,actionY+buttonHeight};
    if(out.actionWrapped) {
        out.details={right-detailsWidth,actionY,right,actionY+buttonHeight};
    } else {
        out.details={actionLeft+auditionWidth+gap+midiWidth+gap,actionY,right,actionY+buttonHeight};
    }

    const double timeBandY=detailed?(out.actionWrapped?100.:56.):
        (out.actionWrapped?104.:56.);
    out.timeBand={area.left+inset,y+timeBandY,right,y+timeBandY+34.};
    if(detailed) {
        if(out.actionWrapped) {
            out.sourceLocation={area.left+inset,y+136,right,y+154};
            out.inputTimes={area.left+inset,y+156,right,y+190};
        } else {
            out.sourceLocation={area.left+inset,y+96,right,y+116};
            out.inputTimes={area.left+inset,y+122,right,y+156};
        }
    }
    return out;
}

inline UILayoutResult computeLayout(UILayoutInput input) {
    UILayoutResult o;
    const double zoom=std::clamp(std::isfinite(input.userZoom)?input.userZoom:1.,1.,1.5);
    const double w=std::clamp(std::isfinite(input.width)?input.width:static_cast<double>(UILayoutResult::defaultWidth),
        static_cast<double>(UILayoutResult::minimumWidth),static_cast<double>(UILayoutResult::maximumWidth))/zoom;
    const double h=std::clamp(std::isfinite(input.height)?input.height:static_cast<double>(UILayoutResult::defaultHeight),
        static_cast<double>(UILayoutResult::minimumHeight),static_cast<double>(UILayoutResult::maximumHeight))/zoom;
    o.contentScaleFactor=std::isfinite(input.contentScaleFactor)&&input.contentScaleFactor>0?
        std::clamp(input.contentScaleFactor,0.5,4.0):1.0;
    o.viewport={0,0,w,h};
    o.mode=w<1050?LayoutMode::Compact:w<1600?LayoutMode::Standard:LayoutMode::Wide;
    const double controlsH=o.mode==LayoutMode::Compact?78:56;
    const double topH=controlsH+68;
    o.topBar={0,0,w,topH};
    if(o.mode==LayoutMode::Compact) {
        o.topControls={UiRect{16,5,w*0.43,35},UiRect{w*0.44,5,w-16,35},
            UiRect{16,41,w*0.36,72},UiRect{w*0.37,41,w*0.62,72},
            UiRect{w*0.63,41,w*0.81,72},UiRect{w*0.82,41,w-16,72}};
    } else {
        const double unit=(w-32)/6;
        for(int i=0;i<6;++i)o.topControls[i]={16+i*unit,8,16+(i+1)*unit-4,46};
    }
    o.constraintControls={UiRect{16,controlsH,w*0.42,controlsH+31},
        UiRect{w*0.43,controlsH,w*0.59,controlsH+31},UiRect{w*0.60,controlsH,w*0.76,controlsH+31},
        UiRect{w*0.77,controlsH,w-16,controlsH+31}};
    const double phraseTop=topH+34;
    o.phrase={16,phraseTop,w-16,phraseTop+160};
    o.timeline={24,phraseTop+20,w-24,phraseTop+89};
    const double contentTop=input.tab==session::Tab::Recommend?o.phrase.bottom+16:topH+16;
    const double compareH=input.compareOpen?(o.mode==LayoutMode::Compact&&h>=600?108:76):0;
    o.compare=input.compareOpen?UiRect{16,h-compareH-12,w-16,h-12}:UiRect{};
    const double contentBottom=input.compareOpen?o.compare.top-10:h-16;
    o.content={16,contentTop,w-16,std::max(contentTop+1,contentBottom)};
    o.library=o.content;
    o.libraryRows=std::max(1,static_cast<int>(std::floor((o.content.height()-130)/36)));
    if(input.inspectorOpen) {
        o.inspectorMode=o.mode==LayoutMode::Wide?InspectorMode::Side:InspectorMode::Overlay;
        if(o.inspectorMode==InspectorMode::Side) {
            o.inspector={w-408,contentTop,w-16,contentBottom};
        } else {
            const double panelW=std::min(650.,w-48);
            // A transient inspector may cover the compare strip on short, zoomed
            // viewports so its controls remain readable below the toolbar.
            const double panelBottom=std::min(h-16,std::max(contentBottom-8,topH+208));
            const auto top=std::max(topH+8,std::min(contentTop+8,panelBottom-250));
            o.inspector={(w-panelW)/2,top,(w+panelW)/2,panelBottom};
        }
    }
    const double laneRight=o.inspectorMode==InspectorMode::Side?o.inspector.left-12:o.content.right;
    const double laneWidth=laneRight-o.content.left;
    o.laneColumns=o.mode==LayoutMode::Compact?1:
        o.mode==LayoutMode::Standard?2:(laneWidth>=1670?4:2);
    const double gap=10;
    const double cardW=(laneWidth-gap*(o.laneColumns-1))/o.laneColumns;
    // Keep both default candidate rows readable even in the narrow one-column mode.
    // The content viewport scrolls vertically when the lanes exceed its height.
    o.laneHeight=192;
    for(int i=0;i<4;++i) {
        const int column=i%o.laneColumns,row=i/o.laneColumns;
        const double x=o.content.left+column*(cardW+gap),y=contentTop+row*(o.laneHeight+gap);
        o.lanes[i]={x,y,x+cardW,y+o.laneHeight};
    }
    const int rows=(4+o.laneColumns-1)/o.laneColumns;
    o.laneScrollMax=std::max(0.,rows*o.laneHeight+(rows-1)*gap-o.content.height());
    return o;
}
} // namespace harmony::ui
