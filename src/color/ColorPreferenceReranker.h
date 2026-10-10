#pragma once
#include "HarmonyColorAnalyzer.h"
#include "enrichment/ProgressionEnrichmentEngine.h"

namespace harmony::color {
enum class Preference { Off, ContinueSource, Warmer, Cooler, TensionArc, TensionWarmClose };
enum class RankReason { Off, Matched, InsufficientColor, InsufficientSource, FunctionalFallback, QualityProtected, TargetNotMet };
struct ColorRankDecision { std::optional<double> cost; RankReason reason{RankReason::Off}; };
struct ColorRankResult {
    std::vector<std::size_t> order; // Original indices; never copies/edits musical candidates.
    std::vector<ColorRankDecision> decisions; // Indexed by original position, not display rank.
};
class ColorPreferenceReranker {
public:
    static PathColor analyzeEnrichment(const Progression&);
    static ColorRankResult rankEnrichment(const Progression& source,const HarmonicAnalysisResult& analysis,
        std::optional<PhraseIntent> intent,HarmonicTendency tendency,
        std::span<const enrichment::EnrichmentCandidate> candidates,std::span<const PathColor> paths,Preference);
    static ColorRankResult rank(std::span<const TimedChord> source,
        std::span<const KeyCandidate> keys,std::optional<KeySignature> selectedKey,
        std::span<const ContinuationCandidate> candidates,std::span<const PathColor> paths,Preference);
};
const char* preferenceKey(Preference) noexcept;
const char* rankReasonKey(RankReason) noexcept;
}
