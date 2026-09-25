#pragma once
#include "core/CurrentChordLocator.h"
#include "core/HarmonyAnalysis.h"
#include "core/ProgressionMatcher.h"
#include "core/ContinuationEngine.h"
#include "ui/ProgressionTimeline.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/dragging.h"
#include <deque>
#include <functional>
#include <string>
namespace harmony::ui {
class MainView final : public VSTGUI::CView, public VSTGUI::IDropTarget {
public:
    using DropCallback = std::function<void(VSTGUI::IDataPackage*)>;
    using RefreshCallback = std::function<void()>;
    using ClipboardCallback = std::function<void()>;
    using PreferenceCallback = std::function<void(std::optional<harmony::Style>, std::optional<harmony::PhraseIntent>)>;
    MainView(const VSTGUI::CRect&, DropCallback, RefreshCallback, ClipboardCallback, PreferenceCallback);
    VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget() override;
    VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData) override;
    VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData) override;
    void onDragLeave(VSTGUI::DragEventData) override;
    bool onDrop(VSTGUI::DragEventData) override;
    void setHostText(std::string, std::string refreshSummary = {});
    void setProgressionSession(const harmony::ImportedProgressionSession&);
    void setAnalysis(const harmony::HarmonicAnalysisResult&);
    void setMatches(const std::vector<harmony::MatchResult>&, std::string status);
    void setRecommendations(const harmony::RecommendationSet&);
    void setPlaybackPosition(std::optional<double> projectQN, bool playing);
    void setDropReport(std::string, std::string outcome);
    void drawRect(VSTGUI::CDrawContext*, const VSTGUI::CRect&) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint&, const VSTGUI::CButtonState&) override;
private:
    DropCallback drop_; RefreshCallback refresh_; ClipboardCallback clipboard_; PreferenceCallback preference_;
    std::string hostText_{"宿主：Cubase\n格式：VST3 | 等待播放上下文"};
    std::string rawText_{"尚未收到拖放数据。请从 Cubase 和弦轨拖入事件。"};
    std::string parseText_{"解析结果：等待拖入"};
    harmony::ImportedProgressionSession session_;
    harmony::HarmonicAnalysisResult analysis_;
    std::vector<harmony::MatchResult> matches_;
    harmony::RecommendationSet recommendations_;
    std::string matchStatus_;
    bool showMatches_{};
    bool showRecommendations_{true};
    int selectedStyle_{};
    int selectedIntent_{};
    std::size_t selectedMatch_{};
    bool showSkeleton_{};
    std::vector<TimelineBlock> timelineBlocks_;
    harmony::ChordLocation currentLocation_;
    std::optional<double> projectQN_;
    std::optional<VSTGUI::CCoord> playheadX_;
    bool playing_{};
    bool debugExpanded_{};
    std::deque<std::string> recentReports_;
    std::deque<std::string> recentHostSnapshots_;

    void rebuildTimeline();
    VSTGUI::CRect chordTileRect(std::size_t eventIndex) const;
    void drawTimeline(VSTGUI::CDrawContext*, const VSTGUI::CRect& updateRect);
    void drawMatches(VSTGUI::CDrawContext*, const VSTGUI::CRect& bounds);
    void drawRecommendations(VSTGUI::CDrawContext*, const VSTGUI::CRect& bounds);
    std::optional<VSTGUI::CCoord> projectQNToX(double qn) const;
};
}
