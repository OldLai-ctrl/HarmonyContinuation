#pragma once
#include "core/ImportedProgression.h"
#include "core/HarmonyAnalysis.h"
#include "core/ProgressionMatcher.h"
#include "core/ContinuationEngine.h"
#include <optional>
#include <string>
#include <vector>

namespace harmony::session {
enum class Tab : std::uint8_t { Recommend, Library, Match, Diagnostics };
enum class ProductMode : std::uint8_t { Continue, Enrich };
enum class Locale : std::uint8_t { ZhCN, EnUS };
struct PluginSessionState {
    static constexpr std::uint32_t currentSchemaVersion = 5;
    std::uint32_t schemaVersion{currentSchemaVersion};
    ImportedProgressionSession imported;
    std::optional<KeySignature> forcedKey;
    std::optional<Style> style;
    std::optional<PhraseIntent> intent;
    bool skeletonView{};
    Tab tab{Tab::Recommend};
    ProductMode productMode{ProductMode::Continue};
    Locale locale{Locale::ZhCN};
    bool debugExpanded{};
    std::vector<std::string> pinnedCandidateIds;
    std::vector<std::string> pinnedFingerprints; // parallel to IDs; empty means legacy, never resolve by ID alone
    std::uint32_t factoryLibraryVersion{2};
    std::optional<int> meterNumerator;
    std::optional<int> meterDenominator;
    std::uint32_t editorWidth{1100};  // logical VSTGUI units, never physical DPI pixels
    std::uint32_t editorHeight{900};
    HarmonyConstraintSet constraints;
    HarmonicTendency tendency{HarmonicTendency::Balanced};
    std::uint32_t uiZoomPercent{100};

    bool pin(const std::string& id, const std::string& fingerprint = {});
    bool unpin(const std::string& id);
    AnalysisContext analysisContext() const;
    void resolvePins(const RecommendationSet&);
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
