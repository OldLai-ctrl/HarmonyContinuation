#pragma once
#include "core/ImportedProgression.h"
#include "core/HarmonyAnalysis.h"
#include "core/ProgressionMatcher.h"
#include <optional>
#include <string>
#include <vector>

namespace harmony::session {
enum class Tab : std::uint8_t { Recommend, Library, Match, Diagnostics };
struct PluginSessionState {
    ImportedProgressionSession imported;
    std::optional<KeySignature> forcedKey;
    std::optional<Style> style;
    std::optional<PhraseIntent> intent;
    bool skeletonView{};
    Tab tab{Tab::Recommend};
    bool debugExpanded{};
    std::vector<std::string> pinnedCandidateIds;
    std::uint32_t factoryLibraryVersion{1};
    std::optional<int> meterNumerator;
    std::optional<int> meterDenominator;

    bool pin(const std::string& id);
    bool unpin(const std::string& id);
    AnalysisContext analysisContext() const;
};
struct DecodeResult {
    PluginSessionState state;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};
enum class RecomputeScope { None, Ranking, Analysis };
RecomputeScope recomputeScope(const PluginSessionState&, const PluginSessionState&) noexcept;
std::string serialize(const PluginSessionState&);
DecodeResult deserialize(std::string_view);
} // namespace harmony::session
