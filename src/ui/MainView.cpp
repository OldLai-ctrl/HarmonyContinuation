#include "MainView.h"
#include "WeightBarGeometry.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/clinestyle.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace harmony::ui {
using namespace VSTGUI;
namespace {
std::size_t utf8Width(unsigned char lead) {
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

void drawTextRows(CDrawContext* dc, const std::string& text, const CRect& area,
                  int maxRows, CCoord lineHeight, CHoriTxtAlign align) {
    const auto maxCharacters = std::max<std::size_t>(12, static_cast<std::size_t>(area.getWidth() / 14.0));
    std::size_t position{};
    CCoord y = area.top;
    for (int row = 0; row < maxRows && position < text.size(); ++row) {
        const auto endLine = text.find('\n', position);
        const auto logicalEnd = endLine == std::string::npos ? text.size() : endLine;
        auto cursor = position;
        auto characterCount = std::size_t{};
        auto lastSpace = std::string::npos;
        while (cursor < logicalEnd && characterCount < maxCharacters) {
            if (text[cursor] == ' ') lastSpace = cursor;
            cursor = std::min(logicalEnd, cursor + utf8Width(static_cast<unsigned char>(text[cursor])));
            ++characterCount;
        }
        if (cursor < logicalEnd && lastSpace != std::string::npos && lastSpace > position)
            cursor = lastSpace;

        if (cursor == position) {
            if (endLine != std::string::npos) ++position;
            y += lineHeight;
            continue;
        }
        dc->drawString(text.substr(position, cursor - position).c_str(),
                       CRect(area.left, y, area.right, y + lineHeight), align);
        y += lineHeight;
        position = cursor;
        while (position < logicalEnd && text[position] == ' ') ++position;
        if (position >= logicalEnd && endLine != std::string::npos) ++position;
    }
    if (position < text.size() && maxRows > 0)
        dc->drawString("……更多内容请展开诊断面板", CRect(area.left, y - lineHeight, area.right, y), align);
}

std::string capped(std::string value, std::size_t limit) {
    if (value.size() <= limit) return value;
    value.resize(limit);
    value += "\n……报告已按上限截断……";
    return value;
}

bool overlaps(const CRect& a, const CRect& b) {
    return a.right > b.left && a.left < b.right && a.bottom > b.top && a.top < b.bottom;
}
} // namespace

MainView::MainView(const CRect& rect, DropCallback drop, RefreshCallback refresh, ClipboardCallback clipboard)
    : CView(rect), drop_(std::move(drop)), refresh_(std::move(refresh)), clipboard_(std::move(clipboard)) {}

SharedPointer<IDropTarget> MainView::getDropTarget() { return this; }
DragOperation MainView::onDragEnter(DragEventData event) { return event.drag ? DragOperation::Copy : DragOperation::None; }
DragOperation MainView::onDragMove(DragEventData event) { return event.drag ? DragOperation::Copy : DragOperation::None; }
void MainView::onDragLeave(DragEventData) {}
bool MainView::onDrop(DragEventData event) {
    try { if (!event.drag) return false; drop_(event.drag); return true; }
    catch (...) { return false; }
}

void MainView::setHostText(std::string text, std::string refreshSummary) {
    hostText_ = std::move(text);
    if (!refreshSummary.empty()) {
        recentHostSnapshots_.push_front(std::move(refreshSummary));
        while (recentHostSnapshots_.size() > 8) recentHostSnapshots_.pop_back();
    }
    invalid();
}

void MainView::setProgressionSession(const harmony::ImportedProgressionSession& session) {
    if (session_.revision == session.revision && session_.coordinateMode == session.coordinateMode) return;
    session_ = session;
    analysis_ = {};
    showSkeleton_ = false;
    rebuildTimeline();
    currentLocation_ = harmony::locateCurrentChord(session_, projectQN_);
    playheadX_ = projectQN_ ? projectQNToX(*projectQN_) : std::nullopt;
    invalid();
}

void MainView::setAnalysis(const harmony::HarmonicAnalysisResult& analysis) {
    analysis_ = analysis;
    invalid();
}

void MainView::setMatches(const std::vector<harmony::MatchResult>& matches, std::string status) {
    matches_ = matches;
    matchStatus_ = std::move(status);
    selectedMatch_ = 0;
    invalid();
}

void MainView::setPlaybackPosition(std::optional<double> projectQN, bool playing) {
    const auto oldLocation = currentLocation_;
    const auto oldPlayheadX = playheadX_;
    projectQN_ = projectQN && std::isfinite(*projectQN) ? projectQN : std::nullopt;
    playing_ = playing;
    currentLocation_ = harmony::locateCurrentChord(session_, projectQN_);
    playheadX_ = projectQN_ ? projectQNToX(*projectQN_) : std::nullopt;

    if (oldLocation.activeIndex != currentLocation_.activeIndex) {
        if (oldLocation.activeIndex) invalidRect(chordTileRect(*oldLocation.activeIndex));
        if (currentLocation_.activeIndex) invalidRect(chordTileRect(*currentLocation_.activeIndex));
    }

    auto invalidatePlayhead = [this](std::optional<CCoord> x, std::optional<std::size_t> chordIndex) {
        if (!x) return;
        // The playhead extends above and below a chord tile. Clear its full
        // former/current height, and repaint the tile to preserve its labels.
        invalidRect(CRect(*x - 3.0, 320, *x + 3.0, 390));
        if (chordIndex) invalidRect(chordTileRect(*chordIndex));
    };
    if (oldPlayheadX && playheadX_ && std::abs(*oldPlayheadX - *playheadX_) < 0.75)
        return;
    invalidatePlayhead(oldPlayheadX, oldLocation.activeIndex);
    invalidatePlayhead(playheadX_, currentLocation_.activeIndex);
}

void MainView::setDropReport(std::string raw, std::string outcome) {
    rawText_ = capped(std::move(raw), 12000);
    parseText_ = capped(std::move(outcome), 8000);
    std::string history = "最近一次拖放\n" + parseText_ + "\n" + rawText_;
    recentReports_.push_front(capped(std::move(history), 16000));
    while (recentReports_.size() > 10) recentReports_.pop_back();
    invalid();
}

void MainView::rebuildTimeline() {
    timelineBlocks_ = layoutTimeline(session_.events, getViewSize().getWidth() - 48.0);
}

CRect MainView::chordTileRect(std::size_t eventIndex) const {
    const CRect lane(18, 320, getViewSize().right - 18, 390);
    const auto block = std::find_if(timelineBlocks_.begin(), timelineBlocks_.end(),
        [eventIndex](const auto& item) { return item.eventIndex == eventIndex; });
    if (block == timelineBlocks_.end()) return {};
    const auto left = lane.left + 6 + block->x;
    const auto width = std::max(1.0, block->width - 3.0);
    return CRect(left, lane.top + 7, left + width, lane.bottom - 7);
}

std::optional<CCoord> MainView::projectQNToX(double qn) const {
    if (session_.events.empty() || !std::isfinite(qn) || qn < session_.events.front().startQN)
        return std::nullopt;
    const auto origin = session_.events.front().startQN;
    const auto span = session_.events.back().startQN - origin + 2.0;
    if (!std::isfinite(span) || span <= 0.0) return std::nullopt;
    const auto left = 24.0;
    const auto width = getViewSize().getWidth() - 48.0;
    const auto position = std::clamp((qn - origin) / span, 0.0, 1.0);
    return static_cast<CCoord>(left + position * width);
}

void MainView::drawTimeline(CDrawContext* dc, const CRect& updateRect) {
    const CRect bounds = getViewSize();
    const CRect lane(18, 320, bounds.right - 18, 390);
    // This method is also called directly for partial transport repaints.
    dc->setFont(kNormalFont);
    dc->setFillColor(CColor(20, 31, 47, 255));
    dc->drawRect(lane, kDrawFilled);

    if (session_.empty()) {
        dc->setFontColor(CColor(231, 239, 250, 255));
        dc->drawString("尚未导入和弦进行。请从 Cubase 和弦轨拖入事件。",
                       CRect(30, 343, bounds.right - 30, 369), kLeftText);
        return;
    }

    const CColor colors[] = {CColor(50, 116, 180, 255), CColor(40, 139, 129, 255),
                             CColor(172, 111, 53, 255), CColor(108, 91, 178, 255)};
    for (const auto& block : timelineBlocks_) {
        if (showSkeleton_ && !analysis_.full.empty() &&
            std::find(analysis_.skeletonIndices.begin(), analysis_.skeletonIndices.end(), block.eventIndex) ==
                analysis_.skeletonIndices.end()) continue;
        const auto tile = chordTileRect(block.eventIndex);
        if (!overlaps(tile, updateRect)) continue;
        const bool active = currentLocation_.activeIndex == block.eventIndex;
        dc->setFillColor(colors[block.eventIndex % 4]);
        dc->drawRect(tile, kDrawFilled);
        dc->setFrameColor(active ? CColor(255, 225, 112, 255) : CColor(135, 172, 215, 255));
        dc->setLineWidth(active ? 2.4 : 1.0);
        if (block.openRightEdge) {
            const CCoord dash[] = {4., 3.};
            dc->setLineStyle(CLineStyle(CLineStyle::kLineCapButt, CLineStyle::kLineJoinMiter, 0, 2, dash));
        } else {
            dc->setLineStyle(CLineStyle{});
        }
        dc->drawRect(tile);
        dc->setLineWidth(1.0);

        const auto& event = session_.events[block.eventIndex];
        dc->setFontColor(CColor(255, 255, 255, 255));
        dc->drawString(event.name.c_str(), CRect(tile.left + 2, tile.top + 2, tile.right - 2, tile.top + 20), kCenterText);
        if (block.eventIndex < analysis_.full.size()) {
            const auto& harmonic = analysis_.full[block.eventIndex];
            const auto label = harmony::formatDegree(harmonic) + "  " + harmony::formatFunction(harmonic.function);
            dc->drawString(label.c_str(), CRect(tile.left + 1, tile.top + 22, tile.right - 1, tile.top + 40), kCenterText);
            std::string badge;
            if (harmony::hasRole(harmonic.roles, harmony::Role::SecondaryDominant)) badge = "次属";
            else if (harmony::hasRole(harmonic.roles, harmony::Role::SecondaryLeadingTone)) badge = "导音";
            else if (harmony::hasRole(harmonic.roles, harmony::Role::Borrowed)) badge = "借用";
            else if (harmony::hasRole(harmonic.roles, harmony::Role::Approach)) badge = "趋近";
            if (!badge.empty())
                dc->drawString(badge.c_str(), CRect(tile.left + 1, tile.top + 39, tile.right - 1, tile.bottom - 8), kCenterText);
            const auto bar = weightBarRect(tile.left, tile.right, tile.bottom,
                                            harmonic.structuralWeight, dc->getScaleFactor());
            if (bar.right > bar.left) {
                dc->setFillColor(CColor(255, 217, 115, 255));
                dc->drawRect(CRect(bar.left, bar.top, bar.right, bar.bottom), kDrawFilled);
            }
        } else {
            std::string duration = "延续中";
            if (event.durationQN) {
                std::ostringstream value;
                value << std::fixed << std::setprecision(2) << *event.durationQN << " 拍";
                duration = value.str();
            }
            dc->drawString(duration.c_str(), CRect(tile.left + 2, tile.top + 23, tile.right - 2, tile.bottom - 2), kCenterText);
        }
    }
    dc->setLineStyle(CLineStyle{});

    if (playheadX_) {
        dc->setFillColor(CColor(255, 237, 138, 255));
        dc->drawRect(CRect(*playheadX_ - 1.5, lane.top, *playheadX_ + 1.5, lane.bottom), kDrawFilled);
    }
}

void MainView::drawMatches(CDrawContext* dc, const CRect& bounds) {
    dc->setFont(kNormalFont);
    dc->setFontColor(CColor(151, 184, 223, 255));
    dc->drawString(matchStatus_.c_str(), CRect(24, 438, bounds.right - 24, 460), kLeftText);
    if (matches_.empty()) return;
    const auto count = std::min<std::size_t>(5, matches_.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto top = 464.0 + static_cast<double>(i) * 43.0;
        if (top + 39 > bounds.bottom - 18) break;
        const auto& match = matches_[i];
        const CRect row(24, top, bounds.right - 24, top + 39);
        dc->setFillColor(i == selectedMatch_ ? CColor(37, 63, 89, 255) : CColor(27, 42, 60, 255));
        dc->drawRect(row, kDrawFilled);
        dc->setFillColor(CColor(255, 217, 115, 255));
        dc->drawRect(CRect(row.left, row.bottom - 3,
                           row.left + (row.getWidth() * std::clamp(match.similarity, 0.f, 1.f)), row.bottom), kDrawFilled);
        dc->setFontColor(CColor(239, 245, 255, 255));
        std::ostringstream line;
        line << static_cast<int>(std::round(match.similarity * 100.f)) << " Match   ";
        for (std::size_t k = 0; k < match.templateLabels.size(); ++k) {
            if (k) line << " — ";
            line << match.templateLabels[k];
        }
        line << "   [" << harmony::formatKey(match.key) << "]";
        dc->drawString(line.str().c_str(), CRect(row.left + 7, row.top + 1, row.right - 7, row.top + 19), kLeftText);
        std::ostringstream sub;
        sub << match.templateName << "   结构 " << static_cast<int>(std::round(match.subScores.skeletonHarmony * 100.f))
            << "  完整 " << static_cast<int>(std::round(match.subScores.fullHarmony * 100.f))
            << "  节奏 " << static_cast<int>(std::round(match.subScores.rhythmSimilarity * 100.f))
            << "  对应 " << match.templateMatchStart << "-" << match.templateMatchEnd
            << "  后续位置 " << match.continuationStartIndex;
        dc->setFontColor(CColor(163, 193, 220, 255));
        dc->drawString(sub.str().c_str(), CRect(row.left + 7, row.top + 18, row.right - 7, row.bottom - 3), kLeftText);
    }
    const auto traceTop = 464.0 + static_cast<double>(count) * 43.0 + 10.0;
    if (traceTop >= bounds.bottom - 30) return;
    const auto& selected = matches_[std::min(selectedMatch_, matches_.size() - 1)];
    dc->setFontColor(CColor(255, 225, 112, 255));
    dc->drawString("对应关系 / 点击上方候选查看", CRect(24, traceTop, bounds.right - 24, traceTop + 22), kLeftText);
    std::ostringstream trace;
    for (const auto& step : selected.alignmentTrace) {
        trace << harmony::alignmentOpName(step.operation) << "  ";
        if (step.queryIndex && *step.queryIndex < session_.events.size()) trace << session_.events[*step.queryIndex].name;
        else trace << "GAP";
        trace << "  ↔  ";
        if (step.templateIndex && *step.templateIndex < selected.templateLabels.size())
            trace << selected.templateLabels[*step.templateIndex];
        else trace << "GAP";
        trace << "  [" << harmony::reasonNames(step.reasons) << "]\n";
    }
    dc->setFontColor(CColor(210, 222, 239, 255));
    const auto rows = std::max(1, static_cast<int>((bounds.bottom - traceTop - 35.0) / 16.0));
    drawTextRows(dc, trace.str(), CRect(24, traceTop + 23, bounds.right - 24, bounds.bottom - 18), rows, 16, kLeftText);
}

void MainView::drawRect(CDrawContext* dc, const CRect& updateRect) {
    // Transport changes invalidate only the timeline lane. Redraw its cached
    // layout under the dirty region, without formatting diagnostics or painting
    // the rest of the editor at the transport polling rate.
    if (updateRect.top >= 320 && updateRect.bottom <= 390) {
        drawTimeline(dc, updateRect);
        return;
    }

    const CRect bounds = getViewSize();
    dc->setFillColor(CColor(16, 24, 39, 255));
    dc->drawRect(bounds, kDrawFilled);
    dc->setFont(kNormalFont);
    dc->setFontColor(CColor(239, 245, 255, 255));
    dc->drawString("和声续写  /  Cubase 宿主同步", CRect(20, 12, bounds.right - 20, 38), kLeftText);
    dc->setFontColor(CColor(152, 178, 212, 255));
    std::string keyLine = "拖入和弦进行，自动跟随 Cubase 工程位置";
    if (analysis_.selectedKey && !analysis_.keyCandidates.empty()) {
        keyLine = "调性推测：" + harmony::formatKey(analysis_.selectedKey->key);
        if (analysis_.keyCandidates.front().confidence < 0.35f) keyLine += "（有歧义）";
        keyLine += "  候选权重 " +
                   std::to_string(static_cast<int>(std::round(analysis_.keyCandidates.front().confidence * 100))) + "%";
        for (std::size_t i = 1; i < std::min<std::size_t>(3, analysis_.keyCandidates.size()); ++i) {
            keyLine += "   /   " + harmony::formatKey(analysis_.keyCandidates[i].key) + " " +
                       std::to_string(static_cast<int>(std::round(analysis_.keyCandidates[i].confidence * 100))) + "%";
        }
    }
    dc->drawString(keyLine.c_str(), CRect(20, 39, bounds.right - 20, 59), kLeftText);

    dc->setFillColor(CColor(25, 39, 59, 255));
    dc->drawRect(CRect(16, 70, bounds.right - 16, 140), kDrawFilled);
    dc->setFrameColor(CColor(55, 155, 231, 255));
    dc->setLineWidth(1.2);
    dc->drawRect(CRect(16, 70, bounds.right - 16, 140));
    dc->setFontColor(CColor(116, 202, 255, 255));
    dc->drawString("将 Cubase 和弦事件拖放至此", CRect(30, 88, bounds.right - 30, 115), kCenterText);
    dc->setFontColor(CColor(168, 188, 216, 255));
    dc->drawString("支持 VST-XML 1.3/1.4；解析和定位均不在音频线程执行。",
                   CRect(28, 117, bounds.right - 28, 135), kCenterText);

    dc->setFillColor(CColor(20, 31, 47, 255));
    dc->drawRect(CRect(16, 150, bounds.right - 16, 282), kDrawFilled);
    dc->setFrameColor(CColor(56, 75, 101, 255));
    dc->drawRect(CRect(16, 150, bounds.right - 16, 282));
    dc->setFontColor(CColor(151, 184, 223, 255));
    dc->drawString("宿主状态 / 播放位置自动同步 | 点击左侧刷新诊断 | 右侧检查剪贴板",
                   CRect(24, 155, bounds.right - 240, 176), kLeftText);
    dc->setFontColor(CColor(221, 230, 243, 255));
    drawTextRows(dc, hostText_, CRect(24, 180, bounds.right - 24, 276), 6, 15, kLeftText);
    dc->setFontColor(CColor(116, 202, 255, 255));
    dc->drawString("剪贴板诊断", CRect(bounds.right - 228, 155, bounds.right - 24, 176), kRightText);

    dc->setFontColor(CColor(151, 184, 223, 255));
    dc->drawString("和弦进行时间轴  /  工程四分音符（QN）", CRect(20, 292, bounds.right - 20, 314), kLeftText);
    drawTimeline(dc, CRect(0, 0, bounds.right, bounds.bottom));

    dc->setFontColor(showSkeleton_ ? CColor(151, 184, 223, 255) : CColor(255, 225, 112, 255));
    dc->drawString("[ FULL ]", CRect(20, 402, 114, 425), kLeftText);
    dc->setFontColor(showSkeleton_ ? CColor(255, 225, 112, 255) : CColor(151, 184, 223, 255));
    dc->drawString("[ SKELETON ]", CRect(120, 402, 260, 425), kLeftText);
    dc->setFontColor(showMatches_ ? CColor(255, 225, 112, 255) : CColor(151, 184, 223, 255));
    dc->drawString("[ MATCH ]", CRect(280, 402, 382, 425), kLeftText);
    dc->setFontColor(showMatches_ ? CColor(151, 184, 223, 255) : CColor(255, 225, 112, 255));
    dc->drawString(debugExpanded_ ? "[ 诊断 / 折叠 ]" : "[ 诊断 / 展开 ]",
                   CRect(390, 402, bounds.right - 20, 425), kLeftText);
    dc->setFillColor(CColor(20, 31, 47, 255));
    dc->drawRect(CRect(16, 430, bounds.right - 16, bounds.bottom - 12), kDrawFilled);
    if (showMatches_) { drawMatches(dc, bounds); return; }
    dc->setFontColor(CColor(210, 222, 239, 255));
    std::string shown;
    if (debugExpanded_) {
        shown = "宿主 / 处理上下文\n" + hostText_;
        if (!recentHostSnapshots_.empty()) {
            shown += "\n最近手动刷新（最新在前，最多 8 条）\n";
            for (const auto& snapshot : recentHostSnapshots_) shown += snapshot + "\n";
        }
        shown += "\n" + parseText_ + "\n" + rawText_;
        if (recentReports_.size() > 1) {
            shown += "\n最近检查记录（最新在前，最多 10 条）\n";
            for (std::size_t i = 1; i < recentReports_.size(); ++i) shown += recentReports_[i] + "\n";
        }
    } else {
        shown = parseText_ + "\n" + rawText_.substr(0, 180);
        if (rawText_.size() > 180) shown += "……展开诊断面板查看详情";
    }
    const auto rows = std::max(1, static_cast<int>((bounds.bottom - 448) / 16));
    drawTextRows(dc, shown, CRect(24, 438, bounds.right - 24, bounds.bottom - 18), rows, 16, kLeftText);
}

CMouseEventResult MainView::onMouseDown(CPoint& where, const CButtonState&) {
    if (where.y >= 145 && where.y < 284 && where.x > getViewSize().right - 240 && clipboard_) {
        try { clipboard_(); } catch (...) {}
    } else if (where.y >= 145 && where.y < 284 && refresh_) {
        try { refresh_(); } catch (...) {}
    } else if (where.y >= 399 && where.y < 429 && where.x < 265 && !analysis_.full.empty()) {
        const bool skeleton = where.x >= 118;
        if (showSkeleton_ != skeleton) { showSkeleton_ = skeleton; invalid(); }
    } else if (where.y >= 399 && where.y < 429 && where.x >= 275 && where.x < 385) {
        showMatches_ = true;
        invalid();
    } else if (showMatches_ && where.y >= 464 && where.y < 464 + 43 * 5) {
        const auto index = static_cast<std::size_t>((where.y - 464) / 43);
        if (index < matches_.size()) { selectedMatch_ = index; invalid(); }
    } else if ((where.y >= 399 && where.y < 429 && where.x >= 385) ||
               (!showMatches_ && where.y >= 430)) {
        showMatches_ = false;
        debugExpanded_ = !debugExpanded_;
        invalid();
    }
    return kMouseEventHandled;
}
} // namespace harmony::ui
