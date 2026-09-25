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
    double width{1100}, height{900}, contentScaleFactor{1};
    session::Tab tab{session::Tab::Recommend};
    bool inspectorOpen{}, compareOpen{};
};
struct UILayoutResult {
    static constexpr int minimumWidth=900, minimumHeight=640;
    static constexpr int defaultWidth=1100, defaultHeight=900;
    static constexpr int maximumWidth=2200, maximumHeight=1400;
    LayoutMode mode{LayoutMode::Standard};
    InspectorMode inspectorMode{InspectorMode::None};
    int laneColumns{2}, libraryRows{1};
    double contentScaleFactor{1};
    UiRect viewport,topBar,phrase,timeline,content,inspector,compare,library;
    std::array<UiRect,4> lanes{};
    std::array<UiRect,6> topControls{}; // key, style, intent, view, recommend, library
    double laneHeight{}, laneScrollMax{};
};
struct CandidateRowGeometry {
    UiRect row,timeline,score,intent,pin,audition,midi,snapshot;
};
inline CandidateRowGeometry candidateRowGeometry(UiRect lane,int row,bool exportControls) {
    CandidateRowGeometry out;
    const double rowHeight=(lane.height()-30)/3;
    const double y=lane.top+27+row*rowHeight,right=lane.right-6;
    const double actionWidth=exportControls?148:77,pin=right-actionWidth;
    const bool roomy=rowHeight>44;
    const double actionY=roomy?y+rowHeight-26:y+4;
    const double scoreLeft=pin-37;
    out.row={lane.left+6,y,right,y+rowHeight-2};
    out.timeline={lane.left+11,y+5,roomy?right-8:std::max(lane.left+115,scoreLeft-4),
        y+std::min(25.,rowHeight-5)};
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

inline UILayoutResult computeLayout(UILayoutInput input) {
    UILayoutResult o;
    const double w=std::clamp(std::isfinite(input.width)?input.width:1100.,900.,2200.);
    const double h=std::clamp(std::isfinite(input.height)?input.height:900.,640.,1400.);
    o.contentScaleFactor=std::isfinite(input.contentScaleFactor)&&input.contentScaleFactor>0?
        std::clamp(input.contentScaleFactor,0.5,4.0):1.0;
    o.viewport={0,0,w,h};
    o.mode=w<1050?LayoutMode::Compact:w<1600?LayoutMode::Standard:LayoutMode::Wide;
    const double topH=o.mode==LayoutMode::Compact?78:56;
    o.topBar={0,0,w,topH};
    if(o.mode==LayoutMode::Compact) {
        o.topControls={UiRect{16,5,w*0.43,35},UiRect{w*0.44,5,w-16,35},
            UiRect{16,41,w*0.36,72},UiRect{w*0.37,41,w*0.62,72},
            UiRect{w*0.63,41,w*0.81,72},UiRect{w*0.82,41,w-16,72}};
    } else {
        const double unit=(w-32)/6;
        for(int i=0;i<6;++i)o.topControls[i]={16+i*unit,8,16+(i+1)*unit-4,46};
    }
    const double phraseTop=topH+34;
    o.phrase={16,phraseTop,w-16,phraseTop+160};
    o.timeline={24,phraseTop+20,w-24,phraseTop+89};
    const double contentTop=o.phrase.bottom+16;
    const double compareH=input.compareOpen?(o.mode==LayoutMode::Compact?108:76):0;
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
            o.inspector={(w-panelW)/2,contentTop+8,(w+panelW)/2,contentBottom-8};
        }
    }
    const double laneRight=o.inspectorMode==InspectorMode::Side?o.inspector.left-12:o.content.right;
    const double laneWidth=laneRight-o.content.left;
    o.laneColumns=o.mode==LayoutMode::Compact?1:
        o.mode==LayoutMode::Standard?2:(laneWidth>=1670?4:2);
    const double gap=10;
    const double cardW=(laneWidth-gap*(o.laneColumns-1))/o.laneColumns;
    // Keep all three candidate rows readable even in the narrow one-column mode.
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
