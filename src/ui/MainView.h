#pragma once
#include "core/CurrentChordLocator.h"
#include "core/ContinuationEngine.h"
#include "enrichment/EnrichmentProvenance.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "localization/Localization.h"
#include "session/PluginSessionState.h"
#include "session/ProductServices.h"
#include "ui/ProgressionTimeline.h"
#include "ui/UILayout.h"
#include "ui/ScrollableCandidateList.h"
#include "ui/OverlayPolicy.h"
#include "ui/EffectiveScale.h"
#include "ui/ColorHint.h"
#include "ui/WhyExplanation.h"
#include "ui/RecommendationBrowse.h"
#include "color/ColorPreferenceReranker.h"
#include "midi/MidiWorkflow.h"
#include "wish/WishWorkflow.h"
#include "preview/PreviewSequence.h"
#include "preview/PreviewTransport.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/dragging.h"
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace VSTGUI { class CTextEdit; }
namespace harmony::ui {
class MainView final : public VSTGUI::CView, public VSTGUI::IDropTarget {
public:
    struct EditorUiState {
        std::shared_ptr<wish::WishWorkflow> wishWorkspace;
        wish::WishSettings wishSettings;
        wish::WishInput wishInput;
        bool wishMode{},wishAdvanced{},wishRequested{};
        std::size_t wishPage{};
        std::string wishSelected,wishExpanded;
        double wishScroll{};
        std::string continuationId,continuationFingerprint,enrichmentId,enrichmentFingerprint;
        std::optional<std::size_t> chord;
        std::array<double,4> continuationScroll{};std::array<double,3> enrichmentScroll{};
        double contentScroll{},timelineScroll{},inspectorScroll{};
        std::vector<std::string> pinnedEnrichmentIds;
        bool showColorHints{true};
        bool detailedCards{};
        std::array<std::size_t,5> browsePages{};
        std::array<double,5> browseScroll{};
        std::array<std::string,5> browseSelected{};
        std::size_t browseCategory{};
        color::Preference colorPreference{color::Preference::Off};
    };
    void prepareForDetach() { seekGesture_.reset();previewSeekHoverId_.clear();previewSeekCursorHover_=false;actions_={};midiPending_.reset();midiClick_={}; }
    EditorUiState captureEditorUiState() const;
    void restoreEditorUiState(const EditorUiState&);
    struct Actions {
        std::function<void(VSTGUI::IDataPackage*)> drop;
        std::function<void()> refresh;
        std::function<void()> clipboard;
        std::function<void(const session::PluginSessionState&)> stateChanged;
        std::function<std::string(const ContinuationCandidate&, const session::SaveMetadata&)> save;
        std::function<std::string(const ProgressionTemplate&)> updateUser;
        std::function<std::string(const std::string&)> deleteUser;
        std::function<void(const ContinuationCandidate&)> audition;
        std::function<void()> stopAudition;
        std::function<void()> pauseAudition,resumeAudition;
        std::function<void(bool)> previewBackend;
        std::function<void(const std::string&,double)> seekAudition;
        std::function<void(bool,double,bool)> previewSettings; // custom tempo, BPM, suffix only
        std::function<std::string(const ContinuationCandidate&)> exportMidi;
        std::function<std::string(const ContinuationCandidate&)> saveSnapshot;
        std::function<std::string(const ProgressionTemplate&)> exportLibraryMidi;
        std::function<void(const ContinuationCandidate&)> benchmarkSelect;
        std::function<void(const enrichment::EnrichmentCandidate&)> auditionEnrichment;
        std::function<std::string(const enrichment::EnrichmentCandidate&)> exportEnrichmentMidi;
        std::function<std::string(const enrichment::EnrichmentCandidate&)> saveEnrichmentSnapshot;
        std::function<midi::PayloadResult(const ContinuationCandidate&)> midiPayload;
        std::function<midi::PayloadResult(const enrichment::EnrichmentCandidate&)> enrichmentMidiPayload;
        std::function<void(bool,const std::filesystem::path&)> importMidi;
        std::function<void(std::vector<std::filesystem::path>)> importMidiFiles;
        std::function<std::string()> hostDiagnostics;
        std::function<std::string()> exportHostDiagnostics;
        std::function<void(bool,bool)> observeDrop,observeMidiDrag;
        std::function<std::string(const midi::MidiClipPayload&)> saveMidiPayload;
    };
    MainView(const VSTGUI::CRect&, Actions);
    VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget() override;
    VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData) override;
    VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData) override;
    void onDragLeave(VSTGUI::DragEventData) override;
    bool onDrop(VSTGUI::DragEventData) override;
    void setSessionState(const session::PluginSessionState&);
    void setEditorSizeState(int width,int height) noexcept {
        state_.editorWidth=static_cast<std::uint32_t>(width);
        state_.editorHeight=static_cast<std::uint32_t>(height);
    }
    void clearSessionDirty() { sessionDirty_=false; invalid(); }
    void setHostText(std::string, std::string refreshSummary = {});
    void setProgressionSession(const ImportedProgressionSession&);
    void setAnalysis(const HarmonicAnalysisResult&);
    void setMatches(const std::vector<MatchResult>&, std::string status);
    void setRecommendations(const RecommendationSet&);
    void setEnrichments(const enrichment::EnrichmentResult& value);
    void setPreviewPosition(std::string candidateId,double positionQN,double totalQN);
    void setPreviewTransport(preview::PlaybackState,bool hostBackend,bool hostAvailable,bool pending=false);
    void setSnapshotMode(bool enabled);
    preview::SourceRouteResult continuationRoute(const ContinuationCandidate&) const;
    const wish::WishItem* wishItem(const std::string&) const;
    bool currentWishCandidate(const ContinuationCandidate&) const;
    void pollWish();
    bool runWishSmoke(VSTGUI::CDrawContext*,const std::function<void(const std::string&)>& capture = {});
    void setActionStatus(std::string status) { actionStatus_=std::move(status); invalid(); }
    void setPlaybackPosition(std::optional<double> projectQN, bool playing);
    void setDropReport(std::string, std::string outcome, bool inputAttempt = true);
    void setLibrary(std::vector<ProgressionTemplate> factory, std::vector<ProgressionTemplate> user,
                    std::string error = {});
    void setWorkerStatus(std::uint64_t generation, double ms, bool busy, std::size_t factoryCount,
                         std::size_t userCount, bool recommendationResultReady = false);
    void drawRect(VSTGUI::CDrawContext*, const VSTGUI::CRect&) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&, const VSTGUI::CButtonState&) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint&,const VSTGUI::CButtonState&) override;
    void onKeyboardEvent(VSTGUI::KeyboardEvent&) override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent&) override;
    void resizeLayout(int width,int height);
    void setSimulatedContentScale(double scale);
    void setUserZoom(std::uint32_t percent);
    bool runHostInteractionSmoke();
    bool runStructureSmoke();
    bool runCompactSmoke(VSTGUI::CDrawContext*);
    bool runBrowseSmoke(VSTGUI::CDrawContext*,const std::function<void(const std::string&)>& capture = {});
    void setAuditionState(bool custom,double bpm,bool suffix,bool hostValid,double effectiveBpm);
    std::string recommendationUiDiagnostics();
    bool runZoomSmoke();
    bool runMidiWorkflowSmoke();
    bool runCandidateScrollSmoke();
    bool runColorHintSmoke(VSTGUI::CDrawContext*);
    bool runColorSortSmoke(VSTGUI::CDrawContext*,const Progression&,const RecommendationSet&);
    bool runEnrichmentColorSmoke(VSTGUI::CDrawContext*,const Progression&,const enrichment::EnrichmentResult&);
    bool runWhyV2Smoke(VSTGUI::CDrawContext*,bool enrichmentMode);
    bool runEnrichmentHintSmoke(VSTGUI::CDrawContext*,const std::function<bool(VSTGUI::CRect,ColorTone)>&);
    bool runColorVisibilitySmoke(VSTGUI::CDrawContext*,const std::function<bool(VSTGUI::CRect,VSTGUI::CRect,ColorTone,bool)>&);
private:
    enum class CompactAction { Select, Audition, Midi, Details, Density, Overview, PagePrevious, PageNext,
        Wish, WishPreset, WishAdvanced, WishInput, WishClear, WishTonic, WishMode, WishStrict, WishStyle, WishColor,
        WishOne, WishTen, WishAgain, WishSnapshot, WishCopy,
        OverviewClose, OverviewCopy, OverviewSelect, Intent, Enrichment, Key, Style,
        Advanced, Library, Import, FullView, InputChord, Play, Pause, Stop, Backend, TempoMode, Tempo, Scope };
    struct CompactHit {
        VSTGUI::CRect rect; CompactAction action; std::string id; std::size_t index{}; bool enabled{true};
        std::optional<VSTGUI::CRect> clip;
        bool contains(VSTGUI::CPoint p) const {
            if(clip&&!clip->pointInside(p))return false;
            if(!rect.pointInside(p))return false;
            if(action!=CompactAction::Select)return true;
            const auto dx=p.x-(rect.left+rect.right)/2,dy=p.y-(rect.top+rect.bottom)/2;
            return dx*dx+dy*dy<=rect.getWidth()*rect.getWidth()/4;
        }
    };
    std::vector<CompactHit> compactHits_;
    std::shared_ptr<wish::WishWorkflow> wishWorkspace_;
    std::shared_ptr<const wish::WishView> wishView_;
    wish::WishSettings wishSettings_;
    wish::WishInput wishInput_;
    bool wishMode_{},wishAdvanced_{},wishRequested_{};
    std::size_t wishPage_{};
    std::string wishSelected_,wishExpanded_;
    double wishScroll_{};
    void ensureWish();
    void resetWish();
    void drawPrimaryTabs(VSTGUI::CDrawContext*,const CompactLayoutResult&);
    void drawWish(VSTGUI::CDrawContext*,const CompactLayoutResult&);
    bool onWishAction(const CompactHit&,VSTGUI::CPoint&);
    bool compactInCards_{};
    std::string compactSelected_;
    std::size_t compactGroup_{};
    std::array<std::vector<BrowseEntry>,5> browseRows_;
    std::array<std::size_t,5> browsePages_{};
    std::array<double,5> browseScroll_{};
    std::array<std::string,5> browseSelected_{};
    bool browseDirty_{true},overviewOpen_{};
    double overviewScroll_{},overviewScrollMax_{};
    UiRect overviewRect_,overviewBody_;
    std::size_t browseCategory() const {return state_.productMode==session::ProductMode::Enrich?4:compactGroup_;}
    void refreshBrowse();
    const std::vector<BrowseEntry>& browseRows();
    void changeBrowseCategory(std::size_t);
    void focusBrowseEntry(std::size_t);
    void drawBrowseControls(VSTGUI::CDrawContext*,const CompactLayoutResult&);
    void drawRecommendationOverview(VSTGUI::CDrawContext*);
    std::string recommendationListText();
    std::string browseName(const BrowseEntry&) const;
    std::string browsePath(const BrowseEntry&) const;
    void drawRecommendationAlignment(VSTGUI::CDrawContext*,const ContinuationCandidate&,UiRect);
    bool detailedCards_{}, auditionCustom_{}, auditionSuffix_{}, auditionHostValid_{};
    double auditionBpm_{120}, auditionEffectiveBpm_{120}, compactScroll_{}, compactScrollMax_{};
    VSTGUI::CRect compactHover_{}, compactPressed_{};
    UiRect compactDetailRect_;
    std::unordered_map<std::string,double> compactRibbonScroll_;
    struct RibbonHit { VSTGUI::CRect rect; std::string id; double maximum{}; };
    std::vector<RibbonHit> compactRibbons_;
    void drawCompact(VSTGUI::CDrawContext*);
    void drawCompactEnrichments(VSTGUI::CDrawContext*,const CompactLayoutResult&);
    void drawCompactTransport(VSTGUI::CDrawContext*);
    bool onCompactMouseDown(VSTGUI::CPoint&);
    void compactButton(VSTGUI::CDrawContext*,UiRect,const std::string&,CompactAction,
        std::string id={},std::size_t index=0,bool selected=false,bool enabled=true,bool circular=false);
    void drawCompactRibbon(VSTGUI::CDrawContext*,const preview::SourceRouteResult&,UiRect);
    void drawRelativeColorCurve(VSTGUI::CDrawContext*,const preview::SourceRouteResult&,std::optional<KeySignature>,UiRect);
    static VSTGUI::CColor enrichmentProvenanceColor(enrichment::EnrichmentProvenanceKind,bool accent=false) noexcept;
    static double relativeColorTimelineX(double left,double width,double qn,double totalQN) noexcept;
    struct SeekHit { VSTGUI::CRect rect;std::string id;double originX{},width{},totalQN{},minimumQN{};VSTGUI::CRect cursorRect; };
    struct SeekGesture { SeekHit hit;std::uint64_t generation{};double targetQN{}; };
    std::vector<SeekHit> seekHits_;
    std::optional<SeekGesture> seekGesture_;
    std::string previewSeekHoverId_;
    bool previewSeekCursorHover_{};
    bool beginPreviewSeek(VSTGUI::CPoint);
    void movePreviewSeek(VSTGUI::CPoint);
    double previewDisplayQN(const std::string&) const;
    preview::PlaybackState auditionState_{preview::PlaybackState::Stopped};
    bool auditionHostBackend_{},auditionHostAvailable_{},auditionPending_{};
    Actions actions_;
    session::PluginSessionState state_;
    session::RecommendationVisibilityPolicy visibility_;
    std::string hostText_, rawText_, parseText_, matchStatus_, libraryError_, actionStatus_;
    HarmonicAnalysisResult analysis_;
    std::vector<MatchResult> matches_;
    RecommendationSet recommendations_;
    color::Preference colorPreference_{color::Preference::Off};
    bool colorRankDirty_{true};
    std::array<color::ColorRankResult,4> colorRanks_;
    bool enrichmentColorRankDirty_{true};
    std::array<color::ColorRankResult,3> enrichmentColorRanks_;
    void refreshEnrichmentColorRanking();
    const std::vector<std::size_t>& enrichmentOrder(std::size_t);
    void refreshColorRanking();
    session::RecommendationPresentation continuationPresentation();
    void setColorPreference(color::Preference);
    void setColorHints(bool value){showColorHints_=value;invalid();}
    bool showColorHints_{true}; // UI session preference; no persisted schema changes.
    std::vector<color::TimedChord> colorSource_;
    std::unordered_map<std::string,color::PathColor> colorPaths_;
    const color::PathColor& candidateColor(const ContinuationCandidate&);
    const color::PathColor& candidateColor(const enrichment::EnrichmentCandidate&);
    ColorHint candidateHint(const ContinuationCandidate& c) {return showColorHints_?colorHint(candidateColor(c),state_.locale):ColorHint{};}
    ColorHint candidateHint(const enrichment::EnrichmentCandidate& c) {return showColorHints_?enrichmentColorHint(candidateColor(c),state_.locale):ColorHint{};}
    void drawColorHint(VSTGUI::CDrawContext*,const ColorHint&,VSTGUI::CRect,VSTGUI::CRect);
    enrichment::EnrichmentResult enrichments_;
    std::vector<ContinuationCandidate> pinnedSnapshots_;
    std::vector<std::string> pinnedEnrichmentIds_;
    std::vector<ProgressionTemplate> factory_, user_;
    std::vector<TimelineBlock> timelineBlocks_;
    ChordLocation currentLocation_;
    std::optional<double> projectQN_;
    std::optional<VSTGUI::CCoord> playheadX_;
    std::string previewCandidateId_;
    double previewQN_{}, previewTotalQN_{};
    bool playing_{}, workerBusy_{}, recommendationResultReady_{}, sessionDirty_{}, snapshotMode_{};
    std::uint64_t generation_{};
    double computationMs_{};
    std::size_t factoryCount_{}, userCount_{};
    std::optional<std::size_t> selectedChord_;
    std::optional<std::pair<std::size_t,std::size_t>> selectedCandidate_;
    std::optional<std::pair<std::size_t,std::size_t>> selectedEnrichment_;
    std::optional<std::pair<bool,std::size_t>> selectedLibrary_;
    std::size_t libraryPage_{};
    std::optional<Style> libraryStyle_;
    std::optional<PhraseIntent> libraryIntent_;
    std::optional<Mode> libraryMode_;
    int librarySource_{-1}; // -1 all, 0 factory, 1 user
    std::optional<ComplexityLevel> libraryComplexity_;
    std::string libraryTechnique_;
    bool libraryFavoriteOnly_{};
    std::string librarySearch_;
    std::deque<std::string> recentReports_, recentHostSnapshots_;
    UILayoutResult layout_;
    std::array<double,4> continuationScroll_{};
    std::array<double,3> enrichmentScroll_{};
    double contentScale_{1}, contentScroll_{}, timelineScroll_{}, timelineContentWidth_{}, inspectorScroll_{}, inspectorScrollMax_{};
    std::uint64_t paintGeneration_{};
    enum class Form { None, Save, Rename, Search, Melody, Tempo } form_{Form::None};
    VSTGUI::CTextEdit* nameEdit_{};
    VSTGUI::CTextEdit* tagsEdit_{};
    VSTGUI::CTextEdit* noteEdit_{};
    session::SaveMetadata formMetadata_;
    MelodyConstraint formMelody_;
    VSTGUI::CPoint midiMouseDown_;
    std::function<void()> midiClick_;
    std::optional<midi::MidiClipPayload> midiPending_;
    bool armMidi(const ContinuationCandidate&,VSTGUI::CPoint);
    bool armMidi(const enrichment::EnrichmentCandidate&,VSTGUI::CPoint);
    double userZoom() const noexcept { return EffectiveScale(contentScale_,state_.uiZoomPercent/100.0).user; }
    VSTGUI::CRect editRect(VSTGUI::CRect) const;
    void notifyState();
    std::string t(std::string_view key) const { return std::string(localization::text(state_.locale,key)); }
    std::string tIntent(PhraseIntent intent) const {
        switch(intent) {
            case PhraseIntent::Resolve:return t("group.resolve");
            case PhraseIntent::Develop:return t("group.develop");
            case PhraseIntent::Loop:return t("group.loop");
            case PhraseIntent::Color:return t("group.color");
            default:return t("label.auto");
        }
    }
    void rebuildTimeline();
    VSTGUI::CRect chordTileRect(std::size_t) const;
    std::optional<VSTGUI::CCoord> projectQNToX(double) const;
    void drawTimeline(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawTop(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawPhrase(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawRecommendations(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawEnrichments(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawCompare(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawMiniTimeline(VSTGUI::CDrawContext*, const ContinuationCandidate&, VSTGUI::CRect);
    void drawResponsiveLibrary(VSTGUI::CDrawContext*);
    void drawResponsiveDiagnostics(VSTGUI::CDrawContext*);
    void drawResponsiveInspector(VSTGUI::CDrawContext*);
    void drawResponsiveForm(VSTGUI::CDrawContext*);
    VSTGUI::CMouseEventResult onMouseDownResponsive(VSTGUI::CPoint&);
    OverlayKind activeOverlayKind() const noexcept;
    void dismissTransientOverlay();
    void focusTransientOverlay();
    void refreshLayout();
    const ContinuationCandidate* selectedCandidate() const;
    const ContinuationCandidate* findCandidate(const std::string&) const;
    const enrichment::EnrichmentCandidate* selectedEnrichment() const;
    const enrichment::EnrichmentCandidate* findEnrichment(const std::string&) const;
    void beginForm(Form, std::string initial);
    void endForm();
    void submitForm();
    int popup(const std::vector<std::string>&, VSTGUI::CPoint);
    std::vector<std::pair<bool,std::size_t>> filteredLibrary() const;
};
} // namespace harmony::ui
