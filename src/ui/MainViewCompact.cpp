#include "MainView.h"
#include "product/ProductVersion.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgraphicstransform.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

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

void MainView::drawCompactRibbon(CDrawContext* dc,const ContinuationCandidate& c,UiRect area,bool detailed){
    struct Cell {std::string label;double qn;bool input{},suggested{};};
    std::vector<Cell> cells;double inputQN{};bool unknown=false;
    const double anchor=state_.imported.events.empty()?0:state_.imported.events.front().startQN;
    for(const auto& e:state_.imported.events){
        const bool open=e.openEnded||!e.durationQN;
        const auto qn=open?c.suggestedCurrentChordDurationQN:e.durationQN;
        if(!qn||!std::isfinite(*qn)||*qn<=0){unknown=true;break;}
        const double start=e.startQN-anchor;
        if(start<inputQN-1e-7){unknown=true;break;}
        if(detailed&&start>inputQN+1e-7)cells.push_back({"—",start-inputQN,true,false});
        inputQN=start+*qn;if(detailed)cells.push_back({e.name,*qn,true,open});
    }
    if(unknown){text(dc,state_.locale==session::Locale::ZhCN?"输入时值待定或重叠 · 展开查看":"Input duration pending or overlapping · see details",area,11,secondary);return;}
    if(!detailed)cells.push_back({state_.locale==session::Locale::ZhCN?"输入":"Input",inputQN,true,
        std::any_of(state_.imported.events.begin(),state_.imported.events.end(),[](const auto& e){return e.openEnded||!e.durationQN;})});
    for(const auto& e:c.continuation){if(!std::isfinite(e.durationQN)||e.durationQN<=0){text(dc,"Duration pending",area,11,secondary);return;}cells.push_back({e.label,e.durationQN,false,false});}
    if(cells.empty())return;
    double total{},minQN=1e9;for(const auto& e:cells){total+=e.qn;minQN=std::min(minQN,e.qn);}
    if(total<=0||minQN<=0)return;
    double minimumWidth=0;
    for(const auto& e:cells){const double labelWidth=std::max(52.,e.label.size()*7.+8.);minimumWidth=std::max(minimumWidth,total/e.qn*labelWidth);}
    const double width=std::max(area.width(),minimumWidth),maximum=width-area.width();
    auto& offset=compactRibbonScroll_[c.id];offset=std::clamp(offset,0.,maximum);
    auto ribbonHit=rect(area);if(compactInCards_)ribbonHit.bound(rect(computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport));
    if(ribbonHit.getWidth()>0&&ribbonHit.getHeight()>0)compactRibbons_.push_back({ribbonHit,c.id,maximum});
    ConcatClip clip(*dc,rect(area));double x=area.left-offset,position{};
    for(const auto& e:cells){const double w=width*e.qn/total;
        fill(dc,{x,area.top,x+w-1,area.bottom-3},e.input?CColor(48,58,73,255):CColor(52,75,112,255));
        text(dc,e.label,{x+3,area.top,x+w-3,area.top+19},13,fg,kCenterText);
        text(dc,(e.suggested?std::string("≈ "):std::string{})+number(e.qn),{x+2,area.top+18,x+w-2,area.bottom-3},10,secondary,kCenterText);
        position+=e.qn;x+=w;
        if(std::abs(position-inputQN)<1e-7)fill(dc,{x-1,area.top,x+1,area.bottom-3},blue);
    }
    if(previewCandidateId_==c.id&&previewTotalQN_>0){
        const double qn=previewQN_; // Controller reports the position in the full phrase for either scope.
        const double p=area.left-offset+width*std::clamp(qn/total,0.,1.);fill(dc,{p-1,area.top,p+1,area.bottom-3},fg);
    }
    if(maximum>0){const double thumb=area.width()*area.width()/width;
        const double left=area.left+(area.width()-thumb)*offset/maximum;fill(dc,{left,area.bottom-2,left+thumb,area.bottom},secondary);}
}

void MainView::drawCompact(CDrawContext* dc){
    const bool zh=state_.locale==session::Locale::ZhCN;
    const auto g=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    compactHits_.clear();compactRibbons_.clear();compactDetailRect_={};
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
    if(state_.productMode==session::ProductMode::Enrich){
        text(dc,zh?"升级进行 · 润色 / 丰富 / 高级":"Enrichment · Polish / Rich / Advanced",g.recommendationHeading,14);
        drawEnrichments(dc,rect(g.viewport));drawResponsiveInspector(dc);drawCompactTransport(dc);drawResponsiveForm(dc);return;
    }
    text(dc,zh?"推荐续写":"CONTINUATIONS",g.recommendationHeading,14);
    std::size_t moreCount=0,exploreCount=0;
    for(const auto& group:recommendations_.groups){const auto access=session::continuationAccess(group);moreCount+=access.more.size();exploreCount+=access.exploratory;}
    text(dc,zh?"时值 QN · 长轨道可滚轮横移":"QN · scroll long tracks",{g.recommendationHeading.left+110,g.recommendationHeading.top,g.recommendationHeading.right-154,g.recommendationHeading.bottom},10,secondary);
    compactButton(dc,{g.recommendationHeading.right-148,g.recommendationHeading.top,g.recommendationHeading.right,g.recommendationHeading.bottom},
        (zh?"更多 ":"More ")+std::to_string(moreCount)+(exploreCount?(zh?" · 探索 ":" · Explore ")+std::to_string(exploreCount):""),CompactAction::More,{},0,false,moreCount>0&&!workerBusy_&&recommendationResultReady_);
    const double intentW=std::min(90.,g.intentToolbar.width()/4);
    constexpr PhraseIntent intents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};
    for(std::size_t i=0;i<4;++i)compactButton(dc,{g.intentToolbar.left+i*intentW,g.intentToolbar.top,g.intentToolbar.left+(i+1)*intentW-4,g.intentToolbar.bottom},tIntent(intents[i]),CompactAction::Intent,{},i,compactGroup_==i);
    auto presentation=continuationPresentation();auto shown=presentation[compactGroup_];
    if(!compactExtra_.empty()){const auto& group=recommendations_.groups[compactGroup_];
        const auto it=std::find_if(group.begin(),group.end(),[&](const auto& c){return c.id==compactExtra_;});
        if(it!=group.end()){const auto i=static_cast<std::size_t>(it-group.begin());if(std::find(shown.begin(),shown.end(),i)==shown.end())shown.insert(shown.begin(),i);}}
    if(compactSelected_.empty()&&!shown.empty())compactSelected_=recommendations_.groups[compactGroup_][shown.front()].id;
    const bool ready=!workerBusy_&&recommendationResultReady_;
    double cy=g.cardsViewport.top-compactScroll_,end=cy;
    {
    ConcatClip clip(*dc,rect(g.cardsViewport));
    compactInCards_=true;
    if(selectedChord_&&*selectedChord_<analysis_.full.size()){
        compactDetailRect_={g.cardsViewport.left,cy,g.cardsViewport.right,cy+250};layout_.inspector=compactDetailRect_;layout_.inspectorMode=InspectorMode::Overlay;
        drawResponsiveInspector(dc);cy+=258;end=cy;
    }
    if(!ready||shown.empty())text(dc,workerBusy_?t("status.analyzing"):
        exploreCount?(zh?"暂无默认推荐，可从“更多 / 探索”查看近似路线":"No default result. Open More / Explore."):t("candidate.none"),
        {g.cardsViewport.left+8,cy+16,g.cardsViewport.right-8,cy+65},12,secondary);
    if(ready)for(std::size_t row=0;row<shown.size();++row){
        const auto index=shown[row];const auto& c=recommendations_.groups[compactGroup_][index];
        const auto cg=compactCandidateGeometry(g.cardsViewport,cy,detailedCards_);
        const bool expanded=selectedCandidate_&&selectedCandidate_->first==compactGroup_&&selectedCandidate_->second==index;
        const bool selected=compactSelected_==c.id;
        if(cg.row.bottom>=g.cardsViewport.top&&cg.row.top<=g.cardsViewport.bottom){
            rounded(dc,cg.row,selected?CColor(40,46,58,255):surface,selected?blue:edge,10);
            compactButton(dc,cg.select,selected?"✓":std::to_string(row+1),CompactAction::Select,c.id,index,selected,true,true);
            const auto match=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return representativeMatch(c,m);});
            const auto libraryItem=[&]() -> const ProgressionTemplate*{for(const auto* catalog:{&factory_,&user_})for(const auto& p:*catalog)if(p.id==c.primaryTemplate)return &p;return nullptr;}();
            const auto name=libraryItem?((zh&&!libraryItem->nameZh.empty())?libraryItem->nameZh:(!zh&&!libraryItem->nameEn.empty())?libraryItem->nameEn:libraryItem->name):match!=matches_.end()?match->templateName:c.primaryTemplate;
            text(dc,name,cg.name,14);
            double qn=0;for(const auto& e:c.continuation)qn+=e.durationQN;
            const double bar=state_.meterNumerator&&state_.meterDenominator&&*state_.meterDenominator>0?*state_.meterNumerator*4. / *state_.meterDenominator:0;
            text(dc,(zh?"续写 ":"Next ")+number(bar>0?qn/bar:qn)+(bar>0?(zh?" 小节 · ":" bars · "):" QN · ")+std::to_string(c.continuation.size())+(zh?" 和弦":" chords"),cg.facts,10,secondary,kRightText);
            const auto sourceCount=libraryItem?libraryItem->full.size():match!=matches_.end()?match->templateLabels.size():0;
            text(dc,(zh?"输入位置 ":"Input position ")+std::to_string(c.matchStart+1)+"–"+std::to_string(c.matchEnd+1)+"/"+(sourceCount?std::to_string(sourceCount):"?"),cg.location,10,secondary);
            if(sourceCount&&sourceCount<=24){const auto start=cg.location.left+(zh?114.:132.);const double unit=std::min(7.,(cg.location.right-start)/sourceCount);
                if(unit>=3)for(std::size_t i=0;i<sourceCount;++i)fill(dc,{start+i*unit,cg.location.top+6,start+i*unit+unit-2,cg.location.top+11},i>=c.matchStart&&i<=c.matchEnd?blue:edge);}
            std::size_t substitutions=0,gaps=0,insertions=0;
            if(match!=matches_.end())for(const auto& step:match->alignmentTrace){substitutions+=step.operation==AlignmentOp::Substitute;gaps+=step.operation==AlignmentOp::TemplateDeletion;insertions+=step.operation==AlignmentOp::QueryInsertion;}
            std::string difference;
            const auto append=[&](std::size_t count,const char* chinese,const char* english){if(!count)return;if(!difference.empty())difference+=" · ";difference+=std::to_string(count)+(zh?chinese:english);};
            append(substitutions,"处替代"," substitutions");append(insertions,"处输入插入"," input insertions");append(gaps,"个来源未覆盖"," source gaps");
            if(difference.empty())difference=match==matches_.end()?(zh?"匹配明细待取":"Match details pending"):(zh?"逐项对齐":"Aligned");
            if(c.exploratory)difference=(zh?std::string("探索 · "):std::string("Explore · "))+difference;
            text(dc,difference,cg.match,10,secondary,kRightText);
            compactButton(dc,cg.audition,zh?"▶ 试听":"▶ Play",CompactAction::Audition,c.id,index,false,static_cast<bool>(actions_.audition));
            compactButton(dc,cg.midi,"⠿ MIDI",CompactAction::Midi,c.id,index,false,static_cast<bool>(actions_.midiPayload));
            compactButton(dc,cg.details,zh?(expanded?"收起 −":"详情 +"):(expanded?"Less −":"Details +"),CompactAction::Details,c.id,index,expanded);
            drawCompactRibbon(dc,c,cg.timeBand,detailedCards_);
            if(detailedCards_){
                text(dc,(zh?"来源定位 · 实际匹配调性 ":"Source position · actual match key ")+formatKey(c.key)+" · "+difference,cg.sourceLocation,11,secondary);
                if(match!=matches_.end()&&!match->templateLabels.empty()){
                    const auto a=cg.inputTimes;const auto& labels=match->templateLabels;
                    const double width=std::max(a.width(),labels.size()*58.);auto& offset=compactRibbonScroll_[c.id+"@source"];
                    offset=std::clamp(offset,0.,width-a.width());auto sourceHit=rect(a);sourceHit.bound(rect(g.cardsViewport));
                    if(sourceHit.getWidth()>0&&sourceHit.getHeight()>0)compactRibbons_.push_back({sourceHit,c.id+"@source",width-a.width()});
                    ConcatClip sourceClip(*dc,rect(a));const double cell=width/labels.size();
                    for(std::size_t i=0;i<labels.size();++i){
                        bool matched=false,gap=false;
                        for(const auto& step:match->alignmentTrace)if(step.templateIndex&&*step.templateIndex==i){matched|=step.queryIndex.has_value();gap|=step.operation==AlignmentOp::TemplateDeletion;}
                        const double x=a.left-offset+i*cell;
                        fill(dc,{x,a.top,x+cell-1,a.bottom-3},matched?CColor(66,82,111,255):raised);
                        text(dc,labels[i],{x+2,a.top,x+cell-3,a.top+18},11,matched?fg:secondary,kCenterText);
                        text(dc,std::to_string(i+1)+(gap?(zh?" 未覆盖":" gap"):matched?(zh?" 输入":" input"):""),{x+2,a.top+18,x+cell-3,a.bottom-3},9,secondary,kCenterText);
                    }
                }
            }
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
}

void MainView::drawCompactTransport(CDrawContext* dc){
    const bool zh=state_.locale==session::Locale::ZhCN;const auto g=computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom());
    const bool ready=!workerBusy_&&recommendationResultReady_;
    fill(dc,g.footer,surface);const double fy=g.footer.top+8,fl=g.footer.left+12,fr=g.footer.right-12;
    compactButton(dc,{fl,fy,fl+48,fy+36},"▶",CompactAction::Play,{},0,false,ready&&(state_.productMode==session::ProductMode::Enrich?(selectedEnrichment()&&static_cast<bool>(actions_.auditionEnrichment)):(findCandidate(compactSelected_)&&static_cast<bool>(actions_.audition))));
    compactButton(dc,{fl+54,fy,fl+94,fy+36},"■",CompactAction::Stop,{},0,false,static_cast<bool>(actions_.stopAudition));
    text(dc,previewCandidateId_.empty()?(zh?"已停止 · Windows 设备试听":"Stopped · Windows device audition"):(zh?"正在试听 · Windows 音频设备":"Playing · Windows audio device"),
        {fl+108,fy,fr,fy+19},11,secondary);
    const auto objectId=!previewCandidateId_.empty()?previewCandidateId_:
        state_.productMode==session::ProductMode::Enrich&&selectedEnrichment()?selectedEnrichment()->id:compactSelected_;
    std::string object="—";
    if(const auto* candidate=findCandidate(objectId))object=(zh?"当前对象 · ":"Current · ")+candidate->primaryTemplate;
    else if(const auto* candidate=findEnrichment(objectId))object=(zh?"升级进行 · ":"Enrichment · ")+candidate->id;
    text(dc,object+(!actionStatus_.empty()?" · "+actionStatus_:""),{fl+108,fy+18,fr,fy+39},11);
    const double by=fy+44;
    compactButton(dc,{fl,by,fl+98,by+30},state_.productMode==session::ProductMode::Enrich?(zh?"完整升级进行":"Full enrichment"):auditionSuffix_?(zh?"仅后续":"Suffix only"):(zh?"完整进行":"Full phrase"),CompactAction::Scope,{},0,auditionSuffix_,state_.productMode!=session::ProductMode::Enrich);
    compactButton(dc,{fl+106,by,fl+220,by+30},auditionCustom_?(zh?"自定义 BPM":"Custom BPM"):(zh?"跟随宿主":"Follow host"),CompactAction::TempoMode);
    compactButton(dc,{fl+228,by,fl+310,by+30},number(auditionCustom_?auditionBpm_:auditionEffectiveBpm_)+" BPM",CompactAction::Tempo);
    text(dc,auditionCustom_?(zh?"仅影响试听":"Audition only"):auditionHostValid_?(zh?"宿主速度有效":"Host tempo valid"):(zh?"宿主不可用 · 回退 ":"Host unavailable · fallback ")+number(auditionEffectiveBpm_),
        {fl+318,by,fr,by+30},10,secondary);
    if(g.narrow)text(dc,zh?"试听范围与 MIDI 导出独立 · 改速后重播":"Audition and MIDI are independent · tempo restarts playback",{fl,by+35,fr,by+57},10,secondary);
}

bool MainView::onCompactMouseDown(CPoint& where){
    if(form_!=Form::None)return false;
    for(const auto& h:compactHits_)if(h.contains(where)){
        if(!h.enabled)return true;
        compactPressed_=h.rect;
        const auto id=h.id;const auto index=h.index;const auto action=h.action;
        const bool zh=state_.locale==session::Locale::ZhCN;
        const auto* c=findCandidate(id);
        const auto settings=[&]{if(actions_.previewSettings)actions_.previewSettings(auditionCustom_,auditionBpm_,auditionSuffix_);};
        switch(action){
        case CompactAction::Density:detailedCards_=!detailedCards_;compactScroll_=0;break;
        case CompactAction::Select:if(c)compactSelected_=id;break;
        case CompactAction::Audition:if(c&&recommendationResultReady_&&!workerBusy_&&actions_.audition){compactSelected_=id;actions_.audition(*c);}break;
        case CompactAction::Midi:if(c&&recommendationResultReady_&&!workerBusy_)armMidi(*c,where);break;
        case CompactAction::Details:if(c){if(selectedCandidate_&&selectedCandidate_->first==compactGroup_&&selectedCandidate_->second==index)selectedCandidate_.reset();
            else{selectedCandidate_={{compactGroup_,index}};selectedChord_.reset();selectedEnrichment_.reset();inspectorScroll_=0;}}break;
        case CompactAction::Intent:compactGroup_=index;compactExtra_.clear();compactSelected_.clear();selectedCandidate_.reset();compactScroll_=0;
            {constexpr PhraseIntent intents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};state_.intent=intents[index];notifyState();}break;
        case CompactAction::More:{
            std::vector<std::pair<std::size_t,std::size_t>> refs;std::vector<std::string> labels;
            for(std::size_t group=0;group<4;++group){const auto access=session::continuationAccess(recommendations_.groups[group]);
                for(const auto i:access.more){const auto& candidate=recommendations_.groups[group][i];std::string path;
                    for(const auto& e:candidate.continuation){if(!path.empty())path+=" → ";path+=e.label;}
                    refs.emplace_back(group,i);labels.push_back((candidate.exploratory?(zh?std::string("探索 · "):std::string("Explore · ")):tIntent(candidate.intent)+" · ")+path);}}
            const auto revision=state_.imported.revision,generation=generation_;const auto choice=popup(labels,where);
            if(choice>=0&&static_cast<std::size_t>(choice)<refs.size()&&revision==state_.imported.revision&&generation==generation_&&!workerBusy_&&recommendationResultReady_){
                const auto [group,i]=refs[choice];compactGroup_=group;compactExtra_=recommendations_.groups[group][i].id;compactSelected_=compactExtra_;compactScroll_=0;selectedCandidate_.reset();}break;}
        case CompactAction::Play:if(!workerBusy_&&recommendationResultReady_){if(state_.productMode==session::ProductMode::Enrich){if(const auto* selected=selectedEnrichment();selected&&actions_.auditionEnrichment)actions_.auditionEnrichment(*selected);}else if(const auto* selected=findCandidate(compactSelected_);selected&&actions_.audition)actions_.audition(*selected);}break;
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
            const auto n=popup({state_.productMode==session::ProductMode::Continue?(zh?"升级进行 / Enrichment":"Enrichment"):(zh?"返回续写":"Back to continuation"),zh?"色彩偏好":"Color preference",zh?"旋律约束":"Melody constraint",zh?"和声倾向":"Harmonic tendency",zh?"固定 / 取消固定当前候选":"Pin / unpin selected",zh?"查看固定候选":"Pinned candidates",zh?"设置 / 诊断 / About":"Settings / Diagnostics / About"},where);
            if(n==0){state_.productMode=state_.productMode==session::ProductMode::Continue?session::ProductMode::Enrich:session::ProductMode::Continue;selectedCandidate_.reset();selectedEnrichment_.reset();notifyState();}
            else if(n==4){if(const auto* selected=findCandidate(compactSelected_)){if(state_.unpin(selected->id))std::erase_if(pinnedSnapshots_,[&](const auto& c){return c.id==selected->id;});else if(state_.pin(selected->id,session::continuationFingerprint(*selected)))pinnedSnapshots_.push_back(*selected);notifyState();}}
            else if(n==5){std::vector<std::string> names;for(const auto& pin:pinnedSnapshots_)names.push_back(pin.primaryTemplate+" · "+formatKey(pin.key));const auto picked=popup(names,where);if(picked>=0&&static_cast<std::size_t>(picked)<pinnedSnapshots_.size()){compactExtra_=pinnedSnapshots_[picked].id;compactSelected_=compactExtra_;for(std::size_t group=0;group<4;++group)for(const auto& c:recommendations_.groups[group])if(c.id==compactExtra_)compactGroup_=group;compactScroll_=0;}}
            else if(n>=1){const auto control=n==1?layout_.constraintControls[3]:n==2?layout_.constraintControls[0]:n==3?layout_.constraintControls[2]:layout_.topControls[3];CPoint oldPoint{(control.left+control.right)/2,(control.top+control.bottom)/2};onMouseDownResponsive(oldPoint);}break;}
        }
        invalid();return true;
    }
    if(state_.tab!=session::Tab::Recommend)return false;
    if(state_.productMode!=session::ProductMode::Continue)return where.y<computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport.top;
    if(compactDetailRect_.contains(where.x,where.y)&&computeCompactLayout(getViewSize().getWidth(),getViewSize().getHeight(),userZoom()).cardsViewport.contains(where.x,where.y))return false;
    // Only the explicit controls act; text and card background are not implicit buttons.
    return true;
}

bool MainView::runCompactSmoke(CDrawContext* dc){
    if(!dc||workerBusy_||!recommendationResultReady_)return false;
    const auto savedActions=actions_;const auto savedState=state_;const auto savedUi=captureEditorUiState();
    const auto oldSize=getViewSize();const auto savedGroup=compactGroup_;const auto savedSelected=compactSelected_;
    const auto savedExtra=compactExtra_,savedStatus=actionStatus_;const auto oldGeneration=generation_;
    std::size_t auditions=0,stops=0,settings=0,payloads=0,saves=0;std::string auditionId;
    actions_.audition=[&](const auto& c){++auditions;auditionId=c.id;};
    actions_.stopAudition=[&]{++stops;};actions_.previewSettings=[&](bool,double,bool){++settings;};
    midi::CandidateExportContext exportContext;exportContext.imported=state_.imported;
    exportContext.tempo=120;
    actions_.midiPayload=[&](const auto& candidate){++payloads;return midi::candidatePayload(exportContext,candidate);};
    actions_.saveMidiPayload=[&](const auto& payload){saves+=!payload.smfBytes.empty();return std::string("Saved (test receiver)");};
    actions_.stateChanged={};bool okay=true;
    const auto paint=[&]{dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();};
    const auto get=[&](CompactAction action)->std::optional<CompactHit>{for(const auto& h:compactHits_)if(h.action==action&&h.enabled)return h;return std::nullopt;};
    const auto click=[&](CompactAction action){const auto h=get(action);if(!h){okay=false;return;}
        CPoint point{(h->rect.left+h->rect.right)/2*userZoom(),(h->rect.top+h->rect.bottom)/2*userZoom()};
        onMouseDown(point,CButtonState{kLButton});onMouseUp(point,CButtonState{kLButton});};
    try{
        state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Continue;
        const auto presentation=continuationPresentation();bool found=false;
        for(std::size_t group=0;group<4;++group)if(!presentation[group].empty()){compactGroup_=group;found=true;break;}
        okay=okay&&found;compactSelected_.clear();selectedCandidate_.reset();compactExtra_.clear();detailedCards_=false;compactScroll_=0;
        for(const auto zoom:{100u,125u,150u}){
            state_.uiZoomPercent=zoom;resizeLayout(800,780);paint();
            const auto g=computeCompactLayout(800,780,userZoom());
            okay=okay&&g.cardsViewport.bottom<=g.footer.top&&g.footer.bottom<=780/userZoom()+.1;
            std::size_t circles=0;
            for(const auto& h:compactHits_)if(h.action==CompactAction::Select){
                ++circles;okay=okay&&std::abs(h.rect.getWidth()-h.rect.getHeight())<.01&&h.rect.getWidth()==28;
                okay=okay&&!h.contains({h.rect.left+.1,h.rect.top+.1});}
            okay=okay&&circles>0&&get(CompactAction::Play).has_value()&&get(CompactAction::Density).has_value();
        }
        state_.uiZoomPercent=100;resizeLayout(800,780);paint();
        click(CompactAction::InputChord);paint();okay=okay&&selectedChord_.has_value()&&compactDetailRect_.height()>0;
        {CPoint close{compactDetailRect_.left+26,compactDetailRect_.top+20};onMouseDown(close,CButtonState{kLButton});onMouseUp(close,CButtonState{kLButton});}
        paint();okay=okay&&!selectedChord_;
        state_.uiZoomPercent=150;resizeLayout(800,640);paint();
        const auto small=computeCompactLayout(800,640,userZoom());
        for(const auto& band:compactRibbons_)if(band.id!="@input")okay=okay&&band.rect.bottom<=small.cardsViewport.bottom&&band.rect.top>=small.cardsViewport.top;
        state_.productMode=session::ProductMode::Enrich;paint();okay=okay&&layout_.content.bottom<=small.footer.top&&get(CompactAction::Stop).has_value();
        state_.tab=session::Tab::Library;paint();okay=okay&&layout_.content.bottom<=small.footer.top&&get(CompactAction::Stop).has_value();
        state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Continue;state_.uiZoomPercent=100;resizeLayout(800,780);paint();
        const auto expectedId=compactSelected_;
        click(CompactAction::Play);okay=okay&&auditions==1&&auditionId==expectedId;
        click(CompactAction::Stop);okay=okay&&stops==1;
        click(CompactAction::Scope);click(CompactAction::TempoMode);okay=okay&&settings==2;
        click(CompactAction::Midi);okay=okay&&payloads==1&&saves==1;
        click(CompactAction::Details);paint();okay=okay&&selectedCandidate_.has_value()&&compactDetailRect_.height()>0;
        click(CompactAction::Details);paint();okay=okay&&!selectedCandidate_;
        click(CompactAction::Density);paint();okay=okay&&detailedCards_&&captureEditorUiState().detailedCards;
        okay=okay&&std::any_of(compactRibbons_.begin(),compactRibbons_.end(),[](const auto& h){return h.id.find("@source")!=std::string::npos;});
        const auto access=session::continuationAccess(recommendations_.groups[1]);
        for(const auto i:access.more)if(recommendations_.groups[1][i].exploratory){
            compactGroup_=1;compactExtra_=recommendations_.groups[1][i].id;compactSelected_=compactExtra_;selectedCandidate_.reset();compactScroll_=0;paint();
            okay=okay&&get(CompactAction::More).has_value()&&get(CompactAction::Midi)&&get(CompactAction::Midi)->id==compactExtra_;break;}
        const auto stale=get(CompactAction::Midi);const auto count=payloads;
        setWorkerStatus(generation_+1,0,true,factoryCount_,userCount_,false);paint();
        if(stale){CPoint point{(stale->rect.left+stale->rect.right)/2,(stale->rect.top+stale->rect.bottom)/2};onMouseDown(point,CButtonState{kLButton});}
        okay=okay&&payloads==count&&!get(CompactAction::Midi)&&!get(CompactAction::Play);
    }catch(...){okay=false;}
    actions_=savedActions;state_=savedState;restoreEditorUiState(savedUi);compactGroup_=savedGroup;compactSelected_=savedSelected;compactExtra_=savedExtra;
    generation_=oldGeneration;workerBusy_=false;recommendationResultReady_=true;auditionCustom_=false;auditionSuffix_=false;actionStatus_=savedStatus;
    detailedCards_=false;compactScroll_=0;selectedCandidate_.reset();resizeLayout(static_cast<int>(oldSize.getWidth()),static_cast<int>(oldSize.getHeight()));
    paint();return okay;
}
} // namespace harmony::ui
