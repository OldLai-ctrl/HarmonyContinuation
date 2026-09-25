#include "MainView.h"
#include "WeightBarGeometry.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/clinestyle.h"
#include "vstgui/lib/cframe.h"
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
}
MainView::MainView(const CRect& rect, Actions actions) : CView(rect), actions_(std::move(actions)) {}
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
    invalidRect(CRect(690,60,1080,92));
}
void MainView::rebuildTimeline() { timelineBlocks_=layoutTimeline(state_.imported.events,getViewSize().getWidth()-48.0); }
CRect MainView::chordTileRect(std::size_t i) const {
    const auto it=std::find_if(timelineBlocks_.begin(),timelineBlocks_.end(),[i](const auto& b){return b.eventIndex==i;});
    if (it==timelineBlocks_.end()) return {};
    const auto left=24.0+it->x; return CRect(left,126,left+std::max(1.0,it->width-3.0),193);
}
std::optional<CCoord> MainView::projectQNToX(double qn) const {
    const auto& events=state_.imported.events; if (events.empty() || !std::isfinite(qn) || qn<events.front().startQN) return std::nullopt;
    const auto span=events.back().startQN-events.front().startQN+2.0;
    if (span<=0 || !std::isfinite(span)) return std::nullopt;
    return static_cast<CCoord>(24.0+std::clamp((qn-events.front().startQN)/span,0.0,1.0)*(getViewSize().getWidth()-48.0));
}
void MainView::setPlaybackPosition(std::optional<double> qn,bool playing) {
    const auto oldLocation=currentLocation_; const auto oldX=playheadX_;
    projectQN_=qn && std::isfinite(*qn)?qn:std::nullopt; playing_=playing;
    currentLocation_=locateCurrentChord(state_.imported,projectQN_); playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt;
    if (oldLocation.activeIndex) invalidRect(chordTileRect(*oldLocation.activeIndex));
    if (currentLocation_.activeIndex) invalidRect(chordTileRect(*currentLocation_.activeIndex));
    if (oldX) invalidRect(CRect(*oldX-3,120,*oldX+3,200));
    if (playheadX_) invalidRect(CRect(*playheadX_-3,120,*playheadX_+3,200));
}
void MainView::drawTimeline(CDrawContext* dc,const CRect& update) {
    const auto bounds=getViewSize(); const CRect lane(18,120,bounds.right-18,200);
    box(dc,lane,CColor(19,32,49,255));
    if (state_.imported.events.empty()) {
        line(dc,"Drag Chords From Cubase   →   HarmonyContinuation",CRect(24,143,bounds.right-24,177),muted,kCenterText); return;
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
    box(dc,CRect(0,0,bounds.right,55),CColor(25,40,58,255));
    line(dc,keyLabel(state_,analysis_),CRect(18,10,290,42),state_.forcedKey?accent:pale);
    if (!state_.forcedKey && analysis_.keyCandidates.size()>1 &&
        (analysis_.keyCandidates[0].confidence<0.35f ||
         analysis_.keyCandidates[0].confidence-analysis_.keyCandidates[1].confidence<0.09f))
        line(dc,"Ambiguous",CRect(195,36,285,53),muted);
    line(dc,"Style: "+(state_.style?styleName(*state_.style):"Auto"),CRect(305,10,515,43));
    line(dc,"Intent: "+intentLabel(state_.intent),CRect(525,10,710,43));
    line(dc,state_.skeletonView?"View: SKELETON":"View: FULL",CRect(715,10,875,43),accent);
    line(dc,"RECOMMEND",CRect(875,10,975,43),state_.tab==session::Tab::Recommend?accent:muted);
    line(dc,"LIBRARY",CRect(982,10,1077,43),state_.tab==session::Tab::Library?accent:muted);
    line(dc,"CURRENT PHRASE",CRect(20,70,240,99),pale);
    if (workerBusy_) line(dc,"Analyzing…",CRect(690,68,bounds.right-25,91),muted,kRightText);
    else if (matchStatus_.find("Factory library")!=std::string::npos || matchStatus_.find("User library")!=std::string::npos)
        line(dc,"Library Error · see Library",CRect(690,68,bounds.right-25,91),CColor(255,173,173,255),kRightText);
    else if (!actionStatus_.empty()) line(dc,actionStatus_,CRect(560,68,bounds.right-25,91),muted,kRightText);
    line(dc,"MATCH",CRect(918,92,992,116),muted,kRightText);
    line(dc,state_.debugExpanded?"DEBUG ON":"DEBUG",CRect(997,92,1075,116),state_.debugExpanded?accent:muted,kRightText);
}
void MainView::drawPhrase(CDrawContext* dc,const CRect& bounds) {
    drawTimeline(dc,CRect(0,0,bounds.right,bounds.bottom));
    line(dc,"FULL",CRect(23,208,75,232),state_.skeletonView?muted:accent);
    line(dc,"SKELETON",CRect(85,208,185,232),state_.skeletonView?accent:muted);
    if (state_.imported.events.empty()) return;
    if (selectedChord_ && *selectedChord_<analysis_.full.size()) {
        const auto& event=state_.imported.events[*selectedChord_]; const auto& a=analysis_.full[*selectedChord_];
        const auto duration=event.durationQN?fixed(*event.durationQN)+" QN":"OPEN";
        const auto target=a.target?std::to_string(a.target->degree):"—";
        line(dc,event.name+"    Degree "+formatDegree(a)+"    Function "+formatFunction(a.function)+
                "    Weight "+fixed(a.structuralWeight,2)+"    Confidence "+fixed(a.analysisConfidence,2),
             CRect(23,234,bounds.right-23,256),pale);
        line(dc,"Roles "+roles(a.roles)+"    Target "+target+"    Duration "+duration+
                "    Start "+fixed(event.startQN)+" QN",CRect(23,258,bounds.right-23,280),muted);
    } else line(dc,"Click a chord for details",CRect(23,237,bounds.right-23,270),muted);
}
void MainView::drawMiniTimeline(CDrawContext* dc,const ContinuationCandidate& c,CRect area) {
    const auto& events=state_.imported.events;
    const std::size_t start=0;
    double total{};
    for (std::size_t i=start;i<events.size();++i)
        total+=(i+1==events.size()?c.suggestedCurrentChordDurationQN.value_or(4.0):events[i].durationQN.value_or(4.0));
    for (const auto& e:c.continuation) total+=e.durationQN;
    if (total<=0) return;
    double cursor=area.left;
    const auto drawBlock=[&](std::string label,double duration,CColor color,bool final) {
        const double width=final?area.right-cursor:area.getWidth()*std::max(0.0,duration)/total;
        if (width<1) return;
        box(dc,CRect(cursor,area.top,cursor+width-2,area.bottom),color);
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
    if (state_.imported.events.empty()) { line(dc,"Drag Chords From Cubase",CRect(20,440,bounds.right-20,490),pale,kCenterText); return; }
    constexpr const char* titles[]{"RESOLVE","DEVELOP","LOOP","COLOR"};
    const std::size_t chosen=state_.intent?static_cast<std::size_t>(*state_.intent):0;
    const std::size_t preferred=chosen==0?4:chosen==1?1:chosen==2?0:chosen==3?2:3;
    for (std::size_t visual=0;visual<4;++visual) {
        const auto group=visual==0 && preferred<4?preferred:
            (preferred<4 && visual<=preferred?visual-1:visual);
        const auto top=292.0+visual*115.0;
        box(dc,CRect(16,top,bounds.right-16,top+110),panel);
        line(dc,titles[group],CRect(24,top+4,190,top+27),accent);
        if (recommendations_.groups[group].size()>3)
            line(dc,"Show More",CRect(920,top+4,1070,top+27),muted,kRightText);
        const auto visible=visibility_.visibleIndices(recommendations_.groups[group]);
        if (visible.empty()) { line(dc,workerBusy_?"Analyzing…":"No strong option",CRect(205,top+4,bounds.right-22,top+27),muted); continue; }
        for (std::size_t row=0;row<visible.size() && row<3;++row) {
            const auto& c=recommendations_.groups[group][visible[row]];
            const auto y=top+27+row*26;
            box(dc,CRect(23,y,bounds.right-23,y+24),CColor(33,49,69,255));
            drawMiniTimeline(dc,c,CRect(33,y+3,715,y+21));
            line(dc,std::to_string(static_cast<int>(std::round(c.rankingScore))),CRect(720,y+2,760,y+22),pale,kCenterText);
            line(dc,cadenceName(c.cadence),CRect(765,y+2,840,y+22),muted,kCenterText);
            line(dc,primaryStyle(c.styles),CRect(845,y+2,930,y+22),muted,kCenterText);
            line(dc,std::find(state_.pinnedCandidateIds.begin(),state_.pinnedCandidateIds.end(),c.id)!=state_.pinnedCandidateIds.end()?"● Pin":"○ Pin",
                 CRect(936,y+2,995,y+22),accent);
            if (actions_.audition) {
                line(dc,previewCandidateId_==c.id?"■":"▶",CRect(1000,y+2,1032,y+22),accent,kCenterText);
                line(dc,c.supportCount>1?"×"+std::to_string(c.supportCount):"Details",CRect(1037,y+2,1080,y+22),muted);
            } else line(dc,c.supportCount>1?"×"+std::to_string(c.supportCount):"Details",CRect(1000,y+2,1070,y+22),muted);
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
    box(dc,CRect(16,758,bounds.right-16,bounds.bottom-13),CColor(27,41,58,255));
    line(dc,"COMPARE  ·  "+std::to_string(state_.pinnedCandidateIds.size())+" / 3",CRect(24,765,260,789),accent);
    std::size_t shown{};
    for (const auto& id:state_.pinnedCandidateIds) {
        const auto* c=findCandidate(id);
        const auto left=24.0+shown*350.0;
        if (c) {
            std::string path;
            for (const auto& event:c->continuation) { if (!path.empty()) path+=" → "; path+=event.label; }
            line(dc,std::string(intentName(c->intent))+"   "+std::to_string(static_cast<int>(std::round(c->rankingScore)))+
                    "   "+cadenceName(c->cadence),CRect(left,795,left+320,816),pale);
            line(dc,path+"  ·  "+styleFlags(c->styles),CRect(left,817,left+320,838),muted);
            if (c->suggestedCurrentChordDurationQN)
                line(dc,"OPEN "+fixed(*c->suggestedCurrentChordDurationQN)+" QN",CRect(left,839,left+320,858),muted);
        } else line(dc,"Pinned path unavailable after recompute",CRect(left,795,left+320,835),muted);
        line(dc,"Unpin ×",CRect(left+250,765,left+325,789),muted);
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
void MainView::drawLibrary(CDrawContext* dc,const CRect& bounds) {
    box(dc,CRect(16,287,bounds.right-16,bounds.bottom-16),panel);
    line(dc,"LIBRARY    Factory "+std::to_string(factory_.size())+"  ·  User "+std::to_string(user_.size()),CRect(24,296,550,323),accent);
    line(dc,"Style: "+(libraryStyle_?styleName(*libraryStyle_):"All"),CRect(24,328,245,354),muted);
    line(dc,"Intent: "+intentLabel(libraryIntent_),CRect(255,328,430,354),muted);
    line(dc,std::string("Mode: ")+(libraryMode_?(*libraryMode_==Mode::Major?"Major":"Minor"):"All"),CRect(440,328,580,354),muted);
    line(dc,"Search: "+(librarySearch_.empty()?"Name / tag":librarySearch_),CRect(590,328,930,354),muted);
    line(dc,"QA",CRect(970,328,1060,354),accent);
    if (!libraryError_.empty()) line(dc,"Library Error: "+libraryError_,CRect(24,360,bounds.right-24,389),CColor(255,173,173,255));
    const auto filtered=filteredLibrary();
    const auto offset=libraryPage_*11;
    for (std::size_t row=0;row<11 && offset+row<filtered.size();++row) {
        const auto [factory,index]=filtered[offset+row]; const auto& item=factory?factory_[index]:user_[index];
        const auto y=385.0+row*36.0;
        box(dc,CRect(24,y,bounds.right-24,y+33),CColor(32,49,69,255));
        line(dc,factory?"Factory":"User",CRect(31,y+4,105,y+29),factory?muted:accent);
        line(dc,item.name,CRect(112,y+4,425,y+29),pale);
        line(dc,styleFlags(item.styles),CRect(435,y+4,670,y+29),muted);
        line(dc,intentName(item.intent),CRect(680,y+4,790,y+29),muted);
        line(dc,item.mode==Mode::Major?"Major":"Minor",CRect(805,y+4,900,y+29),muted);
        line(dc,"Inspect",CRect(950,y+4,1065,y+29),accent);
    }
    line(dc,"◀ Previous",CRect(28,795,175,830),muted);
    line(dc,"Page "+std::to_string(libraryPage_+1)+" / "+std::to_string(std::max<std::size_t>(1,(filtered.size()+10)/11)),
         CRect(400,795,700,830),muted,kCenterText);
    line(dc,"Next ▶",CRect(925,795,1070,830),muted,kRightText);
    if (selectedLibrary_) {
        const auto [factory,index]=*selectedLibrary_;
        const auto* item=factory?(index<factory_.size()?&factory_[index]:nullptr):(index<user_.size()?&user_[index]:nullptr);
        if (!item) return;
        box(dc,CRect(320,370,1076,783),CColor(18,30,47,255));
        line(dc,"×    "+item->name+"   ["+item->id+"]",CRect(335,379,1060,407),accent);
        line(dc,"Style  "+styleFlags(item->styles)+"    Intent  "+intentName(item->intent)+"    Mode  "+
             (item->mode==Mode::Major?"Major":"Minor")+"    Cadence  "+cadenceName(item->cadence),CRect(335,413,1060,441),pale);
        line(dc,"Prior  "+fixed(item->priorWeight,2)+"    Rhythm  "+std::to_string(item->meterNumerator)+"/"+
             std::to_string(item->meterDenominator),CRect(335,441,1060,469),muted);
        std::string full, skeleton, rhythm;
        for (std::size_t i=0;i<item->full.size();++i) {
            if (i) { full+=" → "; rhythm+=" · "; }
            full+=formatMatchEvent(item->full[i]);
            rhythm+=item->full[i].durationQN?fixed(*item->full[i].durationQN):"?";
        }
        for (const auto i:item->skeletonIndices) if (i<item->full.size()) { if (!skeleton.empty()) skeleton+=" → "; skeleton+=formatMatchEvent(item->full[i]); }
        line(dc,"FULL: "+full,CRect(335,478,1060,510),pale);
        line(dc,"SKELETON: "+skeleton,CRect(335,515,1060,545),muted);
        line(dc,"Rhythm QN: "+rhythm,CRect(335,551,1060,580),muted);
        std::string tags; for (const auto& t:item->tags) { if (!tags.empty()) tags+=", "; tags+=t; }
        line(dc,"Tags: "+tags,CRect(335,588,1060,619),muted);
        std::size_t nearCount{};
        for (const auto& other:factory_) if (other.id!=item->id && other.fingerprint.skeletonDegrees==item->fingerprint.skeletonDegrees &&
            other.fingerprint.rhythmShape==item->fingerprint.rhythmShape) ++nearCount;
        line(dc,"Near duplicate warning: "+std::to_string(nearCount),CRect(335,624,1060,653),muted);
        double total{}; for (const auto& event:item->full) total+=event.durationQN.value_or(4.0);
        double cursor=337;
        if (total>0) for (std::size_t i=0;i<item->full.size();++i) {
            const auto width=i+1==item->full.size()?1058-cursor:718.0*item->full[i].durationQN.value_or(4.0)/total;
            if (width>2) {
                box(dc,CRect(cursor,660,cursor+width-2,691),CColor(50,116,180,255));
                if (width>35) line(dc,formatMatchEvent(item->full[i]),CRect(cursor+2,662,cursor+width-4,689),pale,kCenterText);
            }
            cursor+=width;
        }
        if (!factory) {
            line(dc,"Rename / metadata",CRect(345,706,545,740),accent);
            line(dc,"Delete",CRect(600,706,715,740),CColor(255,175,175,255));
        } else line(dc,"Factory · read only",CRect(345,706,670,740),muted);
    }
}
void MainView::drawDiagnostics(CDrawContext* dc,const CRect& bounds) {
    box(dc,CRect(16,287,bounds.right-16,bounds.bottom-15),panel);
    line(dc,"MATCH / DEBUG    [Refresh host]    [Clipboard]",CRect(24,296,bounds.right-24,325),accent);
    line(dc,matchStatus_,CRect(24,330,bounds.right-24,357),pale);
    line(dc,"Analysis / Match / Recommendation generation: "+std::to_string(generation_)+
         "    Worker: "+(workerBusy_?"busy":"idle")+"    Last: "+fixed(computationMs_,2)+" ms",CRect(24,358,bounds.right-24,385),muted);
    line(dc,"Factory v"+std::to_string(state_.factoryLibraryVersion)+" · "+std::to_string(factoryCount_)+
         "    User "+std::to_string(userCount_)+"    Session dirty: "+(sessionDirty_?"yes":"no"),CRect(24,386,bounds.right-24,413),muted);
    if (!matches_.empty()) {
        const auto& m=matches_.front();
        line(dc,"Top match: "+m.templateName+" · "+fixed(m.similarity,2)+" · "+formatKey(m.key),CRect(24,420,bounds.right-24,447),pale);
        std::string trace;
        for (const auto& step:m.alignmentTrace) {
            if (!trace.empty()) trace+="   ";
            trace+=(step.operation==AlignmentOp::Match?"│":step.operation==AlignmentOp::QueryInsertion?"+":step.operation==AlignmentOp::TemplateDeletion?"-":"≠");
            trace+=step.queryIndex?std::to_string(*step.queryIndex):"_";
            trace+="↔"; trace+=step.templateIndex?std::to_string(*step.templateIndex):"_";
        }
        line(dc,"USER ↔ TEMPLATE   "+trace,CRect(24,452,bounds.right-24,479),muted);
    }
    std::string diagnostic=hostText_+"\n"+parseText_+"\n"+rawText_;
    std::istringstream in(diagnostic); std::string row; double y=496;
    while (y<bounds.bottom-22 && std::getline(in,row)) { line(dc,row.substr(0,165),CRect(24,y,bounds.right-24,y+18),muted); y+=18; }
}
void MainView::drawInspector(CDrawContext* dc,const CRect& bounds) {
    const auto* c=selectedCandidate(); if (!c || state_.tab!=session::Tab::Recommend) return;
    box(dc,CRect(430,280,bounds.right-18,751),CColor(18,30,47,255));
    line(dc,"×    RECOMMENDATION INSPECTOR",CRect(445,292,bounds.right-32,322),accent);
    line(dc,"Intent  "+std::string(intentName(c->intent))+"     Key  "+formatKey(c->key)+
         "     Score  "+std::to_string(static_cast<int>(std::round(c->rankingScore))),CRect(445,329,bounds.right-32,356),pale);
    const auto match=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return m.templateId==c->primaryTemplate;});
    line(dc,"Match  "+fixed(c->matchSimilarity,2)+"    Skeleton  "+fixed(match!=matches_.end()?match->subScores.skeletonHarmony:0,2)+
         "    FULL  "+fixed(match!=matches_.end()?match->subScores.fullHarmony:0,2)+"    Rhythm  "+fixed(c->subscores.rhythm,2),CRect(445,367,bounds.right-32,394),muted);
    line(dc,"Style  "+fixed(c->subscores.style,2)+"    Intent  "+fixed(c->subscores.intent,2)+
         "    Prior  "+fixed(c->subscores.prior,2)+"    Support  "+std::to_string(c->supportCount),CRect(445,397,bounds.right-32,424),muted);
    line(dc,"Cadence  "+std::string(cadenceName(c->cadence))+"    Path  "+session::continuationFingerprint(*c),
         CRect(445,427,bounds.right-32,454),muted);
    std::string sources; for (const auto& id:c->supportingTemplates) { if (!sources.empty()) sources+=", "; sources+=id; }
    line(dc,"Sources  "+sources,CRect(445,459,bounds.right-32,488),muted);
    line(dc,"EXISTING                               │ RECOMMENDED",CRect(445,493,bounds.right-32,520),accent);
    drawMiniTimeline(dc,*c,CRect(447,523,bounds.right-38,553));
    if (match!=matches_.end()) {
        line(dc,"USER                         TEMPLATE   ·   Alignment",CRect(445,565,bounds.right-32,590),muted);
        std::size_t shown{};
        for (const auto& step:match->alignmentTrace) {
            if (shown++>=5) break;
            const auto y=591.0+(shown-1)*22.0;
            const auto user=step.queryIndex && *step.queryIndex<state_.imported.events.size()?state_.imported.events[*step.queryIndex].name:"gap";
            const auto templ=step.templateIndex && *step.templateIndex<match->templateLabels.size()?match->templateLabels[*step.templateIndex]:"gap";
            const auto op=step.operation==AlignmentOp::Match?"│":step.operation==AlignmentOp::QueryInsertion?"+":
                step.operation==AlignmentOp::TemplateDeletion?"−":"≠";
            line(dc,user+"       "+op+"       "+templ,CRect(460,y,bounds.right-40,y+20),step.operation==AlignmentOp::Match?CColor(130,202,164,255):muted);
        }
    }
    line(dc,"Save as Custom Progression",CRect(670,708,bounds.right-37,741),accent,kRightText);
}
void MainView::drawForm(CDrawContext* dc,const CRect& bounds) {
    if (form_==Form::None) return;
    box(dc,CRect(290,310,bounds.right-290,605),CColor(23,39,59,255));
    line(dc,form_==Form::Save?"SAVE CUSTOM PROGRESSION":form_==Form::Rename?"EDIT USER PROGRESSION":"SEARCH LIBRARY",
         CRect(315,325,bounds.right-315,355),accent);
    line(dc,form_==Form::Search?"Name / tag":"Name",CRect(315,365,440,390),muted);
    if (form_!=Form::Search) {
        line(dc,"Style: "+(formMetadata_.style?styleName(*formMetadata_.style):"Auto"),CRect(315,425,600,452),pale);
        line(dc,"Intent: "+std::string(intentName(formMetadata_.intent)),CRect(315,455,600,482),pale);
        line(dc,"Tags (optional)",CRect(315,486,600,510),muted);
    }
    line(dc,"Cancel",CRect(485,563,610,595),muted);
    line(dc,form_==Form::Search?"Apply":"Save",CRect(680,563,790,595),accent);
}
void MainView::drawDebugOverlay(CDrawContext* dc,const CRect& bounds) {
    if (!state_.debugExpanded) return;
    box(dc,CRect(bounds.right-365,204,bounds.right-20,283),CColor(34,49,68,255));
    line(dc,"Generation "+std::to_string(generation_)+" · "+(workerBusy_?"busy":"idle")+
        " · "+fixed(computationMs_,2)+" ms",CRect(bounds.right-352,208,bounds.right-30,230),pale);
    line(dc,"Factory v"+std::to_string(state_.factoryLibraryVersion)+" / "+std::to_string(factoryCount_)+
        " · User "+std::to_string(userCount_),CRect(bounds.right-352,233,bounds.right-30,255),muted);
    line(dc,std::string("Session dirty ")+(sessionDirty_?"yes":"no"),CRect(bounds.right-352,258,bounds.right-30,280),muted);
}
void MainView::drawRect(CDrawContext* dc,const CRect& update) {
    const auto bounds=getViewSize();
    if (update.top>=120 && update.bottom<=200) { drawTimeline(dc,update); return; }
    box(dc,bounds,background);
    drawTop(dc,bounds); drawPhrase(dc,bounds);
    switch(state_.tab) {
        case session::Tab::Recommend: drawRecommendations(dc,bounds); break;
        case session::Tab::Library: drawLibrary(dc,bounds); break;
        default: drawDiagnostics(dc,bounds); break;
    }
    drawDebugOverlay(dc,bounds); drawInspector(dc,bounds); drawForm(dc,bounds);
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
    nameEdit_=new CTextEdit(CRect(440,363,785,401),nullptr,0,initial.c_str());
    nameEdit_->setBackColor(CColor(242,247,252,255)); nameEdit_->setFontColor(CColor(20,30,45,255));
    getFrame()->addView(nameEdit_);
    if (kind!=Form::Search) {
        tagsEdit_=new CTextEdit(CRect(440,510,785,548),nullptr,0,"");
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
    try {
        if (form_!=Form::None) {
            if (where.y>=561 && where.y<603) {
                if (where.x>=460 && where.x<630) endForm();
                else if (where.x>=650 && where.x<810) submitForm();
            } else if (where.y>=420 && where.y<455 && form_!=Form::Search) {
                const auto choice=popup({"Auto","Pop","Rock","R&B","Jazz","City Pop / J-pop","Functional"},where);
                constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                if (choice>=0) formMetadata_.style=choice==0?std::nullopt:std::optional<Style>(styles[choice-1]);
                invalid();
            } else if (where.y>=455 && where.y<487 && form_!=Form::Search) {
                const auto choice=popup({"Develop","Resolve","Loop","Color"},where);
                if (choice>=0) formMetadata_.intent=static_cast<PhraseIntent>(choice+1);
                invalid();
            }
            return kMouseEventHandled;
        }
        if (where.y<55) {
            if (where.x<295) {
                std::vector<std::string> names{"Auto"};
                for (int i=0;i<12;++i) {
                    names.push_back(formatKey(KeySignature{static_cast<PitchClass>(i),Mode::Major}));
                    names.push_back(formatKey(KeySignature{static_cast<PitchClass>(i),Mode::Minor}));
                }
                const auto choice=popup(names,where);
                if (choice>=0) { state_.forcedKey=choice==0?std::nullopt:std::optional<KeySignature>(KeySignature{
                    static_cast<PitchClass>((choice-1)/2),((choice-1)%2)?Mode::Minor:Mode::Major}); notifyState(); }
            } else if (where.x<520) {
                const auto choice=popup({"Auto","Pop","Rock","R&B","Jazz","City Pop / J-pop","Functional"},where);
                constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                if (choice>=0) { state_.style=choice==0?std::nullopt:std::optional<Style>(styles[choice-1]); notifyState(); }
            } else if (where.x<715) {
                const auto choice=popup({"Auto","Develop","Resolve","Loop","Color"},where);
                if (choice>=0) { state_.intent=choice==0?std::nullopt:std::optional<PhraseIntent>(static_cast<PhraseIntent>(choice)); notifyState(); }
            } else if (where.x<870) { state_.skeletonView=!state_.skeletonView; notifyState(); }
            else if (where.x<980) { state_.tab=session::Tab::Recommend; selectedLibrary_.reset(); notifyState(); }
            else { state_.tab=session::Tab::Library; selectedCandidate_.reset(); notifyState(); }
            return kMouseEventHandled;
        }
        if (where.y>=90 && where.y<118 && where.x>900) {
            if (where.x<995) state_.tab=session::Tab::Diagnostics;
            else state_.debugExpanded=!state_.debugExpanded;
            notifyState(); return kMouseEventHandled;
        }
        if (where.y>=120 && where.y<202) {
            for (const auto& b:timelineBlocks_) {
                const auto rect=chordTileRect(b.eventIndex);
                if (where.x>=rect.left && where.x<rect.right) { selectedChord_=b.eventIndex; invalid(); break; }
            }
            return kMouseEventHandled;
        }
        if (where.y>=203 && where.y<235 && !analysis_.full.empty()) {
            state_.skeletonView=where.x>=82 && where.x<190; notifyState(); return kMouseEventHandled;
        }
        if (state_.tab==session::Tab::Diagnostics || state_.tab==session::Tab::Match) {
            if (where.y>=290 && where.y<325) {
                if (where.x<550 && actions_.refresh) actions_.refresh();
                else if (actions_.clipboard) actions_.clipboard();
            }
            return kMouseEventHandled;
        }
        if (state_.tab==session::Tab::Library) {
            if (selectedLibrary_) {
                if (where.y>=379 && where.y<410 && where.x>=320 && where.x<400) { selectedLibrary_.reset(); invalid(); return kMouseEventHandled; }
                if (!selectedLibrary_->first && selectedLibrary_->second<user_.size() && where.y>=699 && where.y<751) {
                    const auto item=user_[selectedLibrary_->second];
                    if (where.x>=340 && where.x<570) {
                        formMetadata_.intent=item.intent; formMetadata_.style.reset();
                        for (auto s:{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional})
                            if (item.styles&static_cast<StyleFlags>(s)) { formMetadata_.style=s; break; }
                        beginForm(Form::Rename,item.name);
                        if (tagsEdit_) { std::string tags; for (const auto& tag:item.tags) { if (!tags.empty()) tags+=", "; tags+=tag; }
                            tagsEdit_->setText(tags.c_str()); }
                    } else if (where.x>=590 && where.x<750 && actions_.deleteUser) {
                        const auto choice=popup({"Cancel","Delete "+item.name},where);
                        if (choice==1) { actionStatus_=actions_.deleteUser(item.id); selectedLibrary_.reset(); invalid(); }
                    }
                    return kMouseEventHandled;
                }
                return kMouseEventHandled;
            }
            if (where.y>=326 && where.y<358) {
                if (where.x<250) {
                    const auto n=popup({"All","Pop","Rock","R&B","Jazz","City Pop / J-pop","Functional"},where);
                    constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                    if (n>=0) libraryStyle_=n?std::optional<Style>(styles[n-1]):std::nullopt;
                } else if (where.x<435) {
                    const auto n=popup({"Auto","Develop","Resolve","Loop","Color"},where);
                    if (n>=0) libraryIntent_=n?std::optional<PhraseIntent>(static_cast<PhraseIntent>(n)):std::nullopt;
                } else if (where.x<585) {
                    const auto n=popup({"All","Major","Minor"},where);
                    if (n>=0) libraryMode_=n?std::optional<Mode>(n==1?Mode::Major:Mode::Minor):std::nullopt;
                } else if (where.x<940) beginForm(Form::Search,librarySearch_);
                libraryPage_=0; invalid(); return kMouseEventHandled;
            }
            if (where.y>=795 && where.y<836) {
                if (where.x<200 && libraryPage_>0) --libraryPage_;
                else if (where.x>895 && (libraryPage_+1)*11<filteredLibrary().size()) ++libraryPage_;
                invalid(); return kMouseEventHandled;
            }
            if (where.y>=385 && where.y<781) {
                const auto row=static_cast<std::size_t>((where.y-385)/36);
                const auto filtered=filteredLibrary(); const auto i=libraryPage_*11+row;
                if (i<filtered.size()) { selectedLibrary_=filtered[i]; invalid(); }
            }
            return kMouseEventHandled;
        }
        if (selectedCandidate_) {
            if (where.y>=290 && where.y<325 && where.x>=435 && where.x<480) { selectedCandidate_.reset(); invalid(); return kMouseEventHandled; }
            if (where.y>=703 && where.y<751 && where.x>655) {
                if (const auto* c=selectedCandidate()) {
                    formMetadata_.name="My "+std::string(intentName(c->intent))+" Progression";
                    formMetadata_.style=state_.style;
                    if (!formMetadata_.style) for (auto s:{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional})
                        if (c->styles&static_cast<StyleFlags>(s)) { formMetadata_.style=s; break; }
                    formMetadata_.intent=c->intent;
                    beginForm(Form::Save,formMetadata_.name);
                }
                return kMouseEventHandled;
            }
            if (where.x>=430 && where.y>=280 && where.y<751) return kMouseEventHandled;
        }
        if (where.y>=758 && where.y<793 && where.x>=250) {
            const auto index=static_cast<std::size_t>((where.x-24)/350);
            if (index<state_.pinnedCandidateIds.size() && where.x>=24+index*350+250) {
                const auto id=state_.pinnedCandidateIds[index]; state_.unpin(id);
                std::erase_if(pinnedSnapshots_,[&](const auto& candidate){return candidate.id==id;});
                notifyState(); return kMouseEventHandled;
            }
        }
        constexpr std::size_t chosenMap[]{4,1,0,2,3};
        const std::size_t pref=chosenMap[state_.intent?static_cast<std::size_t>(*state_.intent):0];
        for (std::size_t visual=0;visual<4;++visual) {
            const auto group=visual==0 && pref<4?pref:(pref<4 && visual<=pref?visual-1:visual);
            const auto top=292.0+visual*115.0;
            if (where.y>=top && where.y<top+27 && where.x>900 && recommendations_.groups[group].size()>3) {
                std::vector<std::string> options;
                for (const auto& c:recommendations_.groups[group]) {
                    std::string path; for (const auto& e:c.continuation) { if (!path.empty()) path+=" → "; path+=e.label; }
                    options.push_back(std::to_string(static_cast<int>(std::round(c.rankingScore)))+" · "+path);
                }
                const auto selected=popup(options,where);
                if (selected>=0) { selectedCandidate_={{group,static_cast<std::size_t>(selected)}}; invalid(); }
                return kMouseEventHandled;
            }
            if (where.y<top+27 || where.y>=top+105) continue;
            const auto row=static_cast<std::size_t>((where.y-top-27)/26);
            const auto visible=visibility_.visibleIndices(recommendations_.groups[group]);
            if (row>=visible.size() || row>=3) return kMouseEventHandled;
            const auto index=visible[row]; const auto& c=recommendations_.groups[group][index];
            if (where.x>=930 && where.x<1000) {
                if (state_.unpin(c.id)) std::erase_if(pinnedSnapshots_,[&](const auto& candidate){return candidate.id==c.id;});
                else {
                    if (!state_.pin(c.id,session::continuationFingerprint(c))) actionStatus_="Compare holds at most 3 candidates";
                    else pinnedSnapshots_.push_back(c);
                }
                notifyState();
            } else if (where.x>=1000 && where.x<1037 && actions_.audition) {
                actions_.audition(c);
            } else { selectedCandidate_={{group,index}}; invalid(); }
            return kMouseEventHandled;
        }
    } catch (...) { actionStatus_="Action failed"; invalid(); }
    return kMouseEventHandled;
}
} // namespace harmony::ui
