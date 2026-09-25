#include "MainView.h"
#include "WeightBarGeometry.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/clinestyle.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/platform/iplatformframe.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace harmony::ui {
using namespace VSTGUI;
namespace {
const CColor background{16,24,39,255}, panel{24,37,55,255}, pale{232,241,250,255};
const CColor muted{148,173,200,255}, accent{255,218,120,255};
void box(CDrawContext* dc, CRect r, CColor c) { dc->setFillColor(c); dc->drawRect(r,kDrawFilled); }
void line(CDrawContext* dc, std::string text, CRect r, CColor color=pale, CHoriTxtAlign align=kLeftText) {
    dc->setFont(kNormalFont); dc->setFontColor(color); dc->drawString(text.c_str(),r,align);
}
std::string fixed(double value, int precision=1) { std::ostringstream out; out<<std::fixed<<std::setprecision(precision)<<value; return out.str(); }
std::string styleName(Style s) {
    switch(s) { case Style::Pop:return "Pop"; case Style::Rock:return "Rock"; case Style::Rnb:return "R&B";
        case Style::Jazz:return "Jazz"; case Style::CityPop:return "City Pop / J-pop"; default:return "Functional"; }
}
std::string styleFlags(StyleFlags flags) {
    std::string out; for (auto s:{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional})
        if (flags&static_cast<StyleFlags>(s)) { if (!out.empty()) out+=", "; out+=styleName(s); }
    return out.empty()?"—":out;
}
std::string primaryStyle(StyleFlags flags) {
    for (auto s:{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional})
        if (flags&static_cast<StyleFlags>(s)) return s==Style::CityPop?"City Pop":styleName(s);
    return "—";
}
std::string roles(RoleFlags flags) {
    std::string out;
    for (const auto& [role,name]:std::initializer_list<std::pair<Role,const char*>>{
        {Role::Structural,"Structural"},{Role::Embellishing,"Embellishing"},{Role::Passing,"Pass"},
        {Role::Approach,"Approach"},{Role::SecondaryDominant,"V/X"},{Role::SecondaryLeadingTone,"vii°/X"},
        {Role::Borrowed,"Borrowed"},{Role::Substitution,"Substitution"},{Role::Inversion,"Inversion"},
        {Role::Extension,"Extension"},{Role::Cadential,"Cad."}})
        if (hasRole(flags,role)) { if (!out.empty()) out+=", "; out+=name; }
    return out.empty()?"—":out;
}
std::string badge(RoleFlags flags) {
    if (hasRole(flags,Role::SecondaryDominant)) return "V/X";
    if (hasRole(flags,Role::SecondaryLeadingTone)) return "vii°/X";
    if (hasRole(flags,Role::Borrowed)) return "Borrowed";
    if (hasRole(flags,Role::Passing)) return "Pass";
    if (hasRole(flags,Role::Approach)) return "Approach";
    if (hasRole(flags,Role::Cadential)) return "Cad.";
    return {};
}
std::string keyLabel(const session::PluginSessionState& s, const HarmonicAnalysisResult& a) {
    if (s.forcedKey) return "Key: "+formatKey(*s.forcedKey)+"  FORCED";
    if (a.selectedKey) return "Key: Auto · "+formatKey(a.selectedKey->key);
    return "Key: Auto";
}
std::string intentLabel(std::optional<PhraseIntent> intent) { return intent?intentName(*intent):"Auto"; }
bool containsText(std::string hay, std::string needle) {
    std::transform(hay.begin(),hay.end(),hay.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return hay.find(needle)!=std::string::npos;
}
CRect viewRect(UiRect r) { return CRect(r.left,r.top,r.right,r.bottom); }
std::size_t groupForVisual(std::size_t visual,std::optional<PhraseIntent> intent) {
    constexpr std::size_t chosenMap[]{4,1,0,2,3};
    const auto chosen=intent?static_cast<std::size_t>(*intent):0;
    const auto preferred=chosenMap[std::min(chosen,std::size_t{4})];
    return visual==0 && preferred<4?preferred:(preferred<4&&visual<=preferred?visual-1:visual);
}
}
MainView::MainView(const CRect& rect, Actions actions) : CView(rect), actions_(std::move(actions)) {
    layout_=computeLayout({rect.getWidth(),rect.getHeight(),1,session::Tab::Recommend,false,false});
}
void MainView::resizeLayout(int width,int height) {
    setViewSize(CRect(0,0,width,height),true);
    setMouseableArea(CRect(0,0,width,height));
    layout_=computeLayout({static_cast<double>(width),static_cast<double>(height),contentScale_,state_.tab,
        state_.tab==session::Tab::Recommend?(selectedChord_.has_value()||selectedCandidate_.has_value()):
            state_.tab==session::Tab::Library&&selectedLibrary_.has_value(),
        !state_.pinnedCandidateIds.empty()});
    contentScroll_=std::clamp(contentScroll_,0.,layout_.laneScrollMax);
    rebuildTimeline();
    playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt;
    if(nameEdit_) {
        const double w=std::min(590.,layout_.viewport.width()-48),h=300;
        const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
        nameEdit_->setViewSize(CRect(x+140,y+53,x+w-22,y+91));
        if(tagsEdit_)tagsEdit_->setViewSize(CRect(x+140,y+201,x+w-22,y+239));
    }
    invalid();
}
void MainView::setSimulatedContentScale(double scale) {
    contentScale_=std::isfinite(scale)&&scale>0?scale:1;
    resizeLayout(static_cast<int>(getViewSize().getWidth()),static_cast<int>(getViewSize().getHeight()));
}
void MainView::refreshLayout() {
    const auto priorWidth=layout_.timeline.width();
    layout_=computeLayout({getViewSize().getWidth(),getViewSize().getHeight(),contentScale_,state_.tab,
        state_.tab==session::Tab::Recommend?(selectedChord_.has_value()||selectedCandidate_.has_value()):
            state_.tab==session::Tab::Library&&selectedLibrary_.has_value(),
        !state_.pinnedCandidateIds.empty()});
    contentScroll_=std::clamp(contentScroll_,0.,layout_.laneScrollMax);
    if (priorWidth!=layout_.timeline.width()) rebuildTimeline();
}
void MainView::onMouseWheelEvent(MouseWheelEvent& event) {
    const auto delta=event.deltaY*48.0;
    if(layout_.inspectorMode!=InspectorMode::None&&layout_.inspector.contains(event.mousePosition.x,event.mousePosition.y)) {
        inspectorScroll_=std::clamp(inspectorScroll_-delta,0.,inspectorScrollMax_);
        invalid();event.consumed=true;return;
    }
    if (layout_.timeline.contains(event.mousePosition.x,event.mousePosition.y)) {
        timelineScroll_=std::clamp(timelineScroll_-delta,0.,
            std::max(0.,timelineContentWidth_-layout_.timeline.width()));
        playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt;
        invalid(); event.consumed=true; return;
    }
    if (layout_.content.contains(event.mousePosition.x,event.mousePosition.y)) {
        contentScroll_=std::clamp(contentScroll_-delta,0.,layout_.laneScrollMax);
        invalid(); event.consumed=true; return;
    }
    CView::onMouseWheelEvent(event);
}
SharedPointer<IDropTarget> MainView::getDropTarget() { return this; }
DragOperation MainView::onDragEnter(DragEventData e) { return e.drag?DragOperation::Copy:DragOperation::None; }
DragOperation MainView::onDragMove(DragEventData e) { return e.drag?DragOperation::Copy:DragOperation::None; }
void MainView::onDragLeave(DragEventData) {}
bool MainView::onDrop(DragEventData e) { try { if (!e.drag || !actions_.drop) return false; actions_.drop(e.drag); return true; } catch (...) { return false; } }
void MainView::notifyState() { sessionDirty_=true; if (actions_.stateChanged) actions_.stateChanged(state_); invalid(); }
void MainView::setSessionState(const session::PluginSessionState& s) {
    const bool timelineChanged=state_.imported.revision!=s.imported.revision || state_.imported.events.size()!=s.imported.events.size();
    state_=s;
    if (timelineChanged) {
        pinnedSnapshots_.clear(); selectedChord_.reset(); selectedCandidate_.reset(); rebuildTimeline();
        if (!state_.imported.events.empty()) sessionDirty_=true;
    } else std::erase_if(pinnedSnapshots_,[&](const auto& candidate) {
        return std::find(state_.pinnedCandidateIds.begin(),state_.pinnedCandidateIds.end(),candidate.id)==state_.pinnedCandidateIds.end();
    });
    invalid();
}
void MainView::setHostText(std::string s, std::string snapshot) {
    hostText_=std::move(s); if (!snapshot.empty()) { recentHostSnapshots_.push_front(std::move(snapshot)); if (recentHostSnapshots_.size()>8) recentHostSnapshots_.pop_back(); } invalid();
}
void MainView::setProgressionSession(const ImportedProgressionSession& s) {
    if (state_.imported.revision==s.revision && state_.imported.events.size()==s.events.size()) return;
    state_.imported=s; selectedChord_.reset(); selectedCandidate_.reset(); rebuildTimeline();
    currentLocation_=locateCurrentChord(state_.imported,projectQN_); playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt; invalid();
}
void MainView::setAnalysis(const HarmonicAnalysisResult& a) { analysis_=a; invalid(); }
void MainView::setMatches(const std::vector<MatchResult>& m, std::string status) { matches_=m; matchStatus_=std::move(status); invalid(); }
void MainView::setRecommendations(const RecommendationSet& r) {
    const auto oldSelected=selectedCandidate()?selectedCandidate()->id:std::string{};
    recommendations_=r;
    if (!oldSelected.empty() && (!selectedCandidate() || selectedCandidate()->id!=oldSelected)) selectedCandidate_.reset();
    const bool resolved=std::any_of(recommendations_.groups.begin(),recommendations_.groups.end(),
        [](const auto& group){return !group.empty();});
    const auto oldPins=state_.pinnedCandidateIds;
    if (resolved) state_.resolvePins(recommendations_);
    pinnedSnapshots_.clear();
    for (const auto& id:state_.pinnedCandidateIds) {
        const auto found=std::find_if(pinnedSnapshots_.begin(),pinnedSnapshots_.end(),[&](const auto& c){return c.id==id;});
        if (found==pinnedSnapshots_.end()) for (const auto& group:recommendations_.groups)
            for (const auto& candidate:group) if (candidate.id==id) { pinnedSnapshots_.push_back(candidate); break; }
    }
    if (resolved && oldPins!=state_.pinnedCandidateIds && actions_.stateChanged) actions_.stateChanged(state_);
    invalid();
}
void MainView::setPreviewPosition(std::string id,double qn,double totalQN) {
    previewCandidateId_=std::move(id); previewQN_=qn; previewTotalQN_=totalQN; invalid();
}
void MainView::setSnapshotMode(bool enabled) {
    visibility_.absoluteMinimumScore=enabled?0.f:60.f;
    invalid();
}
void MainView::setDropReport(std::string raw,std::string parsed,bool inputAttempt) {
    rawText_=raw.substr(0,12000); parseText_=parsed.substr(0,8000);
    recentReports_.push_front(parseText_); if (recentReports_.size()>8) recentReports_.pop_back(); invalid();
    if (inputAttempt) {
        if (parseText_.find("导入失败")!=std::string::npos ||
            (parseText_.find("解析成功")==std::string::npos && !parseText_.empty())) actionStatus_="Invalid Input · see Debug";
        else actionStatus_.clear();
    }
}
void MainView::setLibrary(std::vector<ProgressionTemplate> f,std::vector<ProgressionTemplate> u,std::string e) {
    factory_=std::move(f); user_=std::move(u); libraryError_=std::move(e); invalid();
}
void MainView::setWorkerStatus(std::uint64_t g,double ms,bool busy,std::size_t fc,std::size_t uc) {
    generation_=g; computationMs_=ms; workerBusy_=busy;
    if (!busy && std::all_of(recommendations_.groups.begin(),recommendations_.groups.end(),
            [](const auto& group){return group.empty();}) && !state_.pinnedCandidateIds.empty()) {
        state_.resolvePins(recommendations_);
        pinnedSnapshots_.clear();
        if (actions_.stateChanged) actions_.stateChanged(state_);
    }
    if (fc) factoryCount_=fc; if (uc || !userCount_) userCount_=uc;
    invalidRect(CRect(255,layout_.phrase.top-31,layout_.viewport.right-160,layout_.phrase.top));
}
void MainView::rebuildTimeline() {
    auto timeline=layoutScrollableTimeline(state_.imported.events,layout_.timeline.width());
    timelineContentWidth_=timeline.contentWidth;
    timelineScroll_=std::clamp(timelineScroll_,0.,std::max(0.,timelineContentWidth_-layout_.timeline.width()));
    timelineBlocks_=std::move(timeline.blocks);
}
CRect MainView::chordTileRect(std::size_t i) const {
    const auto it=std::find_if(timelineBlocks_.begin(),timelineBlocks_.end(),[i](const auto& b){return b.eventIndex==i;});
    if (it==timelineBlocks_.end()) return {};
    const auto left=layout_.timeline.left+it->x-timelineScroll_;
    return CRect(left,layout_.timeline.top+1,left+std::max(1.0,it->width-3.0),layout_.timeline.bottom-1);
}
std::optional<CCoord> MainView::projectQNToX(double qn) const {
    const auto& events=state_.imported.events; if (events.empty() || !std::isfinite(qn) || qn<events.front().startQN) return std::nullopt;
    const auto span=timelineSpanQN(events);
    if (span<=0 || !std::isfinite(span)) return std::nullopt;
    const double x=layout_.timeline.left+(qn-events.front().startQN)/span*timelineContentWidth_-timelineScroll_;
    if (x<layout_.timeline.left || x>layout_.timeline.right) return std::nullopt;
    return static_cast<CCoord>(x);
}
void MainView::setPlaybackPosition(std::optional<double> qn,bool playing) {
    const auto oldLocation=currentLocation_; const auto oldX=playheadX_;
    projectQN_=qn && std::isfinite(*qn)?qn:std::nullopt; playing_=playing;
    currentLocation_=locateCurrentChord(state_.imported,projectQN_); playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt;
    if (oldLocation.activeIndex) invalidRect(chordTileRect(*oldLocation.activeIndex));
    if (currentLocation_.activeIndex) invalidRect(chordTileRect(*currentLocation_.activeIndex));
    if (oldX) invalidRect(CRect(*oldX-3,layout_.timeline.top,*oldX+3,layout_.timeline.bottom));
    if (playheadX_) invalidRect(CRect(*playheadX_-3,layout_.timeline.top,*playheadX_+3,layout_.timeline.bottom));
}
void MainView::drawTimeline(CDrawContext* dc,const CRect& update) {
    const CRect lane=viewRect(layout_.timeline);
    ConcatClip clip(*dc,lane);
    box(dc,lane,CColor(19,32,49,255));
    if (state_.imported.events.empty()) {
        line(dc,"Drag Chords From Cubase   →   HarmonyContinuation",
            CRect(lane.left+6,lane.top+14,lane.right-6,lane.bottom-8),muted,kCenterText); return;
    }
    const CColor colors[]{CColor(50,116,180,255),CColor(40,139,129,255),CColor(172,111,53,255),CColor(108,91,178,255)};
    for (const auto& block:timelineBlocks_) {
        if (state_.skeletonView && !analysis_.full.empty() &&
            std::find(analysis_.skeletonIndices.begin(),analysis_.skeletonIndices.end(),block.eventIndex)==analysis_.skeletonIndices.end()) continue;
        const auto tile=chordTileRect(block.eventIndex);
        if (!(tile.right>update.left && tile.left<update.right && tile.bottom>update.top && tile.top<update.bottom)) continue;
        const bool active=currentLocation_.activeIndex==block.eventIndex;
        box(dc,tile,colors[block.eventIndex%4]);
        dc->setFrameColor(active?accent:CColor(135,172,215,255)); dc->setLineWidth(active?2.4:1.0);
        if (block.openRightEdge) { const CCoord dash[]{4.,3.}; dc->setLineStyle(CLineStyle(CLineStyle::kLineCapButt,CLineStyle::kLineJoinMiter,0,2,dash)); }
        else dc->setLineStyle(CLineStyle{});
        dc->drawRect(tile); dc->setLineWidth(1.0); dc->setLineStyle(CLineStyle{});
        const auto& event=state_.imported.events[block.eventIndex];
        line(dc,event.name,CRect(tile.left+2,tile.top+2,tile.right-2,tile.top+20),pale,kCenterText);
        if (block.eventIndex<analysis_.full.size()) {
            const auto& a=analysis_.full[block.eventIndex];
            line(dc,formatDegree(a)+"  "+formatFunction(a.function),CRect(tile.left+1,tile.top+21,tile.right-1,tile.top+39),pale,kCenterText);
            const auto role=(hasRole(a.roles,Role::SecondaryDominant) || hasRole(a.roles,Role::SecondaryLeadingTone))?
                formatDegree(a):badge(a.roles);
            if (!role.empty()) line(dc,role,CRect(tile.left+1,tile.top+39,tile.right-1,tile.top+57),CColor(255,233,180,255),kCenterText);
            const auto bar=weightBarRect(tile.left,tile.right,tile.bottom,a.structuralWeight,dc->getScaleFactor());
            if (bar.right>bar.left) box(dc,CRect(bar.left,bar.top,bar.right,bar.bottom),accent);
        }
    }
    if (playheadX_) box(dc,CRect(*playheadX_-1.5,lane.top,*playheadX_+1.5,lane.bottom),CColor(255,237,138,255));
}
void MainView::drawTop(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    box(dc,viewRect(layout_.topBar),CColor(25,40,58,255));
    const std::array<std::string,6> labels{
        keyLabel(state_,analysis_),"Style: "+(state_.style?styleName(*state_.style):"Auto"),
        "Intent: "+intentLabel(state_.intent),state_.skeletonView?"View: SKELETON":"View: FULL",
        "RECOMMEND","LIBRARY"};
    for(std::size_t i=0;i<labels.size();++i)
        line(dc,labels[i],viewRect(layout_.topControls[i]),
            i==0&&state_.forcedKey?accent:i==3?accent:
            i==4&&state_.tab==session::Tab::Recommend?accent:
            i==5&&state_.tab==session::Tab::Library?accent:i>=4?muted:pale);
    const auto titleY=layout_.phrase.top-31;
    line(dc,"CURRENT PHRASE",CRect(20,titleY,235,titleY+27),pale);
    const auto status=workerBusy_?"Analyzing…":!actionStatus_.empty()?actionStatus_:
        matchStatus_.find("library")!=std::string::npos?"Library Error · see Library":"";
    if (!status.empty()) line(dc,status,CRect(255,titleY,layout_.viewport.right-170,titleY+27),muted,kRightText);
    line(dc,"MATCH",CRect(layout_.viewport.right-150,titleY,layout_.viewport.right-90,titleY+27),muted,kRightText);
    line(dc,state_.debugExpanded?"DEBUG ON":"DEBUG",CRect(layout_.viewport.right-85,titleY,layout_.viewport.right-20,titleY+27),
        state_.debugExpanded?accent:muted,kRightText);
}
void MainView::drawPhrase(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    drawTimeline(dc,viewRect(layout_.timeline));
    const auto y=layout_.timeline.bottom+5;
    line(dc,"FULL",CRect(24,y,76,y+24),state_.skeletonView?muted:accent);
    line(dc,"SKELETON",CRect(86,y,190,y+24),state_.skeletonView?accent:muted);
    if (timelineContentWidth_>layout_.timeline.width()+1)
        line(dc,"Wheel to scroll phrase",CRect(layout_.timeline.right-215,y,layout_.timeline.right,y+24),muted,kRightText);
    if (state_.imported.events.empty()) return;
    if (selectedChord_ && *selectedChord_<analysis_.full.size()) {
        const auto& event=state_.imported.events[*selectedChord_]; const auto& a=analysis_.full[*selectedChord_];
        const auto duration=event.durationQN?fixed(*event.durationQN)+" QN":"OPEN";
        const auto target=a.target?std::to_string(a.target->degree):"—";
        line(dc,event.name+"    Degree "+formatDegree(a)+"    Function "+formatFunction(a.function)+
                "    Weight "+fixed(a.structuralWeight,2)+"    Confidence "+fixed(a.analysisConfidence,2),
             CRect(23,y+27,layout_.viewport.right-23,y+49),pale);
        line(dc,"Roles "+roles(a.roles)+"    Target "+target+"    Duration "+duration+
                "    Start "+fixed(event.startQN)+" QN",CRect(23,y+50,layout_.viewport.right-23,y+72),muted);
    } else line(dc,"Click a chord for details",CRect(23,y+28,layout_.viewport.right-23,y+55),muted);
}
void MainView::drawMiniTimeline(CDrawContext* dc,const ContinuationCandidate& c,CRect area) {
    const auto& events=state_.imported.events;
    const std::size_t start=0;
    double total{};
    for (std::size_t i=start;i<events.size();++i)
        total+=(i+1==events.size()?c.suggestedCurrentChordDurationQN.value_or(4.0):events[i].durationQN.value_or(4.0));
    for (const auto& e:c.continuation) total+=e.durationQN;
    if (total<=0) return;
    ConcatClip clip(*dc,CRect(area.left,area.top-3,area.right,area.bottom+3));
    double cursor=area.left;
    const auto drawBlock=[&](std::string label,double duration,CColor color,bool final) {
        const double width=final?area.right-cursor:area.getWidth()*std::max(0.0,duration)/total;
        if (width<1) {cursor+=std::max(0.0,width);return;}
        if (width>2)box(dc,CRect(cursor,area.top,cursor+width-2,area.bottom),color);
        if (width>29) line(dc,label,CRect(cursor+1,area.top+1,cursor+width-3,area.bottom-1),pale,kCenterText);
        cursor+=width;
    };
    for (std::size_t i=start;i<events.size();++i) {
        const auto duration=i+1==events.size()?c.suggestedCurrentChordDurationQN.value_or(4.0):events[i].durationQN.value_or(4.0);
        drawBlock(events[i].name,duration,CColor(69,81,99,255),false);
    }
    box(dc,CRect(cursor-1,area.top-2,cursor+1,area.bottom+2),accent);
    for (std::size_t i=0;i<c.continuation.size();++i)
        drawBlock(c.continuation[i].label,c.continuation[i].durationQN,CColor(50,116,180,255),i+1==c.continuation.size());
    if (previewCandidateId_==c.id && previewTotalQN_>0) {
        const auto x=area.left+area.getWidth()*std::clamp(previewQN_/previewTotalQN_,0.0,1.0);
        box(dc,CRect(x-1,area.top-2,x+1,area.bottom+2),accent);
    }
}
void MainView::drawRecommendations(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    if (state_.imported.events.empty()) {
        line(dc,"Drag Chords From Cubase",viewRect(layout_.content),pale,kCenterText); return;
    }
    constexpr const char* titles[]{"RESOLVE","DEVELOP","LOOP","COLOR"};
    {
    ConcatClip clip(*dc,viewRect(layout_.content));
    for (std::size_t visual=0;visual<4;++visual) {
        const auto group=groupForVisual(visual,state_.intent);
        auto lane=layout_.lanes[visual]; lane.top-=contentScroll_; lane.bottom-=contentScroll_;
        if(lane.bottom<layout_.content.top||lane.top>layout_.content.bottom)continue;
        box(dc,viewRect(lane),panel);
        line(dc,titles[group],CRect(lane.left+8,lane.top+3,lane.left+160,lane.top+27),accent);
        if (recommendations_.groups[group].size()>3)
            line(dc,"Show More",CRect(lane.right-112,lane.top+3,lane.right-8,lane.top+27),muted,kRightText);
        const auto visible=visibility_.visibleIndices(recommendations_.groups[group]);
        if (visible.empty()) {
            line(dc,workerBusy_?"Analyzing…":"No strong option",CRect(lane.left+8,lane.top+36,lane.right-8,lane.top+64),muted);
            continue;
        }
        for (std::size_t row=0;row<visible.size()&&row<3;++row) {
            const auto& c=recommendations_.groups[group][visible[row]];
            const bool exportControls=static_cast<bool>(actions_.exportMidi);
            const auto geometry=candidateRowGeometry(lane,static_cast<int>(row),exportControls);
            box(dc,viewRect(geometry.row),CColor(33,49,69,255));
            drawMiniTimeline(dc,c,viewRect(geometry.timeline));
            line(dc,std::to_string(static_cast<int>(std::round(c.rankingScore))),
                 viewRect(geometry.score),pale,kCenterText);
            if(geometry.row.height()>44)line(dc,std::string(intentName(c.intent))+" · "+cadenceName(c.cadence),
                viewRect(geometry.intent),muted);
            line(dc,"Pin",viewRect(geometry.pin),accent,kCenterText);
            if(exportControls) {
                line(dc,previewCandidateId_==c.id?"■":"▶",viewRect(geometry.audition),accent,kCenterText);
                line(dc,"MIDI",viewRect(geometry.midi),accent,kCenterText);
                line(dc,"SNAP",viewRect(geometry.snapshot),muted,kCenterText);
            } else if(actions_.audition)
                line(dc,previewCandidateId_==c.id?"■":"▶",viewRect(geometry.audition),accent,kCenterText);
        }
    }
    }
    drawCompare(dc,bounds);
}
const ContinuationCandidate* MainView::findCandidate(const std::string& id) const {
    for (const auto& candidate:pinnedSnapshots_) if (candidate.id==id) return &candidate;
    for (const auto& group:recommendations_.groups) for (const auto& candidate:group)
        if (candidate.id==id) return &candidate;
    return nullptr;
}
const ContinuationCandidate* MainView::selectedCandidate() const {
    if (!selectedCandidate_) return nullptr;
    const auto [group,index]=*selectedCandidate_;
    return group<4 && index<recommendations_.groups[group].size()?&recommendations_.groups[group][index]:nullptr;
}
void MainView::drawCompare(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    if (state_.pinnedCandidateIds.empty()) return;
    const auto tray=layout_.compare;
    box(dc,viewRect(tray),CColor(27,41,58,255));
    line(dc,"COMPARE  ·  "+std::to_string(state_.pinnedCandidateIds.size())+" / 3",
         CRect(tray.left+8,tray.top+3,tray.right-8,tray.top+25),accent);
    std::size_t shown{};
    for (const auto& id:state_.pinnedCandidateIds) {
        const auto* c=findCandidate(id);
        const bool stacked=layout_.mode==LayoutMode::Compact;
        const auto width=stacked?tray.width()-16:(tray.width()-16)/3;
        const auto left=stacked?tray.left+8:tray.left+8+shown*width;
        const auto y=stacked?tray.top+27+shown*26:tray.top+27;
        if (c) {
            std::string path;
            for (const auto& event:c->continuation) { if (!path.empty()) path+=" → "; path+=event.label; }
            line(dc,std::string(intentName(c->intent))+"   "+std::to_string(static_cast<int>(std::round(c->rankingScore)))+
                    "  "+path,CRect(left,y,left+width-60,y+22),pale);
        } else line(dc,"Pinned path unavailable",CRect(left,y,left+width-60,y+22),muted);
        line(dc,"×",CRect(left+width-42,y,left+width-8,y+22),muted,kCenterText);
        ++shown;
    }
}
std::vector<std::pair<bool,std::size_t>> MainView::filteredLibrary() const {
    std::vector<std::pair<bool,std::size_t>> out;
    const auto append=[&](const auto& entries,bool factory) {
        for (std::size_t i=0;i<entries.size();++i) {
            const auto& item=entries[i];
            if (libraryStyle_ && !(item.styles&static_cast<StyleFlags>(*libraryStyle_))) continue;
            if (libraryIntent_ && item.intent!=*libraryIntent_) continue;
            if (libraryMode_ && item.mode!=*libraryMode_) continue;
            if (!librarySearch_.empty()) {
                bool hit=containsText(item.name,librarySearch_) || containsText(item.id,librarySearch_);
                for (const auto& tag:item.tags) hit=hit || containsText(tag,librarySearch_);
                if (!hit) continue;
            }
            out.emplace_back(factory,i);
        }
    };
    append(factory_,true); append(user_,false); return out;
}
void MainView::drawRect(CDrawContext* dc,const CRect& update) {
    refreshLayout();
    ++paintGeneration_;
    const auto bounds=getViewSize();
    if (update.top>=layout_.timeline.top && update.bottom<=layout_.timeline.bottom) { drawTimeline(dc,update); return; }
    box(dc,bounds,background);
    drawTop(dc,bounds); drawPhrase(dc,bounds);
    switch(state_.tab) {
        case session::Tab::Recommend: drawRecommendations(dc,bounds); break;
        case session::Tab::Library: drawResponsiveLibrary(dc); break;
        default: drawResponsiveDiagnostics(dc); break;
    }
    drawResponsiveInspector(dc); drawResponsiveForm(dc);
    if(state_.debugExpanded) {
        const auto x=bounds.right-310,y=layout_.phrase.bottom-47;
        box(dc,CRect(x,y,bounds.right-18,y+43),CColor(34,49,68,255));
        const auto mode=layout_.mode==LayoutMode::Compact?"Compact":layout_.mode==LayoutMode::Standard?"Standard":"Wide";
        line(dc,std::to_string(static_cast<int>(bounds.getWidth()))+" × "+std::to_string(static_cast<int>(bounds.getHeight()))+
            "  DPI "+fixed(contentScale_,2)+"  "+mode+" / "+std::to_string(layout_.laneColumns),
            CRect(x+8,y+3,bounds.right-22,y+21),pale);
        line(dc,"Paint "+std::to_string(paintGeneration_)+" · worker "+(workerBusy_?"busy":"idle"),
            CRect(x+8,y+21,bounds.right-22,y+39),muted);
    }
}
int MainView::popup(const std::vector<std::string>& items,CPoint point) {
#if defined(_WIN32)
    const auto* platform=getFrame()?getFrame()->getPlatformFrame():nullptr;
    auto window=platform?static_cast<HWND>(platform->getPlatformRepresentation()):nullptr;
    if (!window) return -1;
    auto menu=CreatePopupMenu(); if (!menu) return -1;
    for (std::size_t i=0;i<items.size();++i) {
        const auto len=MultiByteToWideChar(CP_UTF8,0,items[i].c_str(),-1,nullptr,0);
        std::wstring wide(static_cast<std::size_t>(len),L'\0');
        MultiByteToWideChar(CP_UTF8,0,items[i].c_str(),-1,wide.data(),len);
        AppendMenuW(menu,MF_STRING,static_cast<UINT_PTR>(i+1),wide.c_str());
    }
    POINT at{static_cast<LONG>(point.x),static_cast<LONG>(point.y)}; ClientToScreen(window,&at);
    const auto result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,at.x,at.y,0,window,nullptr);
    DestroyMenu(menu); return result?static_cast<int>(result-1):-1;
#else
    (void)items; (void)point; return -1;
#endif
}
void MainView::beginForm(Form kind,std::string initial) {
    if (!getFrame()) return; endForm(); form_=kind;
    const double w=std::min(590.,layout_.viewport.width()-48),h=300;
    const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
    nameEdit_=new CTextEdit(CRect(x+140,y+53,x+w-22,y+91),nullptr,0,initial.c_str());
    nameEdit_->setBackColor(CColor(242,247,252,255)); nameEdit_->setFontColor(CColor(20,30,45,255));
    getFrame()->addView(nameEdit_);
    if (kind!=Form::Search) {
        tagsEdit_=new CTextEdit(CRect(x+140,y+201,x+w-22,y+239),nullptr,0,"");
        tagsEdit_->setBackColor(CColor(242,247,252,255)); tagsEdit_->setFontColor(CColor(20,30,45,255));
        getFrame()->addView(tagsEdit_);
    }
    getFrame()->setFocusView(nameEdit_); invalid();
}
void MainView::endForm() {
    if (getFrame()) { if (nameEdit_) getFrame()->removeView(nameEdit_); if (tagsEdit_) getFrame()->removeView(tagsEdit_); }
    nameEdit_=nullptr; tagsEdit_=nullptr; form_=Form::None; invalid();
}
void MainView::submitForm() {
    if (!nameEdit_) return;
    const auto text=nameEdit_->getText().getString();
    if (form_==Form::Search) { librarySearch_=text; libraryPage_=0; endForm(); return; }
    if (text.empty() || text.size()>128) { actionStatus_="Name must be 1–128 characters"; invalid(); return; }
    formMetadata_.name=text; formMetadata_.tags.clear();
    if (tagsEdit_) {
        std::istringstream in(tagsEdit_->getText().getString()); std::string tag;
        while (std::getline(in,tag,',')) { if (!tag.empty()) formMetadata_.tags.push_back(tag); }
    }
    if (form_==Form::Save) {
        const auto* c=selectedCandidate(); if (c && actions_.save) actionStatus_=actions_.save(*c,formMetadata_);
    } else if (form_==Form::Rename && selectedLibrary_ && !selectedLibrary_->first && selectedLibrary_->second<user_.size()) {
        auto item=user_[selectedLibrary_->second]; item.name=formMetadata_.name; item.intent=formMetadata_.intent;
        item.tags=formMetadata_.tags; item.styles=formMetadata_.style?static_cast<StyleFlags>(*formMetadata_.style):0;
        item.styleWeights.clear(); if (formMetadata_.style) item.styleWeights.emplace_back(*formMetadata_.style,1.f);
        if (actions_.updateUser) actionStatus_=actions_.updateUser(item);
        selectedLibrary_.reset();
    }
    endForm();
}
CMouseEventResult MainView::onMouseDown(CPoint& where,const CButtonState&) {
    return onMouseDownResponsive(where);
}
CMouseEventResult MainView::onMouseDownResponsive(CPoint& where) {
    try {
        refreshLayout();
        if(form_!=Form::None) {
            const double w=std::min(590.,layout_.viewport.width()-48),h=300;
            const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
            if(where.y>=y+h-45&&where.y<=y+h) {
                if(where.x>=x+140&&where.x<x+290)endForm();
                else if(where.x>=x+w-170&&where.x<=x+w)submitForm();
            } else if(form_!=Form::Search&&where.y>=y+110&&where.y<y+144) {
                const auto choice=popup({"Auto","Pop","Rock","R&B","Jazz","City Pop / J-pop","Functional"},where);
                constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                if(choice>=0)formMetadata_.style=choice==0?std::nullopt:std::optional<Style>(styles[choice-1]);invalid();
            } else if(form_!=Form::Search&&where.y>=y+144&&where.y<y+180) {
                const auto choice=popup({"Develop","Resolve","Loop","Color"},where);
                if(choice>=0)formMetadata_.intent=static_cast<PhraseIntent>(choice+1);invalid();
            }
            return kMouseEventHandled;
        }
        for(std::size_t i=0;i<layout_.topControls.size();++i)if(layout_.topControls[i].contains(where.x,where.y)) {
            if(i==0) {
                std::vector<std::string> names{"Auto"};
                for(int j=0;j<12;++j){names.push_back(formatKey({static_cast<PitchClass>(j),Mode::Major}));
                    names.push_back(formatKey({static_cast<PitchClass>(j),Mode::Minor}));}
                const auto choice=popup(names,where);
                if(choice>=0){state_.forcedKey=choice?std::optional<KeySignature>(KeySignature{
                    static_cast<PitchClass>((choice-1)/2),(choice-1)%2?Mode::Minor:Mode::Major}):std::nullopt;notifyState();}
            } else if(i==1) {
                const auto choice=popup({"Auto","Pop","Rock","R&B","Jazz","City Pop / J-pop","Functional"},where);
                constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                if(choice>=0){state_.style=choice?std::optional<Style>(styles[choice-1]):std::nullopt;notifyState();}
            } else if(i==2) {
                const auto choice=popup({"Auto","Develop","Resolve","Loop","Color"},where);
                if(choice>=0){state_.intent=choice?std::optional<PhraseIntent>(static_cast<PhraseIntent>(choice)):std::nullopt;notifyState();}
            } else if(i==3){state_.skeletonView=!state_.skeletonView;notifyState();}
            else if(i==4){state_.tab=session::Tab::Recommend;selectedLibrary_.reset();notifyState();}
            else {state_.tab=session::Tab::Library;selectedCandidate_.reset();notifyState();}
            return kMouseEventHandled;
        }
        const auto titleY=layout_.phrase.top-31;
        if(where.y>=titleY&&where.y<titleY+30&&where.x>layout_.viewport.right-160) {
            if(where.x<layout_.viewport.right-88)state_.tab=session::Tab::Diagnostics;
            else state_.debugExpanded=!state_.debugExpanded;
            notifyState();return kMouseEventHandled;
        }
        if(layout_.timeline.contains(where.x,where.y)) {
            for(const auto& b:timelineBlocks_) {
                const auto r=chordTileRect(b.eventIndex);
                if(where.x>=r.left&&where.x<r.right){selectedChord_=b.eventIndex;selectedCandidate_.reset();selectedLibrary_.reset();inspectorScroll_=0;invalid();break;}
            }
            return kMouseEventHandled;
        }
        if(where.y>=layout_.timeline.bottom&&where.y<layout_.timeline.bottom+30&&where.x<195) {
            state_.skeletonView=where.x>=82;notifyState();return kMouseEventHandled;
        }
        if(layout_.inspectorMode!=InspectorMode::None&&layout_.inspector.contains(where.x,where.y)) {
            const auto area=layout_.inspector;
            if(where.y<area.top+39) {selectedCandidate_.reset();selectedChord_.reset();selectedLibrary_.reset();invalid();return kMouseEventHandled;}
            if(where.y>=area.bottom-43) {
                if(state_.tab==session::Tab::Recommend&&selectedCandidate()) {
                    const auto* c=selectedCandidate();formMetadata_.name="My "+std::string(intentName(c->intent))+" Progression";
                    formMetadata_.style=state_.style;formMetadata_.intent=c->intent;beginForm(Form::Save,formMetadata_.name);
                } else if(state_.tab==session::Tab::Library&&selectedLibrary_&&actions_.exportLibraryMidi) {
                    const auto [factory,index]=*selectedLibrary_;
                    const auto* item=factory?(index<factory_.size()?&factory_[index]:nullptr):(index<user_.size()?&user_[index]:nullptr);
                    if(item){actionStatus_=actions_.exportLibraryMidi(*item);invalid();}
                }
                return kMouseEventHandled;
            }
            if(state_.tab==session::Tab::Library&&selectedLibrary_&&!selectedLibrary_->first&&where.y>=area.bottom-75) {
                const auto item=user_[selectedLibrary_->second];
                if(where.x<area.left+area.width()/2) {
                    formMetadata_.name=item.name;formMetadata_.intent=item.intent;
                    formMetadata_.style.reset();
                    for(auto style:{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional})
                        if(item.styles&static_cast<StyleFlags>(style)){formMetadata_.style=style;break;}
                    beginForm(Form::Rename,item.name);
                    if(tagsEdit_){std::string tags;for(const auto& tag:item.tags){if(!tags.empty())tags+=", ";tags+=tag;}
                        tagsEdit_->setText(tags.c_str());}
                } else if(actions_.deleteUser) {
                    const auto choice=popup({"Cancel","Delete "+item.name},where);
                    if(choice==1){actionStatus_=actions_.deleteUser(item.id);selectedLibrary_.reset();invalid();}
                }
            }
            return kMouseEventHandled;
        }
        if(!layout_.content.contains(where.x,where.y)) {
            if(layout_.compare.contains(where.x,where.y)) {
                const bool stacked=layout_.mode==LayoutMode::Compact;
                const double cell=stacked?layout_.compare.width()-16:(layout_.compare.width()-16)/3;
                const auto index=stacked?static_cast<std::size_t>(std::max(0.,std::floor((where.y-layout_.compare.top-27)/26))):
                    static_cast<std::size_t>(std::max(0.,std::floor((where.x-layout_.compare.left-8)/cell)));
                if(index<state_.pinnedCandidateIds.size()) {
                    const double left=stacked?layout_.compare.left+8:layout_.compare.left+8+index*cell;
                    if(where.x>=left+cell-50){const auto id=state_.pinnedCandidateIds[index];state_.unpin(id);
                        std::erase_if(pinnedSnapshots_,[&](const auto& c){return c.id==id;});notifyState();}
                }
            }
            return kMouseEventHandled;
        }
        if(state_.tab==session::Tab::Diagnostics||state_.tab==session::Tab::Match) {
            if(where.y<layout_.content.top+38) {
                if(where.x<layout_.content.left+280&&actions_.refresh)actions_.refresh();
                else if(actions_.clipboard)actions_.clipboard();
            }
            return kMouseEventHandled;
        }
        if(state_.tab==session::Tab::Library) {
            const auto area=layout_.content;
            const double right=layout_.inspectorMode==InspectorMode::Side?layout_.inspector.left-10:area.right;
            if(where.x>=right)return kMouseEventHandled;
            if(where.y>=area.top+36&&where.y<area.top+67) {
                const double unit=(right-area.left-20)/4;
                const auto which=std::clamp(static_cast<int>((where.x-area.left-10)/unit),0,3);
                if(which==0){const auto n=popup({"All","Pop","Rock","R&B","Jazz","City Pop / J-pop","Functional"},where);
                    constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                    if(n>=0)libraryStyle_=n?std::optional<Style>(styles[n-1]):std::nullopt;}
                else if(which==1){const auto n=popup({"Auto","Develop","Resolve","Loop","Color"},where);
                    if(n>=0)libraryIntent_=n?std::optional<PhraseIntent>(static_cast<PhraseIntent>(n)):std::nullopt;}
                else if(which==2){const auto n=popup({"All","Major","Minor"},where);
                    if(n>=0)libraryMode_=n?std::optional<Mode>(n==1?Mode::Major:Mode::Minor):std::nullopt;}
                else beginForm(Form::Search,librarySearch_);
                libraryPage_=0;invalid();return kMouseEventHandled;
            }
            if(where.y>=area.bottom-36){const auto filtered=filteredLibrary();
                if(where.x<area.left+180&&libraryPage_>0)--libraryPage_;
                else if(where.x>right-180&&(libraryPage_+1)*static_cast<std::size_t>(layout_.libraryRows)<filtered.size())++libraryPage_;
                invalid();return kMouseEventHandled;}
            if(where.y>=area.top+88&&where.y<area.bottom-37) {
                const auto row=static_cast<std::size_t>((where.y-area.top-88)/36);
                const auto filtered=filteredLibrary();
                const auto index=libraryPage_*static_cast<std::size_t>(layout_.libraryRows)+row;
                if(index<filtered.size()){selectedLibrary_=filtered[index];selectedCandidate_.reset();selectedChord_.reset();inspectorScroll_=0;invalid();}
            }
            return kMouseEventHandled;
        }
        for(std::size_t visual=0;visual<4;++visual) {
            auto lane=layout_.lanes[visual];lane.top-=contentScroll_;lane.bottom-=contentScroll_;
            if(!lane.contains(where.x,where.y))continue;
            const auto group=groupForVisual(visual,state_.intent);
            if(where.y<lane.top+27) {
                if(where.x>=lane.right-120&&recommendations_.groups[group].size()>3) {
                    std::vector<std::string> options;
                    for(const auto& c:recommendations_.groups[group]) {
                        std::string path;for(const auto& e:c.continuation){if(!path.empty())path+=" → ";path+=e.label;}
                        options.push_back(std::to_string(static_cast<int>(std::round(c.rankingScore)))+" · "+path);
                    }
                    const auto selected=popup(options,where);
                    if(selected>=0){selectedCandidate_={{group,static_cast<std::size_t>(selected)}};selectedChord_.reset();inspectorScroll_=0;invalid();}
                }
                return kMouseEventHandled;
            }
            const auto rowHeight=(lane.height()-30)/3;
            const auto row=static_cast<std::size_t>((where.y-lane.top-27)/rowHeight);
            const auto visible=visibility_.visibleIndices(recommendations_.groups[group]);
            if(row>=visible.size()||row>=3)return kMouseEventHandled;
            const auto index=visible[row];const auto& c=recommendations_.groups[group][index];
            const auto geometry=candidateRowGeometry(lane,static_cast<int>(row),static_cast<bool>(actions_.exportMidi));
            if(geometry.pin.contains(where.x,where.y)){
                if(state_.unpin(c.id))std::erase_if(pinnedSnapshots_,[&](const auto& item){return item.id==c.id;});
                else if(state_.pin(c.id,session::continuationFingerprint(c)))pinnedSnapshots_.push_back(c);
                notifyState();
            } else if(geometry.audition.contains(where.x,where.y)&&actions_.audition)actions_.audition(c);
            else if(geometry.midi.contains(where.x,where.y)&&actions_.exportMidi){actionStatus_=actions_.exportMidi(c);invalid();}
            else if(geometry.snapshot.contains(where.x,where.y)&&actions_.saveSnapshot){actionStatus_=actions_.saveSnapshot(c);invalid();}
            else {selectedCandidate_={{group,index}};selectedChord_.reset();inspectorScroll_=0;invalid();}
            return kMouseEventHandled;
        }
    } catch(...) {actionStatus_="Action failed";invalid();}
    return kMouseEventHandled;
}
void MainView::drawResponsiveLibrary(CDrawContext* dc) {
    const auto area=layout_.content;
    const double right=layout_.inspectorMode==InspectorMode::Side?layout_.inspector.left-10:area.right;
    box(dc,viewRect(area),panel);
    line(dc,"LIBRARY   Factory "+std::to_string(factory_.size())+" · User "+std::to_string(user_.size()),
        CRect(area.left+10,area.top+6,right-10,area.top+32),accent);
    const double filterY=area.top+36;
    const double unit=(right-area.left-20)/4;
    line(dc,"Style: "+(libraryStyle_?styleName(*libraryStyle_):"All"),CRect(area.left+10,filterY,area.left+10+unit,filterY+28),muted);
    line(dc,"Intent: "+intentLabel(libraryIntent_),CRect(area.left+10+unit,filterY,area.left+10+unit*2,filterY+28),muted);
    line(dc,std::string("Mode: ")+(libraryMode_?(*libraryMode_==Mode::Major?"Major":"Minor"):"All"),
        CRect(area.left+10+unit*2,filterY,area.left+10+unit*3,filterY+28),muted);
    line(dc,"Search: "+(librarySearch_.empty()?"Name / tag":librarySearch_),
        CRect(area.left+10+unit*3,filterY,right-10,filterY+28),muted);
    if(!libraryError_.empty())line(dc,"Library Error: "+libraryError_,CRect(area.left+10,area.top+67,right-10,area.top+89),
        CColor(255,173,173,255));
    const auto filtered=filteredLibrary();
    const auto rows=static_cast<std::size_t>(layout_.libraryRows),offset=libraryPage_*rows;
    const double listTop=area.top+88;
    {
    ConcatClip clip(*dc,CRect(area.left,listTop,right,area.bottom-37));
    for(std::size_t row=0;row<rows&&offset+row<filtered.size();++row) {
        const auto [factory,index]=filtered[offset+row];const auto& item=factory?factory_[index]:user_[index];
        const double y=listTop+row*36;
        box(dc,CRect(area.left+8,y,right-8,y+33),CColor(32,49,69,255));
        const double nameEnd=right-(layout_.mode==LayoutMode::Compact?220:435);
        line(dc,item.name,CRect(area.left+16,y+3,nameEnd,y+29),pale);
        line(dc,primaryStyle(item.styles),CRect(nameEnd+8,y+3,nameEnd+115,y+29),muted);
        line(dc,intentName(item.intent),CRect(nameEnd+120,y+3,nameEnd+220,y+29),muted);
        if(layout_.mode!=LayoutMode::Compact) {
            line(dc,item.mode==Mode::Major?"Major":"Minor",CRect(nameEnd+225,y+3,nameEnd+315,y+29),muted);
            line(dc,factory?"Factory":"User",CRect(nameEnd+320,y+3,right-16,y+29),accent);
        }
    }
    }
    line(dc,"◀ Previous",CRect(area.left+10,area.bottom-32,area.left+175,area.bottom-4),muted);
    line(dc,"Page "+std::to_string(libraryPage_+1)+" / "+
        std::to_string(std::max<std::size_t>(1,(filtered.size()+rows-1)/rows)),
        CRect(area.left+185,area.bottom-32,right-185,area.bottom-4),muted,kCenterText);
    line(dc,"Next ▶",CRect(right-165,area.bottom-32,right-10,area.bottom-4),muted,kRightText);
}

void MainView::drawResponsiveDiagnostics(CDrawContext* dc) {
    const auto area=layout_.content;
    box(dc,viewRect(area),panel);
    line(dc,"MATCH / DEBUG    [Refresh host]    [Clipboard]",CRect(area.left+10,area.top+7,area.right-10,area.top+34),accent);
    line(dc,matchStatus_,CRect(area.left+10,area.top+39,area.right-10,area.top+65),pale);
    line(dc,"Generation "+std::to_string(generation_)+"    Worker "+(workerBusy_?"busy":"idle")+
        "    Last "+fixed(computationMs_,2)+" ms",CRect(area.left+10,area.top+70,area.right-10,area.top+96),muted);
    if(!matches_.empty())line(dc,"Top match: "+matches_.front().templateName+" · "+fixed(matches_.front().similarity,2),
        CRect(area.left+10,area.top+102,area.right-10,area.top+130),pale);
    std::istringstream in(hostText_+"\n"+parseText_+"\n"+rawText_);std::string row;double y=area.top+140;
    ConcatClip clip(*dc,viewRect(area));
    while(y<area.bottom-20&&std::getline(in,row)) {line(dc,row.substr(0,165),CRect(area.left+10,y,area.right-10,y+18),muted);y+=18;}
}

void MainView::drawResponsiveInspector(CDrawContext* dc) {
    if(layout_.inspectorMode==InspectorMode::None)return;
    const auto area=layout_.inspector;
    box(dc,viewRect(area),CColor(18,30,47,255));
    line(dc,"×   INSPECTOR",CRect(area.left+12,area.top+8,area.right-12,area.top+34),accent);
    double y=area.top+40-inspectorScroll_;
    {
    ConcatClip clip(*dc,CRect(area.left,area.top+38,area.right,area.bottom-45));
    const auto add=[&](std::string text,CColor color=pale) {
        line(dc,std::move(text),CRect(area.left+14,y,area.right-14,y+23),color);y+=27;
    };
    if(state_.tab==session::Tab::Recommend&&selectedCandidate()) {
        const auto& c=*selectedCandidate();
        add(std::string(intentName(c.intent))+" · "+formatKey(c.key)+" · score "+
            std::to_string(static_cast<int>(std::round(c.rankingScore))));
        add("Source  "+c.primaryTemplate,muted);
        add("Match  "+fixed(c.matchSimilarity,2),muted);
        add("Cadence  "+std::string(cadenceName(c.cadence)),muted);
        add("Style  "+styleFlags(c.styles),muted);
        add("Rhythm  "+fixed(c.subscores.rhythm,2)+"   Style  "+fixed(c.subscores.style,2),muted);
        add("Intent  "+fixed(c.subscores.intent,2)+"   Prior  "+fixed(c.subscores.prior,2),muted);
        add("Support  "+std::to_string(c.supportCount),muted);
        add("OPEN  "+(c.suggestedCurrentChordDurationQN?fixed(*c.suggestedCurrentChordDurationQN):"fallback")+" QN",muted);
        const auto path=session::continuationFingerprint(c);
        add("Path  "+path,muted);
        drawMiniTimeline(dc,c,CRect(area.left+14,y+2,area.right-14,y+27));y+=38;
        const auto match=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return m.templateId==c.primaryTemplate;});
        if(match!=matches_.end())add("Skeleton "+fixed(match->subScores.skeletonHarmony,2)+
            "  FULL "+fixed(match->subScores.fullHarmony,2),muted);
        std::string sources;for(const auto& id:c.supportingTemplates){if(!sources.empty())sources+=", ";sources+=id;}
        add("Sources  "+sources,muted);
        if(match!=matches_.end())for(const auto& step:match->alignmentTrace) {
            const auto user=step.queryIndex&&*step.queryIndex<state_.imported.events.size()?state_.imported.events[*step.queryIndex].name:"gap";
            const auto templ=step.templateIndex&&*step.templateIndex<match->templateLabels.size()?match->templateLabels[*step.templateIndex]:"gap";
            add(user+"  ↔  "+templ,muted);
        }
    } else if(state_.tab==session::Tab::Library&&selectedLibrary_) {
        const auto [factory,index]=*selectedLibrary_;
        const auto* item=factory?(index<factory_.size()?&factory_[index]:nullptr):(index<user_.size()?&user_[index]:nullptr);
        if(!item)return;
        add(item->name);
        add("ID  "+item->id,muted);
        add("Style  "+styleFlags(item->styles),muted);
        add("Intent  "+std::string(intentName(item->intent)),muted);
        add(std::string("Mode  ")+(item->mode==Mode::Major?"Major":"Minor"),muted);
        std::string path;for(const auto& e:item->full){if(!path.empty())path+=" → ";path+=formatMatchEvent(e);}
        add("FULL  "+path,muted);
        std::string skeleton;for(const auto skeletonIndex:item->skeletonIndices)if(skeletonIndex<item->full.size()){
            if(!skeleton.empty())skeleton+=" → ";skeleton+=formatMatchEvent(item->full[skeletonIndex]);}
        add("SKELETON  "+skeleton,muted);
        std::string rhythm;for(const auto& e:item->full){if(!rhythm.empty())rhythm+=" · ";rhythm+=e.durationQN?fixed(*e.durationQN):"?";}
        add("Rhythm QN  "+rhythm,muted);
        add("Cadence  "+std::string(cadenceName(item->cadence)),muted);
        add("Meter  "+std::to_string(item->meterNumerator)+"/"+std::to_string(item->meterDenominator),muted);
        add("Prior  "+fixed(item->priorWeight,2),muted);
        std::string tags;for(const auto& t:item->tags){if(!tags.empty())tags+=", ";tags+=t;}add("Tags  "+tags,muted);
        std::size_t nearCount{};for(const auto& other:factory_)if(other.id!=item->id&&
            other.fingerprint.skeletonDegrees==item->fingerprint.skeletonDegrees&&
            other.fingerprint.rhythmShape==item->fingerprint.rhythmShape)++nearCount;
        add("Near duplicate  "+std::to_string(nearCount),muted);
    } else if(selectedChord_&&*selectedChord_<analysis_.full.size()) {
        const auto& c=state_.imported.events[*selectedChord_];const auto& a=analysis_.full[*selectedChord_];
        add(c.name+" · "+formatDegree(a));
        add("Function  "+formatFunction(a.function));
        add("Weight  "+fixed(a.structuralWeight,2));
        add("Roles  "+roles(a.roles));
        add("Target  "+(a.target?std::to_string(a.target->degree):"—"));
        add("Confidence  "+fixed(a.analysisConfidence,2));
        add("Timing  "+fixed(c.startQN)+" QN, "+(c.durationQN?fixed(*c.durationQN):"OPEN"));
    }
    inspectorScrollMax_=std::max(0.,y+inspectorScroll_-(area.bottom-45));
    }
    if(state_.tab==session::Tab::Recommend&&selectedCandidate())
        line(dc,"Save as Custom Progression",CRect(area.left+14,area.bottom-39,area.right-14,area.bottom-8),accent,kRightText);
    else if(state_.tab==session::Tab::Library&&selectedLibrary_) {
        if(selectedLibrary_->first)line(dc,"Factory · read only",CRect(area.left+14,area.bottom-70,area.right-14,area.bottom-45),muted);
        else line(dc,"Rename     Delete",CRect(area.left+14,area.bottom-70,area.right-14,area.bottom-45),accent);
        if(actions_.exportLibraryMidi)line(dc,"Export MIDI",CRect(area.left+14,area.bottom-39,area.right-14,area.bottom-8),accent,kRightText);
    }
}

void MainView::drawResponsiveForm(CDrawContext* dc) {
    if(form_==Form::None)return;
    const double w=std::min(590.,layout_.viewport.width()-48),h=300;
    const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
    box(dc,CRect(x,y,x+w,y+h),CColor(23,39,59,255));
    line(dc,form_==Form::Save?"SAVE CUSTOM PROGRESSION":form_==Form::Rename?"EDIT USER PROGRESSION":"SEARCH LIBRARY",
        CRect(x+22,y+12,x+w-22,y+43),accent);
    line(dc,form_==Form::Search?"Name / tag":"Name",CRect(x+22,y+53,x+140,y+84),muted);
    if(form_!=Form::Search) {
        line(dc,"Style: "+(formMetadata_.style?styleName(*formMetadata_.style):"Auto"),CRect(x+22,y+115,x+w-22,y+142),pale);
        line(dc,"Intent: "+std::string(intentName(formMetadata_.intent)),CRect(x+22,y+146,x+w-22,y+174),pale);
        line(dc,"Tags (optional)",CRect(x+22,y+183,x+w-22,y+210),muted);
    }
    line(dc,"Cancel",CRect(x+150,y+h-39,x+270,y+h-7),muted);
    line(dc,form_==Form::Search?"Apply":"Save",CRect(x+w-150,y+h-39,x+w-25,y+h-7),accent,kRightText);
}
} // namespace harmony::ui
