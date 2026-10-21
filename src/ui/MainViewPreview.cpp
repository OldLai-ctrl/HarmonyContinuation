#include "MainView.h"
#include "enrichment/EnrichmentProvenance.h"
#include "color/RelativeColorCurve.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace harmony::ui {
using namespace VSTGUI;
using enrichment::EnrichmentProvenanceKind;
using enrichment::EnrichmentProvenanceMark;
using enrichment::enrichmentProvenance;
void MainView::setPreviewTransport(preview::PlaybackState state,bool host,bool available,bool pending){
    if(auditionState_==state&&auditionHostBackend_==host&&auditionHostAvailable_==available&&auditionPending_==pending)return;
    auditionState_=state;auditionHostBackend_=host;auditionHostAvailable_=available;auditionPending_=pending;invalid();
}
double MainView::previewDisplayQN(const std::string& id) const {
    return seekGesture_&&seekGesture_->hit.id==id?seekGesture_->targetQN:previewCandidateId_==id?previewQN_:-1.;
}
bool MainView::beginPreviewSeek(CPoint point){
    if(state_.tab!=session::Tab::Recommend||form_!=Form::None||overviewOpen_||workerBusy_||!recommendationResultReady_||auditionPending_||!actions_.seekAudition)return false;
    for(const auto& hit:seekHits_)if(hit.rect.pointInside(point)){
        if(!previewCandidateId_.empty()&&previewCandidateId_!=hit.id&&actions_.stopAudition)actions_.stopAudition();
        compactSelected_=hit.id;seekGesture_=SeekGesture{hit,generation_,0};movePreviewSeek(point);return true;
    }
    return false;
}
void MainView::movePreviewSeek(CPoint point){
    if(!seekGesture_)return;
    auto& gesture=*seekGesture_;const auto& hit=gesture.hit;
    gesture.targetQN=std::clamp((point.x-hit.originX)/hit.width*hit.totalQN,hit.minimumQN,hit.totalQN);invalid();
}
void MainView::drawRelativeColorCurve(CDrawContext* dc,const preview::SourceRouteResult& route,std::optional<KeySignature> key,UiRect area){
    const bool zh=state_.locale==session::Locale::ZhCN;
    static const auto font=owned(new CFontDesc("Microsoft YaHei UI",10));
    const auto label=[&](const std::string& value,UiRect box,CColor color=CColor(175,181,191,255)){
        dc->setFont(font,10);dc->setFontColor(color);ConcatClip clip(*dc,{box.left,box.top,box.right,box.bottom});
        dc->drawString(value.c_str(),{box.left,box.top,box.right,box.bottom},kLeftText);
    };
    label(zh?"相对色彩 0–100 · 结构指数，不代表推荐质量":"Relative color 0–100 · structure, not quality",{area.left,area.top,area.right,area.top+17});
    if(!route){label(zh?"来源不可用，无法分析":"Source unavailable",{area.left,area.top+20,area.right,area.bottom});return;}
    Progression path=route.imported.events;const auto& c=route.candidate;
    const double anchor=path.empty()?0:path.front().startQN;
    double boundary{};
    for(auto& e:path){e.startQN-=anchor;if(e.openEnded||!e.durationQN){e.durationQN=c.suggestedCurrentChordDurationQN;e.openEnded=!e.durationQN;}
        if(e.durationQN)boundary=std::max(boundary,e.startQN+*e.durationQN);}
    double end=boundary;
    for(const auto& e:c.continuation){ChordEvent event=e.harmonicData.value_or(ChordEvent{});event.name=e.label;event.quality=e.quality;event.startQN=end;event.durationQN=e.durationQN;event.openEnded=false;path.push_back(event);end+=e.durationQN;}
    if(!std::isfinite(end)||end<=0){label(zh?"时值待定，无法绘制时间轴":"Timing unavailable",{area.left,area.top+20,area.right,area.bottom});return;}
    const auto qnLabel=[](double value){char buffer[48];std::snprintf(buffer,sizeof(buffer),"%.2f",value);std::string text=buffer;while(text.size()>1&&text.back()=='0')text.pop_back();if(text.back()=='.')text.pop_back();return text;};
    const auto* enriched=findEnrichment(c.id);
    const bool inferredTime=enriched&&std::any_of(enriched->progression.begin(),enriched->progression.end(),[](const auto& event){return event.openEnded||!event.durationQN;});
    label(std::string(inferredTime?"≈ 0 – ":"0 – ")+qnLabel(end)+" QN",{area.right-100,area.top+15,area.right,area.top+29});
    const auto points=color::analyzeRelativeColorCurve(path,key);
    std::vector<EnrichmentProvenanceMark> marks;
    if(enriched)marks=enrichmentProvenance(state_.imported.events,enriched->progression);
    const UiRect plot{area.left+25,area.top+30,area.right-5,area.bottom-24};
    if(plot.height()<25||plot.width()<=0)return;
    dc->setFrameColor({68,74,84,255});dc->setLineWidth(1);
    for(const int level:{0,50,100}){const double y=plot.bottom-plot.height()*level/100.;dc->drawLine({plot.left,y},{plot.right,y});
        label(std::to_string(level),{area.left,y-7,plot.left-2,y+7});}
    std::optional<CPoint> previous;bool missing=false;
    const auto timelineX=[&](double qn){return plot.left+plot.width()*qn/end;};
    for(std::size_t i=0;i<points.size();++i){const auto& point=points[i];
        if(!point.startQN||!point.durationQN||!point.value){previous.reset();missing=true;continue;}
        const double qn=*point.startQN+*point.durationQN*.5,x=timelineX(qn),y=plot.bottom-plot.height()**point.value/100.;
        const bool inserted=i<marks.size()&&marks[i].kind==EnrichmentProvenanceKind::Inserted;
        const bool replaced=i<marks.size()&&marks[i].kind==EnrichmentProvenanceKind::Replaced;
        const CColor color=inserted?CColor(111,224,190,255):replaced?CColor(240,158,190,255):CColor(166,186,255,255);
        dc->setFrameColor(color);dc->setLineWidth(1.5);if(previous)dc->drawLine(*previous,{x,y});
        dc->setFillColor(color);
        if(inserted)dc->drawRect({x-4,y-4,x+4,y+4},kDrawFilled);
        else dc->drawEllipse({x-3,y-3,x+3,y+3},kDrawFilled);
        const double space=plot.width()**point.durationQN/end;
        if(space>=52)label((inserted?(zh?"新增 ":"New "):replaced?(zh?"替换 ":"Replaced "):"")+path[i].name+" "+std::to_string(int(std::round(*point.value))),
            {x-space*.5+2,plot.bottom+3,x+space*.5-2,area.bottom},color);
        previous=CPoint{x,y};
    }
    const double display=previewDisplayQN(c.id);
    if(display>=0){dc->setFrameColor({244,244,246,255});dc->setLineWidth(1);const auto x=timelineX(std::clamp(display,0.,end));dc->drawLine({x,plot.top},{x,plot.bottom});}
    if(inferredTime&&!missing)label(zh?"OPEN：按试听规划时值":"OPEN: planned preview timing",{plot.left+5,area.top+14,area.right-110,area.top+28});
    if(missing)label(zh?"数据不足的节点留空":"Unavailable nodes are left blank",{plot.left+5,area.top+14,area.right-110,area.top+28});
    auto hit=CRect{plot.left,plot.top,plot.right,plot.bottom};
    if(compactInCards_)hit.bound({layout_.content.left,layout_.content.top,layout_.content.right,layout_.content.bottom});
    if(hit.getWidth()>0&&hit.getHeight()>0)seekHits_.push_back({hit,c.id,plot.left,plot.width(),end,auditionSuffix_&&!findEnrichment(c.id)?boundary:0.});
}
}
