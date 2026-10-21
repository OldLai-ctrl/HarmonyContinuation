#include "MainView.h"
#include "product/ProductVersion.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgraphicstransform.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cclipboard.h"
#include "RecommendationAlignment.h"
#include "EnrichmentProvenance.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <stdexcept>

namespace harmony::ui {
using namespace VSTGUI;
namespace {
const CColor bg{27,29,33,255},surface{36,39,45,255},raised{43,46,53,255},edge{74,80,91,255},
    fg{242,243,245,255},secondary{175,181,191,255},blue{166,186,255,255};
CRect rect(UiRect r){return {r.left,r.top,r.right,r.bottom};}
std::string number(double n){std::ostringstream s;s<<std::fixed<<std::setprecision(6)<<n;auto value=s.str();while(value.size()>1&&value.back()=='0')value.pop_back();if(value.back()=='.')value.pop_back();return value;}
std::string compactStyleName(Style s){switch(s){case Style::Pop:return "Pop";case Style::Rock:return "Rock";case Style::Rnb:return "R&B";case Style::Jazz:return "Jazz";case Style::CityPop:return "City Pop";default:return "Functional";}}
void fill(CDrawContext* dc,UiRect r,CColor c){dc->setFillColor(c);dc->drawRect(rect(r),kDrawFilled);}
void text(CDrawContext* dc,const std::string& s,UiRect r,double size=12,CColor c=fg,CHoriTxtAlign align=kLeftText){
    static const auto font=owned(new CFontDesc("Microsoft YaHei UI",12));
    dc->setFont(font,size);dc->setFontColor(c);ConcatClip clip(*dc,rect(r));dc->drawString(s.c_str(),rect(r),align);
}
void rounded(CDrawContext* dc,UiRect r,CColor color,CColor border=edge,double radius=8){
    auto path=owned(dc->createRoundRectGraphicsPath(rect(r),radius));if(!path)return;
    dc->setFillColor(color);dc->drawGraphicsPath(path,CDrawContext::kPathFilled);
    dc->setFrameColor(border);dc->setLineWidth(1);dc->drawGraphicsPath(path,CDrawContext::kPathStroked);
}
}

void MainView::refreshBrowse(){
    if(!browseDirty_)return;
    const auto priority=continuationPresentation();
    refreshColorRanking();
    for(std::size_t g=0;g<4;++g){
        auto preferred=priority[g];
        preferred.insert(preferred.end(),colorRanks_[g].order.begin(),colorRanks_[g].order.end());
        auto& rows=browseRows_[g];rows.clear();
        for(const auto i:qualifiedBrowseOrder(recommendations_.groups[g],preferred))
            rows.push_back({g,i,!recommendations_.groups[g][i].exploratory&&std::find(priority[g].begin(),priority[g].end(),i)!=priority[g].end(),recommendations_.groups[g][i].exploratory});
    }
    auto& rows=browseRows_[4];rows.clear();
    for(std::size_t g=0;g<3;++g)for(const auto i:enrichmentOrder(g))
        rows.push_back({g,i,rows.size()<3,false});
    for(std::size_t g=0;g<5;++g){
        browsePages_[g]=browsePage(browseRows_[g].size(),browsePages_[g]).page;
        const auto valid=[&](const std::string& id){return std::any_of(browseRows_[g].begin(),browseRows_[g].end(),[&](const auto& row){
            return (g==4?enrichments_.groups[row.group][row.index].id:recommendations_.groups[row.group][row.index].id)==id;});};
        if(!valid(browseSelected_[g]))browseSelected_[g].clear();
        if(g==browseCategory()&&!valid(compactSelected_))compactSelected_.clear();
    }
    browseDirty_=false;
}
const std::vector<BrowseEntry>& MainView::browseRows(){refreshBrowse();return browseRows_[browseCategory()];}
void MainView::changeBrowseCategory(std::size_t category){
    seekGesture_.reset();if(category!=browseCategory()&&actions_.stopAudition)actions_.stopAudition();
    browseScroll_[browseCategory()]=compactScroll_;browseSelected_[browseCategory()]=compactSelected_;
    state_.productMode=category==4?session::ProductMode::Enrich:session::ProductMode::Continue;
    if(category<4){compactGroup_=category;constexpr PhraseIntent intents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};state_.intent=intents[category];}
    compactSelected_=browseSelected_[category];compactScroll_=browseScroll_[category];
    selectedCandidate_.reset();selectedEnrichment_.reset();selectedChord_.reset();
    overviewOpen_=false;midiPending_.reset();midiClick_={};notifyState();
}
void MainView::focusBrowseEntry(std::size_t rank){
    const auto& rows=browseRows();if(rank>=rows.size())return;
    const auto& row=rows[rank];compactSelected_=browseCategory()==4?enrichments_.groups[row.group][row.index].id:recommendations_.groups[row.group][row.index].id;
    browsePages_[browseCategory()]=rank/recommendationPageSize;compactScroll_=0;
    const auto geometry=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    for(auto i=browsePage(rows.size(),browsePages_[browseCategory()]).begin;i<rank;++i){
        auto card=compactCandidateGeometry(geometry.cardsViewport,0,detailedCards_);
        if(detailedCards_)card.row.bottom=card.timeBand.bottom+128.;
        if(browseCategory()!=4&&!snapshotMode_){const auto& c=recommendations_.groups[rows[i].group][rows[i].index];
            const auto m=std::find_if(matches_.begin(),matches_.end(),[&](const auto& v){return representativeMatch(c,v);});
            const bool gaps=m!=matches_.end()&&std::any_of(m->alignmentTrace.begin(),m->alignmentTrace.end(),[](const auto& st){return st.operation==AlignmentOp::QueryInsertion||st.operation==AlignmentOp::TemplateDeletion;});
            const bool substitutions=m!=matches_.end()&&std::any_of(m->alignmentTrace.begin(),m->alignmentTrace.end(),[](const auto& st){return st.operation==AlignmentOp::Substitute;});
            if(detailedCards_)card.row.bottom=card.timeBand.bottom+128.;
            if(gaps||(detailedCards_&&substitutions))card.row.bottom=(detailedCards_?card.row.bottom:card.timeBand.bottom)+116.;
        }
        compactScroll_+=card.row.height()+8;
    }
    selectedCandidate_.reset();selectedEnrichment_.reset();selectedChord_.reset();overviewOpen_=false;
}
std::string MainView::browseName(const BrowseEntry& row) const {
    if(browseCategory()==4){constexpr const char* names[]{"group.polish","group.rich","group.advanced"};return t(names[row.group]);}
    const auto& c=recommendations_.groups[row.group][row.index];
    for(const auto* catalog:{&factory_,&user_})for(const auto& p:*catalog)if(p.id==c.primaryTemplate)
        return state_.locale==session::Locale::ZhCN&&!p.nameZh.empty()?p.nameZh:!p.nameEn.empty()?p.nameEn:p.name;
    for(const auto& m:matches_)if(representativeMatch(c,m))return m.templateName;
    return c.primaryTemplate;
}
std::string MainView::browsePath(const BrowseEntry& row) const {
    std::string path;const auto append=[&](const auto& label){if(!path.empty())path+=" → ";path+=label;};
    if(browseCategory()==4){for(const auto& e:enrichments_.groups[row.group][row.index].progression)append(e.name);}
    else {const auto& c=recommendations_.groups[row.group][row.index];const auto route=continuationRoute(c);
        for(const auto& e:route?route.candidate.continuation:c.continuation)append(e.label);}
    return path;
}
std::string MainView::recommendationListText(){
    const bool zh=state_.locale==session::Locale::ZhCN;std::ostringstream out;
    const auto& rows=browseRows();out<<(zh?"完整推荐清单":"Complete recommendations")<<" ("<<rows.size()<<")\n";
    for(std::size_t rank=0;rank<rows.size();++rank){const auto& r=rows[rank];
        out<<rank+1<<". "<<(r.exploratory?(zh?"探索":"Explore"):(zh?"默认":"Default"))<<(r.priority?" [Top 3]":"")<<" | "<<browseName(r)<<" | "<<browsePath(r);
        if(browseCategory()==4){const auto& c=enrichments_.groups[r.group][r.index];out<<" | "<<(zh?"升级进行":"Enrichment")<<" | score="<<c.score<<" | id="<<c.id<<" | fingerprint="<<c.fingerprint;}
        else {const auto& c=recommendations_.groups[r.group][r.index];std::size_t ins{},skip{};
            for(const auto& m:matches_)if(representativeMatch(c,m))for(const auto& step:m.alignmentTrace){ins+=step.operation==AlignmentOp::QueryInsertion;skip+=step.operation==AlignmentOp::TemplateDeletion;}
            out<<" | "<<tIntent(c.intent)<<" | score="<<c.rankingScore<<" | base="<<c.eligibilityScore<<" | id="<<c.id<<" | source="<<c.primaryTemplate
               <<" | key="<<formatKey(c.key)<<" | match="<<c.matchStart+1<<'-'<<c.matchEnd+1<<" | inputInsertions="<<ins<<" | templateSkips="<<skip
               <<" | intentEvidence="<<completionReasonKey(c.completion.reason)<<" | fingerprint="<<continuationMusicFingerprint(c);}
        out<<'\n';
    }return out.str();
}
void MainView::drawBrowseControls(CDrawContext* dc,const CompactLayoutResult& g){
    const bool zh=state_.locale==session::Locale::ZhCN;const auto& rows=browseRows();
    const auto page=browsePage(rows.size(),browsePages_[browseCategory()]);browsePages_[browseCategory()]=page.page;
    const bool ready=!workerBusy_&&recommendationResultReady_;
    compactButton(dc,{g.recommendationHeading.right-150,g.recommendationHeading.top,g.recommendationHeading.right,g.recommendationHeading.bottom},
        (zh?"推荐总览（":"Overview (")+std::to_string(rows.size())+(zh?"）":")"),CompactAction::Overview,{},0,false,ready);
    const double y=g.footer.bottom-25;
    text(dc,(zh?"共 ":"Total ")+std::to_string(page.total)+(zh?" 条 · 优先推荐 Top 3":" · Priority Top 3"),{g.cardsViewport.left+4,y,g.cardsViewport.right-220,y+22},10,secondary);
    text(dc,(page.pages?std::to_string(page.page+1):"0")+" / "+std::to_string(page.pages),{g.cardsViewport.right-150,y,g.cardsViewport.right-68,y+22},11,secondary,kCenterText);
    compactButton(dc,{g.cardsViewport.right-216,y,g.cardsViewport.right-152,y+22},zh?"上一页":"Prev",CompactAction::PagePrevious,{},0,false,ready&&page.page>0);
    compactButton(dc,{g.cardsViewport.right-64,y,g.cardsViewport.right,y+22},zh?"下一页":"Next",CompactAction::PageNext,{},0,false,ready&&page.page+1<page.pages);
}
void MainView::drawRecommendationOverview(CDrawContext* dc){
    if(!overviewOpen_)return;
    const auto g=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    const bool zh=state_.locale==session::Locale::ZhCN;
    fill(dc,g.viewport,CColor(15,17,22,245));compactHits_.clear();compactRibbons_.clear();seekHits_.clear();compactInCards_=false;
    overviewRect_={12,18,g.viewport.right-12,g.viewport.bottom-18};rounded(dc,overviewRect_,surface,blue,10);
    const auto& rows=browseRows();const double l=overviewRect_.left+12,r=overviewRect_.right-12,y=overviewRect_.top;
    text(dc,(zh?"推荐总览（":"Recommendation overview (")+std::to_string(rows.size())+(zh?"）":")"),{l,y+8,r-212,y+36},15);
    compactButton(dc,{r-204,y+8,r-68,y+38},zh?"复制完整推荐清单":"Copy full list",CompactAction::OverviewCopy);
    compactButton(dc,{r-60,y+8,r,y+38},zh?"关闭":"Close",CompactAction::OverviewClose);
    text(dc,zh?"当前分类全部合格结果 · 点击行定位卡片 · Esc 关闭":"All qualified results in this category · select a row · Esc closes",{l,y+42,r,y+64},10,secondary);
    const double height=g.narrow?60.:48.;overviewBody_={l,y+70,r,overviewRect_.bottom-30};
    overviewScrollMax_=std::max(0.,rows.size()*height-overviewBody_.height());overviewScroll_=std::clamp(overviewScroll_,0.,overviewScrollMax_);
    {
    ConcatClip clip(*dc,rect(overviewBody_));
    for(std::size_t i=0;i<rows.size();++i){const auto& row=rows[i];const double top=overviewBody_.top+i*height-overviewScroll_;
        if(top+height<overviewBody_.top||top>overviewBody_.bottom)continue;
        fill(dc,{l,top,r,top+height-2},i%2?raised:CColor(31,35,43,255));
        const auto label=std::to_string(i+1)+" · "+(row.exploratory?(zh?"探索":"Explore"):row.priority?(zh?"默认·优先":"Default·Top3"):(zh?"默认":"Default"));
        text(dc,label,{l+4,top+2,l+92,top+24},11,row.exploratory?CColor(235,193,125,255):blue);
        text(dc,browseName(row),{l+94,top+2,r-104,top+24},11);
        const auto score=browseCategory()==4?enrichments_.groups[row.group][row.index].score:recommendations_.groups[row.group][row.index].rankingScore;
        const auto intent=browseCategory()==4?(zh?std::string("升级"):std::string("Enrich")):tIntent(recommendations_.groups[row.group][row.index].intent);
        text(dc,intent+" · "+number(std::round(double(score)*10.)/10.),{r-104,top+2,r-4,top+24},10,secondary,kRightText);
        text(dc,browsePath(row),{l+8,top+23,r-8,top+height-3},11);
        auto hit=rect({l,top,r,top+height-2});hit.bound(rect(overviewBody_));if(hit.getHeight()>0)compactHits_.push_back({hit,CompactAction::OverviewSelect,{},i,true,{}});
    }}
    text(dc,zh?"探索：通过基础资格，实际用途证据不足。复制清单含来源、匹配位置和缺口。":"Explore: qualified with insufficient intent evidence. Copy includes source and alignment diagnostics.",
        {l,overviewRect_.bottom-27,r,overviewRect_.bottom-5},10,secondary);
    if(overviewScrollMax_>0){const double h=overviewBody_.height()*overviewBody_.height()/(overviewBody_.height()+overviewScrollMax_);
        const double top=overviewBody_.top+(overviewBody_.height()-h)*overviewScroll_/overviewScrollMax_;fill(dc,{r-3,top,r,top+h},secondary);}
}

void MainView::setAuditionState(bool custom,double bpm,bool suffix,bool valid,double effective){
    if(auditionCustom_==custom&&auditionBpm_==bpm&&auditionSuffix_==suffix&&
       auditionHostValid_==valid&&auditionEffectiveBpm_==effective)return;
    auditionCustom_=custom;auditionBpm_=bpm;auditionSuffix_=suffix;auditionHostValid_=valid;auditionEffectiveBpm_=effective;
    const auto g=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    invalidRect(editRect(rect(g.footer)));
}

void MainView::compactButton(CDrawContext* dc,UiRect r,const std::string& label,CompactAction action,
    std::string id,std::size_t index,bool selected,bool enabled,bool circular){
    const auto area=rect(r);
    const bool hover=area==compactHover_,pressed=area==compactPressed_;
    auto color=selected?CColor(48,61,91,255):hover?CColor(54,60,70,255):raised;
    if(pressed)color=CColor(25,31,44,255);
    if(circular){dc->setFillColor(color);dc->setFrameColor(selected?blue:edge);dc->setLineWidth(1);dc->drawEllipse(area,kDrawFilledAndStroked);}
    else rounded(dc,r,color,selected?blue:edge,6);
    text(dc,label,r,11,enabled?(selected?blue:fg):CColor(113,119,130,255),kCenterText);
    auto hit=area;
    if(compactInCards_){
        hit.bound(rect(computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport));
    }
    if(hit.getWidth()>0&&hit.getHeight()>0)compactHits_.push_back({area,action,std::move(id),index,enabled,
        compactInCards_?std::optional<CRect>(hit):std::nullopt});
}

void MainView::drawCompactRibbon(CDrawContext* dc,const preview::SourceRouteResult& route,UiRect area){
    if(!route){text(dc,(state_.locale==session::Locale::ZhCN?"来源路线不可用 · ":"Source route unavailable · ")+route.error,area,11,secondary);return;}
    struct Cell {std::string label;double qn;bool source{},suggested{};std::optional<std::size_t> sourceIndex;};
    std::vector<Cell> cells;double sourceQN{};bool unknown=false;
    const auto& candidate=route.candidate;
    const double anchor=route.imported.events.empty()?0:route.imported.events.front().startQN;
    for(std::size_t sourceIndex=0;sourceIndex<route.imported.events.size();++sourceIndex){const auto& e=route.imported.events[sourceIndex];
        const bool open=e.openEnded||!e.durationQN;
        const auto qn=open?candidate.suggestedCurrentChordDurationQN:e.durationQN;
        if(!qn||!std::isfinite(*qn)||*qn<=0){unknown=true;break;}
        const double start=e.startQN-anchor;
        if(start<sourceQN-1e-7){unknown=true;break;}
        if(start>sourceQN+1e-7)cells.push_back({"—",start-sourceQN,true,false,{}});
        sourceQN=start+*qn;cells.push_back({e.name,*qn,true,open,sourceIndex});
    }
    if(unknown){text(dc,state_.locale==session::Locale::ZhCN?"来源进行时值待定或重叠":"Source route duration pending or overlapping",area,11,secondary);return;}
    for(std::size_t i=0;i<candidate.continuation.size();++i){const auto& e=candidate.continuation[i];if(!std::isfinite(e.durationQN)||e.durationQN<=0){text(dc,"Duration pending",area,11,secondary);return;}cells.push_back({e.label,e.durationQN,false,false,candidate.continuationStart+i});}
    if(cells.empty())return;
    double total{},minQN=1e9;for(const auto& e:cells){total+=e.qn;minQN=std::min(minQN,e.qn);}
    if(total<=0||minQN<=0)return;
    double minimumWidth=0;
    for(const auto& e:cells){const double labelWidth=std::max(findEnrichment(candidate.id)?80.:52.,e.label.size()*7.+8.);minimumWidth=std::max(minimumWidth,total/e.qn*labelWidth);}
    const double width=std::max(area.width(),minimumWidth),maximum=width-area.width();
    auto& offset=compactRibbonScroll_[candidate.id];offset=std::clamp(offset,0.,maximum);
    auto ribbonHit=rect(area);if(compactInCards_)ribbonHit.bound(rect(computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport));
    if(ribbonHit.getWidth()>0&&ribbonHit.getHeight()>0)compactRibbons_.push_back({ribbonHit,candidate.id,maximum});
    const auto match=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return representativeMatch(candidate,m);});
    const bool degrees=!snapshotMode_&&match!=matches_.end()&&!match->templateLabels.empty()&&route.imported.events.size()==candidate.continuationStart;
    std::vector<EnrichmentProvenanceMark> provenance;
    if(const auto* enriched=findEnrichment(candidate.id))provenance=enrichmentProvenance(state_.imported.events,enriched->progression);
    auto seek=rect({area.left,area.bottom-9,area.right,area.bottom-3});
    if(compactInCards_)seek.bound(rect(computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport));
    if(seek.getWidth()>0&&seek.getHeight()>0)seekHits_.push_back({seek,candidate.id,area.left-offset,width,total,auditionSuffix_&&!findEnrichment(candidate.id)?sourceQN:0.});
    ConcatClip clip(*dc,rect(area));double x=area.left-offset,position{};
    for(const auto& e:cells){const double w=width*e.qn/total;
        const auto kind=e.sourceIndex&&*e.sourceIndex<provenance.size()?provenance[*e.sourceIndex].kind:EnrichmentProvenanceKind::Original;
        const bool inserted=kind==EnrichmentProvenanceKind::Inserted,replaced=kind==EnrichmentProvenanceKind::Replaced;
        fill(dc,{x,area.top,x+w-1,area.bottom-3},inserted?CColor(37,85,78,255):replaced?CColor(87,49,75,255):e.source?CColor(48,58,73,255):CColor(52,75,112,255));
        const auto badge=inserted?(state_.locale==session::Locale::ZhCN?"新增 ":"New "):replaced?(state_.locale==session::Locale::ZhCN?"替换 ":"Replaced "):"";
        text(dc,std::string(badge)+e.label,{x+3,area.top,x+w-3,area.top+19},detailedCards_?13:11,fg,kCenterText);
        const auto degree=degrees&&e.sourceIndex?" · "+match->templateLabels[*e.sourceIndex%match->templateLabels.size()]:std::string{};
        text(dc,(e.suggested?std::string("≈ "):std::string{})+number(e.qn)+" QN"+degree,{x+2,area.top+18,x+w-2,area.bottom-3},10,secondary,kCenterText);
        position+=e.qn;x+=w;
        if(std::abs(position-sourceQN)<1e-7)fill(dc,{x-1,area.top,x+1,area.bottom-3},blue);
    }
    if(previewDisplayQN(candidate.id)>=0){
        const double p=area.left-offset+width*std::clamp(previewDisplayQN(candidate.id)/total,0.,1.);fill(dc,{p-1,area.top,p+1,area.bottom-3},fg);
    }
    if(maximum>0){const double thumb=area.width()*area.width()/width;
        const double left=area.left+(area.width()-thumb)*offset/maximum;fill(dc,{left,area.bottom-2,left+thumb,area.bottom},secondary);}
}

void MainView::drawRecommendationAlignment(CDrawContext* dc,const ContinuationCandidate& c,UiRect area){
    const bool zh=state_.locale==session::Locale::ZhCN;
    text(dc,(zh?std::string("输入与匹配 · 上方播放来源，下方原输入 · "):std::string("Input alignment · source above / query below · "))+formatKey(c.key),{area.left,area.top,area.right,area.top+18},10,secondary);
    const auto found=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return representativeMatch(c,m);});
    const auto cells=found==matches_.end()?std::vector<RecommendationAlignmentCell>{}:recommendationAlignment(state_.imported,c,*found,snapshotMode_);
    if(cells.empty()){text(dc,zh?"原始输入匹配关系不可用":"Original query alignment unavailable",{area.left,area.top+22,area.right,area.bottom},11,secondary);return;}
    const double top=area.top+21,width=std::max(area.width(),cells.size()*100.);
    auto& offset=compactRibbonScroll_[c.id+"@source"];offset=std::clamp(offset,0.,width-area.width());
    auto hit=rect({area.left,top,area.right,area.bottom});hit.bound(rect(computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport));
    if(hit.getWidth()>0&&hit.getHeight()>0)compactRibbons_.push_back({hit,c.id+"@source",width-area.width()});
    ConcatClip clip(*dc,rect({area.left,top,area.right,area.bottom}));const double cellWidth=width/cells.size();std::size_t suffix{};
    const auto route=continuationRoute(c);
    for(std::size_t i=0;i<cells.size();++i){const auto& cell=cells[i];const double x=area.left-offset+i*cellWidth;
        const bool insert=cell.kind==AlignmentCellKind::InputInsertion,skip=cell.kind==AlignmentCellKind::TemplateSkip;
        const auto color=insert?CColor(92,70,38,255):skip?CColor(91,48,63,255):cell.kind==AlignmentCellKind::Continuation?CColor(52,75,112,255):CColor(48,58,73,255);
        fill(dc,{x,top,x+cellWidth-1,area.bottom-3},color);
        std::string chord="—",duration,degree;
        if(cell.queryIndex){const auto& e=state_.imported.events[*cell.queryIndex];chord=e.name;duration=e.openEnded||!e.durationQN?"OPEN":number(*e.durationQN)+" QN";}
        else if(cell.kind==AlignmentCellKind::Continuation){const auto& events=route?route.candidate.continuation:c.continuation;
            if(suffix<events.size()){chord=events[suffix].label;duration=number(events[suffix].durationQN)+" QN";}++suffix;}
        if(cell.templateIndex)degree=found->templateLabels[*cell.templateIndex];
        if(insert)degree=zh?"输入插入":"Input insertion";
        text(dc,chord,{x+3,top,x+cellWidth-3,top+21},12,fg,kCenterText);
        text(dc,duration,{x+3,top+21,x+cellWidth-3,top+36},10,secondary,kCenterText);
        text(dc,degree,{x+3,top+36,x+cellWidth-3,top+57},11,fg,kCenterText);
        if(skip)text(dc,zh?"模板跳过":"Template skip",{x+3,top+56,x+cellWidth-3,area.bottom-3},10,CColor(255,192,205,255),kCenterText);
        if(cell.continuationBoundary)fill(dc,{x-1,top,x+1,area.bottom-3},blue);
    }
}

void MainView::drawCompact(CDrawContext* dc){
    const bool zh=state_.locale==session::Locale::ZhCN;
    const auto g=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    compactHits_.clear();compactRibbons_.clear();seekHits_.clear();compactDetailRect_={};
    fill(dc,g.viewport,bg);fill(dc,g.header,surface);
    text(dc,"HarmonyContinuation",{16,8,g.header.right-228,35},g.narrow?13:16);
    compactButton(dc,{g.header.right-218,8,g.header.right-122,40},zh?(detailedCards_?"简略模式":"详细模式"):(detailedCards_?"Compact":"Detailed"),CompactAction::Density);
    compactButton(dc,{g.header.right-114,8,g.header.right-64,40},zh?"曲库":"Library",CompactAction::Library);
    compactButton(dc,{g.header.right-56,8,g.header.right-8,40},zh?"高级":"More",CompactAction::Advanced);
    rounded(dc,g.input,surface,edge,10);
    const double l=g.input.left+10,r=g.input.right-10,y=g.input.top;
    text(dc,zh?"当前输入":"INPUT",{l,y+4,l+85,y+25},11,secondary);
    compactButton(dc,{r-83,y+4,r,y+34},zh?"导入 MIDI":"Import MIDI",CompactAction::Import);
    std::string source;for(const auto& e:state_.imported.events){if(!source.empty())source+=" → ";source+=e.name;}
    if(source.empty())source=zh?"拖入和弦或导入 MIDI":"Drop chords or import MIDI";
    text(dc,source,{l,y+29,r,y+54},17);
    const auto inputArea=UiRect{l,y+58,r,y+94};
    // Unknown input durations are explicitly OPEN; the display does not assign them equal musical lengths.
    double total=0;for(const auto& e:state_.imported.events)if(e.durationQN&&!e.openEnded)total+=*e.durationQN;
    const bool open=std::any_of(state_.imported.events.begin(),state_.imported.events.end(),[](const auto& e){return e.openEnded||!e.durationQN;});
    double minDuration=1e9;for(const auto& e:state_.imported.events)if(e.durationQN&&!e.openEnded&&*e.durationQN>0)minDuration=std::min(minDuration,*e.durationQN);
    const double trackWidth=std::max(inputArea.width(),total>0?total/minDuration*64+(open?72.:0.):72.);
    const double maximum=trackWidth-inputArea.width();auto& inputOffset=compactRibbonScroll_["@input"];inputOffset=std::clamp(inputOffset,0.,maximum);compactRibbons_.push_back({rect(inputArea),"@input",maximum});
    const double available=trackWidth-(open?72.:0.);double x=inputArea.left-inputOffset;std::size_t inputIndex=0;
    {ConcatClip clip(*dc,rect(inputArea));for(const auto& e:state_.imported.events){
        const bool pending=e.openEnded||!e.durationQN;const double w=pending?72.:(total>0?available**e.durationQN/total:0.);
        const auto index=inputIndex++;if(w<=0)continue;if(state_.skeletonView&&std::find(analysis_.skeletonIndices.begin(),analysis_.skeletonIndices.end(),index)==analysis_.skeletonIndices.end()){x+=w;continue;}
        const CRect tile(x,inputArea.top,x+w-2,inputArea.bottom);auto hit=tile;hit.bound(rect(inputArea));
        if(hit.getWidth()>0)compactHits_.push_back({tile,CompactAction::InputChord,{},index,!workerBusy_&&recommendationResultReady_,hit});
        fill(dc,{x,inputArea.top,x+w-2,inputArea.bottom},compactHover_==tile?CColor(54,60,70,255):raised);
        text(dc,e.name,{x+3,inputArea.top,x+w-5,inputArea.top+19},12,fg,kCenterText);
        text(dc,pending?"OPEN":number(*e.durationQN)+" QN",{x+2,inputArea.top+18,x+w-4,inputArea.bottom},10,secondary,kCenterText);x+=w;
    }}
    const auto key=state_.forcedKey?formatKey(*state_.forcedKey):workerBusy_?(zh?std::string("分析中"):std::string("Analyzing")):recommendationResultReady_&&analysis_.selectedKey?formatKey(analysis_.selectedKey->key):"—";
    compactButton(dc,{l,y+100,l+170,y+130},(state_.forcedKey?(zh?"固定 ":"Fixed "):(zh?"Auto 首选 ":"Auto preferred "))+key,CompactAction::Key);
    compactButton(dc,{l+176,y+100,l+278,y+130},t("label.style")+": "+(state_.style?compactStyleName(*state_.style):t("label.auto")),CompactAction::Style);
    compactButton(dc,{r-96,y+100,r,y+130},state_.skeletonView?"SKELETON":"FULL",CompactAction::FullView);
    text(dc,state_.meterNumerator&&state_.meterDenominator?std::to_string(*state_.meterNumerator)+"/"+std::to_string(*state_.meterDenominator):"—",{l+284,y+100,r-100,y+130},11,secondary);
    text(dc,state_.productMode==session::ProductMode::Enrich?(zh?"升级进行":"ENRICHMENTS"):(zh?"推荐续写":"CONTINUATIONS"),g.recommendationHeading,14);
    constexpr PhraseIntent intents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};
    for(std::size_t i=0;i<4;++i)compactButton(dc,compactIntentTabGeometry(g.intentToolbar,i),tIntent(intents[i]),
        CompactAction::Intent,{},i,state_.productMode==session::ProductMode::Continue&&compactGroup_==i);
    compactButton(dc,compactIntentTabGeometry(g.intentToolbar,4),zh?"升级进行":"Enrich",CompactAction::Enrichment,{},0,
        state_.productMode==session::ProductMode::Enrich);
    if(state_.productMode==session::ProductMode::Enrich){
        drawCompactEnrichments(dc,g);compactInCards_=false;drawCompactTransport(dc);drawResponsiveForm(dc);drawRecommendationOverview(dc);return;
    }
    const auto& all=browseRows();const auto page=browsePage(all.size(),browsePages_[browseCategory()]);
    if(compactSelected_.empty()&&page.begin<page.end)compactSelected_=recommendations_.groups[compactGroup_][all[page.begin].index].id;
    const bool ready=!workerBusy_&&recommendationResultReady_;
    double cy=g.cardsViewport.top-compactScroll_,end=cy;
    {
    ConcatClip clip(*dc,rect(g.cardsViewport));
    compactInCards_=true;
    if(selectedChord_&&*selectedChord_<analysis_.full.size()){
        compactDetailRect_={g.cardsViewport.left,cy,g.cardsViewport.right,cy+250};layout_.inspector=compactDetailRect_;layout_.inspectorMode=InspectorMode::Overlay;
        drawResponsiveInspector(dc);cy+=258;end=cy;
    }
    if(!ready||all.empty())text(dc,workerBusy_?t("status.analyzing"):t("candidate.none"),
        {g.cardsViewport.left+8,cy+16,g.cardsViewport.right-8,cy+65},12,secondary);
    if(ready)for(std::size_t row=page.begin;row<page.end;++row){
        const auto index=all[row].index;const auto& c=recommendations_.groups[compactGroup_][index];
        const auto route=continuationRoute(c);
        auto cg=compactCandidateGeometry(g.cardsViewport,cy,detailedCards_);
        const auto aligned=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return representativeMatch(c,m);});
        const bool gaps=aligned!=matches_.end()&&std::any_of(aligned->alignmentTrace.begin(),aligned->alignmentTrace.end(),[](const auto& st){return st.operation==AlignmentOp::QueryInsertion||st.operation==AlignmentOp::TemplateDeletion;});
        const bool substitutionsPresent=aligned!=matches_.end()&&std::any_of(aligned->alignmentTrace.begin(),aligned->alignmentTrace.end(),[](const auto& st){return st.operation==AlignmentOp::Substitute;});
        const bool showAlignment=!snapshotMode_&&(gaps||(detailedCards_&&substitutionsPresent));
        if(detailedCards_)cg.row.bottom=cg.timeBand.bottom+128.;
        const double alignmentTop=detailedCards_?cg.row.bottom:cg.timeBand.bottom;
        if(showAlignment)cg.row.bottom=alignmentTop+116.;
        const bool expanded=selectedCandidate_&&selectedCandidate_->first==compactGroup_&&selectedCandidate_->second==index;
        const bool selected=compactSelected_==c.id;
        if(cg.row.bottom>=g.cardsViewport.top&&cg.row.top<=g.cardsViewport.bottom){
            rounded(dc,cg.row,selected?CColor(40,46,58,255):surface,selected?blue:edge,10);
            compactButton(dc,cg.select,selected?"✓":std::to_string(row+1),CompactAction::Select,c.id,index,selected,true,true);
            const auto match=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return representativeMatch(c,m);});
            const auto libraryItem=[&]() -> const ProgressionTemplate*{for(const auto* catalog:{&factory_,&user_})for(const auto& p:*catalog)if(p.id==c.primaryTemplate)return &p;return nullptr;}();
            const auto name=libraryItem?((zh&&!libraryItem->nameZh.empty())?libraryItem->nameZh:(!zh&&!libraryItem->nameEn.empty())?libraryItem->nameEn:libraryItem->name):match!=matches_.end()?match->templateName:c.primaryTemplate;
            text(dc,(all[row].priority?(zh?std::string("优先 · "):std::string("Top 3 · ")):c.exploratory?(zh?std::string("探索 · "):std::string("Explore · ")):std::string{})+name,cg.name,13);
            double qn=0;if(route)for(const auto& e:route.candidate.continuation)qn+=e.durationQN;
            const double bar=route&&route.meterDenominator>0?route.meterNumerator*4. / route.meterDenominator:0;
            text(dc,route?(zh?"续写 ":"Next ")+number(bar>0?qn/bar:qn)+(bar>0?(zh?" 小节 · ":" bars · "):" QN · ")+std::to_string(route.candidate.continuation.size())+(zh?" 和弦":" chords"):
                (zh?"来源不可用":"Source unavailable"),cg.facts,10,secondary,kRightText);
            const auto sourceCount=libraryItem?libraryItem->full.size():match!=matches_.end()?match->templateLabels.size():0;
            text(dc,(zh?"输入位置 ":"Input position ")+std::to_string(c.matchStart+1)+"–"+std::to_string(c.matchEnd+1)+"/"+(sourceCount?std::to_string(sourceCount):"?"),cg.location,10,secondary);
            if(sourceCount&&sourceCount<=24){const auto start=cg.location.left+(zh?114.:132.);const double unit=std::min(7.,(cg.location.right-start)/sourceCount);
                if(unit>=3)for(std::size_t i=0;i<sourceCount;++i)fill(dc,{start+i*unit,cg.location.top+6,start+i*unit+unit-2,cg.location.top+11},i>=c.matchStart&&i<=c.matchEnd?blue:edge);}
            std::size_t substitutions=0,gaps=0,insertions=0;
            if(match!=matches_.end())for(const auto& step:match->alignmentTrace){substitutions+=step.operation==AlignmentOp::Substitute;gaps+=step.operation==AlignmentOp::TemplateDeletion;insertions+=step.operation==AlignmentOp::QueryInsertion;}
            std::string difference;
            const auto append=[&](std::size_t count,const char* chinese,const char* english){if(!count)return;if(!difference.empty())difference+=" · ";difference+=std::to_string(count)+(zh?chinese:english);};
            append(substitutions,"处替代"," substitutions");append(insertions,"处输入插入"," input insertions");append(gaps,"处模板跳过"," template skips");
            if(difference.empty())difference=match==matches_.end()?(zh?"匹配明细待取":"Match details pending"):(zh?"逐项对齐":"Aligned");
            if(c.exploratory)difference=(zh?std::string("探索 · "):std::string("Explore · "))+difference;
            text(dc,difference,cg.match,10,secondary,kRightText);
            compactButton(dc,cg.audition,previewCandidateId_==c.id&&auditionState_==preview::PlaybackState::Paused?(zh?"▶ 继续":"Resume"):previewCandidateId_==c.id&&auditionState_==preview::PlaybackState::Playing?(zh?"■ 停止":"Stop"):zh?"▶ 试听":"▶ Play",CompactAction::Audition,c.id,index,false,
                !auditionPending_&&static_cast<bool>(actions_.audition)&&static_cast<bool>(route));
            compactButton(dc,cg.midi,"⠿ MIDI",CompactAction::Midi,c.id,index,false,
                static_cast<bool>(actions_.midiPayload)&&static_cast<bool>(route));
            compactButton(dc,cg.details,zh?(expanded?"收起 −":"详情 +"):(expanded?"Less −":"Details +"),CompactAction::Details,c.id,index,expanded);
            drawCompactRibbon(dc,route,cg.timeBand);
            if(detailedCards_)drawRelativeColorCurve(dc,route,c.key,{cg.timeBand.left,cg.timeBand.bottom+5,cg.timeBand.right,cg.timeBand.bottom+123});
            if(showAlignment)drawRecommendationAlignment(dc,c,{cg.timeBand.left,alignmentTop+4,cg.timeBand.right,cg.row.bottom-7});

        }
        cy=cg.row.bottom+8;
        if(expanded){compactDetailRect_={g.cardsViewport.left,cy,g.cardsViewport.right,cy+290};
            layout_.inspector=compactDetailRect_;layout_.inspectorMode=InspectorMode::Overlay;
            if(compactDetailRect_.bottom>=g.cardsViewport.top&&compactDetailRect_.top<g.cardsViewport.bottom)drawResponsiveInspector(dc);
            cy+=298;}
        end=cy;
    }
    compactInCards_=false;
    }
    compactScrollMax_=std::max(0.,end+compactScroll_-g.cardsViewport.bottom);
    compactScroll_=std::clamp(compactScroll_,0.,compactScrollMax_);
    if(compactScrollMax_>0){const double h=g.cardsViewport.height()*g.cardsViewport.height()/(g.cardsViewport.height()+compactScrollMax_);
        const double top=g.cardsViewport.top+(g.cardsViewport.height()-h)*compactScroll_/compactScrollMax_;
        fill(dc,{g.cardsViewport.right-3,top,g.cardsViewport.right,top+h},secondary);}
    drawCompactTransport(dc);
    drawResponsiveForm(dc);
    drawRecommendationOverview(dc);
}

void MainView::drawCompactEnrichments(CDrawContext* dc,const CompactLayoutResult& g){
    const bool zh=state_.locale==session::Locale::ZhCN;
    constexpr const char* titles[]{"group.polish","group.rich","group.advanced"};
    const bool ready=!workerBusy_&&recommendationResultReady_;
    double cy=g.cardsViewport.top-compactScroll_,end=cy;
    compactInCards_=true;
    {
    ConcatClip clip(*dc,rect(g.cardsViewport));
    const auto& rows=browseRows();const auto page=browsePage(rows.size(),browsePages_[4]);
    if(ready&&rows.empty())text(dc,t("status.noEnrichment"),{g.cardsViewport.left+8,cy+8,g.cardsViewport.right-8,cy+42},11,secondary);
    if(ready)for(std::size_t row=page.begin;row<page.end;++row){
            const auto group=rows[row].group,index=rows[row].index;
            const auto& candidates=enrichments_.groups[group];
            if(compactSelected_.empty())compactSelected_=candidates[index].id;
            const auto& candidate=candidates[index];
            auto cg=compactCandidateGeometry(g.cardsViewport,cy,detailedCards_);
            if(detailedCards_)cg.row.bottom=cg.timeBand.bottom+128.;
            const bool expanded=selectedEnrichment_&&selectedEnrichment_->first==group&&selectedEnrichment_->second==index;
            const bool selected=compactSelected_==candidate.id||expanded;
            if(cg.row.bottom>=g.cardsViewport.top&&cg.row.top<=g.cardsViewport.bottom){
                rounded(dc,cg.row,selected?CColor(40,46,58,255):surface,selected?blue:edge,10);
                compactButton(dc,cg.select,selected?"✓":std::to_string(row+1),CompactAction::Select,candidate.id,index,selected,true,true);
                std::string path;for(const auto& event:candidate.progression){if(!path.empty())path+=" → ";path+=event.name;}
                text(dc,(rows[row].priority?(zh?std::string("优先 · "):std::string("Top 3 · ")):std::string{})+path,cg.name,13);
                text(dc,(zh?"评分 ":"Score ")+number(std::round(double(candidate.score)*10.)/10.)+" · "+std::to_string(candidate.progression.size())+
                    (zh?" 和弦":" chords"),cg.facts,10,secondary,kRightText);
                text(dc,t(titles[group])+" · "+(zh?"已保留骨架 ":"skeleton ")+number(candidate.skeletonPreservation),cg.location,10,secondary);
                std::string badges;for(const auto technique:candidate.techniques){if(!badges.empty())badges+=" · ";badges+=t("technique."+std::string(enrichment::techniqueName(technique)));}
                const auto hint=candidateHint(candidate);
                if(!hint.label.empty())drawColorHint(dc,hint,rect(cg.match),rect(cg.row));else text(dc,badges,cg.match,10,secondary,kRightText);
                compactButton(dc,cg.audition,previewCandidateId_==candidate.id&&auditionState_==preview::PlaybackState::Paused?(zh?"▶ 继续":"Resume"):previewCandidateId_==candidate.id&&auditionState_==preview::PlaybackState::Playing?(zh?"■ 停止":"Stop"):zh?"▶ 试听":"▶ Play",CompactAction::Audition,candidate.id,index,false,
                    !auditionPending_&&static_cast<bool>(actions_.auditionEnrichment));
                compactButton(dc,cg.midi,"⠿ MIDI",CompactAction::Midi,candidate.id,index,false,
                    static_cast<bool>(actions_.enrichmentMidiPayload));
                compactButton(dc,cg.details,zh?(expanded?"收起 −":"详情 +"):(expanded?"Less −":"Details +"),
                    CompactAction::Details,candidate.id,index,expanded);
                preview::SourceRouteResult route;route.candidate.id=candidate.id;
                if(!route.imported.replace(candidate.progression,TimelineCoordinateMode::RelativeToSelection))
                    route.error="invalid enrichment timeline";
                else {
                    const auto realized=preview::buildSequence(route.imported,nullptr,preview::defaultTempoBPM);
                    if(!realized)route.error=realized.error;
                    else if(realized.sequence.events.size()!=route.imported.events.size())route.error="enrichment timeline mismatch";
                    else for(std::size_t i=0;i<route.imported.events.size();++i){
                        route.imported.events[i].durationQN=realized.sequence.events[i].durationQN;
                        route.imported.events[i].openEnded=false;
                    }
                }
                drawCompactRibbon(dc,route,cg.timeBand);
                if(detailedCards_)drawRelativeColorCurve(dc,route,state_.forcedKey?state_.forcedKey:analysis_.selectedKey?std::optional<KeySignature>{analysis_.selectedKey->key}:std::nullopt,
                    {cg.timeBand.left,cg.timeBand.bottom+5,cg.timeBand.right,cg.row.bottom-5});
            }
            cy=cg.row.bottom+8;
            if(expanded){
                compactDetailRect_={g.cardsViewport.left,cy,g.cardsViewport.right,cy+290};
                layout_.inspector=compactDetailRect_;layout_.inspectorMode=InspectorMode::Overlay;
                if(compactDetailRect_.bottom>=g.cardsViewport.top&&compactDetailRect_.top<g.cardsViewport.bottom)drawResponsiveInspector(dc);
                cy+=298;
            }
            end=cy;
    }
    compactInCards_=false;
    }
    compactScrollMax_=std::max(0.,end+compactScroll_-g.cardsViewport.bottom);
    compactScroll_=std::clamp(compactScroll_,0.,compactScrollMax_);
    if(compactScrollMax_>0){const double h=g.cardsViewport.height()*g.cardsViewport.height()/(g.cardsViewport.height()+compactScrollMax_);
        const double top=g.cardsViewport.top+(g.cardsViewport.height()-h)*compactScroll_/compactScrollMax_;
        fill(dc,{g.cardsViewport.right-3,top,g.cardsViewport.right,top+h},secondary);}
}

void MainView::drawCompactTransport(CDrawContext* dc){
    const bool zh=state_.locale==session::Locale::ZhCN;const auto g=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    const bool ready=!workerBusy_&&recommendationResultReady_;
    fill(dc,g.footer,surface);const double fy=g.footer.top+8,fl=g.footer.left+12,fr=g.footer.right-12;
    const auto* selectedContinuation=findCandidate(compactSelected_);
    compactButton(dc,{fl,fy,fl+48,fy+36},auditionState_==preview::PlaybackState::Paused?"▶ 继续":"▶",CompactAction::Play,{},0,false,ready&&!auditionPending_&&(state_.productMode==session::ProductMode::Enrich?
        (findEnrichment(compactSelected_)&&static_cast<bool>(actions_.auditionEnrichment)):
        (selectedContinuation&&static_cast<bool>(actions_.audition)&&static_cast<bool>(continuationRoute(*selectedContinuation)))));
    compactButton(dc,{fl+54,fy,fl+94,fy+36},"Ⅱ",CompactAction::Pause,{},0,false,auditionState_==preview::PlaybackState::Playing&&!auditionPending_&&static_cast<bool>(actions_.pauseAudition));
    compactButton(dc,{fl+100,fy,fl+140,fy+36},"■",CompactAction::Stop,{},0,false,static_cast<bool>(actions_.stopAudition));
    const auto status=auditionPending_?(zh?"音频处理中":"Audio pending"):auditionState_==preview::PlaybackState::Playing?(zh?"正在试听":"Playing"):auditionState_==preview::PlaybackState::Paused?(zh?"已暂停":"Paused"):(zh?"已停止":"Stopped");
    text(dc,std::string(status)+" · "+(auditionHostBackend_?"Host Audio":"Windows Audio"),{fl+152,fy,fr-132,fy+19},11,secondary);
    compactButton(dc,{fr-128,fy,fr,fy+32},auditionHostBackend_?"Host Audio":"Windows Audio",CompactAction::Backend,{},0,auditionHostBackend_,!auditionPending_&&static_cast<bool>(actions_.previewBackend));
    const auto objectId=!previewCandidateId_.empty()?previewCandidateId_:
        state_.productMode==session::ProductMode::Enrich&&selectedEnrichment()?selectedEnrichment()->id:compactSelected_;
    std::string object="—";
    if(const auto* candidate=findCandidate(objectId))object=(zh?"当前对象 · ":"Current · ")+candidate->primaryTemplate;
    else if(const auto* candidate=findEnrichment(objectId))object=(zh?"升级进行 · ":"Enrichment · ")+candidate->id;
    text(dc,object+(!actionStatus_.empty()?" · "+actionStatus_:""),{fl+152,fy+18,fr-132,fy+39},11);
    const double by=fy+44;
    compactButton(dc,{fl,by,fl+98,by+30},state_.productMode==session::ProductMode::Enrich?(zh?"完整升级进行":"Full enrichment"):auditionSuffix_?(zh?"仅后续":"Suffix only"):(zh?"完整进行":"Full phrase"),CompactAction::Scope,{},0,auditionSuffix_,state_.productMode!=session::ProductMode::Enrich);
    compactButton(dc,{fl+106,by,fl+220,by+30},auditionCustom_?(zh?"自定义 BPM":"Custom BPM"):(zh?"跟随宿主":"Follow host"),CompactAction::TempoMode);
    compactButton(dc,{fl+228,by,fl+310,by+30},number(auditionCustom_?auditionBpm_:auditionEffectiveBpm_)+" BPM",CompactAction::Tempo);
    text(dc,auditionCustom_?(zh?"仅影响试听":"Audition only"):auditionHostValid_?(zh?"宿主速度有效":"Host tempo valid"):(zh?"宿主不可用 · 回退 ":"Host unavailable · fallback ")+number(auditionEffectiveBpm_),
        {fl+318,by,fr,by+30},10,secondary);
    if(state_.tab==session::Tab::Recommend)drawBrowseControls(dc,g);

}

bool MainView::onCompactMouseDown(CPoint& where){
    if(form_!=Form::None)return false;
    if(!overviewOpen_&&state_.productMode==session::ProductMode::Enrich&&compactDetailRect_.contains(where.x,where.y))return false;
    for(const auto& h:compactHits_)if(h.contains(where)){
        if(!h.enabled)return true;
        compactPressed_=h.rect;
        const auto id=h.id;const auto index=h.index;const auto action=h.action;
        const bool zh=state_.locale==session::Locale::ZhCN;
        const auto* c=findCandidate(id);
        const auto enrichmentRef=[&]()->std::optional<std::pair<std::size_t,std::size_t>>{
            for(std::size_t group=0;group<enrichments_.groups.size();++group)
                for(std::size_t i=0;i<enrichments_.groups[group].size();++i)
                    if(enrichments_.groups[group][i].id==id)return std::pair{group,i};
            return std::nullopt;
        };
        const auto* enriched=findEnrichment(id);
        const auto settings=[&]{if(actions_.previewSettings)actions_.previewSettings(auditionCustom_,auditionBpm_,auditionSuffix_);};
        switch(action){
        case CompactAction::Density:detailedCards_=!detailedCards_;compactScroll_=0;break;
        case CompactAction::Select:
            seekGesture_.reset();if(!previewCandidateId_.empty()&&previewCandidateId_!=id&&actions_.stopAudition)actions_.stopAudition();
            if(state_.productMode==session::ProductMode::Enrich){if(enriched)compactSelected_=id;}
            else if(c)compactSelected_=id;
            break;
        case CompactAction::Audition:
            if(previewCandidateId_==id&&auditionState_==preview::PlaybackState::Paused&&actions_.resumeAudition){actions_.resumeAudition();break;}
            if(state_.productMode==session::ProductMode::Enrich&&enriched&&recommendationResultReady_&&!workerBusy_&&actions_.auditionEnrichment){
                compactSelected_=id;actions_.auditionEnrichment(*enriched);
            } else if(c&&recommendationResultReady_&&!workerBusy_&&actions_.audition&&continuationRoute(*c)){
                compactSelected_=id;actions_.audition(*c);
            }
            break;
        case CompactAction::Midi:
            if(state_.productMode==session::ProductMode::Enrich&&enriched&&recommendationResultReady_&&!workerBusy_)armMidi(*enriched,where);
            else if(c&&recommendationResultReady_&&!workerBusy_&&continuationRoute(*c))armMidi(*c,where);
            break;
        case CompactAction::Details:
            if(state_.productMode==session::ProductMode::Enrich&&enrichmentRef()){
                if(selectedEnrichment_==enrichmentRef())selectedEnrichment_.reset();
                else{selectedEnrichment_=enrichmentRef();selectedChord_.reset();selectedCandidate_.reset();inspectorScroll_=0;}
            } else if(c){if(selectedCandidate_&&selectedCandidate_->first==compactGroup_&&selectedCandidate_->second==index)selectedCandidate_.reset();
                else{selectedCandidate_={{compactGroup_,index}};selectedChord_.reset();selectedEnrichment_.reset();inspectorScroll_=0;}}
            break;
        case CompactAction::Intent:changeBrowseCategory(index);break;
        case CompactAction::Enrichment:changeBrowseCategory(4);break;
        case CompactAction::Overview:overviewOpen_=true;overviewScroll_=0;midiPending_.reset();midiClick_={};break;
        case CompactAction::OverviewClose:overviewOpen_=false;break;
        case CompactAction::OverviewCopy:CClipboard::setString(recommendationListText().c_str());break;
        case CompactAction::OverviewSelect:{
            focusBrowseEntry(index);break;}
        case CompactAction::PagePrevious:case CompactAction::PageNext:{
            auto& page=browsePages_[browseCategory()];if(action==CompactAction::PagePrevious){if(page)--page;}else ++page;
            page=browsePage(browseRows().size(),page).page;compactScroll_=0;
            selectedCandidate_.reset();selectedEnrichment_.reset();selectedChord_.reset();midiPending_.reset();midiClick_={};break;}
        case CompactAction::Pause:if(actions_.pauseAudition)actions_.pauseAudition();break;
        case CompactAction::Backend:if(actions_.previewBackend)actions_.previewBackend(!auditionHostBackend_);break;
        case CompactAction::Play:if(auditionState_==preview::PlaybackState::Playing&&previewCandidateId_==compactSelected_)break;if(auditionState_==preview::PlaybackState::Paused&&previewCandidateId_==compactSelected_&&actions_.resumeAudition){actions_.resumeAudition();break;}if(!workerBusy_&&recommendationResultReady_){if(state_.productMode==session::ProductMode::Enrich){if(const auto* selected=findEnrichment(compactSelected_);selected&&actions_.auditionEnrichment)actions_.auditionEnrichment(*selected);}else if(const auto* selected=findCandidate(compactSelected_);selected&&actions_.audition&&continuationRoute(*selected))actions_.audition(*selected);}break;
        case CompactAction::Stop:if(actions_.stopAudition)actions_.stopAudition();actionStatus_=zh?"试听已停止":"Audition stopped";break;
        case CompactAction::TempoMode:auditionCustom_=!auditionCustom_;settings();break;
        case CompactAction::Scope:auditionSuffix_=!auditionSuffix_;settings();break;
        case CompactAction::Tempo:beginForm(Form::Tempo,number(auditionCustom_?auditionBpm_:auditionEffectiveBpm_));break;
        case CompactAction::FullView:state_.skeletonView=!state_.skeletonView;notifyState();break;
        case CompactAction::InputChord:if(index<analysis_.full.size()){selectedChord_=index;selectedCandidate_.reset();selectedEnrichment_.reset();inspectorScroll_=0;compactScroll_=0;}break;
        case CompactAction::Import:if(actions_.importMidi){const auto n=popup({t("midi.complete"),t("midi.open")},where);if(n>=0)actions_.importMidi(n==1,{});}break;
        case CompactAction::Key:{std::vector<std::string> keys{t("label.auto")};for(int i=0;i<12;++i){keys.push_back(formatKey({static_cast<PitchClass>(i),Mode::Major}));keys.push_back(formatKey({static_cast<PitchClass>(i),Mode::Minor}));}
            const auto n=popup(keys,where);if(n>=0){state_.forcedKey=n?std::optional<KeySignature>({static_cast<PitchClass>((n-1)/2),(n-1)%2?Mode::Minor:Mode::Major}):std::nullopt;notifyState();}break;}
        case CompactAction::Style:{const auto n=popup({t("label.auto"),"Pop","Rock","R&B","Jazz","City Pop / J-pop",t("tag.functional")},where);constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
            if(n>=0){state_.style=n?std::optional<Style>(styles[n-1]):std::nullopt;notifyState();}break;}
        case CompactAction::Library:state_.tab=session::Tab::Library;selectedCandidate_.reset();notifyState();break;
        case CompactAction::Advanced:{
            // Existing advanced handlers retain actual curve/library/diagnostic operations.
            const auto n=popup({zh?"色彩偏好":"Color preference",zh?"旋律约束":"Melody constraint",zh?"和声倾向":"Harmonic tendency",zh?"固定 / 取消固定当前候选":"Pin / unpin selected",zh?"查看固定候选":"Pinned candidates",zh?"设置 / 诊断 / About":"Settings / Diagnostics / About"},where);
            if(n==3){if(const auto* selected=findCandidate(compactSelected_)){if(state_.unpin(selected->id))std::erase_if(pinnedSnapshots_,[&](const auto& item){return item.id==selected->id;});else if(state_.pin(selected->id,session::continuationFingerprint(*selected)))pinnedSnapshots_.push_back(*selected);notifyState();}}
            else if(n==4){std::vector<std::string> names;for(const auto& pin:pinnedSnapshots_)names.push_back(pin.primaryTemplate+" · "+formatKey(pin.key));const auto picked=popup(names,where);if(picked>=0&&static_cast<std::size_t>(picked)<pinnedSnapshots_.size()){const auto target=pinnedSnapshots_[picked].id;refreshBrowse();bool found=false;
                for(std::size_t group=0;group<4;++group)for(std::size_t rank=0;rank<browseRows_[group].size();++rank){const auto& row=browseRows_[group][rank];if(recommendations_.groups[group][row.index].id==target){changeBrowseCategory(group);browsePages_[group]=rank/recommendationPageSize;compactSelected_=target;compactScroll_=0;found=true;}}
                if(!found)actionStatus_=zh?"固定候选不在本次合格结果中":"Pinned candidate is not qualified in the current result";}}
            else if(n>=0){const auto control=n==0?layout_.constraintControls[3]:n==1?layout_.constraintControls[0]:n==2?layout_.constraintControls[2]:layout_.topControls[3];CPoint oldPoint{(control.left+control.right)/2,(control.top+control.bottom)/2};onMouseDownResponsive(oldPoint);}break;}
        }
        invalid();return true;
    }
    if(overviewOpen_){if(!overviewRect_.contains(where.x,where.y)){overviewOpen_=false;invalid();}return true;}
    if(state_.tab!=session::Tab::Recommend)return false;
    if(state_.productMode==session::ProductMode::Enrich){
        if(compactDetailRect_.contains(where.x,where.y))return false;
        return true;
    }
    if(compactDetailRect_.contains(where.x,where.y)&&computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport.contains(where.x,where.y))return false;
    // Only the explicit controls act; text and card background are not implicit buttons.
    return true;
}

bool MainView::runBrowseSmoke(CDrawContext* dc,const std::function<void(const std::string&)>& capture){
    if(!dc||workerBusy_||!recommendationResultReady_)return false;
    const auto savedUi=captureEditorUiState();const auto savedState=state_;const auto savedRec=recommendations_;
    const auto savedMatches=matches_;const auto savedActions=actions_;const auto savedSelection=compactSelected_;
    const auto savedSnapshot=snapshotMode_;const auto savedGeneration=generation_;
    bool okay=true;
    const auto require=[&](bool value,const char* stage){if(!value)throw std::runtime_error(stage);};
    std::size_t paintCount{};double worstPaintMs{},totalPaintMs{};
    const auto paint=[&]{const auto started=std::chrono::steady_clock::now();dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();++paintCount;totalPaintMs+=ms;worstPaintMs=std::max(worstPaintMs,ms);};
    const auto click=[&](CompactAction action,std::optional<std::size_t> rank={}){
        const auto it=std::find_if(compactHits_.begin(),compactHits_.end(),[&](const auto& h){return h.action==action&&h.enabled&&(!rank||h.index==*rank);});
        require(it!=compactHits_.end(),"browser control missing");const auto hit=*it;
        CPoint p{(hit.rect.left+hit.rect.right)/2*userZoom(),(hit.rect.top+hit.rect.bottom)/2*userZoom()};
        onMouseDown(p,CButtonState{kLButton});onMouseUp(p,CButtonState{kLButton});
    };
    try {
        actions_.stateChanged={};state_.uiZoomPercent=100;state_.tab=session::Tab::Recommend;
        state_.productMode=session::ProductMode::Continue;compactGroup_=0;detailedCards_=false;compactScroll_=0;
        snapshotMode_=false;selectedCandidate_.reset();selectedEnrichment_.reset();selectedChord_.reset();browsePages_.fill(0);browseDirty_=true;
        const ContinuationCandidate* seed{};for(const auto& g:savedRec.groups)if(!g.empty()){seed=&g.front();break;}
        require(seed,"browser test needs a real realized source");
        for(const auto count:{0u,1u,3u,4u,10u,11u,20u,21u}){
            RecommendationSet fixture;
            // Presentation-only fixtures exercise identities and boundaries. They do not enter the matcher.
            for(std::size_t i=0;i<count;++i){auto c=*seed;c.id="browser-fixture-"+std::to_string(i);c.intent=PhraseIntent::Resolve;c.exploratory=i%4==3;fixture.groups[0].push_back(c);}
            setRecommendations(fixture);compactSelected_.clear();compactScroll_=0;browsePages_[0]=0;
            const auto& rows=browseRows();require(rows.size()==count,"browser lost qualified candidates");
            std::vector<std::string> visited;
            const auto pageCount=browsePage(count,0).pages;
            for(std::size_t page=0;page<pageCount;++page){
                require(browsePages_[0]==page,"next page state");paint();const auto range=browsePage(count,page);
                for(auto i=range.begin;i<range.end;++i)visited.push_back(recommendations_.groups[0][rows[i].index].id);
                if(page+1<pageCount){click(CompactAction::PageNext);paint();}
            }
            std::sort(visited.begin(),visited.end());require(visited.size()==count&&std::adjacent_find(visited.begin(),visited.end())==visited.end(),"pages duplicate or omit candidates");
            paint();click(CompactAction::Overview);paint();require(overviewOpen_,"overview opens");
            const auto list=recommendationListText();std::size_t position{};
            for(const auto& row:rows){const auto id="id="+recommendations_.groups[row.group][row.index].id+" |";
                const auto found=list.find(id,position);require(found!=std::string::npos,"overview copy misses or reorders a card");position=found+id.size();}
            if(count){click(CompactAction::OverviewSelect,0);paint();require(!overviewOpen_&&browsePages_[0]==0,"overview locates correct page");}
            else {click(CompactAction::OverviewClose);paint();}
        }
        // Loading Session intent and replacing results must restore a valid category/object.
        auto restoredState=state_;restoredState.intent=PhraseIntent::Color;setSessionState(restoredState);
        require(compactGroup_==3,"Session restores the saved browsing category");
        compactGroup_=0;browseSelected_[0]="removed-candidate";compactSelected_="removed-candidate";
        setRecommendations({});paint();changeBrowseCategory(1);changeBrowseCategory(0);paint();
        require(compactSelected_.empty()&&browseSelected_[0].empty(),"removed selection cannot survive a refresh");
        // Real data: overview, category state and page actions all resolve the original candidate.
        setRecommendations(savedRec);matches_=savedMatches;compactSelected_.clear();browsePages_.fill(0);compactScroll_=0;
        std::size_t selectedGroup{};for(std::size_t g=0;g<4;++g)if(savedRec.groups[g].size()>savedRec.groups[selectedGroup].size())selectedGroup=g;
        compactGroup_=selectedGroup;paint();const auto count=browseRows().size();require(count>0,"actual list empty");
        std::string auditionId,midiId;actions_.audition=[&](const auto& c){auditionId=c.id;};
        actions_.midiPayload=[&](const auto& c){midiId=c.id;const auto route=continuationRoute(c);if(!route)return midi::PayloadResult{{},route.error};
            midi::CandidateExportContext context{route.imported,120,{route.meterNumerator,route.meterDenominator}};return midi::candidatePayload(context,route.candidate);};
        actions_.saveMidiPayload=[](const auto&){return std::string("browser smoke");};paint();
        click(CompactAction::Select);const auto expected=compactSelected_;
        click(CompactAction::Audition);click(CompactAction::Midi);
        require(auditionId==expected&&midiId==expected,"selected audition/MIDI identity");
        click(CompactAction::Details);paint();require(selectedCandidate()&&selectedCandidate()->id==expected,"detail identity");
        click(CompactAction::Details);paint();
        if(count>recommendationPageSize){
            click(CompactAction::PageNext);paint();click(CompactAction::Select);const auto nextId=compactSelected_;
            click(CompactAction::Audition);click(CompactAction::Midi);click(CompactAction::Details);paint();
            require(nextId!=expected&&auditionId==nextId&&midiId==nextId&&selectedCandidate()&&selectedCandidate()->id==nextId,"second-page action identity");
            click(CompactAction::PagePrevious);paint();require(!selectedCandidate_,"paging closes the old detail object");
            focusBrowseEntry(0);paint();
        }
        click(CompactAction::Overview);paint();
        if(capture)capture("recommendation-overview.png");
        KeyboardEvent escape;escape.type=EventType::KeyDown;escape.virt=VirtualKey::Escape;onKeyboardEvent(escape);
        require(escape.consumed&&!overviewOpen_,"Escape closes overview");paint();
        if(capture)capture("recommendation-list.png");
        const auto before=captureEditorUiState();changeBrowseCategory(4);paint();changeBrowseCategory(selectedGroup);paint();
        require(compactSelected_==expected&&browsePages_[selectedGroup]==before.browsePages[selectedGroup],"category returns to selected object/page");
        const auto restore=captureEditorUiState();restoreEditorUiState(restore);paint();require(compactSelected_==expected,"editor reopen browse identity");
        bool capturedInsertion=false,capturedSkip=false;
        for(std::size_t g=0;g<4;++g)for(std::size_t rank=0;rank<browseRows_[g].size();++rank){const auto& row=browseRows_[g][rank];const auto& c=recommendations_.groups[g][row.index];
            const auto m=std::find_if(matches_.begin(),matches_.end(),[&](const auto& value){return representativeMatch(c,value);});
            if(m==matches_.end())continue;
            const auto cells=recommendationAlignment(state_.imported,c,*m);
            const auto insertion=std::any_of(cells.begin(),cells.end(),[](const auto& cell){return cell.kind==AlignmentCellKind::InputInsertion;});
            if(insertion&&capture&&!capturedInsertion){compactGroup_=g;detailedCards_=true;focusBrowseEntry(rank);paint();capture("R01-input-insertion.png");capturedInsertion=true;}
            const auto skip=std::any_of(cells.begin(),cells.end(),[](const auto& cell){return cell.kind==AlignmentCellKind::TemplateSkip;});
            if(skip&&capture&&!capturedSkip){compactGroup_=g;detailedCards_=true;focusBrowseEntry(rank);paint();capture("template-skip.png");capturedSkip=true;}
        }
        if(capture){
            changeBrowseCategory(4);refreshBrowse();
            for(std::size_t rank=0;rank<browseRows_[4].size();++rank){const auto row=browseRows_[4][rank];const auto& enriched=enrichments_.groups[row.group][row.index];
                const auto marks=enrichmentProvenance(state_.imported.events,enriched.progression);
                if(std::any_of(marks.begin(),marks.end(),[](const auto& mark){return mark.kind==EnrichmentProvenanceKind::Inserted;})){
                    focusBrowseEntry(rank);detailedCards_=false;paint();capture("dev15-enrichment-inserted.png");
                    detailedCards_=true;focusBrowseEntry(rank);paint();capture("dev15-enrichment-color-curve.png");break;
                }
            }
        }
        overviewOpen_=true;setWorkerStatus(savedGeneration+1,0,true,factoryCount_,userCount_,false);require(!overviewOpen_&&!midiPending_,"new work invalidates modal and MIDI gesture");
    } catch(const std::exception& e){std::fprintf(stderr,"Browse smoke: %s\n",e.what());okay=false;}
    actions_=savedActions;state_=savedState;matches_=savedMatches;setRecommendations(savedRec);snapshotMode_=savedSnapshot;
    generation_=savedGeneration;workerBusy_=false;recommendationResultReady_=true;restoreEditorUiState(savedUi);compactSelected_=savedSelection;
    overviewOpen_=false;paint();std::printf("Browse UI paint calls=%zu averageMs=%.3f maximumMs=%.3f\n",paintCount,totalPaintMs/std::max<std::size_t>(1,paintCount),worstPaintMs);return okay;
}

bool MainView::runCompactSmoke(CDrawContext* dc){
    if(!dc||workerBusy_||!recommendationResultReady_)return false;
    const auto savedActions=actions_;const auto savedState=state_;const auto savedUi=captureEditorUiState();
    const auto oldSize=getViewSize();const auto savedGroup=compactGroup_;const auto savedSelected=compactSelected_;
    const auto savedStatus=actionStatus_;const auto oldGeneration=generation_;
    const bool savedSnapshotMode=snapshotMode_;
    std::size_t auditions=0,stops=0,settings=0,payloads=0,saves=0,enrichmentAuditions=0,enrichmentPayloads=0;
    std::string auditionId,enrichmentId;bool routeProjection=false;
    bool okay=true;
    actions_.stopAudition=[&]{++stops;};actions_.previewSettings=[&](bool,double,bool){++settings;};
    midi::CandidateExportContext exportContext;exportContext.imported=state_.imported;
    exportContext.tempo=120;
    actions_.audition=[&](const auto& candidate){
        const auto route=continuationRoute(candidate);if(!route)return;
        const auto sequence=preview::buildSequence(route.imported,&route.candidate,120.0);
        if(sequence){routeProjection=true;++auditions;auditionId=candidate.id;}
    };
    actions_.midiPayload=[&](const auto& candidate){
        const auto route=continuationRoute(candidate);if(!route)return midi::PayloadResult{{},route.error};
        auto context=exportContext;context.imported=route.imported;context.meter={route.meterNumerator,route.meterDenominator};
        auto result=midi::candidatePayload(context,route.candidate);payloads+=static_cast<bool>(result);return result;
    };
    actions_.auditionEnrichment=[&](const auto& candidate){++enrichmentAuditions;enrichmentId=candidate.id;};
    actions_.enrichmentMidiPayload=[&](const auto& candidate){
        auto result=midi::candidatePayload(exportContext,candidate);enrichmentPayloads+=static_cast<bool>(result);return result;
    };
    actions_.saveMidiPayload=[&](const auto& payload){saves+=!payload.smfBytes.empty();return std::string("Saved (test receiver)");};
    actions_.stateChanged={};
    const auto paint=[&]{dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();};
    const auto get=[&](CompactAction action,std::optional<std::size_t> index={})->std::optional<CompactHit>{for(const auto& h:compactHits_)if(h.action==action&&h.enabled&&(!index||h.index==*index))return h;return std::nullopt;};
    const auto click=[&](CompactAction action,std::optional<std::size_t> index={}){const auto h=get(action,index);if(!h){okay=false;return;}
        CPoint point{(h->rect.left+h->rect.right)/2*userZoom(),(h->rect.top+h->rect.bottom)/2*userZoom()};
        onMouseDown(point,CButtonState{kLButton});onMouseUp(point,CButtonState{kLButton});};
    const auto checkpoint=[&](const char* stage){if(!okay)throw std::runtime_error(stage);};
    try{
        state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Continue;browsePages_.fill(0);
        const auto presentation=continuationPresentation();bool found=false;
        for(std::size_t group=0;group<4;++group)if(!presentation[group].empty()){compactGroup_=group;found=true;break;}
        const auto continuationGroup=compactGroup_;
        okay=okay&&found;compactSelected_.clear();selectedCandidate_.reset();detailedCards_=false;compactScroll_=0;
        for(const auto zoom:{100u,125u,150u}){
            state_.uiZoomPercent=zoom;resizeLayout(800,780);paint();
            const auto g=computeCompactLayout(800,780,userZoom());
            okay=okay&&g.cardsViewport.bottom<=g.footer.top&&g.footer.bottom<=780/userZoom()+.1;
            std::size_t circles=0;
            for(const auto& h:compactHits_)if(h.action==CompactAction::Select){
                ++circles;okay=okay&&std::abs(h.rect.getWidth()-h.rect.getHeight())<.01&&h.rect.getWidth()==28;
                okay=okay&&!h.contains({h.rect.left+.1,h.rect.top+.1});}
            std::size_t tabs=0;for(const auto& h:compactHits_)tabs+=h.action==CompactAction::Intent||h.action==CompactAction::Enrichment;
            okay=okay&&tabs==5&&circles>0&&get(CompactAction::Play).has_value()&&get(CompactAction::Density).has_value();
        }
        checkpoint("five tabs and default cards across zoom levels");
        state_.uiZoomPercent=100;resizeLayout(800,780);paint();
        click(CompactAction::InputChord);paint();okay=okay&&selectedChord_.has_value()&&compactDetailRect_.height()>0;
        {CPoint close{compactDetailRect_.left+26,compactDetailRect_.top+20};onMouseDown(close,CButtonState{kLButton});onMouseUp(close,CButtonState{kLButton});}
        paint();okay=okay&&!selectedChord_;
        checkpoint("input inspector");
        state_.uiZoomPercent=150;resizeLayout(800,640);paint();
        const auto small=computeCompactLayout(800,640,userZoom());
        for(const auto& band:compactRibbons_)if(band.id!="@input")okay=okay&&band.rect.bottom<=small.cardsViewport.bottom&&band.rect.top>=small.cardsViewport.top;
        click(CompactAction::Enrichment);paint();okay=okay&&state_.productMode==session::ProductMode::Enrich&&
            layout_.content.bottom<=small.footer.top&&get(CompactAction::Stop).has_value();
        state_.uiZoomPercent=100;resizeLayout(800,780);paint();
        if(get(CompactAction::Select)){
            click(CompactAction::Select);paint();okay=okay&&findEnrichment(compactSelected_)!=nullptr;
            const auto expected=compactSelected_;
            click(CompactAction::Audition);okay=okay&&enrichmentAuditions==1&&enrichmentId==expected;
            click(CompactAction::Midi);okay=okay&&enrichmentPayloads==1&&saves==1;
            click(CompactAction::Details);paint();okay=okay&&selectedEnrichment_&&compactDetailRect_.height()>0;
            click(CompactAction::Details);paint();okay=okay&&!selectedEnrichment_;
            saves=0;
        }
        checkpoint("enrichment select, audition, MIDI and details");
        click(CompactAction::Intent,continuationGroup);paint();okay=okay&&state_.productMode==session::ProductMode::Continue;
        state_.tab=session::Tab::Library;paint();okay=okay&&layout_.content.bottom<=computeCompactLayout(800,780).footer.top&&get(CompactAction::Stop).has_value();
        state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Continue;state_.uiZoomPercent=100;resizeLayout(800,780);paint();
        checkpoint("return from enrichment and library");
        const auto expectedId=compactSelected_;
        click(CompactAction::Play);okay=okay&&auditions==1&&auditionId==expectedId&&routeProjection;
        checkpoint("continuation audition");
        std::size_t pauses=0,resumes=0,seeks=0;double soughtQN=-1;
        const auto* playingCandidate=findCandidate(expectedId);const auto playingRoute=continuationRoute(*playingCandidate);
        const auto playingSequence=preview::buildSequence(playingRoute.imported,&playingRoute.candidate,120.);
        const auto identityBeforeSeek=session::continuationFingerprint(*playingCandidate);
        actions_.pauseAudition=[&]{++pauses;setPreviewTransport(preview::PlaybackState::Paused,false,false);};
        actions_.resumeAudition=[&]{++resumes;setPreviewTransport(preview::PlaybackState::Playing,false,false);};
        actions_.seekAudition=[&](const std::string& id,double qn){if(id==expectedId){++seeks;soughtQN=qn;}};
        setPreviewPosition(expectedId,1.,playingSequence.sequence.totalQN);setPreviewTransport(preview::PlaybackState::Playing,false,false);paint();
        click(CompactAction::Pause);paint();okay=okay&&pauses==1&&auditionState_==preview::PlaybackState::Paused;
        click(CompactAction::Play);paint();okay=okay&&resumes==1&&auditions==1;
        const auto seek=std::find_if(seekHits_.begin(),seekHits_.end(),[&](const auto& h){return h.id==expectedId;});
        okay=okay&&seek!=seekHits_.end();
        if(seek!=seekHits_.end()){
            const auto hit=*seek;CPoint point{(hit.rect.left+hit.rect.right)*.5,hit.rect.bottom-1};
            onMouseDown(point,CButtonState{kLButton});okay=okay&&seeks==0&&seekGesture_.has_value();
            point.x=hit.rect.right;onMouseMoved(point,CButtonState{kLButton});okay=okay&&seeks==0;
            const auto expectedQN=std::clamp((point.x-hit.originX)/hit.width*hit.totalQN,hit.minimumQN,hit.totalQN);
            onMouseUp(point,CButtonState{kLButton});okay=okay&&seeks==1&&std::abs(soughtQN-expectedQN)<1e-7&&!seekGesture_;
        }
        okay=okay&&session::continuationFingerprint(*findCandidate(expectedId))==identityBeforeSeek;
        checkpoint("pause resume and one seek committed on release without changing candidate identity");
        setPreviewPosition({},0,0);setPreviewTransport(preview::PlaybackState::Stopped,false,false);

        const auto previousStops=stops;click(CompactAction::Stop);okay=okay&&stops==previousStops+1;
        click(CompactAction::Scope);click(CompactAction::TempoMode);okay=okay&&settings==2;
        checkpoint("continuation preview settings");
        click(CompactAction::Midi);okay=okay&&payloads==1&&saves==1;
        checkpoint("continuation MIDI");
        click(CompactAction::Details);paint();okay=okay&&selectedCandidate_.has_value()&&compactDetailRect_.height()>0;
        click(CompactAction::Details);paint();okay=okay&&!selectedCandidate_;
        checkpoint("continuation detail toggles");
        click(CompactAction::Density);paint();okay=okay&&detailedCards_&&captureEditorUiState().detailedCards;
        okay=okay&&!seekHits_.empty();
        checkpoint("continuation transport, MIDI and details");
        const auto access=session::continuationAccess(recommendations_.groups[1]);
        for(const auto i:access.more)if(recommendations_.groups[1][i].exploratory){
            compactGroup_=1;compactSelected_=recommendations_.groups[1][i].id;selectedCandidate_.reset();compactScroll_=0;
            const auto& rows=browseRows();const auto chosen=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.index==i;});
            if(chosen==rows.end()){okay=false;break;}browsePages_[1]=(chosen-rows.begin())/recommendationPageSize;paint();
            okay=okay&&get(CompactAction::Overview).has_value()&&get(CompactAction::Midi).has_value();break;}
        const auto stale=get(CompactAction::Midi);const auto count=payloads;
        setWorkerStatus(generation_+1,0,true,factoryCount_,userCount_,false);paint();
        if(stale){CPoint point{(stale->rect.left+stale->rect.right)/2,(stale->rect.top+stale->rect.bottom)/2};onMouseDown(point,CButtonState{kLButton});}
        okay=okay&&payloads==count&&!get(CompactAction::Midi)&&!get(CompactAction::Play);
    }catch(const std::exception& error){std::fprintf(stderr,"Compact smoke: %s\n",error.what());okay=false;}
    catch(...){okay=false;}
    actions_=savedActions;state_=savedState;restoreEditorUiState(savedUi);compactGroup_=savedGroup;compactSelected_=savedSelected;
    snapshotMode_=savedSnapshotMode;
    generation_=oldGeneration;workerBusy_=false;recommendationResultReady_=true;auditionCustom_=false;auditionSuffix_=false;actionStatus_=savedStatus;
    detailedCards_=false;compactScroll_=0;selectedCandidate_.reset();resizeLayout(static_cast<int>(oldSize.getWidth()),static_cast<int>(oldSize.getHeight()));
    paint();return okay;
}
} // namespace harmony::ui
