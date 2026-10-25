#include "MainView.h"
#include "wish/WishCatalog.h"
#include "snapshot/RecommendationSnapshot.h"
#include "preview/OfflinePreviewRenderer.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cclipboard.h"
#include <chrono>
#include <set>
#include <sstream>
#include <thread>
#include <stdexcept>

namespace harmony::ui {
using namespace VSTGUI;
namespace {
const CColor wishFg{242,243,245,255},wishMuted{175,181,191,255},wishBlue{166,186,255,255};
void wishText(CDrawContext* dc,const std::string& s,UiRect r,double size=11,CColor color=wishFg) {
    static const auto font=owned(new CFontDesc("Microsoft YaHei UI",11));
    dc->setFont(font,size);dc->setFontColor(color);ConcatClip clip(*dc,{r.left,r.top,r.right,r.bottom});
    dc->drawString(s.c_str(),{r.left,r.top,r.right,r.bottom},kLeftText);
}
void wishBox(CDrawContext* dc,UiRect r,CColor color=CColor(36,39,45,255)) {
    dc->setFillColor(color);dc->setFrameColor(CColor(74,80,91,255));
    dc->drawRect({r.left,r.top,r.right,r.bottom},kDrawFilledAndStroked);
}
constexpr wish::MusicalMode wishModes[]{wish::MusicalMode::Ionian,wish::MusicalMode::Aeolian,
    wish::MusicalMode::HarmonicMinor,wish::MusicalMode::MelodicMinorAscending,wish::MusicalMode::ClassicalMelodicMinor,
    wish::MusicalMode::Dorian,wish::MusicalMode::Phrygian,wish::MusicalMode::Lydian,wish::MusicalMode::Mixolydian,wish::MusicalMode::Locrian};
constexpr const char* wishModeLabels[]{"自然大调","自然小调","和声小调","旋律小调↑","古典旋律小调","Dorian","Phrygian","Lydian","Mixolydian","Locrian"};
wish::WishSettings wishPreset(std::size_t index) {
    wish::WishSettings settings;if(index>0&&index<=3)settings.filter.modes={wishModes[index-1]};return settings;
}
std::string wishModeName(wish::MusicalMode mode,bool zh=true) {
    constexpr const char* english[]{"Major","Natural minor","Harmonic minor","Jazz melodic minor","Classical melodic minor","Dorian","Phrygian","Lydian","Mixolydian","Locrian"};
    for(std::size_t i=0;i<std::size(wishModes);++i)if(wishModes[i]==mode)return zh?wishModeLabels[i]:english[i];return "?";
}
std::string tonicName(int n) {constexpr const char* names[]{"C","C♯/D♭","D","E♭","E","F","F♯/G♭","G","A♭","A","B♭","B"};return names[(n%12+12)%12];}
std::string wishStyleName(Style style) {
    switch(style){case Style::Pop:return "Pop";case Style::Rock:return "Rock";case Style::Rnb:return "R&B";
        case Style::Jazz:return "Jazz";case Style::CityPop:return "City Pop";default:return "Functional";}
}
std::string usageName(wish::EditorialUsage usage) {
    switch(usage){case wish::EditorialUsage::High:return "高";case wish::EditorialUsage::Medium:return "中";
        case wish::EditorialUsage::Low:return "低";default:return "未评定";}
}
std::string wishInformation(const wish::WishItem& item) {
    std::ostringstream out;out<<(item.match?"当前输入约束抽取":"自由随机 · 完整来源")<<'\n';
    for(const auto& source:item.sources)out<<"来源 "<<source.id<<"\n源指纹 "<<source.fingerprint<<'\n';
    out<<"完整音乐身份 "<<item.musicIdentity<<'\n';
    if(item.match)out<<"匹配位置 "<<item.match->templateMatchStart+1<<"–"<<item.match->templateMatchEnd+1<<'\n';
    for(const auto& m:item.memberships)out<<"归属 "<<tonicName(m.tonicPitchClass)<<' '<<wishModeName(m.membership.mode)
        <<" · 常用程度 "<<usageName(m.membership.usage)<<"（编辑评定）\n依据 "<<m.membership.usageBasis
        <<"\nWhy "<<m.membership.musicalEvidence<<"\n审校 "<<m.membership.reviewer<<'\n';
    if(item.memberships.empty())out<<"无可消费的已审核归属；不据音阶兼容补标签。\n";
    return out.str();
}
bool containsWishCandidate(const std::shared_ptr<const wish::WishView>& view,
                           const std::string& id) {
    return view && !id.empty() && std::any_of(view->items.begin(), view->items.end(),
        [&](const auto& item) { return item->route.candidate.id == id; });
}
}

void MainView::ensureWish() {
    if(wishWorkspace_)return;
    auto catalog=wishAnnotations_ ? *wishAnnotations_ : wish::annotationsForFactory({}, static_cast<int>(state_.factoryLibraryVersion));
    if(!catalog){actionStatus_="调式标注读取失败："+catalog.error;return;}
    wishWorkspace_=std::make_shared<wish::WishWorkflow>(factory_,std::move(catalog.catalog));
    resetWish();
}
void MainView::invalidateWishWorkspace() {
    const bool stopWishAudition = wishMode_ && !previewCandidateId_.empty() && wishItem(previewCandidateId_);
    wishWorkspace_.reset();wishView_.reset();wishRequested_=false;wishPage_=0;wishScroll_=0;
    wishSelected_.clear();wishExpanded_.clear();
    if (wishMode_) {
        if (stopWishAudition && actions_.stopAudition) actions_.stopAudition();
        if (stopWishAudition) { previewCandidateId_.clear();auditionState_=preview::PlaybackState::Stopped; }
        compactSelected_.clear();compactScroll_=0;compactRibbonScroll_.clear();seekGesture_.reset();
        previewSeekHoverId_.clear();previewSeekCursorHover_=false;midiPending_.reset();midiClick_={};
    }
}
void MainView::setWishAnnotations(wish::AnnotationLoadResult annotations) {
    const auto identity = annotations ? wish::serializeAnnotations(annotations.catalog) : "ERROR:" + annotations.error;
    if (identity == wishAnnotationIdentity_) return;
    wishAnnotationIdentity_ = identity;
    wishAnnotations_ = std::move(annotations);
    invalidateWishWorkspace();
    if (wishMode_) ensureWish();
}
void MainView::resetWish() {
    const auto previousView=wishView_;
    if(wishSettings_.useInput){
        wishInput_.imported=state_.imported;wishInput_.analysis=state_.analysisContext();
        wishInput_.request={};wishInput_.request.constraints=state_.constraints;
    }else wishInput_.clear();
    if(wishWorkspace_)wishWorkspace_->reset(wishSettings_,wishInput_,
        static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto nextView=wishWorkspace_?wishWorkspace_->view():nullptr;
    if(containsWishCandidate(previousView,previewCandidateId_)&&
       !containsWishCandidate(nextView,previewCandidateId_)){
        if(actions_.clearAudition)actions_.clearAudition();else if(actions_.stopAudition)actions_.stopAudition();
        previewCandidateId_.clear();auditionState_=preview::PlaybackState::Stopped;auditionPending_=false;
    }
    wishView_=std::move(nextView);
    wishPage_=0;wishScroll_=0;wishSelected_.clear();wishExpanded_.clear();wishRequested_=false;
    if(wishMode_)actionStatus_.clear();
    if(wishMode_){compactSelected_.clear();compactScroll_=0;compactHits_.clear();compactRibbonScroll_.clear();
        seekGesture_.reset();previewSeekHoverId_.clear();previewSeekCursorHover_=false;midiPending_.reset();midiClick_={};}
    invalid();
}
void MainView::pollWish() {
    if(!wishWorkspace_)return;auto next=wishWorkspace_->view();
    if(next==wishView_)return;
    const auto previousView=wishView_;
    const auto previousCount=previousView?previousView->items.size():0;
    const bool revisionChanged=previousView&&next->revision!=previousView->revision;
    const bool displayChanged=previousView&&next->displayGeneration!=previousView->displayGeneration;
    const bool batchChanged=previousView&&next->batchSerial!=previousView->batchSerial;
    const bool appendedBatch=batchChanged&&next->lastDrawAppended;
    const auto newView=next;
    const auto selectionRemoved=[&](const std::string& id){
        return containsWishCandidate(previousView,id)&&!containsWishCandidate(newView,id);
    };
    bool previewRemoved=false;
    if(selectionRemoved(previewCandidateId_)){
        if(actions_.clearAudition)actions_.clearAudition();else if(actions_.stopAudition)actions_.stopAudition();
        previewCandidateId_.clear();auditionState_=preview::PlaybackState::Stopped;auditionPending_=false;
        previewRemoved=true;
    }
    wishView_=std::move(next);
    if(selectionRemoved(wishSelected_))wishSelected_.clear();
    if(selectionRemoved(compactSelected_))compactSelected_.clear();
    if(!wishExpanded_.empty()&&!containsWishCandidate(wishView_,wishExpanded_))wishExpanded_.clear();

    const bool replacedOrCleared=revisionChanged||(displayChanged&&!appendedBatch)||
        (batchChanged&&!wishView_->lastDrawAppended);
    if(replacedOrCleared){
        wishPage_=0;wishScroll_=0;
        if(wishMode_){compactScroll_=0;compactRibbonScroll_.clear();
            seekGesture_.reset();previewSeekHoverId_.clear();previewSeekCursorHover_=false;
            midiPending_.reset();midiClick_={};}
    }else if(wishView_&&!wishView_->busy&&appendedBatch&&wishView_->items.size()>previousCount){
        wishPage_=previousCount/recommendationPageSize;wishScroll_=0;
        if(wishMode_){compactScroll_=0;if(compactSelected_.empty())compactSelected_=wishView_->items[previousCount]->route.candidate.id;}
    }
    if(wishMode_||previewRemoved)invalid();
}
const wish::WishItem* MainView::wishItem(const std::string& id) const {
    if(!wishView_||!wishWorkspace_||wishWorkspace_->view()->revision!=wishView_->revision||
       wishWorkspace_->view()->displayGeneration!=wishView_->displayGeneration)return nullptr;
    for(const auto& item:wishView_->items)if(item->route.candidate.id==id)return item.get();return nullptr;
}
bool MainView::currentWishCandidate(const ContinuationCandidate& candidate) const {
    const auto* item=wishItem(candidate.id);
    return item&&item->route.candidate.primaryTemplate==candidate.primaryTemplate&&
        session::continuationFingerprint(item->route.candidate)==session::continuationFingerprint(candidate);
}
void MainView::drawPrimaryTabs(CDrawContext* dc,const CompactLayoutResult& g) {
    constexpr PhraseIntent intents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};
    for(std::size_t i=0;i<4;++i)compactButton(dc,compactIntentTabGeometry(g.intentToolbar,i),tIntent(intents[i]),CompactAction::Intent,{},i,
        !wishMode_&&state_.productMode==session::ProductMode::Continue&&compactGroup_==i);
    compactButton(dc,compactIntentTabGeometry(g.intentToolbar,4),state_.locale==session::Locale::ZhCN?"升级进行":"Enrich",CompactAction::Enrichment,{},0,
        !wishMode_&&state_.productMode==session::ProductMode::Enrich);
    compactButton(dc,compactIntentTabGeometry(g.intentToolbar,5),state_.locale==session::Locale::ZhCN?"和弦祈愿":"Chord Wish",CompactAction::Wish,{},0,wishMode_);
}

void MainView::drawWish(CDrawContext* dc,const CompactLayoutResult& g) {
    ensureWish();pollWish();layout_.content=g.cardsViewport;
    const bool zh=state_.locale==session::Locale::ZhCN;
    const auto tr=[&](const char* cn,const char* en){return zh?cn:en;};
    const double l=g.input.left+10,r=g.input.right-10,y=g.input.top;
    wishBox(dc,g.input);
    compactButton(dc,{l,y+7,l+100,y+35},tr("快捷预设","Presets"),CompactAction::WishPreset);
    compactButton(dc,{l+106,y+7,l+192,y+35},tr("进阶设置","Advanced"),CompactAction::WishAdvanced,{},0,wishAdvanced_);
    compactButton(dc,{r-258,y+7,r-152,y+35},wishKeepHistory_?tr("保留抽卡历史 ✓","Keep history ✓"):tr("保留抽卡历史","Keep history"),CompactAction::WishHistory,{},0,wishKeepHistory_);
    compactButton(dc,{r-146,y+7,r,y+35},tr("清空卡池","Clear cards"),CompactAction::WishClear);
    std::string source=wishSettings_.useInput?tr("使用当前输入：","Current input: "):tr("自由随机 · 未关联当前输入","Free random · no input linked");
    if(wishSettings_.useInput){for(const auto& e:wishInput_.imported.events)source+=e.name+"  ";if(wishInput_.imported.events.empty())source+=tr("无有效输入，请先导入进行","No valid input; import a progression first");}
    wishText(dc,source,{l,y+39,r,y+61},12);
    std::string modes;for(const auto mode:wishSettings_.filter.modes){if(!modes.empty())modes+=" / ";modes+=wishModeName(mode,zh);}
    const auto summary=(wishSettings_.filter.fixedTonic?tonicName(int(*wishSettings_.filter.fixedTonic)):tr("主音不限","Any tonic"))+" · "+
        (modes.empty()?tr("调式不限","Any mode"):modes)+(wishSettings_.filter.strict?tr(" · 严格"," · Strict"):"");
    wishText(dc,summary,{l,y+63,r,y+82},10,wishMuted);
    const bool busy=wishView_&&wishView_->busy;
    const bool ready=wishWorkspace_&&!busy&&(!wishSettings_.useInput||!wishInput_.imported.events.empty());
    compactButton(dc,{l,y+89,l+86,y+123},tr("一抽","Draw 1"),CompactAction::WishOne,{},0,false,ready);
    compactButton(dc,{l+94,y+89,l+180,y+123},tr("十抽","Draw 10"),CompactAction::WishTen,{},0,false,ready);
    compactButton(dc,{l+188,y+89,l+298,y+123},tr("再来十抽","10 more"),CompactAction::WishAgain,{},0,false,ready&&wishRequested_&&wishView_&&wishView_->remaining>0);
    wishText(dc,busy?tr("正在抽取…","Drawing…"):wishView_&&!wishView_->error.empty()?tr("无法抽取，见下方说明","Cannot draw; see below"):wishRequested_?tr("本批无放回","Without replacement"):tr("抽取后可试听 / MIDI","Draw, then preview / MIDI"),{l+307,y+94,r,y+121},10,wishMuted);
    std::string status=tr("和弦祈愿 · ","Chord Wish · ");
    if(busy)status+=tr("构建合格卡池","Building eligible pool");
    else if(wishView_&&!wishView_->error.empty())status+=tr("输入或参数无效：","Invalid input/settings: ")+wishView_->error;
    else if(wishRequested_&&wishView_)status+=tr("本次 ","Drawn ")+std::to_string(wishView_->lastDraw)+tr(" 条 · 卡池 "," · Pool ")+std::to_string(wishView_->poolSize)+tr(" · 剩余 "," · Left ")+std::to_string(wishView_->remaining);
    else status+=tr("默认全部不限制","All filters off by default");
    wishText(dc,status,g.recommendationHeading,11,wishBlue);drawPrimaryTabs(dc,g);
    const auto page=browsePage(wishView_?wishView_->items.size():0,wishPage_);wishPage_=page.page;
    double cy=g.cardsViewport.top-compactScroll_;
    {
        ConcatClip clip(*dc,{g.cardsViewport.left,g.cardsViewport.top,g.cardsViewport.right,g.cardsViewport.bottom});compactInCards_=true;
        if(page.total==0){
            const std::string emptyText=wishRequested_&&wishView_&&wishView_->poolSize>0?
                tr("显示列表为空；当前卡池剩余 ","No cards displayed; remaining in pool: ")+std::to_string(wishView_->remaining):
                tr("实际候选 0 条；保持当前筛选。未审核归属不参与调式卡池。","0 eligible candidates; filters retained. Only reviewed modal memberships qualify.");
            wishText(dc,(!wishAnnotations_||!*wishAnnotations_)?tr("标注未加载；候选数量未知。","Annotations unavailable; candidate count unknown."):wishRequested_?emptyText:std::string(tr("选择参数后点击一抽或十抽；抽取不会自动播放。","Choose settings, then Draw 1 / Draw 10. Drawing does not start playback.")),
                {g.cardsViewport.left+10,cy+12,g.cardsViewport.right-10,cy+42},11,wishMuted);
            std::string reason;
            if (!wishAnnotations_ || !*wishAnnotations_) reason=tr("标注未就绪，候选数量未知；请查看加载错误。","Annotation loading failed; count unknown. See loading error.");
            else for(const auto mode:wishSettings_.filter.modes) {
                if(mode!=wish::MusicalMode::Locrian&&mode!=wish::MusicalMode::ClassicalMelodicMinor)continue;
                if(mode==wish::MusicalMode::ClassicalMelodicMinor&&wishSettings_.filter.strict)
                    reason+=tr("古典旋小：严格方向校验尚未实现；","Classical melodic minor: strict direction check unavailable; ");
                else if(wish::reviewedSourceCount(wishAnnotations_->catalog,mode)==0)
                    reason+=wishModeName(mode,zh)+" 0: "+(zh?wish::emptyModeReason(mode,false):std::string(mode==wish::MusicalMode::Locrian?"no reviewed centre evidence":"no directional melody evidence"))+"; ";
            }
            if(!reason.empty())wishText(dc,reason,{g.cardsViewport.left+10,cy+46,g.cardsViewport.right-10,cy+82},10,wishMuted);
        }
        for(std::size_t i=page.begin;i<page.end;++i){
            const auto& item=*wishView_->items[i];const auto& c=item.route.candidate;auto cg=compactCandidateGeometry(g.cardsViewport,cy,detailedCards_);
            // Wish facts use the whole card width below the immutable source ribbon.
            const auto bottom=cg.timeBand.bottom;cg.row.bottom=bottom+(detailedCards_?147:35);
            const bool selected=compactSelected_==c.id;wishBox(dc,cg.row,selected?CColor(40,46,58,255):CColor(36,39,45,255));
            compactButton(dc,cg.select,selected?"✓":std::to_string(i+1),CompactAction::Select,c.id,i,selected,true,true);
            std::string name=c.primaryTemplate;for(const auto& p:factory_)if(p.id==c.primaryTemplate){name=zh?(p.nameZh.empty()?p.name:p.nameZh):(p.nameEn.empty()?p.name:p.nameEn);break;}
            wishText(dc,name,cg.name,12);
            wishText(dc,item.match?tr("输入约束 · 续写","Input · continuation"):tr("自由随机 · 完整来源","Free · complete source"),cg.facts,10,wishBlue);
            wishText(dc,tr("来源 ","Source ")+c.primaryTemplate,cg.location,10,wishMuted);
            wishText(dc,item.match?tr("匹配 ","Match ")+std::to_string(c.matchStart+1)+"–"+std::to_string(c.matchEnd+1):tr("无输入匹配","No input match"),cg.match,10,wishMuted);
            compactButton(dc,cg.audition,previewCandidateId_==c.id&&auditionState_==preview::PlaybackState::Playing?tr("■ 停止","■ Stop"):tr("▶ 试听","▶ Preview"),CompactAction::Audition,c.id,i,false,!busy&&!auditionPending_&&bool(actions_.audition));
            compactButton(dc,cg.midi,"⠿ MIDI",CompactAction::Midi,c.id,i,false,!busy&&bool(actions_.midiPayload));
            compactButton(dc,cg.details,wishExpanded_==c.id?tr("收起 −","Less −"):tr("详情 +","More +"),CompactAction::Details,c.id,i,wishExpanded_==c.id);
            drawCompactRibbon(dc,item.route,cg.timeBand);
            std::string labels;for(const auto& m:item.memberships){if(!labels.empty())labels+=" / ";labels+=tonicName(m.tonicPitchClass)+" "+wishModeName(m.membership.mode)+"（常用·"+usageName(m.membership.usage)+"）";}
            wishText(dc,labels.empty()?"无可用已审核归属 · 自由卡池保留":labels,{g.cardsViewport.left+10,bottom+3,g.cardsViewport.right-8,bottom+28},10,wishMuted);
            if(detailedCards_)drawRelativeColorCurve(dc,item.route,c.key,{g.cardsViewport.left+8,bottom+29,g.cardsViewport.right-8,bottom+141});
            cy=cg.row.bottom+8;
            if(wishExpanded_==c.id){
                const double left=g.cardsViewport.left+10,right=g.cardsViewport.right-10;
                if(item.match){drawRecommendationAlignment(dc,c,{left,cy,right,cy+110});cy+=118;}
                compactButton(dc,{left,cy,left+96,cy+30},"保存快照",CompactAction::WishSnapshot,c.id,i,false,bool(actions_.saveSnapshot));
                compactButton(dc,{left+104,cy,left+242,cy+30},"复制完整来源 / Why",CompactAction::WishCopy,c.id,i);cy+=36;
                const auto paragraph=[&](const std::string& text){
                    std::string row;dc->setFont(kNormalFont);
                    for(std::size_t at=0;at<text.size();){const auto b=static_cast<unsigned char>(text[at]);const auto n=b<128?1:b<224?2:b<240?3:4;auto part=text.substr(at,n);at+=n;
                        if(dc->getStringWidth((row+part).c_str())>right-left&&!row.empty()){wishText(dc,row,{left,cy,right,cy+20},10,wishMuted);cy+=22;row.clear();}row+=part;}
                    if(!row.empty()){wishText(dc,row,{left,cy,right,cy+20},10,wishMuted);cy+=22;}
                };
                const auto path=color::HarmonyColorAnalyzer::analyzeContinuation(color::HarmonyColorAnalyzer::prepare(item.route.imported.events),c);
                for(const auto& sentence:explainWhy("Why：按合格卡池随机抽取；常用程度为编辑评定。",colorHint(path,state_.locale),state_.locale,color::Preference::Off).sentences)paragraph(sentence);
                if(item.match){paragraph("Unified基础资格与硬约束保持；随机抽取没有重新评分。");const auto realization=realizationWhy(c,state_.locale);if(!realization.empty())paragraph(realization);}
                for(const auto& m:item.memberships){paragraph(tonicName(m.tonicPitchClass)+" "+wishModeName(m.membership.mode)+" · 常用程度 "+usageName(m.membership.usage));paragraph(m.membership.usageBasis);paragraph(m.membership.musicalEvidence);}
                for(const auto& source:item.sources)paragraph("完整来源："+source.id+" · "+(source.annotationState==wish::AnnotationState::Reviewed?"已审校":source.annotationState==wish::AnnotationState::Unconfirmed?"含未确认解释":source.annotationState==wish::AnnotationState::SourceMismatch?"来源失配":"未审核/无标注"));
                paragraph(item.match?"完整来源按匹配保留前缀和续写边界；快照冻结实际音乐及匹配。":"本卡为完整来源，续写为空；快照冻结实际音乐。归属目录与抽样设置不写入Snapshot3。");
                cy+=8;
            }
        }
        compactInCards_=false;
    }
    compactScrollMax_=std::max(0.,cy+compactScroll_-g.cardsViewport.bottom);compactScroll_=std::clamp(compactScroll_,0.,compactScrollMax_);
    if(compactScrollMax_>0){
        const double height=g.cardsViewport.height()*g.cardsViewport.height()/(g.cardsViewport.height()+compactScrollMax_);
        const double top=g.cardsViewport.top+(g.cardsViewport.height()-height)*compactScroll_/compactScrollMax_;
        dc->setFillColor(wishMuted);dc->drawRect({g.cardsViewport.right-3,top,g.cardsViewport.right,top+height},kDrawFilled);
    }
    wishScroll_=compactScroll_;wishSelected_=compactSelected_;drawCompactTransport(dc);drawResponsiveForm(dc);
    if(wishAdvanced_){
        compactHits_.clear();compactRibbons_.clear();seekHits_.clear();compactInCards_=false;
        const double left=20,right=g.viewport.right-20,top=64;wishBox(dc,{left,top,right,top+300},CColor(39,43,51,255));
        wishText(dc,tr("祈愿进阶 · 多选调式取并集","Advanced · selected modes form a union"),{left+12,top+6,right-75,top+32},13,wishBlue);
        compactButton(dc,{right-65,top+5,right-9,top+33},tr("完成","Done"),CompactAction::WishAdvanced);
        const double mid=(left+right)/2;
        compactButton(dc,{left+10,top+40,mid-4,top+70},wishSettings_.useInput?tr("使用当前输入 ✓","Use current input ✓"):tr("自由随机 ✓","Free random ✓"),CompactAction::WishInput);
        compactButton(dc,{mid+4,top+40,right-10,top+70},tr("清空卡池","Clear cards"),CompactAction::WishClear);
        compactButton(dc,{left+10,top+77,mid-4,top+107},tr("主音：","Tonic: ")+(wishSettings_.filter.fixedTonic?tonicName(int(*wishSettings_.filter.fixedTonic)):tr("不固定","Any")),CompactAction::WishTonic);
        compactButton(dc,{mid+4,top+77,right-10,top+107},tr("风格：","Style: ")+(wishSettings_.style?wishStyleName(*wishSettings_.style):tr("不限制","Any")),CompactAction::WishStyle);
        const double width=(right-left-24)/5;
        for(std::size_t i=0;i<std::size(wishModes);++i){const double x=left+10+(i%5)*width,yy=top+116+(i/5)*34;
            const bool chosen=std::find(wishSettings_.filter.modes.begin(),wishSettings_.filter.modes.end(),wishModes[i])!=wishSettings_.filter.modes.end();
            const bool focused=wishModes[i]==wish::MusicalMode::Locrian||wishModes[i]==wish::MusicalMode::ClassicalMelodicMinor;
            std::string label=wishModeName(wishModes[i],zh);
            if(!zh&&wishModes[i]==wish::MusicalMode::ClassicalMelodicMinor)label="Classical mel.";
            if(!zh&&wishModes[i]==wish::MusicalMode::MelodicMinorAscending)label="Jazz melodic ↑";
            if(focused)label+=wishAnnotations_&&*wishAnnotations_?
                (wish::reviewedSourceCount(wishAnnotations_->catalog,wishModes[i])==0?" 0":""):" ?";
            compactButton(dc,{x,yy,x+width-4,yy+29},label,CompactAction::WishMode,{},i,chosen);}
        compactButton(dc,{left+10,top+190,mid-4,top+220},wishSettings_.filter.strict?tr("严格匹配 ✓","Strict ✓"):tr("严格匹配：关","Strict: off"),CompactAction::WishStrict,{},0,wishSettings_.filter.strict,!wishSettings_.filter.modes.empty());
        const char* colors[]{tr("不限制","Any"),tr("升温","Warming"),tr("降温","Cooling"),tr("张力上升","Tension rising"),tr("张力释放","Tension releasing")};
        compactButton(dc,{mid+4,top+190,right-10,top+220},tr("色彩：","Color: ")+std::string(colors[int(wishSettings_.color)]),CompactAction::WishColor);
        wishText(dc,tr("主音指所选归属的中心；未选调式时指模板参考主音。","Tonic = reviewed modal centre; with no mode selected, source reference tonic."),{left+10,top+229,right-10,top+250},10,wishMuted);
        const bool catalogReady=wishAnnotations_&&*wishAnnotations_;
        const auto locrianEmpty=catalogReady&&wish::reviewedSourceCount(wishAnnotations_->catalog,wish::MusicalMode::Locrian)==0;
        const auto classicalEmpty=catalogReady&&wish::reviewedSourceCount(wishAnnotations_->catalog,wish::MusicalMode::ClassicalMelodicMinor)==0;
        wishText(dc,!catalogReady?tr("标注未就绪，数量未知","Annotations unavailable; count unknown"):locrianEmpty?tr("Locrian 0：无已审校中心证据","Locrian 0: no reviewed centre evidence"):tr("Locrian：以已审校归属筛选","Locrian: reviewed memberships only"),{left+10,top+251,mid-4,top+272},10,wishMuted);
        wishText(dc,!catalogReady?tr("请查看加载错误","See annotation loading error"):wishSettings_.filter.strict?tr("古典旋小0：严格方向校验未实现","Classical 0: direction check unavailable"):classicalEmpty?tr("古典旋小0：缺旋律上下行证据","Classical 0: no directional melody evidence"):tr("古典旋小：按已确认归属筛选","Classical: reviewed memberships only"),{mid+4,top+251,right-10,top+272},10,wishMuted);
        wishText(dc,tr("升温 / 张力使用现有结构色彩分析；不代表情绪或质量。","Color reflects harmonic structure; it does not rate mood or musical quality."),{left+10,top+273,right-10,top+294},10,wishMuted);
    }
}

bool MainView::onWishAction(const CompactHit& hit,CPoint& where) {
    const auto action=hit.action;const auto* item=wishItem(hit.id);
    const bool zh=state_.locale==session::Locale::ZhCN;
    switch(action){
        case CompactAction::WishAdvanced:wishAdvanced_=!wishAdvanced_;compactHits_.clear();return true;
        case CompactAction::WishHistory:wishKeepHistory_=!wishKeepHistory_;invalid();return true;
        case CompactAction::WishClear:if(wishWorkspace_){wishWorkspace_->clearDisplay();pollWish();}return true;
        case CompactAction::WishInput:wishSettings_.useInput=!wishSettings_.useInput;resetWish();return true;
        case CompactAction::WishPreset:{const auto n=popup(zh?std::vector<std::string>{"全部不限制","自然大调","自然小调","和声小调"}:std::vector<std::string>{"All filters off","Major","Natural minor","Harmonic minor"},where);if(n>=0){wishSettings_=wishPreset(static_cast<std::size_t>(n));resetWish();}return true;}
        case CompactAction::WishTonic:{std::vector<std::string> names{zh?"不固定主音":"Any tonic"};for(int i=0;i<12;++i)names.push_back(tonicName(i));const auto n=popup(names,where);if(n>=0){wishSettings_.filter.fixedTonic=n?std::optional(static_cast<PitchClass>(n-1)):std::nullopt;resetWish();}return true;}
        case CompactAction::WishMode:if(hit.index<std::size(wishModes)){auto& modes=wishSettings_.filter.modes;const auto mode=wishModes[hit.index];if(std::find(modes.begin(),modes.end(),mode)!=modes.end())std::erase(modes,mode);else modes.push_back(mode);if(modes.empty())wishSettings_.filter.strict=false;resetWish();}return true;
        case CompactAction::WishStrict:wishSettings_.filter.strict=!wishSettings_.filter.strict;resetWish();return true;
        case CompactAction::WishStyle:{const auto n=popup({"不限制","Pop","Rock","R&B","Jazz","City Pop","Functional"},where);constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};if(n>=0){wishSettings_.style=n?std::optional(styles[n-1]):std::nullopt;resetWish();}return true;}
        case CompactAction::WishColor:{const auto n=popup({"不限制","升温","降温","张力上升","张力释放"},where);if(n>=0){wishSettings_.color=static_cast<wish::ColorDirection>(n);resetWish();}return true;}
        case CompactAction::WishOne:case CompactAction::WishTen:case CompactAction::WishAgain:
            if(wishWorkspace_&&wishWorkspace_->draw(action==CompactAction::WishOne?1:10,wishKeepHistory_)){wishRequested_=true;pollWish();}return true;
        case CompactAction::Select:if(item){compactSelected_=hit.id;wishSelected_=hit.id;
            if(previewCandidateId_!=hit.id&&containsWishCandidate(wishView_,previewCandidateId_)){
                if(actions_.clearAudition)actions_.clearAudition();else if(actions_.stopAudition)actions_.stopAudition();
                previewCandidateId_.clear();auditionState_=preview::PlaybackState::Stopped;auditionPending_=false;
            }}return true;
        case CompactAction::Audition:case CompactAction::Play:{if(action==CompactAction::Play)item=wishItem(compactSelected_);if(item&&actions_.audition){compactSelected_=item->route.candidate.id;
            if(previewCandidateId_==compactSelected_&&auditionState_==preview::PlaybackState::Paused&&actions_.resumeAudition)actions_.resumeAudition();else actions_.audition(item->route.candidate);}return true;}
        case CompactAction::Midi:if(item)armMidi(item->route.candidate,where);return true;
        case CompactAction::Details:if(item)wishExpanded_=wishExpanded_==hit.id?std::string{}:hit.id;return true;
        case CompactAction::WishSnapshot:if(item&&actions_.saveSnapshot)actionStatus_=actions_.saveSnapshot(item->route.candidate);return true;
        case CompactAction::WishCopy:if(item)CClipboard::setString(wishInformation(*item).c_str());return true;
        case CompactAction::PagePrevious:case CompactAction::PageNext:if(action==CompactAction::PagePrevious){if(wishPage_)--wishPage_;}else ++wishPage_;compactScroll_=0;wishScroll_=0;wishExpanded_.clear();midiPending_.reset();midiClick_={};return true;
        case CompactAction::Intent:case CompactAction::Enrichment:case CompactAction::Density:case CompactAction::Advanced:
        case CompactAction::Library:case CompactAction::Pause:case CompactAction::Stop:case CompactAction::Backend:
        case CompactAction::Repeat:case CompactAction::TempoMode:case CompactAction::Tempo:case CompactAction::Scope:return false;
        default:return true;
    }
}

bool MainView::runEmptyModeUiSmoke(CDrawContext* dc,const std::function<void(const std::string&)>& capture) {
    if(!dc||!wishAnnotations_||!*wishAnnotations_)return false;
    if(wish::reviewedSourceCount(wishAnnotations_->catalog,wish::MusicalMode::Locrian)!=0||
       wish::reviewedSourceCount(wishAnnotations_->catalog,wish::MusicalMode::ClassicalMelodicMinor)!=0)return false;
    changeBrowseCategory(5);wishAdvanced_=true;wishSettings_.filter.strict=true;
    for(const auto zoom:{100u,150u}) {
        setUserZoom(zoom);dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
        for(const auto index:{4u,9u})if(std::none_of(compactHits_.begin(),compactHits_.end(),[&](const auto& h){
            return h.action==CompactAction::WishMode&&h.index==index&&h.enabled&&h.rect.getWidth()>70;
        }))return false;
        if(capture)capture("empty-modes-"+std::to_string(zoom)+".png");
    }
    return true;
}
bool MainView::runG5UiSmoke(CDrawContext* dc,const std::function<void(const std::string&)>& capture) {
    const auto require=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    require(dc&&factory_.size()==1016&&wishAnnotations_&&*wishAnnotations_,"G5 UI requires loaded V6/1016");
    require(wish::reviewedSourceCount(wishAnnotations_->catalog,wish::MusicalMode::Locrian)==2&&
        wish::reviewedSourceCount(wishAnnotations_->catalog,wish::MusicalMode::ClassicalMelodicMinor)==0,"G5 reviewed memberships");
    const auto originalActions=actions_;const auto original=state_;
    actions_.previewRepeat=[&](bool enabled){setPreviewRepeat(enabled);};
    actions_.clearAudition=[]{};
    const auto paint=[&]{dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();};
    const auto click=[&](CompactAction action){paint();const auto hit=std::find_if(compactHits_.begin(),compactHits_.end(),[&](const auto& h){return h.action==action&&h.enabled;});
        require(hit!=compactHits_.end(),"G5 UI control unavailable");CPoint point{(hit->rect.left+hit->rect.right)*userZoom()/2,(hit->rect.top+hit->rect.bottom)*userZoom()/2};onMouseDown(point,CButtonState{kLButton});};
    const auto wait=[&]{const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);do{pollWish();if(wishView_&&!wishView_->busy)return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<until);throw std::runtime_error("G5 UI worker timeout");};
    setUserZoom(100);setPreviewRepeat(false);
    // Six actual entry states, once each; no layout/input matrix.
    for(std::size_t category=0;category<6;++category){changeBrowseCategory(category);paint();
        require(std::count_if(compactHits_.begin(),compactHits_.end(),[](const auto& h){return h.action==CompactAction::Repeat&&h.enabled;})==1,"shared repeat control");}
    click(CompactAction::Repeat);require(auditionRepeat_,"repeat toggles from sixth entry");
    wishSettings_={};wishSettings_.filter.modes={wish::MusicalMode::Locrian};wishSettings_.filter.strict=true;resetWish();
    // Only the 24 performances of the two new sources are sampled here.
    click(CompactAction::WishOne);wait();require(wishView_->items.size()==1,"G5 draw one");
    click(CompactAction::WishTen);wait();require(wishView_->items.size()==10&&wishView_->poolSize==24,"G5 default replaces; real query count");
    click(CompactAction::WishHistory);click(CompactAction::WishAgain);wait();
    require(wishView_->items.size()==20&&wishView_->remaining==3,"G5 history appends without resetting round");
    wishPage_=1;paint();require(wishPage_==1,"G5 second page");if(capture)capture("g5-zh-100-history.png");
    const auto remaining=wishView_->remaining;const auto revision=wishView_->revision;
    const auto inputRevision=state_.imported.revision;const auto inputCount=state_.imported.events.size();
    click(CompactAction::WishClear);wait();require(wishView_->items.empty()&&wishPage_==0&&wishView_->remaining==remaining&&
        wishView_->revision==revision&&!wishSettings_.useInput&&state_.imported.revision==inputRevision&&state_.imported.events.size()==inputCount,"G5 clear leaves input and round intact");
    const auto editor=captureEditorUiState();restoreEditorUiState(editor);require(wishKeepHistory_&&wishAnnotations_&&wishView_->items.empty(),"G5 restored UI keeps catalog/history");
    wishSettings_.filter.modes={wish::MusicalMode::ClassicalMelodicMinor};wishSettings_.filter.fixedTonic=PitchClass::C;resetWish();
    click(CompactAction::WishTen);wait();require(wishView_->items.empty()&&wishView_->poolSize==0&&wishView_->error.empty(),"G5 classical strict empty");
    state_.locale=session::Locale::EnUS;wishAdvanced_=true;setUserZoom(125);paint();if(capture)capture("g5-en-125-advanced.png");
    actions_=originalActions;state_=original;return true;
}
bool MainView::runWishSmoke(CDrawContext* dc,const std::function<void(const std::string&)>& capture) {
    const auto require=[](bool pass,const char* text){if(!pass)throw std::runtime_error(text);};
    require(factory_.size()==658,"wish UI requires current real Factory V5");
    const auto original=state_;const auto actions=actions_;const auto originalRepeat=auditionRepeat_;
    int played{},payloads{},snapshots{},clearedAuditions{},stoppedAuditions{};
    actions_.clearAudition=[&]{++clearedAuditions;};
    actions_.stopAudition=[&]{++stoppedAuditions;};
    actions_.previewRepeat=[&](bool enabled){setPreviewRepeat(enabled);};
    actions_.audition=[&](const ContinuationCandidate& c){require(currentWishCandidate(c),"preview candidate identity");
        const auto route=continuationRoute(c);const auto built=preview::buildSequence(route.imported,&route.candidate,120);require(bool(built),"actual preview route builds");
        const auto* item=wishItem(c.id);require(item&&wish::performanceFingerprint(built.sequence,route.meterNumerator,route.meterDenominator)==item->musicIdentity,"preview full music identity");
        const auto audio=preview::renderOffline(built.sequence,8000);require(!audio.left.empty()&&audio.peak>0.01,"wish preview produces audible audio");++played;};
    actions_.midiPayload=[&](const ContinuationCandidate& c){const auto route=continuationRoute(c);require(bool(route),"MIDI route");++payloads;return midi::candidatePayload({route.imported,120,{route.meterNumerator,route.meterDenominator}},route.candidate);};
    actions_.saveSnapshot=[&](const ContinuationCandidate& c){const auto* item=wishItem(c.id);require(item,"snapshot immutable item");
        const auto saved=snapshot::capture(item->route.imported,c,item->match?std::vector<MatchResult>{*item->match}:std::vector<MatchResult>{},120,item->route.meterNumerator,item->route.meterDenominator);
        const auto loaded=snapshot::deserialize(snapshot::serialize(saved));require(bool(loaded),"Snapshot3 restore");
        const auto sequence=preview::buildSequence(loaded.value.imported,&loaded.value.candidate,120);require(bool(sequence),"restored complete route");
        require(wish::performanceFingerprint(sequence.sequence,item->route.meterNumerator,item->route.meterDenominator)==item->musicIdentity,"Snapshot3 complete music identity");
        const auto before=midi::candidatePayload({item->route.imported,120,{item->route.meterNumerator,item->route.meterDenominator}},c);
        const auto after=midi::candidatePayload({loaded.value.imported,120,{item->route.meterNumerator,item->route.meterDenominator}},loaded.value.candidate);
        require(before&&after&&before.payload.smfBytes==after.payload.smfBytes,"Snapshot/MIDI byte identity");++snapshots;return std::string("快照身份通过");};
    const auto paint=[&]{dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();};
    const auto click=[&](CompactAction action,std::size_t index=0){paint();const auto h=std::find_if(compactHits_.begin(),compactHits_.end(),[&](const auto& hit){return hit.action==action&&hit.enabled&&(action!=CompactAction::WishMode||hit.index==index);});
        require(h!=compactHits_.end(),"wish control missing or disabled");const auto copy=*h;CPoint p{(copy.rect.left+copy.rect.right)/2,(copy.rect.top+copy.rect.bottom)/2};p.x*=userZoom();p.y*=userZoom();onMouseDown(p,CButtonState{kLButton});};
    const auto wait=[&]{const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);do{pollWish();if(wishView_&&!wishView_->busy)return;std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<deadline);throw std::runtime_error("wish UI worker timeout");};
    changeBrowseCategory(5);require(wishMode_&&!wishSettings_.useInput,"default free input isolation");
    for(std::size_t preset=0;preset<4;++preset){const auto settings=wishPreset(preset);require(!settings.useInput&&!settings.style&&!settings.filter.fixedTonic&&!settings.filter.strict&&settings.color==wish::ColorDirection::Any,"quick preset clears other independent parameters");
        require(preset?settings.filter.modes==std::vector<wish::MusicalMode>{wishModes[preset-1]}:settings.filter.modes.empty(),"quick preset maps directly to core mode enumeration");}
    click(CompactAction::WishAdvanced);click(CompactAction::WishMode,0);click(CompactAction::WishMode,1);require(wishSettings_.filter.modes.size()==2,"advanced modes form a union");
    click(CompactAction::WishStrict);require(wishSettings_.filter.strict,"strict mode uses shared core settings");click(CompactAction::WishMode,0);click(CompactAction::WishMode,1);
    require(wishSettings_.filter.modes.empty()&&!wishSettings_.filter.strict,"removing all modes removes invalid strict request");click(CompactAction::WishAdvanced);
    click(CompactAction::WishOne);wait();require(wishView_->items.size()==1&&played==0,"one draw never autoplays");
    const auto firstIdentity=wishView_->items.front()->musicIdentity;
    const auto historyRevision=wishView_->revision;const auto historyRemaining=wishView_->remaining;
    click(CompactAction::WishHistory);require(wishKeepHistory_&&wishView_->revision==historyRevision&&
        wishView_->remaining==historyRemaining,"history toggle preserves query and round");
    click(CompactAction::WishTen);wait();require(wishView_->items.size()==11&&
        wishView_->items.front()->musicIdentity==firstIdentity,"history appends without resetting the round");
    click(CompactAction::WishAgain);wait();require(wishView_->items.size()==21,"continuous 1+10+10 history");
    std::set<std::string> identities;for(const auto& item:wishView_->items){require(!item->match&&item->route.candidate.continuation.empty(),"free draw has no invented continuation or match");require(identities.insert(item->musicIdentity).second,"single round duplicate music");}
    wishPage_=0;compactScroll_=0;paint();click(CompactAction::Audition);click(CompactAction::Midi);midiClick_={};midiPending_.reset();click(CompactAction::Details);paint();
    click(CompactAction::WishSnapshot);require(played==1&&payloads==1&&snapshots==1,"card route action dispatch");
    wishExpanded_.clear();click(CompactAction::PageNext);paint();require(wishPage_==1,"wish page next");click(CompactAction::Audition);require(played==2,"cross-page preview");
    const auto retained=captureEditorUiState();auto recreated=owned(new MainView(getViewSize(),actions_));recreated->setSessionState(state_);recreated->setLibrary(factory_,{});recreated->restoreEditorUiState(retained);
    require(recreated->wishWorkspace_==wishWorkspace_&&recreated->wishView_->items.size()==21&&recreated->wishPage_==1&&
        recreated->wishKeepHistory_,"editor reopen retains immutable round and display preference");
    changeBrowseCategory(0);require(!wishMode_,"return to existing entrance");changeBrowseCategory(5);require(wishView_->items.size()==21,"switching preserves wish round");
    for(const auto zoom:{100u,125u,150u}){setUserZoom(zoom);wishPage_=0;compactScroll_=0;detailedCards_=false;paint();
        require(std::count_if(compactHits_.begin(),compactHits_.end(),[](const auto& h){return h.action==CompactAction::Intent||h.action==CompactAction::Enrichment||h.action==CompactAction::Wish;})==6,"six equal entrances");
        if(capture)capture("wish-"+std::to_string(zoom)+"-compact");click(CompactAction::Density);require(detailedCards_,"wish detailed density click");paint();
        const bool oldSuffix=auditionSuffix_;auditionSuffix_=true;paint();for(const auto& hit:seekHits_)require(hit.minimumQN==0,"free wish curve/ribbon seeks full route despite old suffix setting");auditionSuffix_=oldSuffix;
        if(capture)capture("wish-"+std::to_string(zoom)+"-detailed");click(CompactAction::Density);require(!detailedCards_,"wish compact density click");
        click(CompactAction::WishAdvanced);paint();if(capture)capture("wish-"+std::to_string(zoom)+"-advanced");click(CompactAction::WishAdvanced);}
    setUserZoom(100);wishPage_=0;compactScroll_=0;paint();
    const auto playedBeforeRepeat=played;if(auditionRepeat_)click(CompactAction::Repeat);
    click(CompactAction::Repeat);require(auditionRepeat_&&played==playedBeforeRepeat,
        "wish repeat routes through shared transport without autoplay");
    paint();if(capture)capture("wish-repeat-on");click(CompactAction::Repeat);
    require(!auditionRepeat_&&played==playedBeforeRepeat,"wish repeat turns off without autoplay");

    std::set<std::string> oldIdentities;for(const auto& item:wishView_->items)oldIdentities.insert(item->musicIdentity);
    const auto replaceRevision=wishView_->revision;const auto replaceRemaining=wishView_->remaining;
    const auto replaceGeneration=wishView_->displayGeneration;const auto replaceBatch=wishView_->batchSerial;
    click(CompactAction::WishHistory);require(!wishKeepHistory_&&wishView_->revision==replaceRevision&&
        wishView_->remaining==replaceRemaining,"turning history off leaves the round untouched");
    click(CompactAction::WishTen);wait();
    require(wishView_->items.size()==10&&wishView_->revision==replaceRevision&&
        wishView_->displayGeneration>replaceGeneration&&wishView_->batchSerial==replaceBatch+1&&
        !wishView_->lastDrawAppended&&wishPage_==0,"default draw replaces cards and resets pagination");
    for(const auto& item:wishView_->items)require(!oldIdentities.contains(item->musicIdentity),
        "replacement continues the no-replacement round");

    setUserZoom(100);detailedCards_=false;click(CompactAction::WishAdvanced);click(CompactAction::WishInput);require(wishSettings_.useInput&&!wishInput_.imported.events.empty(),"current input association explicit");click(CompactAction::WishAdvanced);
    click(CompactAction::WishOne);wait();require(!wishView_->items.empty()&&wishView_->items.front()->match.has_value(),"Unified input-constrained wish");
    click(CompactAction::Audition);click(CompactAction::Midi);midiClick_={};midiPending_.reset();click(CompactAction::Details);paint();click(CompactAction::WishSnapshot);
    if(capture)capture("wish-input");
    const auto contextRevision=wishView_->revision;auto newContext=state_;newContext.forcedKey=KeySignature{PitchClass::D,Mode::Major};setSessionState(newContext);
    require(wishView_->revision>contextRevision&&wishView_->items.empty()&&wishInput_.analysis.forcedKey&&wishInput_.analysis.forcedKey->tonic==PitchClass::D,"input context change invalidates old wish generation");
    setSessionState(original);click(CompactAction::WishOne);wait();require(!wishView_->items.empty(),"current input redraw after context reset");
    const auto stale=wishView_->items.front()->route.candidate;const auto staleIdentity=wishView_->items.front()->musicIdentity;
    const auto clearRevision=wishView_->revision;const auto clearDisplayGeneration=wishView_->displayGeneration;
    const auto clearRemaining=wishView_->remaining;const auto inputRevision=wishInput_.imported.revision;
    const auto inputCount=wishInput_.imported.events.size();const auto inputCoordinate=wishInput_.imported.coordinateMode;
    const auto inputForcedKey=wishInput_.analysis.forcedKey;const auto inputUse=wishSettings_.useInput;
    const auto inputTonic=wishSettings_.filter.fixedTonic;const auto inputModes=wishSettings_.filter.modes;
    const auto inputStrict=wishSettings_.filter.strict;const auto inputStyle=wishSettings_.style;
    const auto inputColor=wishSettings_.color;
    const bool inputHadForcedKey=bool(inputForcedKey);
    wishPage_=4;wishSelected_=stale.id;compactSelected_=stale.id;wishExpanded_=stale.id;
    seekGesture_=SeekGesture{SeekHit{},wishView_->displayGeneration,0};previewSeekHoverId_=stale.id;
    previewSeekCursorHover_=true;require(armMidi(stale,{}),"clear fixture arms a MIDI gesture");
    require(midiPending_&&midiClick_,"clear fixture carries pending MIDI data");previewCandidateId_=stale.id;
    const auto clearAuditionBefore=clearedAuditions;const auto stopAuditionBefore=stoppedAuditions;
    click(CompactAction::WishClear);
    const bool forcedKeyPreserved=inputHadForcedKey==bool(wishInput_.analysis.forcedKey)&&
        (!inputHadForcedKey||(inputForcedKey->tonic==wishInput_.analysis.forcedKey->tonic&&
                              inputForcedKey->mode==wishInput_.analysis.forcedKey->mode));
    require(wishSettings_.useInput==inputUse&&wishInput_.imported.revision==inputRevision&&
        wishInput_.imported.events.size()==inputCount&&wishInput_.imported.coordinateMode==inputCoordinate&&
        forcedKeyPreserved&&wishSettings_.filter.fixedTonic==inputTonic&&
        wishSettings_.filter.modes==inputModes&&wishSettings_.filter.strict==inputStrict&&
        wishSettings_.style==inputStyle&&wishSettings_.color==inputColor,
        "clear preserves input, query settings, and input association");
    require(wishView_->revision==clearRevision&&wishView_->displayGeneration>clearDisplayGeneration&&
        wishView_->remaining==clearRemaining&&wishView_->usesInput&&wishView_->items.empty()&&wishRequested_&&
        wishPage_==0&&wishSelected_.empty()&&compactSelected_.empty()&&wishExpanded_.empty()&&
        !seekGesture_&&previewSeekHoverId_.empty()&&!previewSeekCursorHover_&&!midiClick_&&!midiPending_&&
        !currentWishCandidate(stale),"clear empties visible cards and invalidates old interactions only");
    require(clearedAuditions==clearAuditionBefore+1&&stoppedAuditions==stopAuditionBefore,
        "clear releases only the removed wish audition through clearAudition");
    click(CompactAction::WishOne);wait();
    require(wishView_->usesInput&&!wishView_->items.empty()&&wishView_->items.front()->match.has_value()&&
        wishView_->items.front()->musicIdentity!=staleIdentity,
        "cleared input wish continues its existing no-replacement round");
    require(state_.imported.revision==original.imported.revision&&state_.imported.events.size()==original.imported.events.size(),"wish never clears global MIDI input");
    auto noInput=state_;noInput.imported.events.clear();++noInput.imported.revision;setSessionState(noInput);wishSettings_.useInput=true;resetWish();paint();
    require(std::any_of(compactHits_.begin(),compactHits_.end(),[](const auto& hit){return hit.action==CompactAction::WishOne&&!hit.enabled;}),"missing current input disables continuation draw");
    setSessionState(original);wishSettings_.useInput=false;resetWish();
    click(CompactAction::WishAdvanced);click(CompactAction::WishMode,9);click(CompactAction::WishAdvanced);click(CompactAction::WishTen);wait();require(wishView_->items.empty()&&wishView_->poolSize==0&&wishView_->error.empty(),"unavailable mode has a genuine empty pool");
    wishSettings_={};resetWish();click(CompactAction::WishTen);wait();paint();if(capture)capture("wish-final");
    actions_=actions;setPreviewRepeat(originalRepeat);return true;
}
}
