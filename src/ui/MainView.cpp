#include "io/FileDropRouter.h"
#include "io/MidiDragService.h"
#include "ui/EffectiveScale.h"
#include "MainView.h"
#include "session/ProductServices.h"
#include "WeightBarGeometry.h"
#include "product/ProductVersion.h"
#include "preview/VoiceLeadingMetrics.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/clinestyle.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cdropsource.h"
#include "vstgui/lib/cgraphicstransform.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/platform/iplatformframe.h"
#include <algorithm>
#include <cctype>
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
std::string bassLine(const std::vector<int>& bass) {
    constexpr const char* notes[]{"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    std::string result;
    for(std::size_t i=0;i<std::min<std::size_t>(bass.size(),6);++i) {
        if(i)result+=" → ";
        result+=notes[(bass[i]%12+12)%12];
    }
    if(bass.size()>6)result+=" → …";
    return result;
}
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
    setWantsFocus(true);
    // Keep the existing policy default (3); two rows describe only viewport height.
    layout_=computeLayout({rect.getWidth(),rect.getHeight(),1,session::Tab::Recommend,false,false});
}
void MainView::resizeLayout(int width,int height) {
    setViewSize(CRect(0,0,width,height),true);
    setMouseableArea(CRect(0,0,width,height));
    layout_=computeLayout({static_cast<double>(width),static_cast<double>(height),contentScale_,state_.tab,
        state_.tab==session::Tab::Recommend?(selectedChord_.has_value()||selectedCandidate_.has_value()||selectedEnrichment_.has_value()):
            state_.tab==session::Tab::Library&&selectedLibrary_.has_value(),
        state_.productMode==session::ProductMode::Enrich?!pinnedEnrichmentIds_.empty():!state_.pinnedCandidateIds.empty(),userZoom()});
    contentScroll_=std::clamp(contentScroll_,0.,layout_.laneScrollMax);
    rebuildTimeline();
    playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt;
    if(nameEdit_) {
        const double w=std::min(590.,layout_.viewport.width()-48),h=360;
        const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
        nameEdit_->setViewSize(editRect(CRect(x+140,y+53,x+w-22,y+91)),true);
        if(tagsEdit_)tagsEdit_->setViewSize(editRect(CRect(x+140,y+201,x+w-22,y+239)),true);
        if(noteEdit_)noteEdit_->setViewSize(editRect(CRect(x+140,y+261,x+w-22,y+299)),true);
    }
    invalid();
}
void MainView::setSimulatedContentScale(double scale) {
    contentScale_=std::isfinite(scale)&&scale>0?scale:1;
    resizeLayout(static_cast<int>(getViewSize().getWidth()),static_cast<int>(getViewSize().getHeight()));
}
CRect MainView::editRect(CRect rect) const {
    const auto z=userZoom();return {rect.left*z,rect.top*z,rect.right*z,rect.bottom*z};
}
void MainView::setUserZoom(std::uint32_t percent) {
    if(percent!=100&&percent!=125&&percent!=150)return;
    endForm();state_.uiZoomPercent=percent;resizeLayout(static_cast<int>(getViewSize().getWidth()),
        static_cast<int>(getViewSize().getHeight()));notifyState();
}
bool MainView::runZoomSmoke() {
    const auto saved=state_;const auto savedRecommendations=recommendations_;
    bool okay=true;
    const auto click=[&](UiRect r) {CPoint point{(r.left+r.right)/2*userZoom(),(r.top+r.bottom)/2*userZoom()};
        onMouseDown(point,CButtonState{kLButton});};
    state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Continue;state_.intent.reset();
    selectedChord_.reset();selectedCandidate_.reset();selectedEnrichment_.reset();selectedLibrary_.reset();
    refreshLayout();click(layout_.topControls[5]);okay=okay&&state_.productMode==session::ProductMode::Enrich;
    click(layout_.topControls[4]);okay=okay&&state_.productMode==session::ProductMode::Continue;
    ContinuationCandidate candidate;candidate.id="zoom-smoke";candidate.rankingScore=90;
    candidate.intent=PhraseIntent::Resolve;candidate.continuation.push_back({"C",4,{1,0},ChordQuality::Major,{}});
    recommendations_={};recommendations_.groups[0].push_back(candidate);contentScroll_=0;refreshLayout();
    click(candidateRowGeometry(layout_.lanes[0],0,true).why);okay=okay&&selectedCandidate_.has_value();
    click(layout_.topControls[4]);okay=okay&&activeOverlayKind()!=OverlayKind::Transient;
    if(!timelineBlocks_.empty()) {
        const auto tile=chordTileRect(0);click({tile.left,tile.top,tile.right,tile.bottom});
        okay=okay&&selectedChord_==0;
        click(layout_.topControls[4]);okay=okay&&!selectedChord_;
    }
    state_.tab=session::Tab::Library;refreshLayout();
    okay=okay&&layout_.libraryRows>=1&&layout_.content.height()>=165&&!factory_.empty();
    if(!factory_.empty()) {click({layout_.content.left+12,layout_.content.top+89,
        layout_.content.right-12,layout_.content.top+120});okay=okay&&selectedLibrary_.has_value();}
    state_=saved;recommendations_=savedRecommendations;
    selectedChord_.reset();selectedCandidate_.reset();selectedLibrary_.reset();refreshLayout();invalid();return okay;
}
bool MainView::runCandidateScrollSmoke() {
    const auto savedState=state_;const auto savedActions=actions_;const auto savedPolicy=visibility_;
    const auto savedRecommendations=recommendations_;const auto savedEnrichments=enrichments_;
    const auto savedContinueScroll=continuationScroll_;const auto savedEnrichScroll=enrichmentScroll_;
    const auto savedContentScroll=contentScroll_;
    bool okay=true;
    const midi::MidiClipPayload fake{{'M','T','h','d'},"HC_test.mid",1,3,{}, {'D','A','W'}};
    constexpr PhraseIntent intents[]{PhraseIntent::Resolve,PhraseIntent::Develop,PhraseIntent::Loop,PhraseIntent::Color};
    for(const auto zoom:{100u,125u,150u})for(const bool enrich:{false,true})
    for(const std::size_t count:{0u,1u,2u,3u,6u}) {
        state_.uiZoomPercent=zoom;state_.tab=session::Tab::Recommend;state_.intent.reset();state_.pinnedCandidateIds.clear();
        state_.productMode=enrich?session::ProductMode::Enrich:session::ProductMode::Continue;
        selectedChord_.reset();selectedCandidate_.reset();selectedEnrichment_.reset();pinnedEnrichmentIds_.clear();
        recommendations_={};enrichments_={};continuationScroll_.fill(0.);enrichmentScroll_.fill(0.);
        // Synthetic 6-row coverage exercises reusable geometry without changing production policy.
        visibility_=savedPolicy;visibility_.maxPerGroup=6;
        for(std::size_t group=0;group<(enrich?3u:4u);++group)for(std::size_t index=0;index<count;++index) {
            const auto id="scroll-"+std::to_string(group)+"-"+std::to_string(index);
            if(enrich){enrichment::EnrichmentCandidate c;c.id=id;c.fingerprint=id;c.score=90.f-static_cast<float>(index);
                ChordEvent event;event.name="C";event.durationQN=2.+index;event.openEnded=false;c.progression.push_back(event);
                enrichments_.groups[group].push_back(c);
            } else {ContinuationCandidate c;c.id=id;c.intent=intents[group];c.key={PitchClass::C,Mode::Major};
                c.rankingScore=90.f-static_cast<float>(index);c.suggestedCurrentChordDurationQN=2.+index;
                c.continuation.push_back({"C",2.+index+group*.1,{1,0},ChordQuality::Major,{}});
                recommendations_.groups[group].push_back(c);}
        }
        actions_={};std::string heard,exported,snapshot;
        actions_.audition=[&](const ContinuationCandidate& c){heard=session::continuationFingerprint(c);};
        actions_.auditionEnrichment=[&](const enrichment::EnrichmentCandidate& c){heard=c.fingerprint;};
        actions_.exportMidi=[](const ContinuationCandidate&){return std::string{};};
        actions_.exportEnrichmentMidi=[](const enrichment::EnrichmentCandidate&){return std::string{};};
        actions_.midiPayload=[&](const ContinuationCandidate& c){exported=session::continuationFingerprint(c);return midi::PayloadResult{fake,{}};};
        actions_.enrichmentMidiPayload=[&](const enrichment::EnrichmentCandidate& c){exported=c.fingerprint;return midi::PayloadResult{fake,{}};};
        actions_.saveSnapshot=[&](const ContinuationCandidate& c){snapshot=session::continuationFingerprint(c);return std::string{};};
        actions_.saveEnrichmentSnapshot=[&](const enrichment::EnrichmentCandidate& c){snapshot=c.fingerprint;return std::string{};};
        refreshLayout();
        for(std::size_t group=0;group<(enrich?3u:4u);++group) {
            auto lane=layout_.lanes[group];contentScroll_=std::clamp(lane.bottom-layout_.content.bottom,0.,layout_.laneScrollMax);
            lane.top-=contentScroll_;lane.bottom-=contentScroll_;
            auto& offset=enrich?enrichmentScroll_[group]:continuationScroll_[group];
            ScrollableCandidateList before(lane,count,offset);const auto parent=contentScroll_;
            MouseWheelEvent wheel;wheel.mousePosition={(lane.left+lane.right)/2*userZoom(),(before.viewport.top+20)*userZoom()};wheel.deltaY=-100;
            onMouseWheelEvent(wheel);okay=okay&&wheel.consumed&&contentScroll_==parent;
            ScrollableCandidateList list(lane,count,offset);okay=okay&&offset==list.maximum;
            wheel.consumed=false;onMouseWheelEvent(wheel);okay=okay&&wheel.consumed&&offset==list.maximum&&contentScroll_==parent;
            if(count==0){okay=okay&&!list.hit(lane.left+20,list.viewport.top+10);continue;}
            const std::size_t index=count-1;
            const auto expected=enrich?enrichments_.groups[group][index].fingerprint:session::continuationFingerprint(recommendations_.groups[group][index]);
            const auto point=[&](UiRect r){return CPoint{(r.left+r.right)/2*userZoom(),(r.top+r.bottom)/2*userZoom()};};
            UiRect play,midiRect,why,snap;
            if(enrich){const auto g=enrichmentRowGeometry(lane,list,index);play=g.audition;midiRect=g.midi;why=g.why;snap=g.snapshot;}
            else {const auto g=list.continuationRow(lane,index,true);play=g.audition;midiRect=g.midi;why=g.why;snap=g.snapshot;}
            auto p=point(play);onMouseDown(p,CButtonState{kLButton});okay=okay&&heard==expected;
            p=point(midiRect);onMouseDown(p,CButtonState{kLButton});
            okay=okay&&exported==expected&&midiPending_&&midiPending_->dawClipBytes==fake.dawClipBytes;
            okay=okay&&shouldStartDrag(p,{p.x+8,p.y});midiPending_.reset();midiClick_={};
            p=point(snap);onMouseDown(p,CButtonState{kLButton});okay=okay&&snapshot==expected;
            p=point(why);onMouseDown(p,CButtonState{kLButton});
            okay=okay&&(enrich?(selectedEnrichment_&&selectedEnrichment_->first==group&&selectedEnrichment_->second==index):
                (selectedCandidate_&&selectedCandidate_->first==group&&selectedCandidate_->second==index));
            if(count==6&&group==0){
                const auto saved=captureEditorUiState();auto recreated=owned(new MainView(getViewSize(),{}));
                recreated->setSessionState(state_);recreated->setRecommendations(recommendations_);recreated->setEnrichments(enrichments_);
                recreated->restoreEditorUiState(saved);const auto restored=recreated->captureEditorUiState();
                okay=okay&&restored.continuationId==saved.continuationId&&restored.continuationFingerprint==saved.continuationFingerprint&&
                    restored.enrichmentId==saved.enrichmentId&&restored.enrichmentFingerprint==saved.enrichmentFingerprint;
            }
            dismissTransientOverlay();
            wheel.deltaY=100;wheel.consumed=false;onMouseWheelEvent(wheel);okay=okay&&offset==0&&contentScroll_==parent;
            // A pinned comparison is persistent, and must not disable group scrolling.
            if(enrich)pinnedEnrichmentIds_.push_back(enrichments_.groups[group][index].id);
            else state_.pinnedCandidateIds.push_back(recommendations_.groups[group][index].id);
            wheel.deltaY=-100;wheel.consumed=false;onMouseWheelEvent(wheel);okay=okay&&wheel.consumed&&offset==list.maximum&&contentScroll_==parent;
            pinnedEnrichmentIds_.clear();state_.pinnedCandidateIds.clear();
        }
        if(!enrich){const auto original=session::presentationIndices(recommendations_,savedPolicy);
            for(const auto& group:original)okay=okay&&group.size()<=savedPolicy.maxPerGroup;}
        MouseWheelEvent outside;outside.mousePosition={(layout_.content.left+10)*userZoom(),(layout_.lanes[0].bottom+4)*userZoom()};outside.deltaY=-1;
        contentScroll_=0;onMouseWheelEvent(outside);okay=okay&&outside.consumed&&contentScroll_==std::min(48.,layout_.laneScrollMax);
    }
    state_=savedState;actions_=savedActions;visibility_=savedPolicy;recommendations_=savedRecommendations;enrichments_=savedEnrichments;
    continuationScroll_=savedContinueScroll;enrichmentScroll_=savedEnrichScroll;contentScroll_=savedContentScroll;
    selectedCandidate_.reset();selectedEnrichment_.reset();midiPending_.reset();midiClick_={};refreshLayout();invalid();
    return okay;
}
bool MainView::runMidiWorkflowSmoke() {
    const auto savedState=state_;const auto savedActions=actions_;const auto savedRecommendations=recommendations_;
    const auto savedEnrichments=enrichments_;const auto savedPreview=previewCandidateId_;
    bool okay=true;
    for(const auto zoom:{100u,125u,150u}) {
        state_.uiZoomPercent=zoom;state_.tab=session::Tab::Recommend;state_.intent.reset();state_.pinnedCandidateIds.clear();pinnedEnrichmentIds_.clear();contentScroll_=0;
        recommendations_={};enrichments_={};
        for(int i=0;i<3;++i) {
            ContinuationCandidate c;c.id="midi-test-"+std::to_string(i);c.intent=PhraseIntent::Resolve;c.key={PitchClass::C,Mode::Major};
            c.suggestedCurrentChordDurationQN=2+i;c.rankingScore=90.f-i;
            c.continuation.push_back({i==2?"Cmaj7":"C",2.+i,{1,0},i==2?ChordQuality::Major7:ChordQuality::Major,{}});
            recommendations_.groups[0].push_back(c);
            enrichment::EnrichmentCandidate e;e.id="enrich-test-"+std::to_string(i);e.score=90.f-i;e.fingerprint=e.id;
            ChordEvent event;event.name=i==2?"Am7":"C";event.durationQN=4;event.openEnded=false;e.progression.push_back(event);
            e.techniques.push_back(enrichment::TechniqueID::Inversion);enrichments_.groups[0].push_back(e);
        }
        for(const bool enrich:{false,true})for(const bool more:{false,true}) {
            state_.productMode=enrich?session::ProductMode::Enrich:session::ProductMode::Continue;
            selectedCandidate_.reset();selectedEnrichment_.reset();selectedChord_.reset();selectedLibrary_.reset();
            const std::size_t index=more?2:0;
            if(more){if(enrich)selectedEnrichment_={{0,index}};else selectedCandidate_={{0,index}};}
            refreshLayout();
            const auto expected=enrich?enrichments_.groups[0][index].fingerprint:session::continuationFingerprint(recommendations_.groups[0][index]);
            std::string heard,exported,snapshot;int saved{};
            actions_={};
            actions_.audition=[&](const ContinuationCandidate& c){heard=session::continuationFingerprint(c);};
            actions_.auditionEnrichment=[&](const enrichment::EnrichmentCandidate& c){heard=c.fingerprint;};
            actions_.exportMidi=[](const ContinuationCandidate&){return std::string{};};
            actions_.exportEnrichmentMidi=[](const enrichment::EnrichmentCandidate&){return std::string{};};
            const midi::MidiClipPayload fake{{'M','T','h','d'},"HC_test.mid",1,3,{}, {'D','A','W'}};
            actions_.midiPayload=[&](const ContinuationCandidate& c){exported=session::continuationFingerprint(c);return midi::PayloadResult{fake,{}};};
            actions_.enrichmentMidiPayload=[&](const enrichment::EnrichmentCandidate& c){exported=c.fingerprint;return midi::PayloadResult{fake,{}};};
            actions_.saveMidiPayload=[&](const midi::MidiClipPayload& p){++saved;okay=okay&&p.smfBytes==fake.smfBytes;return std::string{};};
            actions_.saveSnapshot=[&](const ContinuationCandidate& c){snapshot=session::continuationFingerprint(c);return std::string{};};
            actions_.saveEnrichmentSnapshot=[&](const enrichment::EnrichmentCandidate& c){snapshot=c.fingerprint;return std::string{};};
            const auto actionRect=[&](int action) {
                if(more){const auto a=layout_.inspector;const auto width=(a.width()-28)/(enrich?3:4);
                    return UiRect{a.left+14+action*width,a.bottom-39,a.left+14+(action+1)*width,a.bottom-8};}
                const auto lane=layout_.lanes[0];
                if(enrich){const ScrollableCandidateList list(lane,enrichments_.groups[0].size(),0.);
                    const auto g=enrichmentRowGeometry(lane,list,0);return action==0?g.audition:action==1?g.midi:g.snapshot;}
                const auto g=candidateRowGeometry(lane,0,true);return action==0?g.audition:action==1?g.midi:g.snapshot;
            };
            const auto point=[&](UiRect r){return CPoint{(r.left+r.right)/2*userZoom(),(r.top+r.bottom)/2*userZoom()};};
            auto p=point(actionRect(0));onMouseDown(p,CButtonState{kLButton});okay=okay&&heard==expected;
            p=point(actionRect(1));onMouseDown(p,CButtonState{kLButton});
            okay=okay&&exported==expected&&saved==0&&midiPending_.has_value();
            auto moved=p;moved.x+=1;onMouseMoved(moved,CButtonState{kLButton});okay=okay&&saved==0&&midiPending_.has_value();
            okay=okay&&!shouldStartDrag(p,moved)&&shouldStartDrag(p,CPoint{p.x+8,p.y});
            onMouseUp(p,CButtonState{kLButton});okay=okay&&saved==1&&!midiPending_;
            p=point(actionRect(2));onMouseDown(p,CButtonState{kLButton});okay=okay&&snapshot==expected;
            if(more){previewCandidateId_="playing";CPoint outside{0,0};onMouseDown(outside,CButtonState{kLButton});
                okay=okay&&!selectedCandidate_&&!selectedEnrichment_&&previewCandidateId_=="playing";
                if(enrich){selectedEnrichment_={{0,index}};auto changed=enrichments_;changed.groups[0][index].fingerprint="changed";setEnrichments(changed);okay=okay&&!selectedEnrichment_;}
                else {selectedCandidate_={{0,index}};auto changed=recommendations_;changed.groups[0][index].suggestedCurrentChordDurationQN=8.;setRecommendations(changed);okay=okay&&!selectedCandidate_;}
            }
            std::string importedPath;bool importOpen=true;
            actions_.importMidi=[&](bool open,const std::filesystem::path& path){importOpen=open;importedPath=path.generic_string();};
            const char filePath[]="D:/fixture.mid";
            const auto package=CDropSource::create(filePath,sizeof(filePath),IDataPackage::kFilePath);
            const void* received{};IDataPackage::Type type{};
            okay=okay&&package->getCount()==1&&package->getData(0,received,type)==sizeof(filePath)&&type==IDataPackage::kFilePath&&std::string(static_cast<const char*>(received))==filePath;
            const auto center=point(layout_.timeline);
            okay=okay&&onDrop({package,center,{}})&&importedPath==filePath&&!importOpen;
            const auto privateData=CDropSource::create("private",8,IDataPackage::kBinary);
            okay=okay&&!onDrop({privateData,center,{}});
        }
    }
    state_=savedState;actions_=savedActions;recommendations_=savedRecommendations;enrichments_=savedEnrichments;previewCandidateId_=savedPreview;
    selectedCandidate_.reset();selectedEnrichment_.reset();midiPending_.reset();midiClick_={};refreshLayout();invalid();return runCandidateScrollSmoke()&&okay;
}
void MainView::refreshLayout() {
    const auto priorWidth=layout_.timeline.width();
    layout_=computeLayout({getViewSize().getWidth(),getViewSize().getHeight(),contentScale_,state_.tab,
        state_.tab==session::Tab::Recommend?(selectedChord_.has_value()||selectedCandidate_.has_value()||selectedEnrichment_.has_value()):
            state_.tab==session::Tab::Library&&selectedLibrary_.has_value(),
        state_.productMode==session::ProductMode::Enrich?!pinnedEnrichmentIds_.empty():!state_.pinnedCandidateIds.empty(),userZoom()});
    contentScroll_=std::clamp(contentScroll_,0.,layout_.laneScrollMax);
    if (priorWidth!=layout_.timeline.width()) rebuildTimeline();
}
OverlayKind MainView::activeOverlayKind() const noexcept {
    if (form_!=Form::None) return OverlayKind::Modal;
    if (state_.tab==session::Tab::Recommend &&
        (selectedChord_ || selectedCandidate_ || selectedEnrichment_)) return OverlayKind::Transient;
    if (state_.tab==session::Tab::Library && selectedLibrary_) return OverlayKind::Persistent;
    if (!state_.pinnedCandidateIds.empty() || !pinnedEnrichmentIds_.empty()) return OverlayKind::Persistent;
    return OverlayKind::None;
}
void MainView::dismissTransientOverlay() {
    if (activeOverlayKind()!=OverlayKind::Transient) return;
    selectedChord_.reset(); selectedCandidate_.reset(); selectedEnrichment_.reset();
    inspectorScroll_=0;
    refreshLayout(); invalid();
}
void MainView::focusTransientOverlay() {
    if (auto* frame=getFrame()) frame->setFocusView(this);
}
bool MainView::runHostInteractionSmoke(){
    const auto saved=captureEditorUiState();const auto state=state_;
    bool okay=true;
    state_.tab=session::Tab::Recommend;
    if(state_.imported.events.empty()){ChordEvent c;c.name="C";c.durationQN=4;state_.imported.replace({c},TimelineCoordinateMode::RelativeToSelection);}
    selectedChord_=0;refreshLayout();focusTransientOverlay();
    KeyboardEvent key;key.type=EventType::KeyDown;key.virt=VirtualKey::Space;onKeyboardEvent(key);okay=okay&&!key.consumed;
    key.virt=VirtualKey::Escape;onKeyboardEvent(key);okay=okay&&key.consumed&&!selectedChord_;
    key.consumed=false;onKeyboardEvent(key);okay=okay&&!key.consumed;
    state_.pinnedCandidateIds={"persistent"};key.consumed=false;onKeyboardEvent(key);
    okay=okay&&!key.consumed&&state_.pinnedCandidateIds.size()==1;
    beginForm(Form::Search,"typing");
    if(auto* frame=getFrame())okay=okay&&frame->getFocusView()==nameEdit_;
    endForm();
    state_=state;restoreEditorUiState(saved);refreshLayout();return okay;
}
void MainView::onKeyboardEvent(KeyboardEvent& event) {
    if (event.type==EventType::KeyDown && event.virt==VirtualKey::Escape &&
        dismissOnEscape(activeOverlayKind())) {
        dismissTransientOverlay();
        event.consumed=true;
        return;
    }
    CView::onKeyboardEvent(event);
}
void MainView::onMouseWheelEvent(MouseWheelEvent& event) {
    auto point=event.mousePosition;point.x/=userZoom();point.y/=userZoom();
    const auto delta=event.deltaY*48.0;
    if(layout_.inspectorMode!=InspectorMode::None&&layout_.inspector.contains(point.x,point.y)) {
        inspectorScroll_=std::clamp(inspectorScroll_-delta,0.,inspectorScrollMax_);
        invalid();event.consumed=true;return;
    }
    try {
    if(state_.tab==session::Tab::Recommend&&activeOverlayKind()!=OverlayKind::Modal&&
       (activeOverlayKind()!=OverlayKind::Transient||layout_.inspectorMode==InspectorMode::Side)&&layout_.content.contains(point.x,point.y)) {
        const auto presentation=continuationPresentation();
        const bool enrich=state_.productMode==session::ProductMode::Enrich;
        for(std::size_t visual=0;visual<(enrich?3u:4u);++visual) {
            auto lane=layout_.lanes[visual];lane.top-=contentScroll_;lane.bottom-=contentScroll_;
            if(!lane.contains(point.x,point.y))continue;
            const auto group=enrich?visual:groupForVisual(visual,state_.intent);
            auto& scroll=enrich?enrichmentScroll_[group]:continuationScroll_[group];
            const ScrollableCandidateList list(lane,enrich?enrichments_.groups[group].size():presentation[group].size(),scroll);
            // Deliberately consume at group boundaries too: never double-scroll the parent.
            scroll=list.scroll(delta);invalid();event.consumed=true;return;
        }
    }
    } catch(...) {event.consumed=true;return;}
    if (state_.tab==session::Tab::Recommend&&layout_.timeline.contains(point.x,point.y)) {
        timelineScroll_=std::clamp(timelineScroll_-delta,0.,
            std::max(0.,timelineContentWidth_-layout_.timeline.width()));
        playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt;
        invalid(); event.consumed=true; return;
    }
    if (layout_.content.contains(point.x,point.y)) {
        contentScroll_=std::clamp(contentScroll_-delta,0.,layout_.laneScrollMax);
        invalid(); event.consumed=true; return;
    }
    CView::onMouseWheelEvent(event);
}
namespace {
io::FileDropRoute fileRoute(IDataPackage* package){
    std::vector<std::string> names;if(!package)return {};
    bool invalidFile=false;
    for(std::uint32_t i=0;i<std::min<std::uint32_t>(32,package->getCount());++i){
        const void* bytes{};IDataPackage::Type type{};const auto size=package->getData(i,bytes,type);
        if(type!=IDataPackage::kFilePath)continue;
        if(!bytes||size==0||size>32768){invalidFile=true;continue;}
        names.emplace_back(static_cast<const char*>(bytes),size);
    }
    auto route=io::routeFiles(names);route.hasFiles=route.hasFiles||invalidFile;return route;
}
}
SharedPointer<IDropTarget> MainView::getDropTarget() { return this; }
DragOperation MainView::onDragEnter(DragEventData e) {
    try{if(!e.drag)return DragOperation::None;
        const auto route=fileRoute(e.drag);auto point=e.pos;point.x/=userZoom();point.y/=userZoom();
        if(route.hasFiles)return !route.midiFiles.empty()&&layout_.timeline.contains(point.x,point.y)?DragOperation::Copy:DragOperation::None;
        return actions_.drop?DragOperation::Copy:DragOperation::None;
    }catch(...){return DragOperation::None;}
}
DragOperation MainView::onDragMove(DragEventData e) {return onDragEnter(e);}
void MainView::onDragLeave(DragEventData) {}
bool MainView::onDrop(DragEventData e) {
    try {
        if(!e.drag)return false;
        auto point=e.pos;point.x/=userZoom();point.y/=userZoom();const auto route=fileRoute(e.drag);
        if(actions_.observeDrop)actions_.observeDrop(route.hasFiles,!route.midiFiles.empty());
        if(route.hasFiles){
            if(route.midiFiles.empty()||!layout_.timeline.contains(point.x,point.y))return false;
            if(actions_.importMidiFiles){actions_.importMidiFiles(route.midiFiles);return true;}
            if(actions_.importMidi){actions_.importMidi(false,route.midiFiles.front());return true;}
            return false;
        }
        if(!actions_.drop)return false;actions_.drop(e.drag);return true;
    } catch (...) {return false;}
}
MainView::EditorUiState MainView::captureEditorUiState() const {
    EditorUiState out;
    if(const auto* c=selectedCandidate()){out.continuationId=c->id;out.continuationFingerprint=session::continuationFingerprint(*c);}
    if(const auto* c=selectedEnrichment()){out.enrichmentId=c->id;out.enrichmentFingerprint=c->fingerprint;}
    out.chord=selectedChord_;out.continuationScroll=continuationScroll_;out.enrichmentScroll=enrichmentScroll_;
    out.contentScroll=contentScroll_;out.timelineScroll=timelineScroll_;out.inspectorScroll=inspectorScroll_;
    out.pinnedEnrichmentIds=pinnedEnrichmentIds_;out.showColorHints=showColorHints_;out.colorPreference=colorPreference_;return out;
}
void MainView::restoreEditorUiState(const EditorUiState& s){
    showColorHints_=s.showColorHints;setColorPreference(s.colorPreference);
    if(s.chord&&*s.chord<state_.imported.events.size())selectedChord_=s.chord;
    for(std::size_t g=0;g<recommendations_.groups.size();++g)for(std::size_t i=0;i<recommendations_.groups[g].size();++i){
        const auto& c=recommendations_.groups[g][i];if(c.id==s.continuationId&&session::continuationFingerprint(c)==s.continuationFingerprint)selectedCandidate_={{g,i}};
    }
    for(std::size_t g=0;g<enrichments_.groups.size();++g)for(std::size_t i=0;i<enrichments_.groups[g].size();++i){
        const auto& c=enrichments_.groups[g][i];if(c.id==s.enrichmentId&&c.fingerprint==s.enrichmentFingerprint)selectedEnrichment_={{g,i}};
    }
    continuationScroll_=s.continuationScroll;enrichmentScroll_=s.enrichmentScroll;
    for(auto& v:continuationScroll_)if(!std::isfinite(v)||v<0)v=0;
    for(auto& v:enrichmentScroll_)if(!std::isfinite(v)||v<0)v=0;
    contentScroll_=std::isfinite(s.contentScroll)?std::max(0.,s.contentScroll):0.;
    timelineScroll_=std::isfinite(s.timelineScroll)?std::max(0.,s.timelineScroll):0.;
    inspectorScroll_=std::isfinite(s.inspectorScroll)?std::max(0.,s.inspectorScroll):0.;
    pinnedEnrichmentIds_.clear();for(const auto& id:s.pinnedEnrichmentIds)if(findEnrichment(id))pinnedEnrichmentIds_.push_back(id);
    refreshLayout();invalid();
}
void MainView::notifyState() { sessionDirty_=true; if (actions_.stateChanged) actions_.stateChanged(state_); invalid(); }
void MainView::setSessionState(const session::PluginSessionState& s) {
    const bool timelineChanged=state_.imported.revision!=s.imported.revision || state_.imported.events.size()!=s.imported.events.size();
    const bool zoomChanged=state_.uiZoomPercent!=s.uiZoomPercent;
    if(state_.intent!=s.intent||state_.style!=s.style||state_.tendency!=s.tendency||state_.constraints!=s.constraints)enrichmentColorRankDirty_=true;
    state_=s;
    if(timelineChanged){colorSource_=color::HarmonyColorAnalyzer::prepare(state_.imported.events);colorPaths_.clear();colorRankDirty_=true;enrichmentColorRankDirty_=true;}
    if(zoomChanged)resizeLayout(static_cast<int>(getViewSize().getWidth()),static_cast<int>(getViewSize().getHeight()));
    if (timelineChanged) {
        pinnedSnapshots_.clear(); pinnedEnrichmentIds_.clear(); selectedChord_.reset();
        selectedCandidate_.reset(); selectedEnrichment_.reset(); rebuildTimeline();
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
    state_.imported=s; selectedChord_.reset(); selectedCandidate_.reset();
    colorSource_=color::HarmonyColorAnalyzer::prepare(s.events);colorPaths_.clear();colorRankDirty_=true;enrichmentColorRankDirty_=true;
    selectedEnrichment_.reset(); pinnedEnrichmentIds_.clear(); rebuildTimeline();
    currentLocation_=locateCurrentChord(state_.imported,projectQN_); playheadX_=projectQN_?projectQNToX(*projectQN_):std::nullopt; invalid();
}
void MainView::setAnalysis(const HarmonicAnalysisResult& a) { analysis_=a;colorRankDirty_=true;enrichmentColorRankDirty_=true; invalid(); }
void MainView::setMatches(const std::vector<MatchResult>& m, std::string status) { matches_=m; matchStatus_=std::move(status); invalid(); }
void MainView::setRecommendations(const RecommendationSet& r) {
    const auto oldSelected=selectedCandidate()?selectedCandidate()->id:std::string{};
    const auto oldFingerprint=selectedCandidate()?session::continuationFingerprint(*selectedCandidate()):std::string{};
    recommendations_=r;
    colorPaths_.clear();colorRankDirty_=true;enrichmentColorRankDirty_=true;
    continuationScroll_.fill(0.);
    if (!oldSelected.empty() && (!selectedCandidate() || selectedCandidate()->id!=oldSelected ||
        session::continuationFingerprint(*selectedCandidate())!=oldFingerprint)) selectedCandidate_.reset();
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
void MainView::setEnrichments(const enrichment::EnrichmentResult& value) {
    enrichmentScroll_.fill(0.);
    const auto oldSelected=selectedEnrichment()?selectedEnrichment()->id:std::string{};
    const auto oldFingerprint=selectedEnrichment()?selectedEnrichment()->fingerprint:std::string{};
    enrichments_=value;
    colorPaths_.clear();colorRankDirty_=true;enrichmentColorRankDirty_=true;
    if (!oldSelected.empty() && (!selectedEnrichment() || selectedEnrichment()->id!=oldSelected || selectedEnrichment()->fingerprint!=oldFingerprint))
        selectedEnrichment_.reset();
    std::erase_if(pinnedEnrichmentIds_,[&](const std::string& id) { return !findEnrichment(id); });
    refreshLayout(); invalid();
}
void MainView::setPreviewPosition(std::string id,double qn,double totalQN) {
    previewCandidateId_=std::move(id); previewQN_=qn; previewTotalQN_=totalQN; invalid();
}
void MainView::setColorPreference(color::Preference value) {
    if(colorPreference_==value)return;
    colorPreference_=value;colorRankDirty_=true;enrichmentColorRankDirty_=true;continuationScroll_.fill(0.);enrichmentScroll_.fill(0.);invalid();
}
void MainView::refreshColorRanking() {
    if(!colorRankDirty_)return;
    for(std::size_t group=0;group<4;++group) {
        std::vector<color::PathColor> paths;
        if(colorPreference_!=color::Preference::Off)
            for(const auto& candidate:recommendations_.groups[group])paths.push_back(candidateColor(candidate));
        colorRanks_[group]=color::ColorPreferenceReranker::rank(colorSource_,analysis_.keyCandidates,
            state_.forcedKey?state_.forcedKey:analysis_.selectedKey?std::optional<KeySignature>{analysis_.selectedKey->key}:std::nullopt,
            recommendations_.groups[group],paths,colorPreference_);
    }
    colorRankDirty_=false;
}
session::RecommendationPresentation MainView::continuationPresentation() {
    auto result=session::presentationIndices(recommendations_,visibility_);
    if(colorPreference_==color::Preference::Off)return result; // Exact original path, no reranking work.
    refreshColorRanking();
    for(std::size_t group=0;group<4;++group) {
        std::vector<std::size_t> rank(colorRanks_[group].order.size());
        for(std::size_t i=0;i<rank.size();++i)rank[colorRanks_[group].order[i]]=i;
        std::stable_sort(result[group].begin(),result[group].end(),[&](auto a,auto b){return rank[a]<rank[b];});
    }
    return result;
}
void MainView::refreshEnrichmentColorRanking() {
    if(!enrichmentColorRankDirty_)return;
    for(std::size_t group=0;group<3;++group) {
        std::vector<color::PathColor> paths;
        if(colorPreference_!=color::Preference::Off)
            for(const auto& c:enrichments_.groups[group])paths.push_back(candidateColor(c));
        enrichmentColorRanks_[group]=color::ColorPreferenceReranker::rankEnrichment(state_.imported.events,
            analysis_,state_.intent,state_.tendency,enrichments_.groups[group],paths,colorPreference_);
    }
    enrichmentColorRankDirty_=false;
}
const std::vector<std::size_t>& MainView::enrichmentOrder(std::size_t group) {
    refreshEnrichmentColorRanking();return enrichmentColorRanks_[group].order;
}
const color::PathColor& MainView::candidateColor(const ContinuationCandidate& candidate) {
    const auto key="c:"+session::continuationFingerprint(candidate);
    if(const auto found=colorPaths_.find(key);found!=colorPaths_.end())return found->second;
    return colorPaths_.emplace(key,color::HarmonyColorAnalyzer::analyzeContinuation(colorSource_,candidate)).first->second;
}
const color::PathColor& MainView::candidateColor(const enrichment::EnrichmentCandidate& candidate) {
    const auto key="e:"+candidate.id+":"+candidate.fingerprint;
    if(const auto found=colorPaths_.find(key);found!=colorPaths_.end())return found->second;
    return colorPaths_.emplace(key,color::ColorPreferenceReranker::analyzeEnrichment(candidate.progression)).first->second;
}
void MainView::drawColorHint(CDrawContext* dc,const ColorHint& hint,CRect rect) {
    if(hint.label.empty()||rect.getWidth()<20)return;
    const auto tone=hint.tone==ColorTone::Warm?CColor(202,177,119,255):
        hint.tone==ColorTone::Cool?CColor(114,171,188,255):muted;
    box(dc,CRect(rect.left,rect.top+3,rect.left+2,rect.bottom-3),tone);
    rect.left+=7;auto label=hint.label;dc->setFont(kNormalFont);
    if(dc->getStringWidth(label.c_str())>rect.getWidth())
        if(const auto split=label.find(" · ");split!=std::string::npos)label.resize(split);
    line(dc,label,rect,tone);
}
bool MainView::runEnrichmentHintSmoke(CDrawContext* dc,const std::function<bool(CRect,ColorTone)>& painted) {
    session::PluginSessionState state;state.productMode=session::ProductMode::Enrich;
    Progression phrase;
    const char* names[]{"Dmin","C/E","Bb","A"};
    const PitchClass roots[]{PitchClass::D,PitchClass::C,PitchClass::Bb,PitchClass::A};
    for(int i=0;i<4;++i){ChordEvent e;e.name=names[i];e.root=roots[i];e.bass=i==1?PitchClass::E:roots[i];
        e.quality=i?ChordQuality::Major:ChordQuality::Minor;e.startQN=i*2.;e.durationQN=2.;e.openEnded=false;phrase.push_back(e);}
    if(!state.imported.replace(phrase,TimelineCoordinateMode::AbsoluteProjectQN))return false;
    setSessionState(state);
    enrichment::EnrichmentCandidate candidate;candidate.id="rc2-hint";candidate.fingerprint="rc2-hint";
    candidate.progression=phrase;candidate.score=82;
    const auto render=[&](const enrichment::EnrichmentCandidate& c){
        enrichment::EnrichmentResult result;result.groups[0].push_back(c);setEnrichments(result);
        refreshLayout();const auto lane=layout_.lanes[0];const ScrollableCandidateList list(lane,1,0.);
        const auto rect=viewRect(enrichmentRowGeometry(lane,list,0).badges);
        const auto hint=candidateHint(enrichments_.groups[0][0]);
        dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
        return !hint.label.empty()&&painted(rect,hint.tone);
    };
    for(const auto locale:{session::Locale::ZhCN,session::Locale::EnUS}){
        state_.locale=locale;
        // The same ordinary candidate with complete timing retains the old summary.
        if(!render(candidate))return false;
        const auto& complete=candidateColor(enrichments_.groups[0][0]);
        const auto normal=candidateHint(enrichments_.groups[0][0]);
        if(complete.status!=color::Status::Known||normal.staticOnly||normal.label!=colorHint(complete,locale).label)return false;
        // Reproduce the screenshot: the final A is OPEN, bass/inversion are explicit.
        auto open=candidate;open.progression.back().durationQN.reset();open.progression.back().openEnded=true;
        if(!render(open))return false;
        const auto& path=candidateColor(enrichments_.groups[0][0]);const auto hint=candidateHint(enrichments_.groups[0][0]);
        if(path.status!=color::Status::Unknown||path.meanW||!hint.staticOnly||hint.tone!=ColorTone::Neutral||
            hint.explanation.find(t("color.whyMissingTiming"))==std::string::npos||path.positions[1].chord.status!=color::Status::Known||
            !enrichments_.groups[0][0].progression.back().openEnded||enrichments_.groups[0][0].progression.back().durationQN)return false;
        const auto why=explainWhy("functional",hint,locale,color::Preference::Warmer,color::RankReason::InsufficientColor);
        if(why.sentences.size()!=3||why.sentences[1]!=hint.explanation)return false;
        setColorHints(false);if(!candidateHint(enrichments_.groups[0][0]).label.empty())return false;setColorHints(true);
        // Genuine multi-direction ambiguity is never replaced by a static-only tag.
        auto ambiguous=candidate;ambiguous.progression[0].name="Daug";ambiguous.progression[0].quality=ChordQuality::Augmented;
        if(!render(ambiguous))return false;
        const auto fallback=candidateHint(enrichments_.groups[0][0]);
        if(fallback.staticOnly||fallback.tone!=ColorTone::Neutral||fallback.label!=t("color.uncertain"))return false;
    }
    // Continuation still uses its original complete-path hint and renderer.
    state_.productMode=session::ProductMode::Continue;
    ContinuationCandidate c;c.id="rc2-continuation";c.key={PitchClass::D,Mode::Minor};c.rankingScore=85;
    c.continuation={{"Dm",2.,{1,0},ChordQuality::Minor,{}}};RecommendationSet result;result.groups[0].push_back(c);setRecommendations(result);
    refreshLayout();const auto lane=layout_.lanes[0];const ScrollableCandidateList list(lane,1,0.);
    const auto geometry=list.continuationRow(lane,0,false);const auto hint=candidateHint(recommendations_.groups[0][0]);
    auto rect=viewRect(geometry.intent);dc->setFont(kNormalFont);
    rect.left+=std::min(rect.getWidth()*.40,dc->getStringWidth(tIntent(c.intent).c_str())+9.);
    dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
    return !hint.staticOnly&&!hint.label.empty()&&hint.label==colorHint(candidateColor(c),state_.locale).label&&painted(rect,hint.tone);
}
bool MainView::runWhyV2Smoke(CDrawContext* dc,bool enrich) {
    session::PluginSessionState state;state.productMode=enrich?session::ProductMode::Enrich:session::ProductMode::Continue;
    Progression source;
    for(int i=0;i<(enrich?16:1);++i) {
        ChordEvent e;e.name="C";e.root=PitchClass::C;e.bass=PitchClass::C;e.quality=ChordQuality::Major;
        e.startQN=i*4.;e.durationQN=4.;e.openEnded=false;source.push_back(e);
    }
    if(!state.imported.replace(source,TimelineCoordinateMode::RelativeToSelection))return false;
    setSessionState(state);
    RecommendationSet continuation;enrichment::EnrichmentResult upgrades;
    for(int i=0;i<2;++i) {
        ContinuationCandidate c;c.id=i?"why-arc":"why-flat";c.intent=PhraseIntent::Resolve;
        c.key={PitchClass::C,Mode::Major};c.rankingScore=89.f-i;
        c.continuation={{i?"Cmaj7":"C",4.,{1,0},i?ChordQuality::Major7:ChordQuality::Major,{}},{"C",4.,{1,0},ChordQuality::Major,{}}};
        continuation.groups[0].push_back(c);
        enrichment::EnrichmentCandidate e;e.id=c.id;e.fingerprint=c.id;e.score=c.rankingScore;
        e.skeletonPreservation=.985f;e.styleCompatibility=.8f;e.complexityScore=.44f;e.progression=source;
        for(int j=0;j<16&&enrich;++j)if(i?(j==7||j==8):(j==0||j==15)) {
            e.progression[j].name="Cmaj7";e.progression[j].quality=ChordQuality::Major7;
            enrichment::EnrichmentOperation op;op.type=enrichment::OperationType::UpgradeQuality;
            op.technique=enrichment::TechniqueID::SeventhColor;op.sourceIndex=j;op.before="C";op.after="Cmaj7";e.operations.push_back(op);
        }
        e.techniques={enrichment::TechniqueID::SeventhColor};upgrades.groups[0].push_back(e);
    }
    if(enrich)setEnrichments(upgrades);else setRecommendations(continuation);
    refreshLayout();const auto serialized=session::serialize(state_);
    std::string heard,exported;unsigned callbacks{},saved{};
    actions_.stateChanged=[&](const auto&){++callbacks;};
    actions_.audition=[&](const auto& c){heard=c.id;};actions_.auditionEnrichment=[&](const auto& c){heard=c.id;};
    actions_.exportMidi=[](const auto&){return std::string{};};actions_.exportEnrichmentMidi=[](const auto&){return std::string{};};
    const midi::MidiClipPayload fake{{'M','T','h','d'},"why_identity.mid",1,3,{}, {'D','A','W'}};
    actions_.midiPayload=[&](const auto& c){exported=c.id;return midi::PayloadResult{fake,{}};};
    actions_.enrichmentMidiPayload=[&](const auto& c){exported=c.id;return midi::PayloadResult{fake,{}};};
    actions_.saveMidiPayload=[&](const auto&){++saved;return std::string{};};
    const auto order=[&](){return enrich?enrichmentOrder(0):continuationPresentation()[0];};
    const std::vector<std::size_t> original{0,1};if(order()!=original)return false;
    setColorPreference(color::Preference::TensionArc);const auto ranked=order();if(ranked.front()!=1)return false;
    setColorHints(false);if(order()!=ranked||colorPreference_!=color::Preference::TensionArc)return false;
    setColorHints(true);
    const auto candidateHintFor=[&](){return enrich?candidateHint(enrichments_.groups[0][1]):candidateHint(recommendations_.groups[0][1]);};
    for(const auto locale:{session::Locale::ZhCN,session::Locale::EnUS}) {
        state_.locale=locale;
        const auto hint=candidateHintFor();
        const auto functional=enrich?functionalWhy(enrichments_.groups[0][1],locale):
            functionalWhy(evaluateIntentCompletion(recommendations_.groups[0][1]),locale);
        const auto why=explainWhy(functional,hint,locale,colorPreference_,color::RankReason::Matched,true);
        if(why.sentences.size()!=3||hint.label.empty()||why.sentences.front().find("whyV2.")!=std::string::npos)return false;
        setColorHints(false);
        const auto hiddenWhy=explainWhy(functional,candidateHintFor(),locale,colorPreference_,color::RankReason::Matched,true);
        if(hiddenWhy.sentences.size()!=2||order()!=ranked)return false;
        setColorHints(true);
        color::PathColor unknown;unknown.status=color::Status::Unknown;
        auto neutral=colorHint(unknown,locale);
        if(neutral.tone!=ColorTone::Neutral||neutral.label!=t("color.unknown"))return false;
        unknown.status=color::Status::Uncertain;unknown.positions.resize(1);unknown.positions[0].chord.reason=color::Reason::MissingBass;
        neutral=colorHint(unknown,locale);
        const auto fallback=explainWhy(functional,neutral,locale,colorPreference_,color::RankReason::InsufficientColor);
        if(neutral.tone!=ColorTone::Neutral||fallback.sentences.size()!=2||neutral.explanation!=t("color.whyMissingBass"))return false;
    }
    state_.locale=state.locale;
    // The first displayed row must route all actions to original candidate 1.
    selectedCandidate_.reset();selectedEnrichment_.reset();refreshLayout();
    const auto lane=layout_.lanes[0];const ScrollableCandidateList list(lane,2,0.);
    const auto cGeometry=list.continuationRow(lane,0,true);const auto eGeometry=enrichmentRowGeometry(lane,list,0);
    const auto click=[&](UiRect rect,bool release=false){CPoint point{(rect.left+rect.right)/2*userZoom(),(rect.top+rect.bottom)/2*userZoom()};
        onMouseDown(point,CButtonState{kLButton});if(release)onMouseUp(point,CButtonState{kLButton});};
    dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
    click(enrich?eGeometry.audition:cGeometry.audition);click(enrich?eGeometry.midi:cGeometry.midi,true);
    click(enrich?eGeometry.why:cGeometry.why);
    const auto selected=enrich?(selectedEnrichment()?selectedEnrichment()->id:""):(selectedCandidate()?selectedCandidate()->id:"");
    if(heard!="why-arc"||exported!="why-arc"||selected!="why-arc"||saved!=1)return false;
    dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
    state_.debugExpanded=true;dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();state_.debugExpanded=false;
    const auto retained=captureEditorUiState();setColorPreference(color::Preference::Off);
    if(order()!=original)return false;restoreEditorUiState(retained);
    setColorHints(false);if(order()!=ranked||!candidateHintFor().label.empty())return false;
    state_.locale=state.locale;
    return callbacks==0&&serialized==session::serialize(state_);
}
bool MainView::runEnrichmentColorSmoke(CDrawContext* dc,const Progression& source,const enrichment::EnrichmentResult& candidates) {
    session::PluginSessionState state;state.productMode=session::ProductMode::Enrich;
    if(!state.imported.replace(source,TimelineCoordinateMode::RelativeToSelection))return false;
    setSessionState(state);setEnrichments(candidates);refreshLayout();
    const std::vector<std::size_t> baseline{0,1};
    if(enrichmentOrder(0)!=baseline)return false;
    setColorPreference(color::Preference::TensionArc);
    if(enrichmentOrder(0)!=std::vector<std::size_t>({1,0}))return false;
    const auto expected=candidates.groups[0][1].id;
    std::string heard,exported,snapshot;unsigned saved{},regenerated{};
    actions_.stateChanged=[&](const auto&){++regenerated;};
    actions_.auditionEnrichment=[&](const auto& c){heard=c.id;};
    actions_.exportEnrichmentMidi=[](const auto&){return std::string{};};
    actions_.saveEnrichmentSnapshot=[&](const auto& c){snapshot=c.id;return std::string{};};
    const midi::MidiClipPayload fake{{'M','T','h','d'},"color_identity.mid",1,3,{}, {'D','A','W'}};
    actions_.enrichmentMidiPayload=[&](const auto& c){exported=c.id;return midi::PayloadResult{fake,{}};};
    actions_.saveMidiPayload=[&](const auto&){++saved;return std::string{};};
    const auto lane=layout_.lanes[0];const ScrollableCandidateList list(lane,2,0.);
    const auto geometry=enrichmentRowGeometry(lane,list,0);
    const auto click=[&](UiRect rect,bool release=false){CPoint point{(rect.left+rect.right)/2*userZoom(),(rect.top+rect.bottom)/2*userZoom()};
        onMouseDown(point,CButtonState{kLButton});if(release)onMouseUp(point,CButtonState{kLButton});};
    dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
    click(geometry.audition);click(geometry.midi,true);click(geometry.snapshot);click(geometry.pin);
    if(heard!=expected||exported!=expected||snapshot!=expected||saved!=1||
       pinnedEnrichmentIds_!=std::vector<std::string>{expected})return false;
    // Comparison uses the stable pinned ID, then resolves the original container index.
    const auto compare=layout_.compare;
    CPoint point{(compare.left+20)*userZoom(),(compare.top+35)*userZoom()};onMouseDown(point,CButtonState{kLButton});
    if(!selectedEnrichment()||selectedEnrichment()->id!=expected||selectedEnrichment_->second!=1)return false;
    selectedEnrichment_.reset();refreshLayout();
    const auto currentLane=layout_.lanes[0];const ScrollableCandidateList currentList(currentLane,2,0.);
    click(enrichmentRowGeometry(currentLane,currentList,0).why);
    if(!selectedEnrichment()||selectedEnrichment()->id!=expected)return false;
    for(const auto locale:{session::Locale::ZhCN,session::Locale::EnUS}) {
        state_.locale=locale;dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
        if(t("colorSort.enrichmentMatch").empty())return false;
    }
    showColorHints_=false;const auto retained=captureEditorUiState();setColorPreference(color::Preference::Off);
    if(enrichmentOrder(0)!=baseline||!candidateHint(candidates.groups[0][1]).label.empty())return false;
    restoreEditorUiState(retained);
    return colorPreference_==color::Preference::TensionArc&&!showColorHints_&&enrichmentOrder(0).front()==1&&
        selectedEnrichment()&&selectedEnrichment()->id==expected&&regenerated==0;
}
bool MainView::runColorSortSmoke(CDrawContext* dc,const Progression& source,const RecommendationSet& candidates) {
    session::PluginSessionState state;
    if(!state.imported.replace(source,TimelineCoordinateMode::RelativeToSelection))return false;
    setSessionState(state);setRecommendations(candidates);
    const auto baseline=session::presentationIndices(recommendations_,visibility_);
    const auto serialized=session::serialize(state_);
    const auto candidateIdentity=session::continuationFingerprint(recommendations_.groups[0][1]);
    selectedCandidate_={{0,1}};refreshLayout();
    unsigned callbacks{};actions_.stateChanged=[&](const auto&){++callbacks;};
    for(const auto locale:{session::Locale::ZhCN,session::Locale::EnUS}) {
        state_.locale=locale;
        setColorPreference(color::Preference::Off);if(continuationPresentation()!=baseline)return false;
        showColorHints_=false;setColorPreference(color::Preference::TensionArc);
        const auto shown=continuationPresentation();
        if(shown[0].empty()||shown[0].front()!=1||!candidateHint(recommendations_.groups[0][1]).label.empty())return false;
        const auto saved=captureEditorUiState();setColorPreference(color::Preference::Off);restoreEditorUiState(saved);
        if(colorPreference_!=color::Preference::TensionArc||showColorHints_)return false;
        dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw(); // Control, cards and selected Why.
        if(t(color::rankReasonKey(colorRanks_[0].decisions[1].reason)).empty())return false;
        setColorPreference(color::Preference::Off);if(continuationPresentation()!=baseline)return false;
    }
    state_.locale=state.locale;
    return callbacks==0&&serialized==session::serialize(state_)&&
        candidateIdentity==session::continuationFingerprint(recommendations_.groups[0][1])&&selectedCandidate()==&recommendations_.groups[0][1];
}
bool MainView::runColorHintSmoke(CDrawContext* dc) {
    session::PluginSessionState state;
    ChordEvent chord;chord.name="Cmaj7";chord.quality=ChordQuality::Major7;
    chord.root=PitchClass::C;chord.bass=PitchClass::C;chord.durationQN=4.;chord.openEnded=false;
    if(!state.imported.replace({chord},TimelineCoordinateMode::RelativeToSelection))return false;
    state.factoryLibraryVersion=3;setSessionState(state);
    ContinuationCandidate candidate;candidate.id="color-smoke";candidate.primaryTemplate="COMMON_MAJOR_020";
    candidate.key={PitchClass::C,Mode::Major};candidate.intent=PhraseIntent::Resolve;candidate.rankingScore=90;
    ConcreteChordEvent next;next.label="C";next.quality=ChordQuality::Major;next.durationQN=4.;
    candidate.continuation.push_back(next);
    RecommendationSet result;result.groups[0].push_back(candidate);setRecommendations(result);
    selectedCandidate_={{0,0}};refreshLayout();
    const auto identity=session::continuationFingerprint(recommendations_.groups[0].front());
    for(const auto locale:{session::Locale::ZhCN,session::Locale::EnUS}) {
        state_.locale=locale;showColorHints_=true;
        const auto shown=candidateHint(candidate);
        if(shown.tone!=ColorTone::Warm||shown.explanations.size()<2||!candidateColor(candidate).tensionReleasing)return false;
        const auto functional=evaluateIntentCompletion(candidate,{},{}).reason;
        dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw(); // Card and selected Why? inspector.
        const auto serialized=session::serialize(state_);showColorHints_=false;
        const auto hidden=candidateHint(candidate);
        if(!hidden.label.empty()||!hidden.explanations.empty())return false;
        dc->beginDraw();drawRect(dc,getViewSize());dc->endDraw();
        if(serialized!=session::serialize(state_)||identity!=session::continuationFingerprint(recommendations_.groups[0].front())||
           functional!=evaluateIntentCompletion(candidate,{},{}).reason)return false;
    }
    const auto saved=captureEditorUiState();showColorHints_=true;restoreEditorUiState(saved);
    return !showColorHints_&&selectedCandidate()!=nullptr;
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
    invalidRect(editRect(CRect(255,layout_.phrase.top-31,layout_.viewport.right-160,layout_.phrase.top)));
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
    if (oldLocation.activeIndex) invalidRect(editRect(chordTileRect(*oldLocation.activeIndex)));
    if (currentLocation_.activeIndex) invalidRect(editRect(chordTileRect(*currentLocation_.activeIndex)));
    if (oldX) invalidRect(editRect(CRect(*oldX-3,layout_.timeline.top,*oldX+3,layout_.timeline.bottom)));
    if (playheadX_) invalidRect(editRect(CRect(*playheadX_-3,layout_.timeline.top,*playheadX_+3,layout_.timeline.bottom)));
}
void MainView::drawTimeline(CDrawContext* dc,const CRect& update) {
    const CRect lane=viewRect(layout_.timeline);
    ConcatClip clip(*dc,lane);
    box(dc,lane,CColor(19,32,49,255));
    if (state_.imported.events.empty()) {
        line(dc,t("phrase.empty"),
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
        line(dc,event.name,CRect(tile.left+2,tile.top+3,tile.right-2,tile.top+25),pale,kCenterText);
        if (block.eventIndex<analysis_.full.size()) {
            const auto& a=analysis_.full[block.eventIndex];
            const auto duration=event.durationQN?fixed(*event.durationQN)+" QN":"OPEN";
            line(dc,duration,CRect(tile.left+1,tile.top+25,tile.right-1,tile.top+43),pale,kCenterText);
            if(state_.tab==session::Tab::Diagnostics) {
                line(dc,formatDegree(a)+"  "+formatFunction(a.function),
                    CRect(tile.left+1,tile.top+43,tile.right-1,tile.top+61),muted,kCenterText);
            }else if(tile.getWidth()>=90)
                line(dc,formatDegree(a),CRect(tile.left+1,tile.top+43,tile.right-1,tile.top+61),muted,kCenterText);
            const auto bar=weightBarRect(tile.left,tile.right,tile.bottom,a.structuralWeight,dc->getScaleFactor());
            if (bar.right>bar.left) box(dc,CRect(bar.left,bar.top,bar.right,bar.bottom),accent);
        }
    }
    if(state_.tab==session::Tab::Recommend&&state_.productMode==session::ProductMode::Enrich&&
       !enrichments_.opportunities.empty()) {
        const auto index=enrichments_.opportunities.front().afterIndex;
        if(index+1<state_.imported.events.size()) {
            const auto next=chordTileRect(index+1);
            line(dc,"+",CRect(next.left-16,lane.bottom-26,next.left+4,lane.bottom-5),accent,kCenterText);
        }
    }
    if (playheadX_) box(dc,CRect(*playheadX_-1.5,lane.top,*playheadX_+1.5,lane.bottom),CColor(255,237,138,255));
}
void MainView::drawTop(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    box(dc,viewRect(layout_.topBar),CColor(25,40,58,255));
    const std::array<std::string,6> labels{
        t("label.key")+": "+(state_.forcedKey?formatKey(*state_.forcedKey)+" "+t("label.forced"):
            analysis_.selectedKey?t("label.auto")+" · "+formatKey(analysis_.selectedKey->key):t("label.auto")),
        t("label.style")+": "+(state_.style?styleName(*state_.style):t("label.auto")),
        t("label.intent")+": "+(state_.intent?tIntent(*state_.intent):t("label.auto")),t("nav.more"),
        t("mode.continue"),t("mode.enrich")};
    for(std::size_t i=0;i<labels.size();++i)
        line(dc,labels[i],viewRect(layout_.topControls[i]),
            i==0&&state_.forcedKey?accent:i==4&&state_.tab==session::Tab::Recommend&&
                state_.productMode==session::ProductMode::Continue?accent:
            i==5&&state_.tab==session::Tab::Recommend&&
                state_.productMode==session::ProductMode::Enrich?accent:i>=4?muted:pale);
    const auto role=state_.constraints.melody.empty()?t("melody.off"):
        state_.constraints.melody.front().role==ConstraintRole::TopVoice?t("melody.top"):t("melody.present");
    const std::array<std::string,4> constraints{t("melody.label")+": "+role,
        state_.constraints.melody.empty()?t("melody.note"):melodyNoteName(state_.constraints.melody.front())+
            (state_.constraints.melody.size()>1?" +":""),
        t("tendency.label")+": "+t(state_.tendency==HarmonicTendency::Conservative?"tendency.conservative":
            state_.tendency==HarmonicTendency::Bold?"tendency.bold":"tendency.balanced"),
        state_.tab==session::Tab::Recommend?t("colorSort.labelShort")+": "+t(std::string(color::preferenceKey(colorPreference_))+"Short"):""};
    for(std::size_t i=0;i<constraints.size();++i)line(dc,constraints[i],viewRect(layout_.constraintControls[i]),
        i==0&&!state_.constraints.melody.empty()?accent:pale);
    if(state_.tab!=session::Tab::Recommend)return;
    const auto titleY=layout_.phrase.top-31;
    line(dc,t("phrase.title"),CRect(20,titleY,235,titleY+27),pale);
    const auto status=workerBusy_?t("status.analyzing"):!actionStatus_.empty()?actionStatus_:
        matchStatus_.find("library")!=std::string::npos?t("status.libraryError"):"";
    if (!status.empty()) line(dc,status,CRect(255,titleY,layout_.viewport.right-170,titleY+27),muted,kRightText);
    line(dc,t("nav.match"),CRect(layout_.viewport.right-150,titleY,layout_.viewport.right-90,titleY+27),muted,kRightText);
    line(dc,t("label.debug")+(state_.debugExpanded?" ●":""),CRect(layout_.viewport.right-85,titleY,layout_.viewport.right-20,titleY+27),
        state_.debugExpanded?accent:muted,kRightText);
}
void MainView::drawPhrase(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    drawTimeline(dc,viewRect(layout_.timeline));
    const auto y=layout_.timeline.bottom+5;
    line(dc,t("phrase.title")+" · "+std::to_string(state_.imported.events.size())+" "+t("phrase.count"),
        CRect(24,y,layout_.timeline.right-220,y+24),pale);
    if (timelineContentWidth_>layout_.timeline.width()+1)
        line(dc,t("phrase.scroll"),CRect(layout_.timeline.right-215,y,layout_.timeline.right,y+24),muted,kRightText);
    if (state_.imported.events.empty()) return;
    if (selectedChord_ && *selectedChord_<analysis_.full.size()) {
        const auto& event=state_.imported.events[*selectedChord_];
        const auto duration=event.durationQN?fixed(*event.durationQN)+" QN":"OPEN";
        line(dc,event.name+" · "+duration+" · "+t("phrase.starts")+" "+fixed(event.startQN)+" QN",
             CRect(23,y+28,layout_.viewport.right-23,y+55),pale);
    } else line(dc,t("phrase.select")+" · "+t("midi.help"),CRect(23,y+28,layout_.viewport.right-23,y+55),muted);
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
        line(dc,t("phrase.empty"),viewRect(layout_.content),pale,kCenterText); return;
    }
    constexpr const char* titles[]{"group.resolve","group.develop","group.loop","group.color"};
    const auto presentation=continuationPresentation();
    {
    ConcatClip clip(*dc,viewRect(layout_.content));
    for (std::size_t visual=0;visual<4;++visual) {
        const auto group=groupForVisual(visual,state_.intent);
        auto lane=layout_.lanes[visual]; lane.top-=contentScroll_; lane.bottom-=contentScroll_;
        if(lane.bottom<layout_.content.top||lane.top>layout_.content.bottom)continue;
        box(dc,viewRect(lane),panel);
        line(dc,t(titles[group]),CRect(lane.left+8,lane.top+3,lane.left+160,lane.top+27),accent);
        if (recommendations_.groups[group].size()>2)
            line(dc,t("candidate.more"),CRect(lane.right-112,lane.top+3,lane.right-8,lane.top+27),muted,kRightText);
        const auto& visible=presentation[group];
        if (visible.empty()) {
            line(dc,workerBusy_?t("status.analyzing"):t("candidate.none"),CRect(lane.left+8,lane.top+36,lane.right-8,lane.top+64),muted);
            continue;
        }
        const ScrollableCandidateList list(lane,visible.size(),continuationScroll_[group]);
        continuationScroll_[group]=list.offset;
        {
        ConcatClip rowsClip(*dc,viewRect(list.viewport));
        for (std::size_t row=0;row<visible.size();++row) {
            if(!list.visible(row))continue;
            const auto& c=recommendations_.groups[group][visible[row]];
            const bool exportControls=static_cast<bool>(actions_.exportMidi);
            const auto geometry=list.continuationRow(lane,row,exportControls);
            box(dc,viewRect(geometry.row),CColor(33,49,69,255));
            drawMiniTimeline(dc,c,viewRect(geometry.timeline));
            std::string continuation;
            for(const auto& chord:c.continuation){if(!continuation.empty())continuation+=" → ";continuation+=chord.label;}
            line(dc,continuation,viewRect(geometry.summary),pale);
            line(dc,t("candidate.why"),viewRect(geometry.why),muted,kCenterText);
            line(dc,std::to_string(static_cast<int>(std::round(c.rankingScore))),
                 viewRect(geometry.score),pale,kCenterText);
            std::string intent=tIntent(c.intent);
            if(state_.imported.events.back().openEnded&&c.suggestedCurrentChordDurationQN)
                intent+=" · OPEN "+fixed(*c.suggestedCurrentChordDurationQN)+" QN";
            auto hint=candidateHint(c);
            if(!hint.label.empty()){
                auto rect=viewRect(geometry.intent);dc->setFont(kNormalFont);
                const auto width=std::min(rect.getWidth()*.40,dc->getStringWidth(tIntent(c.intent).c_str())+9.);
                auto purpose=rect;purpose.right=purpose.left+width;line(dc,tIntent(c.intent),purpose,muted);
                rect.left+=width;drawColorHint(dc,hint,rect);
            }
            else line(dc,intent,viewRect(geometry.intent),muted);
            line(dc,t("candidate.pin"),viewRect(geometry.pin),accent,kCenterText);
            if(exportControls) {
                line(dc,previewCandidateId_==c.id?"■":"▶",viewRect(geometry.audition),accent,kCenterText);
                line(dc,t("midi.button"),viewRect(geometry.midi),accent,kCenterText);
                line(dc,"SNAP",viewRect(geometry.snapshot),muted,kCenterText);
            } else if(actions_.audition)
                line(dc,previewCandidateId_==c.id?"■":"▶",viewRect(geometry.audition),accent,kCenterText);
        }
        }
        if(list.maximum>0)box(dc,viewRect(list.thumb()),muted);
    }
    }
    drawCompare(dc,bounds);
}
void MainView::drawEnrichments(CDrawContext* dc,const CRect&) {
    if (state_.imported.events.empty()) {
        line(dc,t("phrase.empty"),viewRect(layout_.content),pale,kCenterText);
        return;
    }
    constexpr const char* titles[]{"group.polish","group.rich","group.advanced"};
    {
    ConcatClip clip(*dc,viewRect(layout_.content));
    for (std::size_t group=0;group<3;++group) {
        auto lane=layout_.lanes[group]; lane.top-=contentScroll_; lane.bottom-=contentScroll_;
        if (lane.bottom<layout_.content.top || lane.top>layout_.content.bottom) continue;
        box(dc,viewRect(lane),panel);
        line(dc,t(titles[group]),CRect(lane.left+8,lane.top+3,lane.right-120,lane.top+27),accent);
        const auto& candidates=enrichments_.groups[group];
        if(candidates.size()>2)line(dc,t("candidate.more"),CRect(lane.right-120,lane.top+3,lane.right-8,lane.top+27),accent,kRightText);
        if (candidates.empty()) {
            line(dc,workerBusy_?t("status.analyzing"):t("status.noEnrichment"),
                 CRect(lane.left+8,lane.top+36,lane.right-8,lane.top+64),muted);
            continue;
        }
        const ScrollableCandidateList list(lane,candidates.size(),enrichmentScroll_[group]);
        enrichmentScroll_[group]=list.offset;
        {
        ConcatClip rowsClip(*dc,viewRect(list.viewport));
        for (std::size_t row=0;row<candidates.size();++row) {
            if(!list.visible(row))continue;
            const auto& candidate=candidates[enrichmentOrder(group)[row]];
            const auto geometry=enrichmentRowGeometry(lane,list,row);
            box(dc,viewRect(geometry.row),CColor(33,49,69,255));
            std::string progression;
            for (const auto& event:candidate.progression) {
                if (!progression.empty()) progression+=" → ";
                progression+=event.name;
            }
            line(dc,progression,viewRect(geometry.summary),pale);
            line(dc,std::to_string(static_cast<int>(std::round(candidate.score))),
                 viewRect(geometry.score),accent,kRightText);
            std::string badges;
            for (std::size_t i=0;i<std::min<std::size_t>(3,candidate.techniques.size());++i) {
                if (!badges.empty()) badges+=" · ";
                badges+=t("technique."+std::string(enrichment::techniqueName(candidate.techniques[i])));
            }
            auto hint=candidateHint(candidate);
            if(!hint.label.empty())drawColorHint(dc,hint,viewRect(geometry.badges));
            else line(dc,badges,viewRect(geometry.badges),muted);
            line(dc,std::find(pinnedEnrichmentIds_.begin(),pinnedEnrichmentIds_.end(),candidate.id)!=
                pinnedEnrichmentIds_.end()?t("candidate.unpin"):t("candidate.pin"),
                viewRect(geometry.pin),accent,kCenterText);
            line(dc,t("candidate.why"),viewRect(geometry.why),accent,kCenterText);
            if (actions_.auditionEnrichment)
                line(dc,"▶",viewRect(geometry.audition),accent,kCenterText);
            if (actions_.exportEnrichmentMidi)
                line(dc,t("midi.button"),viewRect(geometry.midi),accent,kCenterText);
            if (actions_.saveEnrichmentSnapshot)
                line(dc,"SNAP",viewRect(geometry.snapshot),muted,kCenterText);
        }
        }
        if(list.maximum>0)box(dc,viewRect(list.thumb()),muted);
    }
    if (!enrichments_.opportunities.empty()) {
        const auto& opportunity=enrichments_.opportunities.front();
        auto lane=layout_.lanes[3]; lane.top-=contentScroll_; lane.bottom-=contentScroll_;
        if (lane.bottom>=layout_.content.top && lane.top<=layout_.content.bottom) {
            box(dc,viewRect(lane),panel);
            line(dc,t("opportunity.title"),CRect(lane.left+8,lane.top+3,lane.right-8,lane.top+27),accent);
            line(dc,state_.imported.events[opportunity.afterIndex].name+" +  →  "+
                 state_.imported.events[opportunity.afterIndex+1].name,
                 CRect(lane.left+12,lane.top+40,lane.right-12,lane.top+68),pale);
            line(dc,t("reason."+std::string(enrichment::techniqueName(opportunity.technique))),
                 CRect(lane.left+12,lane.top+70,lane.right-12,lane.top+104),muted);
        }
    }
    }
    drawCompare(dc,getViewSize());
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
const enrichment::EnrichmentCandidate* MainView::findEnrichment(const std::string& id) const {
    for(const auto& group:enrichments_.groups)for(const auto& candidate:group)
        if(candidate.id==id)return &candidate;
    return nullptr;
}
const enrichment::EnrichmentCandidate* MainView::selectedEnrichment() const {
    if(!selectedEnrichment_)return nullptr;
    const auto [group,index]=*selectedEnrichment_;
    return group<enrichments_.groups.size()&&index<enrichments_.groups[group].size()?
        &enrichments_.groups[group][index]:nullptr;
}
void MainView::drawCompare(CDrawContext* dc,const CRect& bounds) {
    (void)bounds;
    if(state_.productMode==session::ProductMode::Enrich) {
        if(pinnedEnrichmentIds_.empty())return;
        const auto tray=layout_.compare;
        box(dc,viewRect(tray),CColor(27,41,58,255));
        line(dc,t("candidate.compare")+"  ·  "+std::to_string(pinnedEnrichmentIds_.size())+" / 3",
             CRect(tray.left+8,tray.top+3,tray.right-8,tray.top+25),accent);
        const bool stacked=layout_.mode==LayoutMode::Compact;
        const auto width=stacked?tray.width()-16:(tray.width()-16)/3;
        for(std::size_t index=0;index<pinnedEnrichmentIds_.size();++index) {
            const auto* candidate=findEnrichment(pinnedEnrichmentIds_[index]);
            const auto left=stacked?tray.left+8:tray.left+8+index*width;
            const auto y=stacked?tray.top+27+index*26:tray.top+27;
            if(candidate) {
                std::string path;
                for(const auto& event:candidate->progression){if(!path.empty())path+=" → ";path+=event.name;}
                line(dc,path,CRect(left,y,left+width-48,y+21),pale);
                if(!stacked)line(dc,t("inspector.score")+" "+fixed(candidate->score,0)+" · "+
                    t(candidate->group==enrichment::Group::Polish?"group.polish":
                      candidate->group==enrichment::Group::Rich?"group.rich":"group.advanced"),
                    CRect(left,y+20,left+width-48,y+43),muted);
            }else line(dc,t("candidate.pinnedUnavailable"),CRect(left,y,left+width-48,y+21),muted);
            line(dc,"×",CRect(left+width-42,stacked?y:y+20,left+width-8,(stacked?y:y+20)+22),muted,kCenterText);
        }
        return;
    }
    if (state_.pinnedCandidateIds.empty()) return;
    const auto tray=layout_.compare;
    box(dc,viewRect(tray),CColor(27,41,58,255));
    line(dc,t("candidate.compare")+"  ·  "+std::to_string(state_.pinnedCandidateIds.size())+" / 3",
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
            const auto hold=c->suggestedCurrentChordDurationQN?" · OPEN "+fixed(*c->suggestedCurrentChordDurationQN)+" QN":"";
            if(stacked)line(dc,path+" · "+tIntent(c->intent)+" · "+
                std::to_string(static_cast<int>(std::round(c->rankingScore)))+hold+" · "+cadenceName(c->cadence),
                CRect(left,y,left+width-128,y+22),pale);
            else {
                line(dc,path,CRect(left,y,left+width-8,y+20),pale);
                line(dc,tIntent(c->intent)+" · "+
                    std::to_string(static_cast<int>(std::round(c->rankingScore)))+hold+" · "+cadenceName(c->cadence),
                    CRect(left,y+20,left+width-128,y+43),muted);
            }
        } else line(dc,t("candidate.pinnedUnavailable"),CRect(left,y,left+width-128,y+22),muted);
        const auto buttonY=stacked?y:y+20;
        line(dc,t("candidate.play"),CRect(left+width-122,buttonY,left+width-85,buttonY+22),accent,kCenterText);
        line(dc,"MIDI",CRect(left+width-82,buttonY,left+width-44,buttonY+22),accent,kCenterText);
        line(dc,"×",CRect(left+width-42,buttonY,left+width-8,buttonY+22),muted,kCenterText);
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
            if (librarySource_!=-1 && librarySource_!=(factory?0:1)) continue;
            if (libraryComplexity_ && item.complexityLevel!=*libraryComplexity_) continue;
            if (libraryFavoriteOnly_ && !item.favorite) continue;
            if (!libraryTechnique_.empty() && std::find(item.techniques.begin(),item.techniques.end(),
                libraryTechnique_)==item.techniques.end()) continue;
            if (!librarySearch_.empty()) {
                bool hit=containsText(item.name,librarySearch_) || containsText(item.nameZh,librarySearch_) ||
                    containsText(item.nameEn,librarySearch_) || containsText(item.id,librarySearch_) ||
                    containsText(primaryStyle(item.styles),librarySearch_);
                for (const auto& tag:item.tags) hit=hit || containsText(tag,librarySearch_);
                for (const auto& alias:item.aliases) hit=hit || containsText(alias,librarySearch_);
                for (const auto& tag:item.builtInTags)
                    hit=hit || containsText(tag,librarySearch_) || containsText(t("tag."+tag),librarySearch_);
                for (const auto& event:item.full) hit=hit || containsText(formatMatchEvent(event),librarySearch_);
                if (!hit) continue;
            }
            out.emplace_back(factory,i);
        }
    };
    append(factory_,true); append(user_,false); return out;
}
void MainView::drawRect(CDrawContext* dc,const CRect& update) {
    refreshLayout();
    CDrawContext::Transform zoomTransform(*dc,CGraphicsTransform().scale(userZoom(),userZoom()));
    ++paintGeneration_;
    const auto bounds=viewRect(layout_.viewport);
    if (userZoom()==1&&state_.tab==session::Tab::Recommend&&update.top>=layout_.timeline.top && update.bottom<=layout_.timeline.bottom) { drawTimeline(dc,update); return; }
    box(dc,bounds,background);
    drawTop(dc,bounds); if(state_.tab==session::Tab::Recommend)drawPhrase(dc,bounds);
    switch(state_.tab) {
        case session::Tab::Recommend:
            if (state_.productMode==session::ProductMode::Enrich) drawEnrichments(dc,bounds);
            else drawRecommendations(dc,bounds);
            break;
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
    const auto factor=userZoom()*(getFrame()?getFrame()->getZoom():1.);
    POINT at{static_cast<LONG>(point.x*factor),static_cast<LONG>(point.y*factor)}; ClientToScreen(window,&at);
    const auto result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,at.x,at.y,0,window,nullptr);
    DestroyMenu(menu); return result?static_cast<int>(result-1):-1;
#else
    (void)items; (void)point; return -1;
#endif
}
void MainView::beginForm(Form kind,std::string initial) {
    if (!getFrame()) return; endForm(); form_=kind;
    const double w=std::min(590.,layout_.viewport.width()-48),h=360;
    const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
    nameEdit_=new CTextEdit(editRect(CRect(x+140,y+53,x+w-22,y+91)),nullptr,0,initial.c_str());
    nameEdit_->setBackColor(CColor(242,247,252,255)); nameEdit_->setFontColor(CColor(20,30,45,255));
    getFrame()->addView(nameEdit_);
    if (kind!=Form::Search) {
        tagsEdit_=new CTextEdit(editRect(CRect(x+140,y+201,x+w-22,y+239)),nullptr,0,"");
        tagsEdit_->setBackColor(CColor(242,247,252,255)); tagsEdit_->setFontColor(CColor(20,30,45,255));
        getFrame()->addView(tagsEdit_);
        noteEdit_=new CTextEdit(editRect(CRect(x+140,y+261,x+w-22,y+299)),nullptr,0,formMetadata_.note.c_str());
        noteEdit_->setBackColor(CColor(242,247,252,255)); noteEdit_->setFontColor(CColor(20,30,45,255));
        getFrame()->addView(noteEdit_);
    }
    const auto font=owned(new CFontDesc(kNormalFont->getName(),kNormalFont->getSize()*userZoom(),kNormalFont->getStyle()));
    for(auto* edit:{nameEdit_,tagsEdit_,noteEdit_})if(edit)edit->setFont(font);
    if(kind==Form::Melody) {
        tagsEdit_->setText(fixed(formMelody_.startQN,3).c_str());
        noteEdit_->setText(fixed(formMelody_.durationQN,3).c_str());
    }
    getFrame()->setFocusView(nameEdit_); invalid();
}
void MainView::endForm() {
    if (getFrame()) { if (nameEdit_) getFrame()->removeView(nameEdit_); if (tagsEdit_) getFrame()->removeView(tagsEdit_);
        if(noteEdit_)getFrame()->removeView(noteEdit_); }
    nameEdit_=nullptr; tagsEdit_=nullptr; noteEdit_=nullptr; form_=Form::None; invalid();
}
void MainView::submitForm() {
    if (!nameEdit_) return;
    const auto text=nameEdit_->getText().getString();
    if (form_==Form::Search) { librarySearch_=text; libraryPage_=0; endForm(); return; }
    if(form_==Form::Melody) {
        const auto parsed=parseMelodyNote(text);
        if(!parsed){actionStatus_=t("melody.invalid");invalid();return;}
        auto melody=*parsed;melody.role=formMelody_.role;melody.strictness=formMelody_.strictness;
        const auto parseTime=[](const std::string& text) {
            std::istringstream in(text);in.imbue(std::locale::classic());double value{};
            if(!(in>>value))throw std::runtime_error("invalid melody time");
            in>>std::ws;if(!in.eof())throw std::runtime_error("invalid melody time");return value;
        };
        try {
            melody.startQN=parseTime(tagsEdit_->getText().getString());
            melody.durationQN=parseTime(noteEdit_->getText().getString());
            HarmonyConstraintSet constraints{{melody}};
            if(!validConstraints(constraints))throw std::runtime_error("invalid melody time");
            state_.constraints=std::move(constraints);actionStatus_.clear();endForm();notifyState();
        } catch(...) {actionStatus_=t("melody.invalid");invalid();}
        return;
    }
    if (text.empty() || text.size()>128) { actionStatus_=t("form.invalidName"); invalid(); return; }
    formMetadata_.name=text; formMetadata_.tags.clear();
    if (tagsEdit_) {
        std::istringstream in(tagsEdit_->getText().getString()); std::string tag;
        while (std::getline(in,tag,',')) { if (!tag.empty()) formMetadata_.tags.push_back(tag); }
    }
    if(noteEdit_)formMetadata_.note=noteEdit_->getText().getString();
    if (form_==Form::Save) {
        const auto* c=selectedCandidate(); if (c && actions_.save) actionStatus_=actions_.save(*c,formMetadata_);
    } else if (form_==Form::Rename && selectedLibrary_ && !selectedLibrary_->first && selectedLibrary_->second<user_.size()) {
        auto item=user_[selectedLibrary_->second]; item.name=formMetadata_.name; item.intent=formMetadata_.intent;
        item.tags=formMetadata_.tags; item.styles=formMetadata_.style?static_cast<StyleFlags>(*formMetadata_.style):0;
        item.note=formMetadata_.note;
        item.styleWeights.clear(); if (formMetadata_.style) item.styleWeights.emplace_back(*formMetadata_.style,1.f);
        if (actions_.updateUser) actionStatus_=actions_.updateUser(item);
        selectedLibrary_.reset();
    }
    endForm();
}
CMouseEventResult MainView::onMouseDown(CPoint& where,const CButtonState&) {
    midiPending_.reset();midiClick_={};
    CPoint logical{where.x/userZoom(),where.y/userZoom()};
    return onMouseDownResponsive(logical);
}
bool MainView::armMidi(const ContinuationCandidate& candidate,CPoint where) {
    if(!actions_.midiPayload)return false;
    const auto result=actions_.midiPayload(candidate);
    if(!result){actionStatus_=t("midi.failed");invalid();return true;}
    midiPending_=result.payload;midiMouseDown_=where;
    midiClick_=[this,payload=result.payload,candidate]{actionStatus_=actions_.saveMidiPayload?
        actions_.saveMidiPayload(payload):actions_.exportMidi?actions_.exportMidi(candidate):t("midi.failed");invalid();};return true;
}
bool MainView::armMidi(const enrichment::EnrichmentCandidate& candidate,CPoint where) {
    if(!actions_.enrichmentMidiPayload)return false;
    const auto result=actions_.enrichmentMidiPayload(candidate);
    if(!result){actionStatus_=t("midi.failed");invalid();return true;}
    midiPending_=result.payload;midiMouseDown_=where;
    midiClick_=[this,payload=result.payload,candidate]{actionStatus_=actions_.saveMidiPayload?
        actions_.saveMidiPayload(payload):actions_.exportEnrichmentMidi?actions_.exportEnrichmentMidi(candidate):t("midi.failed");invalid();};return true;
}
CMouseEventResult MainView::onMouseMoved(CPoint& where,const CButtonState& buttons) {
    try {
    if(!midiPending_)return kMouseEventNotHandled;
    if(!buttons.isLeftButton()){midiPending_.reset();midiClick_={};return kMouseEventHandled;}
    if(!shouldStartDrag(CPoint(midiMouseDown_.x*userZoom(),midiMouseDown_.y*userZoom()),where))return kMouseEventHandled;
    auto payload=std::move(*midiPending_);midiPending_.reset();midiClick_={};
    const auto file=io::MidiDragService{}.prepare(payload);
    if(actions_.observeMidiDrag)actions_.observeMidiDrag(static_cast<bool>(file),false);
    if(!file){actionStatus_=t("midi.dragFailed");invalid();return kMouseEventHandled;}
    const auto utf8=file.path.u8string();std::string path(utf8.begin(),utf8.end());
    const auto package=CDropSource::create(path.c_str(),static_cast<std::uint32_t>(path.size()+1),IDataPackage::kFilePath);
    auto callback=owned(new DragCallbackFunctions);
    const SharedPointer<MainView> keepAlive(this);
    callback->endedFunc=[keepAlive](IDraggingSession*,CPoint,DragOperation result){if(keepAlive->actions_.observeMidiDrag)keepAlive->actions_.observeMidiDrag(true,result!=DragOperation::None);keepAlive->setActionStatus(keepAlive->t(result==DragOperation::None?"midi.dragFailed":"midi.dragged"));};
    if(!doDrag(DragDescription(package),callback)){actionStatus_=t("midi.dragFailed");invalid();}
    return kMouseEventHandled;
    } catch(...){midiPending_.reset();midiClick_={};actionStatus_=t("midi.dragFailed");invalid();return kMouseEventHandled;}
}
CMouseEventResult MainView::onMouseUp(CPoint& where,const CButtonState&) {
    try {auto click=std::move(midiClick_);midiClick_={};midiPending_.reset();
        if(click&&!shouldStartDrag(CPoint{midiMouseDown_.x*userZoom(),midiMouseDown_.y*userZoom()},where))click();
    } catch(...){actionStatus_=t("midi.saveFailed");invalid();}return kMouseEventHandled;
}
CMouseEventResult MainView::onMouseDownResponsive(CPoint& where) {
    try {
        refreshLayout();
        if(form_!=Form::None) {
            const double w=std::min(590.,layout_.viewport.width()-48),h=360;
            const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
            if(where.y>=y+h-45&&where.y<=y+h) {
                if(where.x>=x+140&&where.x<x+290)endForm();
                else if(where.x>=x+w-170&&where.x<=x+w)submitForm();
            } else if(form_==Form::Melody&&where.y>=y+110&&where.y<y+144) {
                const auto choice=popup({t("melody.present"),t("melody.top")},where);
                if(choice>=0)formMelody_.role=static_cast<ConstraintRole>(choice);invalid();
            } else if(form_==Form::Melody&&where.y>=y+144&&where.y<y+180) {
                const auto choice=popup({t("melody.soft"),t("melody.hard")},where);
                if(choice>=0)formMelody_.strictness=static_cast<ConstraintStrictness>(choice);invalid();
            } else if(form_!=Form::Search&&where.y>=y+110&&where.y<y+144) {
                const auto choice=popup({t("label.auto"),"Pop","Rock","R&B","Jazz","City Pop / J-pop",t("tag.functional")},where);
                constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                if(choice>=0)formMetadata_.style=choice==0?std::nullopt:std::optional<Style>(styles[choice-1]);invalid();
            } else if(form_!=Form::Search&&where.y>=y+144&&where.y<y+180) {
                const auto choice=popup({t("group.develop"),t("group.resolve"),t("group.loop"),t("group.color")},where);
                if(choice>=0)formMetadata_.intent=static_cast<PhraseIntent>(choice+1);invalid();
            }
            return kMouseEventHandled;
        }
        if (dismissOnOutsideClick(activeOverlayKind(),layout_.inspector,where.x,where.y))
            dismissTransientOverlay();
        for(std::size_t i=0;i<layout_.constraintControls.size();++i)
            if(layout_.constraintControls[i].contains(where.x,where.y)) {
                if(i==3) {
                    if(state_.tab==session::Tab::Recommend) {
                        std::vector<std::string> options;for(int p=0;p<6;++p)options.push_back(t(color::preferenceKey(static_cast<color::Preference>(p))));
                        options.push_back(std::string(showColorHints_?"✓ ":"")+t("color.show"));
                        const auto choice=popup(options,where);
                        if(choice>=0&&choice<6)setColorPreference(static_cast<color::Preference>(choice));
                        else if(choice==6){setColorHints(!showColorHints_);}
                    }
                } else if(i==2) {
                    const auto choice=popup({t("tendency.conservative"),t("tendency.balanced"),t("tendency.bold")},where);
                    if(choice>=0){state_.tendency=static_cast<HarmonicTendency>(choice);notifyState();}
                } else {
                    if(state_.constraints.melody.empty()) {
                        formMelody_=*parseMelodyNote("E4");
                        if(!state_.imported.events.empty()) {
                            const auto index=state_.productMode==session::ProductMode::Continue?
                                state_.imported.events.size()-1:selectedChord_.value_or(0);
                            const auto& chord=state_.imported.events[std::min(index,state_.imported.events.size()-1)];
                            formMelody_.durationQN=chord.durationQN.value_or(4);
                            formMelody_.startQN=chord.startQN+(state_.productMode==session::ProductMode::Continue?
                                formMelody_.durationQN:0);
                        }
                    } else formMelody_=state_.constraints.melody.front();
                    if(i==0) {
                        const auto choice=popup({t("melody.off"),t("melody.present"),t("melody.top")},where);
                        if(choice==0){state_.constraints.melody.clear();notifyState();}
                        else if(choice>0){formMelody_.role=static_cast<ConstraintRole>(choice-1);
                            beginForm(Form::Melody,melodyNoteName(formMelody_));}
                    } else beginForm(Form::Melody,melodyNoteName(formMelody_));
                }
                return kMouseEventHandled;
            }
        for(std::size_t i=0;i<layout_.topControls.size();++i)if(layout_.topControls[i].contains(where.x,where.y)) {
            if(i==0) {
                std::vector<std::string> names{t("label.auto")};
                for(int j=0;j<12;++j){names.push_back(formatKey({static_cast<PitchClass>(j),Mode::Major}));
                    names.push_back(formatKey({static_cast<PitchClass>(j),Mode::Minor}));}
                const auto choice=popup(names,where);
                if(choice>=0){state_.forcedKey=choice?std::optional<KeySignature>(KeySignature{
                    static_cast<PitchClass>((choice-1)/2),(choice-1)%2?Mode::Minor:Mode::Major}):std::nullopt;notifyState();}
            } else if(i==1) {
                const auto choice=popup({t("label.auto"),"Pop","Rock","R&B","Jazz","City Pop / J-pop",t("tag.functional")},where);
                constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                if(choice>=0){state_.style=choice?std::optional<Style>(styles[choice-1]):std::nullopt;notifyState();}
            } else if(i==2) {
                const auto choice=popup({t("label.auto"),t("group.develop"),t("group.resolve"),t("group.loop"),t("group.color")},where);
                if(choice>=0){state_.intent=choice?std::optional<PhraseIntent>(static_cast<PhraseIntent>(choice)):std::nullopt;notifyState();}
            } else if(i==3) {
                const auto choice=popup({t("nav.library"),t("nav.diagnostics"),
                    state_.locale==session::Locale::ZhCN?"English":"简体中文",t("nav.about"),
                    t("zoom.label")+" "+std::to_string(state_.uiZoomPercent)+"%",t("midi.import"),t("nav.hostDiagnostics"),t("nav.exportHostDiagnostics"),
                    std::string(showColorHints_?"✓ ":"")+t("color.show")},where);
                if(choice==0) state_.tab=session::Tab::Library;
                else if(choice==1) state_.tab=session::Tab::Diagnostics;
                else if(choice==2) state_.locale=state_.locale==session::Locale::ZhCN?
                    session::Locale::EnUS:session::Locale::ZhCN;
                else if(choice==3) {
                    popup({"HarmonyContinuation "+std::string(product::version),
                        t("about.build")+": "+std::string(product::buildType),
                        t("about.commit")+": "+std::string(product::commit),
                        t("about.factory")+": "+std::to_string(state_.factoryLibraryVersion),
                        t("about.db")+": "+std::to_string(product::databaseSchemaVersion),
                        t("about.session")+": "+std::to_string(session::PluginSessionState::currentSchemaVersion)},where);
                }
                else if(choice==4) {
                    const auto zoom=popup({"100%","125%","150%"},where);
                    if(zoom>=0)setUserZoom(std::array<std::uint32_t,3>{100,125,150}[zoom]);
                }
                else if(choice==8) {setColorHints(!showColorHints_);}
                else if(choice==5&&actions_.importMidi) {
                    const auto option=popup({t("midi.complete"),t("midi.open")},where);
                    if(option>=0)actions_.importMidi(option==1,{});
                }
                else if(choice==6&&actions_.hostDiagnostics){setHostText(actions_.hostDiagnostics());state_.tab=session::Tab::Diagnostics;}
                else if(choice==7&&actions_.exportHostDiagnostics){actionStatus_=actions_.exportHostDiagnostics();}
                if(choice>=0&&choice!=8) notifyState();
            }
            else if(i==4){state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Continue;
                selectedEnrichment_.reset();selectedLibrary_.reset();notifyState();}
            else {state_.tab=session::Tab::Recommend;state_.productMode=session::ProductMode::Enrich;
                selectedCandidate_.reset();selectedLibrary_.reset();notifyState();}
            return kMouseEventHandled;
        }
        const auto titleY=layout_.phrase.top-31;
        if(state_.tab==session::Tab::Recommend&&where.y>=titleY&&where.y<titleY+30&&where.x>layout_.viewport.right-160) {
            if(where.x<layout_.viewport.right-88)state_.tab=session::Tab::Diagnostics;
            else state_.debugExpanded=!state_.debugExpanded;
            notifyState();return kMouseEventHandled;
        }
        if(state_.tab==session::Tab::Recommend&&layout_.timeline.contains(where.x,where.y)) {
            for(const auto& b:timelineBlocks_) {
                const auto r=chordTileRect(b.eventIndex);
                if(where.x>=r.left&&where.x<r.right){selectedChord_=b.eventIndex;selectedCandidate_.reset();
                    selectedEnrichment_.reset();selectedLibrary_.reset();inspectorScroll_=0;refreshLayout();
                    focusTransientOverlay();invalid();break;}
            }
            return kMouseEventHandled;
        }
        if(layout_.inspectorMode!=InspectorMode::None&&layout_.inspector.contains(where.x,where.y)) {
            const auto area=layout_.inspector;
            if(where.y<area.top+39) {selectedCandidate_.reset();selectedEnrichment_.reset();
                selectedChord_.reset();selectedLibrary_.reset();invalid();return kMouseEventHandled;}
            if(where.y>=area.bottom-43) {
                if(state_.tab==session::Tab::Recommend&&(selectedCandidate()||selectedEnrichment())) {
                    const int columns=state_.productMode==session::ProductMode::Enrich?3:4;
                    const auto column=std::clamp(static_cast<int>((where.x-area.left-14)/((area.width()-28)/columns)),0,columns-1);
                    if(const auto* c=selectedEnrichment();c&&state_.productMode==session::ProductMode::Enrich) {
                        if(column==0&&actions_.auditionEnrichment)actions_.auditionEnrichment(*c);
                        else if(column==1)armMidi(*c,where);
                        else if(column==2&&actions_.saveEnrichmentSnapshot)actionStatus_=actions_.saveEnrichmentSnapshot(*c);
                    } else if(const auto* continuation=selectedCandidate()) {
                        if(column==0&&actions_.audition)actions_.audition(*continuation);
                        else if(column==1)armMidi(*continuation,where);
                        else if(column==2&&actions_.saveSnapshot)actionStatus_=actions_.saveSnapshot(*continuation);
                        else if(column==3){formMetadata_.name="My "+std::string(intentName(continuation->intent))+" Progression";formMetadata_.note.clear();
                            formMetadata_.style=state_.style;formMetadata_.intent=continuation->intent;beginForm(Form::Save,formMetadata_.name);}
                    }
                    invalid();return kMouseEventHandled;
                }
                if(state_.tab==session::Tab::Recommend&&state_.productMode==session::ProductMode::Enrich&&
                   selectedEnrichment()&&actions_.saveEnrichmentSnapshot) {
                    actionStatus_=actions_.saveEnrichmentSnapshot(*selectedEnrichment());invalid();
                } else if(state_.tab==session::Tab::Recommend&&selectedCandidate()) {
                    const auto* c=selectedCandidate();formMetadata_.name="My "+std::string(intentName(c->intent))+" Progression";
                    formMetadata_.note.clear();
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
                    formMetadata_.note=item.note;
                    formMetadata_.style.reset();
                    for(auto style:{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional})
                        if(item.styles&static_cast<StyleFlags>(style)){formMetadata_.style=style;break;}
                    beginForm(Form::Rename,item.name);
                    if(tagsEdit_){std::string tags;for(const auto& tag:item.tags){if(!tags.empty())tags+=", ";tags+=tag;}
                        tagsEdit_->setText(tags.c_str());}
                } else if(actions_.deleteUser) {
                    const auto choice=popup({t("form.cancel"),t("library.delete")+" "+item.name},where);
                    if(choice==1){actionStatus_=actions_.deleteUser(item.id);selectedLibrary_.reset();invalid();}
                }
            }
            if(state_.tab==session::Tab::Library&&selectedLibrary_&&!selectedLibrary_->first&&
               where.y>=area.bottom-110&&where.y<area.bottom-75&&actions_.updateUser) {
                auto item=user_[selectedLibrary_->second];item.favorite=!item.favorite;
                actionStatus_=actions_.updateUser(item);
                selectedLibrary_.reset();invalid();
            }
            return kMouseEventHandled;
        }
        if(!layout_.content.contains(where.x,where.y)) {
            if(layout_.compare.contains(where.x,where.y)) {
                if(state_.productMode==session::ProductMode::Enrich) {
                    const bool stacked=layout_.mode==LayoutMode::Compact;
                    const auto cell=stacked?layout_.compare.width()-16:(layout_.compare.width()-16)/3;
                    const auto index=stacked?static_cast<std::size_t>(std::max(0.,std::floor((where.y-layout_.compare.top-27)/26))):
                        static_cast<std::size_t>(std::max(0.,std::floor((where.x-layout_.compare.left-8)/cell)));
                    if(index<pinnedEnrichmentIds_.size()) {
                        const auto left=stacked?layout_.compare.left+8:layout_.compare.left+8+index*cell;
                        if(where.x>=left+cell-43) {
                            pinnedEnrichmentIds_.erase(pinnedEnrichmentIds_.begin()+static_cast<std::ptrdiff_t>(index));
                            refreshLayout();invalid();
                        }else if(const auto* candidate=findEnrichment(pinnedEnrichmentIds_[index])) {
                            for(std::size_t group=0;group<enrichments_.groups.size();++group)
                                for(std::size_t row=0;row<enrichments_.groups[group].size();++row)
                                    if(enrichments_.groups[group][row].id==candidate->id)
                                        selectedEnrichment_={{group,row}};
                            inspectorScroll_=0;refreshLayout();invalid();
                            focusTransientOverlay();
                        }
                    }
                    return kMouseEventHandled;
                }
                const bool stacked=layout_.mode==LayoutMode::Compact;
                const double cell=stacked?layout_.compare.width()-16:(layout_.compare.width()-16)/3;
                const auto index=stacked?static_cast<std::size_t>(std::max(0.,std::floor((where.y-layout_.compare.top-27)/26))):
                    static_cast<std::size_t>(std::max(0.,std::floor((where.x-layout_.compare.left-8)/cell)));
                if(index<state_.pinnedCandidateIds.size()) {
                    const double left=stacked?layout_.compare.left+8:layout_.compare.left+8+index*cell;
                    const auto id=state_.pinnedCandidateIds[index];
                    const auto* candidate=findCandidate(id);
                    const bool actionRow=stacked||where.y>=layout_.compare.top+47;
                    if(actionRow&&where.x>=left+cell-43){state_.unpin(id);
                        std::erase_if(pinnedSnapshots_,[&](const auto& c){return c.id==id;});notifyState();}
                    else if(actionRow&&candidate&&where.x>=left+cell-83&&actions_.exportMidi)
                        {actionStatus_=actions_.exportMidi(*candidate);invalid();}
                    else if(actionRow&&candidate&&where.x>=left+cell-123&&actions_.audition)actions_.audition(*candidate);
                }
            }
            return kMouseEventHandled;
        }
        if(state_.tab==session::Tab::Diagnostics||state_.tab==session::Tab::Match) {
            if(where.y>=layout_.content.top+36&&where.y<layout_.content.top+70) {
                state_.skeletonView=where.x>=layout_.content.left+125&&where.x<layout_.content.left+280;
                notifyState();
            }else if(where.y<layout_.content.top+38) {
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
                if(which==0){const auto n=popup({t("library.all"),"Pop","Rock","R&B","Jazz","City Pop / J-pop",t("tag.functional")},where);
                    constexpr Style styles[]{Style::Pop,Style::Rock,Style::Rnb,Style::Jazz,Style::CityPop,Style::Functional};
                    if(n>=0)libraryStyle_=n?std::optional<Style>(styles[n-1]):std::nullopt;}
                else if(which==1){const auto n=popup({t("label.auto"),t("group.develop"),t("group.resolve"),t("group.loop"),t("group.color")},where);
                    if(n>=0)libraryIntent_=n?std::optional<PhraseIntent>(static_cast<PhraseIntent>(n)):std::nullopt;}
                else if(which==2){const auto n=popup({t("filter.allSources"),t("filter.factoryOnly"),t("filter.userOnly"),
                    t("filter.allComplexity"),t("filter.basic"),t("filter.rich"),t("filter.advanced"),
                    t("filter.allTechniques"),t("filter.secondary"),t("filter.passing"),t("filter.borrowed"),
                    t("filter.allFavorites"),t("filter.favoriteOnly")},where);
                    if(n==0)librarySource_=-1; else if(n==1||n==2)librarySource_=n-1;
                    else if(n==3)libraryComplexity_.reset();
                    else if(n>=4&&n<=6)libraryComplexity_=static_cast<ComplexityLevel>(n-4);
                    else if(n==7)libraryTechnique_.clear();
                    else if(n==8)libraryTechnique_="SecondaryDominant";
                    else if(n==9)libraryTechnique_="PassingDiminished";
                    else if(n==10)libraryTechnique_="BorrowedChord";
                    else if(n==11)libraryFavoriteOnly_=false;
                    else if(n==12)libraryFavoriteOnly_=true;}
                else beginForm(Form::Search,librarySearch_);
                libraryPage_=0;invalid();return kMouseEventHandled;
            }
            if(where.y>=area.bottom-36){const auto filtered=filteredLibrary();
                if(where.x<area.left+180&&libraryPage_>0)--libraryPage_;
                else if(where.x>right-180&&(libraryPage_+1)*static_cast<std::size_t>(layout_.libraryRows)<filtered.size())++libraryPage_;
                else if(where.x>=area.left+180&&where.x<=right-180) {
                    const auto rows=static_cast<std::size_t>(std::max(1,layout_.libraryRows));
                    const auto count=std::max<std::size_t>(1,(filtered.size()+rows-1)/rows);
                    std::vector<std::string> pages;
                    pages.reserve(count);
                    for(std::size_t page=0;page<count;++page)
                        pages.push_back(t("library.page")+" "+std::to_string(page+1));
                    const auto selected=popup(pages,where);
                    if(selected>=0)libraryPage_=static_cast<std::size_t>(selected);
                }
                invalid();return kMouseEventHandled;}
            if(where.y>=area.top+88&&where.y<area.bottom-37) {
                const auto row=static_cast<std::size_t>((where.y-area.top-88)/36);
                const auto filtered=filteredLibrary();
                const auto index=libraryPage_*static_cast<std::size_t>(layout_.libraryRows)+row;
                if(index<filtered.size()){selectedLibrary_=filtered[index];selectedCandidate_.reset();
                    selectedEnrichment_.reset();selectedChord_.reset();inspectorScroll_=0;invalid();}
            }
            return kMouseEventHandled;
        }
        if(state_.productMode==session::ProductMode::Enrich) {
            for(std::size_t group=0;group<3;++group) {
                auto lane=layout_.lanes[group];lane.top-=contentScroll_;lane.bottom-=contentScroll_;
                if(!lane.contains(where.x,where.y))continue;
                if(where.y<lane.top+27&&where.x>=lane.right-120&&enrichments_.groups[group].size()>2) {
                    std::vector<enrichment::EnrichmentCandidate> listed;
                    for(const auto index:enrichmentOrder(group))listed.push_back(enrichments_.groups[group][index]);
                    const auto revision=state_.imported.revision;
                    std::vector<std::string> options;
                    for(const auto& c:listed) {
                        std::string path;for(const auto& chord:c.progression){if(!path.empty())path+=" → ";path+=chord.name;}
                        options.push_back(fixed(c.score,0)+" · "+path);
                    }
                    const auto index=popup(options,where);
                    if(index>=0&&static_cast<std::size_t>(index)<listed.size()&&revision==state_.imported.revision) {
                        const auto& original=listed[index];const auto& current=enrichments_.groups[group];
                        const auto found=std::find_if(current.begin(),current.end(),[&](const auto& c){return c.id==original.id&&c.fingerprint==original.fingerprint;});
                        if(found!=current.end()){selectedEnrichment_={{group,static_cast<std::size_t>(found-current.begin())}};selectedCandidate_.reset();selectedChord_.reset();inspectorScroll_=0;refreshLayout();focusTransientOverlay();invalid();}
                    }
                    return kMouseEventHandled;
                }
                const ScrollableCandidateList list(lane,enrichments_.groups[group].size(),enrichmentScroll_[group]);
                const auto hit=list.hit(where.x,where.y);
                if(!hit)return kMouseEventHandled;
                const auto row=*hit;
                const auto originalIndex=enrichmentOrder(group)[row];
                const auto& candidate=enrichments_.groups[group][originalIndex];
                const auto geometry=enrichmentRowGeometry(lane,list,row);
                if(geometry.pin.contains(where.x,where.y)) {
                    const auto found=std::find(pinnedEnrichmentIds_.begin(),pinnedEnrichmentIds_.end(),candidate.id);
                    if(found!=pinnedEnrichmentIds_.end())pinnedEnrichmentIds_.erase(found);
                    else if(pinnedEnrichmentIds_.size()<3)pinnedEnrichmentIds_.push_back(candidate.id);
                    refreshLayout();
                } else if(geometry.why.contains(where.x,where.y)) {
                    selectedEnrichment_={{group,originalIndex}};selectedChord_.reset();selectedCandidate_.reset();
                    inspectorScroll_=0;refreshLayout();focusTransientOverlay();
                } else if(geometry.snapshot.contains(where.x,where.y) && actions_.saveEnrichmentSnapshot)
                    actionStatus_=actions_.saveEnrichmentSnapshot(candidate);
                else if(geometry.midi.contains(where.x,where.y) && actions_.exportEnrichmentMidi)
                    armMidi(candidate,where);
                else if(geometry.audition.contains(where.x,where.y) && actions_.auditionEnrichment)
                    actions_.auditionEnrichment(candidate);
                else {selectedEnrichment_={{group,originalIndex}};selectedChord_.reset();selectedCandidate_.reset();
                    inspectorScroll_=0;refreshLayout();focusTransientOverlay();}
                invalid();return kMouseEventHandled;
            }
            return kMouseEventHandled;
        }
        const auto presentation=continuationPresentation();
        for(std::size_t visual=0;visual<4;++visual) {
            auto lane=layout_.lanes[visual];lane.top-=contentScroll_;lane.bottom-=contentScroll_;
            if(!lane.contains(where.x,where.y))continue;
            const auto group=groupForVisual(visual,state_.intent);
            if(where.y<lane.top+27) {
                if(where.x>=lane.right-120&&recommendations_.groups[group].size()>2) {
                    refreshColorRanking();
                    std::vector<ContinuationCandidate> listed;
                    for(const auto index:colorRanks_[group].order)listed.push_back(recommendations_.groups[group][index]);
                    const auto revision=state_.imported.revision;
                    std::vector<std::string> options;
                    for(const auto& c:listed) {
                        std::string path;for(const auto& e:c.continuation){if(!path.empty())path+=" → ";path+=e.label;}
                        options.push_back(std::to_string(static_cast<int>(std::round(c.rankingScore)))+" · "+path);
                    }
                    const auto selected=popup(options,where);
                    if(selected>=0&&static_cast<std::size_t>(selected)<listed.size()&&revision==state_.imported.revision) {
                        const auto& original=listed[selected];const auto fingerprint=session::continuationFingerprint(original);const auto& current=recommendations_.groups[group];
                        const auto found=std::find_if(current.begin(),current.end(),[&](const auto& c){return c.id==original.id&&session::continuationFingerprint(c)==fingerprint;});
                        if(found!=current.end()){selectedCandidate_={{group,static_cast<std::size_t>(found-current.begin())}};
                            selectedChord_.reset();selectedEnrichment_.reset();refreshLayout();focusTransientOverlay();inspectorScroll_=0;
                            if(actions_.benchmarkSelect)actions_.benchmarkSelect(*found);invalid();}
                    }
                }
                return kMouseEventHandled;
            }
            const auto& visible=presentation[group];
            const ScrollableCandidateList list(lane,visible.size(),continuationScroll_[group]);
            const auto hit=list.hit(where.x,where.y);
            if(!hit)return kMouseEventHandled;
            const auto row=*hit;
            const auto index=visible[row];const auto& c=recommendations_.groups[group][index];
            const auto geometry=list.continuationRow(lane,row,static_cast<bool>(actions_.exportMidi));
            if(actions_.benchmarkSelect)actions_.benchmarkSelect(c);
            if(geometry.pin.contains(where.x,where.y)){
                if(state_.unpin(c.id))std::erase_if(pinnedSnapshots_,[&](const auto& item){return item.id==c.id;});
                else if(state_.pin(c.id,session::continuationFingerprint(c)))pinnedSnapshots_.push_back(c);
                notifyState();
            } else if(geometry.audition.contains(where.x,where.y)&&actions_.audition)actions_.audition(c);
            else if(geometry.midi.contains(where.x,where.y)&&actions_.exportMidi){if(!armMidi(c,where))actionStatus_=actions_.exportMidi(c);invalid();}
            else if(geometry.snapshot.contains(where.x,where.y)&&actions_.saveSnapshot){actionStatus_=actions_.saveSnapshot(c);invalid();}
            else {selectedCandidate_={{group,index}};selectedChord_.reset();selectedEnrichment_.reset();
                inspectorScroll_=0;refreshLayout();focusTransientOverlay();invalid();}
            return kMouseEventHandled;
        }
    } catch(...) {actionStatus_="Action failed";invalid();}
    return kMouseEventHandled;
}
void MainView::drawResponsiveLibrary(CDrawContext* dc) {
    const auto area=layout_.content;
    const double right=layout_.inspectorMode==InspectorMode::Side?layout_.inspector.left-10:area.right;
    box(dc,viewRect(area),panel);
    line(dc,t("nav.library")+"   "+t("library.factory")+" "+std::to_string(factory_.size())+
        " · "+t("library.user")+" "+std::to_string(user_.size()),
        CRect(area.left+10,area.top+6,right-10,area.top+32),accent);
    const double filterY=area.top+36;
    const double unit=(right-area.left-20)/4;
    line(dc,t("label.style")+": "+(libraryStyle_?styleName(*libraryStyle_):t("library.all")),CRect(area.left+10,filterY,area.left+10+unit,filterY+28),muted);
    line(dc,t("label.intent")+": "+(libraryIntent_?tIntent(*libraryIntent_):t("label.auto")),CRect(area.left+10+unit,filterY,area.left+10+unit*2,filterY+28),muted);
    line(dc,t("library.filter")+": "+
        (librarySource_==0?t("library.factory"):librarySource_==1?t("library.user"):t("library.all"))+
        (libraryComplexity_?" · 复杂度":"")+(libraryFavoriteOnly_?" · ★":""),
        CRect(area.left+10+unit*2,filterY,area.left+10+unit*3,filterY+28),muted);
    line(dc,t("library.search")+": "+(librarySearch_.empty()?t("library.nameTag"):librarySearch_),
        CRect(area.left+10+unit*3,filterY,right-10,filterY+28),muted);
    if(!libraryError_.empty())line(dc,t("library.error")+": "+libraryError_,CRect(area.left+10,area.top+67,right-10,area.top+89),
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
        const auto displayName=state_.locale==session::Locale::ZhCN && !item.nameZh.empty()?item.nameZh:
            state_.locale==session::Locale::EnUS && !item.nameEn.empty()?item.nameEn:item.name;
        line(dc,displayName,CRect(area.left+16,y+3,nameEnd,y+29),pale);
        line(dc,primaryStyle(item.styles),CRect(nameEnd+8,y+3,nameEnd+115,y+29),muted);
        line(dc,tIntent(item.intent),CRect(nameEnd+120,y+3,nameEnd+220,y+29),muted);
        if(layout_.mode!=LayoutMode::Compact) {
            line(dc,item.mode==Mode::Major?"Major":"Minor",CRect(nameEnd+225,y+3,nameEnd+315,y+29),muted);
            line(dc,factory?t("library.factory"):t("library.user"),CRect(nameEnd+320,y+3,right-16,y+29),accent);
        }
    }
    }
    line(dc,"◀ "+t("library.previous"),CRect(area.left+10,area.bottom-32,area.left+175,area.bottom-4),muted);
    line(dc,t("library.page")+" "+std::to_string(libraryPage_+1)+" / "+
        std::to_string(std::max<std::size_t>(1,(filtered.size()+rows-1)/rows)),
        CRect(area.left+185,area.bottom-32,right-185,area.bottom-4),muted,kCenterText);
    line(dc,t("library.next")+" ▶",CRect(right-165,area.bottom-32,right-10,area.bottom-4),muted,kRightText);
}

void MainView::drawResponsiveDiagnostics(CDrawContext* dc) {
    const auto area=layout_.content;
    box(dc,viewRect(area),panel);
    line(dc,t("diagnostics.title")+"    ["+t("diagnostics.refresh")+"]    ["+t("diagnostics.clipboard")+"]",
        CRect(area.left+10,area.top+7,area.right-10,area.top+34),accent);
    line(dc,"FULL",CRect(area.left+12,area.top+39,area.left+120,area.top+66),state_.skeletonView?muted:accent);
    line(dc,"SKELETON",CRect(area.left+125,area.top+39,area.left+280,area.top+66),state_.skeletonView?accent:muted);
    line(dc,matchStatus_,CRect(area.left+10,area.top+72,area.right-10,area.top+98),pale);
    line(dc,t("diagnostics.generation")+" "+std::to_string(generation_)+"    "+t("diagnostics.worker")+" "+
        (workerBusy_?t("diagnostics.busy"):t("diagnostics.idle"))+"    "+t("diagnostics.last")+" "+
        fixed(computationMs_,2)+" ms",CRect(area.left+10,area.top+102,area.right-10,area.top+128),muted);
    double y=area.top+137;
    if(selectedChord_&&*selectedChord_<analysis_.full.size()){
        const auto& chord=analysis_.full[*selectedChord_];
        line(dc,t("inspector.chord")+" "+state_.imported.events[*selectedChord_].name+" · "+formatDegree(chord)+" · "+
            formatFunction(chord.function)+" · "+t("inspector.weight")+" "+fixed(chord.structuralWeight,2)+" · "+roles(chord.roles),
            CRect(area.left+10,y,area.right-10,y+26),pale);y+=30;
    }
    if(!matches_.empty()){
        line(dc,t("diagnostics.topMatch")+": "+matches_.front().templateName+" ["+matches_.front().templateId+"] · "+
            fixed(matches_.front().similarity,2),CRect(area.left+10,y,area.right-10,y+26),pale);y+=30;
    }
    std::istringstream in(hostText_+"\n"+parseText_+"\n"+rawText_);std::string row;
    ConcatClip clip(*dc,viewRect(area));
    while(y<area.bottom-20&&std::getline(in,row)) {line(dc,row.substr(0,165),CRect(area.left+10,y,area.right-10,y+18),muted);y+=18;}
}

void MainView::drawResponsiveInspector(CDrawContext* dc) {
    if(layout_.inspectorMode==InspectorMode::None)return;
    const auto area=layout_.inspector;
    box(dc,viewRect(area),CColor(18,30,47,255));
    line(dc,"×   "+t("inspector.title"),CRect(area.left+12,area.top+8,area.right-12,area.top+34),accent);
    double y=area.top+40-inspectorScroll_;
    {
    ConcatClip clip(*dc,CRect(area.left,area.top+38,area.right,area.bottom-45));
    const auto add=[&](std::string text,CColor color=pale) {
        line(dc,std::move(text),CRect(area.left+14,y,area.right-14,y+23),color);y+=27;
    };
    const auto paragraph=[&](const std::string& text,CColor color=pale) {
        std::string row;dc->setFont(kNormalFont);const auto width=area.width()-28.;
        for(std::size_t i=0;i<text.size();) {
            const auto lead=static_cast<unsigned char>(text[i]);
            const std::size_t length=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            const auto part=text.substr(i,length);i+=length;row+=part;
            if(dc->getStringWidth(row.c_str())>width&&row.size()>part.size()) {
                const auto space=row.rfind(' ',row.size()-part.size()-1);
                if(space!=std::string::npos&&space>0){add(row.substr(0,space),color);row.erase(0,space+1);}
                else {add(row.substr(0,row.size()-part.size()),color);row=part;}
            }
        }
        if(!row.empty())add(row,color);
    };
    const auto colorDetails=[&](const color::PathColor& path,bool enrich) {
        if(!showColorHints_)return;
        add(t("whyV2.metrics"),accent);
        for(std::size_t i=0;i<std::min<std::size_t>(8,path.positions.size());++i) {
            const auto& p=path.positions[i];
            const auto number=[](std::optional<double> value){return value?fixed(*value,2):std::string("—");};
            paragraph(std::to_string(i+1)+"  T "+number(p.chord.T)+"  W "+number(p.chord.W)+"  S "+number(p.chord.S),muted);
            if(p.fromPrevious)paragraph("ΔT "+fixed(p.fromPrevious->deltaT,2)+"  ΔW "+fixed(p.fromPrevious->deltaW,2)+
                "  Ts "+fixed(p.fromPrevious->Ts,2)+"  Ws "+fixed(p.fromPrevious->Ws,2),muted);
        }
        if(path.positions.size()>8)add(t("whyV2.moreMetrics"),muted);
        paragraph(t("color.modelNote"),muted);
        if(enrich)paragraph(t("colorSort.symbolicBass"),muted);
    };
    const auto addPath=[&](std::string title,const std::vector<std::string>& names){
        const auto limit=static_cast<std::size_t>(std::max(18.,(area.width()-40)/8.));
        std::string row=std::move(title);
        for(const auto& name:names){const std::string part=(row.empty()?"":" → ")+name;
            if(row.size()+part.size()>limit&&row.size()>10){add(row);row="    "+name;}
            else row+=part;}
        if(!row.empty())add(row);
    };
    const auto addMelody=[&](const MelodyCompatibility& compatibility) {
        constexpr const char* intervals[]{"1","b9","9","b3","3","11","#11","5","b13","13","b7","7"};
        for(std::size_t i=0;i<std::min<std::size_t>(4,compatibility.observations.size());++i) {
            const auto& observation=compatibility.observations[i];MelodyConstraint note;note.pitchClass=observation.pitchClass;
            const auto relation=observation.relation==MelodyRelation::ChordTone?"melody.chordTone":
                observation.relation==MelodyRelation::AvailableTension?"melody.tension":
                observation.relation==MelodyRelation::Conflict?"melody.conflict":"melody.uncertain";
            add(melodyNoteName(note)+" · "+observation.chord+" · "+t(relation)+" ("+
                intervals[std::clamp(observation.interval,0,11)]+")",
                observation.relation==MelodyRelation::Conflict?accent:muted);
            if(!observation.topVoiceSatisfied)add(t("melody.topConflict"),accent);
        }
        if(state_.debugExpanded&&!compatibility.observations.empty())add(t("melody.score")+" "+fixed(compatibility.score,2),muted);
    };
    if(state_.tab==session::Tab::Recommend&&state_.productMode==session::ProductMode::Enrich&&selectedEnrichment()) {
        const auto& candidate=*selectedEnrichment();
        add(t("inspector.why"),accent);
        const auto hint=candidateHint(candidate);
        std::optional<color::RankReason> reason;bool promoted=false;
        if(colorPreference_!=color::Preference::Off) {
            refreshEnrichmentColorRanking();const auto [group,index]=*selectedEnrichment_;
            const auto& rank=enrichmentColorRanks_[group];reason=rank.decisions[index].reason;
            promoted=static_cast<std::size_t>(std::find(rank.order.begin(),rank.order.end(),index)-rank.order.begin())<index;
        }
        std::vector<std::string> path;for(const auto& chord:candidate.progression)path.push_back(chord.name);
        addPath(t("mode.enrich"),path);
        const auto explanation=explainWhy(functionalWhy(candidate,state_.locale),hint,state_.locale,colorPreference_,reason,promoted);
        for(const auto& sentence:explanation.sentences)paragraph(sentence);
        if(state_.debugExpanded) {
            add(t("whyV2.details"),accent);add("Fingerprint: "+candidate.fingerprint,muted);
            std::vector<std::string> sourceNames,transformedNames;
            for(const auto& chord:state_.imported.events)sourceNames.push_back(chord.name);
            for(const auto& chord:candidate.progression)transformedNames.push_back(chord.name);
            addPath(t("inspector.user"),sourceNames);
            add(t("inspector.score")+"  "+fixed(candidate.score,1)+" · "+
                t("inspector.preservation")+"  "+fixed(candidate.skeletonPreservation,2),muted);
            add(t("inspector.styleFit")+"  "+fixed(candidate.styleCompatibility,2)+" · "+
                t("inspector.complexity")+"  "+fixed(candidate.complexityScore,2),muted);
            addMelody(candidate.melodyCompatibility);
            const auto sourceVoice=preview::measureVoiceLeading(state_.imported.events);
            const auto enrichedVoice=preview::measureVoiceLeading(candidate.progression);
            if(sourceVoice&&enrichedVoice) {
                const bool inversion=std::any_of(candidate.techniques.begin(),candidate.techniques.end(),
                    [](auto technique){return technique==enrichment::TechniqueID::Inversion;});
                if(inversion&&enrichedVoice->bassMotionSemitones<sourceVoice->bassMotionSemitones)
                    add(t("voice.inversionImprovesBass"),accent);
                else if(enrichedVoice->score>sourceVoice->score+0.03f)
                    add(t("voice.smoother"),accent);
                add(t("voice.commonTones")+"  "+std::to_string(enrichedVoice->commonToneCount),muted);
                add(t("voice.bass")+"  "+bassLine(enrichedVoice->bassMidi),muted);
                add(t("voice.upper")+"  "+(enrichedVoice->upperVoiceTotalMotion<=sourceVoice->upperVoiceTotalMotion?
                    t("voice.upperSmoother"):t("voice.upperVaried")),muted);
                if(state_.debugExpanded)add(t("voice.score")+"  "+fixed(enrichedVoice->score,2),muted);
            }
            for(const auto& operation:candidate.operations) {
                add(operation.before+" → "+operation.after,pale);
                add(t("technique."+std::string(enrichment::techniqueName(operation.technique)))+" · "+
                    t("reason."+std::string(enrichment::techniqueName(operation.technique))),muted);
            }
            if(showColorHints_)colorDetails(candidateColor(candidate),true);
        }
    } else if(state_.tab==session::Tab::Recommend&&selectedCandidate()) {
        const auto& c=*selectedCandidate();
        add(t("inspector.why"),accent);
        std::optional<ScaleDegree> phraseStart,current;
        if(analysis_.selectedKey&&analysis_.selectedKey->key.tonic==c.key.tonic&&
           analysis_.selectedKey->key.mode==c.key.mode&&!analysis_.full.empty()) {
            phraseStart=analysis_.full.front().degree;current=analysis_.full.back().degree;
        }
        const auto completion=evaluateIntentCompletion(c,phraseStart,current);
        const auto hint=candidateHint(c);std::optional<color::RankReason> reason;bool promoted=false;
        if(colorPreference_!=color::Preference::Off) {
            refreshColorRanking();const auto [group,index]=*selectedCandidate_;
            const auto& rank=colorRanks_[group];reason=rank.decisions[index].reason;
            promoted=static_cast<std::size_t>(std::find(rank.order.begin(),rank.order.end(),index)-rank.order.begin())<index;
        }
        std::vector<std::string> path;for(const auto& chord:c.continuation)path.push_back(chord.label);
        addPath(t("mode.continue"),path);
        const auto explanation=explainWhy(functionalWhy(completion,state_.locale),hint,state_.locale,colorPreference_,reason,promoted);
        for(const auto& sentence:explanation.sentences)paragraph(sentence);
        if(state_.debugExpanded) {
            add(t("whyV2.details"),accent);add("Fingerprint: "+session::continuationFingerprint(c),muted);
            addMelody(c.melodyCompatibility);
            add(tIntent(c.intent)+" · "+formatKey(c.key)+" · "+t("inspector.score")+" "+fixed(c.rankingScore,1),muted);
            add(t("inspector.completion")+" "+fixed(completion.score,2),muted);
            Progression voiced=state_.imported.events;
            for(const auto& event:c.continuation) {
                ChordEvent next;next.name=event.label;next.quality=event.quality;
                voiced.push_back(std::move(next));
            }
            if(const auto voice=preview::measureVoiceLeading(voiced)) {
                add(t("voice.commonTones")+"  "+std::to_string(voice->commonToneCount)+" · "+
                    t("voice.bass")+"  "+bassLine(voice->bassMidi),muted);
                if(state_.debugExpanded)add(t("voice.score")+"  "+fixed(voice->score,2),muted);
            }
            std::vector<std::string> userNames,skeletonNames,continuationNames;
            for(const auto& chord:state_.imported.events)userNames.push_back(chord.name);
            for(const auto index:analysis_.skeletonIndices)if(index<analysis_.full.size())
                skeletonNames.push_back(formatDegree(analysis_.full[index]));
            for(const auto& chord:c.continuation)continuationNames.push_back(chord.label);
            addPath(t("inspector.user"),userNames);addPath("SKELETON",skeletonNames);
            const auto match=std::find_if(matches_.begin(),matches_.end(),[&](const auto& m){return m.templateId==c.primaryTemplate;});
            if(match!=matches_.end())addPath(t("inspector.matched"),match->templateLabels);

            add(t("inspector.source")+"  "+c.primaryTemplate,muted);
            add(t("nav.match")+"  "+fixed(c.matchSimilarity,2),muted);
            add(t("inspector.cadence")+"  "+std::string(cadenceName(c.cadence)),muted);
            add(t("label.style")+"  "+styleFlags(c.styles),muted);
            add(t("inspector.rhythm")+"  "+fixed(c.subscores.rhythm,2)+"   "+t("label.style")+"  "+fixed(c.subscores.style,2),muted);
            add(t("label.intent")+"  "+fixed(c.subscores.intent,2)+"   "+t("inspector.prior")+"  "+fixed(c.subscores.prior,2),muted);
            add(t("inspector.support")+"  "+std::to_string(c.supportCount),muted);
            add("OPEN  "+(c.suggestedCurrentChordDurationQN?fixed(*c.suggestedCurrentChordDurationQN):t("inspector.fallback"))+" QN",muted);
            drawMiniTimeline(dc,c,CRect(area.left+14,y+2,area.right-14,y+27));y+=38;
            if(match!=matches_.end())add("SKELETON "+fixed(match->subScores.skeletonHarmony,2)+
                "  FULL "+fixed(match->subScores.fullHarmony,2),muted);
            std::string sources;for(const auto& id:c.supportingTemplates){if(!sources.empty())sources+=", ";sources+=id;}
            add(t("inspector.sources")+"  "+sources,muted);
            if(match!=matches_.end())for(const auto& step:match->alignmentTrace) {
                const auto user=step.queryIndex&&*step.queryIndex<state_.imported.events.size()?state_.imported.events[*step.queryIndex].name:t("inspector.gap");
                const auto templ=step.templateIndex&&*step.templateIndex<match->templateLabels.size()?match->templateLabels[*step.templateIndex]:t("inspector.gap");
                add(user+"  ↔  "+templ,muted);
            }
            if(showColorHints_)colorDetails(candidateColor(c),false);
        }
    } else if(state_.tab==session::Tab::Library&&selectedLibrary_) {
        const auto [factory,index]=*selectedLibrary_;
        const auto* item=factory?(index<factory_.size()?&factory_[index]:nullptr):(index<user_.size()?&user_[index]:nullptr);
        if(!item)return;
        add(state_.locale==session::Locale::ZhCN && !item->nameZh.empty()?item->nameZh:
            state_.locale==session::Locale::EnUS && !item->nameEn.empty()?item->nameEn:item->name);
        add("ID  "+item->id,muted);
        add(t("label.style")+"  "+styleFlags(item->styles),muted);
        add(t("label.intent")+"  "+tIntent(item->intent),muted);
        add(t("inspector.mode")+"  "+(item->mode==Mode::Major?t("inspector.major"):t("inspector.minor")),muted);
        std::string path;for(const auto& e:item->full){if(!path.empty())path+=" → ";path+=formatMatchEvent(e);}
        add("FULL  "+path,muted);
        std::string skeleton;for(const auto skeletonIndex:item->skeletonIndices)if(skeletonIndex<item->full.size()){
            if(!skeleton.empty())skeleton+=" → ";skeleton+=formatMatchEvent(item->full[skeletonIndex]);}
        add("SKELETON  "+skeleton,muted);
        std::string rhythm;for(const auto& e:item->full){if(!rhythm.empty())rhythm+=" · ";rhythm+=e.durationQN?fixed(*e.durationQN):"?";}
        add(t("inspector.rhythm")+" QN  "+rhythm,muted);
        add(t("inspector.cadence")+"  "+std::string(cadenceName(item->cadence)),muted);
        add(t("inspector.meter")+"  "+std::to_string(item->meterNumerator)+"/"+std::to_string(item->meterDenominator),muted);
        add(t("inspector.prior")+"  "+fixed(item->priorWeight,2),muted);
        std::string tags;for(const auto& tag:item->tags){if(!tags.empty())tags+=", ";tags+=tag;}add(t("form.tags")+"  "+tags,muted);
        if(!item->note.empty())add(t("form.note")+"  "+item->note,muted);
        std::size_t nearCount{};for(const auto& other:factory_)if(other.id!=item->id&&
            other.fingerprint.skeletonDegrees==item->fingerprint.skeletonDegrees&&
            other.fingerprint.rhythmShape==item->fingerprint.rhythmShape)++nearCount;
        add(t("inspector.nearDuplicate")+"  "+std::to_string(nearCount),muted);
    } else if(selectedChord_&&*selectedChord_<analysis_.full.size()) {
        const auto& c=state_.imported.events[*selectedChord_];const auto& a=analysis_.full[*selectedChord_];
        add(c.name+" · "+formatDegree(a));
        add(t("inspector.function")+"  "+formatFunction(a.function));
        add(t("inspector.weight")+"  "+fixed(a.structuralWeight,2));
        add(t("inspector.roles")+"  "+roles(a.roles));
        add(t("inspector.target")+"  "+(a.target?std::to_string(a.target->degree):"—"));
        add(t("inspector.confidence")+"  "+fixed(a.analysisConfidence,2));
        add(t("inspector.timing")+"  "+fixed(c.startQN)+" QN, "+(c.durationQN?fixed(*c.durationQN):"OPEN"));
    }
    inspectorScrollMax_=std::max(0.,y+inspectorScroll_-(area.bottom-45));
    }
    if(state_.tab==session::Tab::Recommend&&(selectedCandidate()||selectedEnrichment())) {
        const std::size_t count=state_.productMode==session::ProductMode::Enrich?3:4;
        const double cell=(area.width()-28)/count;
        const std::array<std::string,4> titles{t("candidate.play"),t("midi.button"),t("candidate.snapshot"),
            state_.productMode==session::ProductMode::Continue?t("form.save"):t("candidate.why")};
        for(std::size_t i=0;i<count;++i)line(dc,titles[i],CRect(area.left+14+i*cell,area.bottom-39,area.left+14+(i+1)*cell,area.bottom-8),accent,kCenterText);
    } else if(state_.tab==session::Tab::Recommend&&state_.productMode==session::ProductMode::Enrich&&
       selectedEnrichment()&&actions_.saveEnrichmentSnapshot)
        line(dc,t("candidate.snapshot"),CRect(area.left+14,area.bottom-39,area.right-14,area.bottom-8),accent,kRightText);
    else if(state_.tab==session::Tab::Recommend&&selectedCandidate())
        line(dc,t("form.save"),CRect(area.left+14,area.bottom-39,area.right-14,area.bottom-8),accent,kRightText);
    else if(state_.tab==session::Tab::Library&&selectedLibrary_) {
        if(selectedLibrary_->first)line(dc,t("library.readOnly"),CRect(area.left+14,area.bottom-70,area.right-14,area.bottom-45),muted);
        else {line(dc,t("library.rename")+"     "+t("library.delete"),CRect(area.left+14,area.bottom-70,area.right-14,area.bottom-45),accent);
            const auto& item=user_[selectedLibrary_->second];
            line(dc,item.favorite?"★ "+t("library.unfavorite"):"☆ "+t("library.favorite"),CRect(area.left+14,area.bottom-108,area.right-14,area.bottom-77),accent);}
        if(actions_.exportLibraryMidi)line(dc,t("candidate.midi"),CRect(area.left+14,area.bottom-39,area.right-14,area.bottom-8),accent,kRightText);
    }
}

void MainView::drawResponsiveForm(CDrawContext* dc) {
    if(form_==Form::None)return;
    const double w=std::min(590.,layout_.viewport.width()-48),h=360;
    const double x=(layout_.viewport.width()-w)/2,y=(layout_.viewport.height()-h)/2;
    box(dc,CRect(x,y,x+w,y+h),CColor(23,39,59,255));
    line(dc,form_==Form::Melody?t("melody.label"):form_==Form::Save?t("form.save"):form_==Form::Rename?t("form.edit"):t("form.search"),
        CRect(x+22,y+12,x+w-22,y+43),accent);
    line(dc,form_==Form::Melody?t("melody.note"):form_==Form::Search?t("library.nameTag"):t("form.name"),CRect(x+22,y+53,x+140,y+84),muted);
    if(form_==Form::Melody) {
        line(dc,t("melody.role")+": "+t(formMelody_.role==ConstraintRole::TopVoice?"melody.top":"melody.present"),CRect(x+22,y+115,x+w-22,y+142),pale);
        line(dc,t("melody.strictness")+": "+t(formMelody_.strictness==ConstraintStrictness::Hard?"melody.hard":"melody.soft"),CRect(x+22,y+146,x+w-22,y+174),pale);
        line(dc,t("melody.start"),CRect(x+22,y+183,x+w-22,y+210),muted);
        line(dc,t("melody.duration"),CRect(x+22,y+244,x+w-22,y+269),muted);
    } else if(form_!=Form::Search) {
        line(dc,t("label.style")+": "+(formMetadata_.style?styleName(*formMetadata_.style):t("label.auto")),CRect(x+22,y+115,x+w-22,y+142),pale);
        line(dc,t("label.intent")+": "+tIntent(formMetadata_.intent),CRect(x+22,y+146,x+w-22,y+174),pale);
        line(dc,t("form.tags"),CRect(x+22,y+183,x+w-22,y+210),muted);
        line(dc,t("form.note"),CRect(x+22,y+244,x+w-22,y+269),muted);
    }
    line(dc,t("form.cancel"),CRect(x+150,y+h-39,x+270,y+h-7),muted);
    line(dc,form_==Form::Search?t("form.apply"):t("form.save"),CRect(x+w-150,y+h-39,x+w-25,y+h-7),accent,kRightText);
}
} // namespace harmony::ui
