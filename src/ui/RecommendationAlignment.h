#pragma once

#include "../core/ContinuationEngine.h"
#include "../core/ImportedProgression.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace harmony::ui {

enum class AlignmentCellKind { Aligned, InputInsertion, TemplateSkip, Continuation };

struct RecommendationAlignmentCell {
    AlignmentCellKind kind;
    std::optional<std::size_t> queryIndex;
    std::optional<std::size_t> templateIndex;
    bool continuationBoundary{};
};

// Reconstructs the query-to-template trace plus the candidate's actual suffix.
// Snapshot routes omit the original query, so their persisted indices cannot be
// safely presented as aligned rows.
inline std::vector<RecommendationAlignmentCell> recommendationAlignment(
    const ImportedProgressionSession& imported,
    const ContinuationCandidate& candidate,
    const MatchResult& match,
    bool snapshotMode = false) {
    using Cells = std::vector<RecommendationAlignmentCell>;
    const auto empty = [] { return Cells{}; };
    if (snapshotMode || imported.events.empty() || match.alignmentTrace.empty() ||
        match.templateLabels.empty() || candidate.continuation.empty()) return empty();

    const auto templateCount = match.templateLabels.size();
    if (candidate.primaryTemplate != match.templateId ||
        candidate.key.tonic != match.key.tonic || candidate.key.mode != match.key.mode ||
        candidate.matchStart != match.templateMatchStart ||
        candidate.matchEnd != match.templateMatchEnd ||
        candidate.continuationStart != match.continuationStartIndex ||
        match.templateMatchStart > match.templateMatchEnd ||
        match.templateMatchEnd >= templateCount ||
        match.continuationStartIndex != match.templateMatchEnd + 1 ||
        match.continuationStartIndex > templateCount ||
        match.hasContinuation != (match.continuationStartIndex < templateCount) ||
        match.continuationLength != templateCount - match.continuationStartIndex) return empty();

    std::vector<bool> seenQuery(imported.events.size(), false);
    std::vector<bool> seenTemplate(templateCount, false);
    std::optional<std::size_t> previousQuery, previousTemplate;
    bool hasAlignedPair = false;
    Cells cells;
    cells.reserve(match.alignmentTrace.size() + candidate.continuation.size());

    for (const auto& step : match.alignmentTrace) {
        const bool hasQuery = step.queryIndex.has_value();
        const bool hasTemplate = step.templateIndex.has_value();
        AlignmentCellKind kind{};
        switch (step.operation) {
            case AlignmentOp::Match:
            case AlignmentOp::Substitute:
                if (!hasQuery || !hasTemplate) return empty();
                kind = AlignmentCellKind::Aligned;
                hasAlignedPair = true;
                break;
            case AlignmentOp::QueryInsertion:
                if (!hasQuery || hasTemplate) return empty();
                kind = AlignmentCellKind::InputInsertion;
                break;
            case AlignmentOp::TemplateDeletion:
                if (hasQuery || !hasTemplate) return empty();
                kind = AlignmentCellKind::TemplateSkip;
                break;
            default:
                return empty();
        }

        if (hasQuery) {
            const auto queryIndex = *step.queryIndex;
            if (queryIndex >= seenQuery.size() || seenQuery[queryIndex] ||
                (previousQuery && queryIndex <= *previousQuery)) return empty();
            seenQuery[queryIndex] = true;
            previousQuery = queryIndex;
        }
        if (hasTemplate) {
            const auto templateIndex = *step.templateIndex;
            if (templateIndex >= templateCount || seenTemplate[templateIndex] ||
                templateIndex < match.templateMatchStart ||
                templateIndex > match.templateMatchEnd ||
                (previousTemplate && templateIndex <= *previousTemplate)) return empty();
            seenTemplate[templateIndex] = true;
            previousTemplate = templateIndex;
        }
        cells.push_back({kind, step.queryIndex, step.templateIndex, false});
    }

    if (!hasAlignedPair ||
        !std::all_of(seenQuery.begin(), seenQuery.end(), [](bool seen) { return seen; })) return empty();

    // The engine emits the plain suffix, optionally followed by one finite loop
    // wrap up to the template's match start.
    const auto suffixCount = templateCount - match.continuationStartIndex;
    const auto wrapLimit = match.templateMatchStart == 0
        ? (match.hasContinuation ? std::size_t{0} : templateCount)
        : match.templateMatchStart;
    const auto wrapCount = std::min(wrapLimit, templateCount);
    const auto loopedCount = suffixCount + wrapCount;
    if (candidate.continuation.size() != suffixCount &&
        candidate.continuation.size() != loopedCount) return empty();

    for (std::size_t i = 0; i < candidate.continuation.size(); ++i) {
        const auto templateIndex = (match.continuationStartIndex + i) % templateCount;
        cells.push_back({AlignmentCellKind::Continuation, std::nullopt, templateIndex, i == 0});
    }
    return cells;
}

} // namespace harmony::ui
