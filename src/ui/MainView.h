#pragma once
#include "core/CurrentChordLocator.h"
#include "core/ContinuationEngine.h"
#include "session/PluginSessionState.h"
#include "session/ProductServices.h"
#include "ui/ProgressionTimeline.h"
#include "ui/UILayout.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/dragging.h"
#include <deque>
#include <functional>
#include <optional>
#include <string>

namespace VSTGUI { class CTextEdit; }
namespace harmony::ui {
class MainView final : public VSTGUI::CView, public VSTGUI::IDropTarget {
public:
    struct Actions {
        std::function<void(VSTGUI::IDataPackage*)> drop;
        std::function<void()> refresh;
        std::function<void()> clipboard;
        std::function<void(const session::PluginSessionState&)> stateChanged;
        std::function<std::string(const ContinuationCandidate&, const session::SaveMetadata&)> save;
        std::function<std::string(const ProgressionTemplate&)> updateUser;
        std::function<std::string(const std::string&)> deleteUser;
        std::function<void(const ContinuationCandidate&)> audition;
        std::function<std::string(const ContinuationCandidate&)> exportMidi;
        std::function<std::string(const ContinuationCandidate&)> saveSnapshot;
        std::function<std::string(const ProgressionTemplate&)> exportLibraryMidi;
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
    void setPreviewPosition(std::string candidateId,double positionQN,double totalQN);
    void setSnapshotMode(bool enabled);
    void setActionStatus(std::string status) { actionStatus_=std::move(status); invalid(); }
    void setPlaybackPosition(std::optional<double> projectQN, bool playing);
    void setDropReport(std::string, std::string outcome, bool inputAttempt = true);
    void setLibrary(std::vector<ProgressionTemplate> factory, std::vector<ProgressionTemplate> user,
                    std::string error = {});
    void setWorkerStatus(std::uint64_t generation, double ms, bool busy, std::size_t factoryCount,
                         std::size_t userCount);
    void drawRect(VSTGUI::CDrawContext*, const VSTGUI::CRect&) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&, const VSTGUI::CButtonState&) override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent&) override;
    void resizeLayout(int width,int height);
    void setSimulatedContentScale(double scale);
private:
    Actions actions_;
    session::PluginSessionState state_;
    session::RecommendationVisibilityPolicy visibility_;
    std::string hostText_, rawText_, parseText_, matchStatus_, libraryError_, actionStatus_;
    HarmonicAnalysisResult analysis_;
    std::vector<MatchResult> matches_;
    RecommendationSet recommendations_;
    std::vector<ContinuationCandidate> pinnedSnapshots_;
    std::vector<ProgressionTemplate> factory_, user_;
    std::vector<TimelineBlock> timelineBlocks_;
    ChordLocation currentLocation_;
    std::optional<double> projectQN_;
    std::optional<VSTGUI::CCoord> playheadX_;
    std::string previewCandidateId_;
    double previewQN_{}, previewTotalQN_{};
    bool playing_{}, workerBusy_{}, sessionDirty_{};
    std::uint64_t generation_{};
    double computationMs_{};
    std::size_t factoryCount_{}, userCount_{};
    std::optional<std::size_t> selectedChord_;
    std::optional<std::pair<std::size_t,std::size_t>> selectedCandidate_;
    std::optional<std::pair<bool,std::size_t>> selectedLibrary_;
    std::size_t libraryPage_{};
    std::optional<Style> libraryStyle_;
    std::optional<PhraseIntent> libraryIntent_;
    std::optional<Mode> libraryMode_;
    std::string librarySearch_;
    std::deque<std::string> recentReports_, recentHostSnapshots_;
    UILayoutResult layout_;
    double contentScale_{1}, contentScroll_{}, timelineScroll_{}, timelineContentWidth_{}, inspectorScroll_{}, inspectorScrollMax_{};
    std::uint64_t paintGeneration_{};
    enum class Form { None, Save, Rename, Search } form_{Form::None};
    VSTGUI::CTextEdit* nameEdit_{};
    VSTGUI::CTextEdit* tagsEdit_{};
    session::SaveMetadata formMetadata_;
    void notifyState();
    void rebuildTimeline();
    VSTGUI::CRect chordTileRect(std::size_t) const;
    std::optional<VSTGUI::CCoord> projectQNToX(double) const;
    void drawTimeline(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawTop(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawPhrase(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawRecommendations(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawCompare(VSTGUI::CDrawContext*, const VSTGUI::CRect&);
    void drawMiniTimeline(VSTGUI::CDrawContext*, const ContinuationCandidate&, VSTGUI::CRect);
    void drawResponsiveLibrary(VSTGUI::CDrawContext*);
    void drawResponsiveDiagnostics(VSTGUI::CDrawContext*);
    void drawResponsiveInspector(VSTGUI::CDrawContext*);
    void drawResponsiveForm(VSTGUI::CDrawContext*);
    VSTGUI::CMouseEventResult onMouseDownResponsive(VSTGUI::CPoint&);
    void refreshLayout();
    const ContinuationCandidate* selectedCandidate() const;
    const ContinuationCandidate* findCandidate(const std::string&) const;
    void beginForm(Form, std::string initial);
    void endForm();
    void submitForm();
    int popup(const std::vector<std::string>&, VSTGUI::CPoint);
    std::vector<std::pair<bool,std::size_t>> filteredLibrary() const;
};
} // namespace harmony::ui
