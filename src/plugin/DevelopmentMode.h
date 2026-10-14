#pragma once
#include "core/HarmonyAnalysis.h"
#include "core/ContinuationEngine.h"
namespace harmony::plugin {
enum class DevelopmentAlgorithmPreset {
    Legacy,
    HarmonyFirst,
    Abstract
};

inline const char* algorithmPresetName(DevelopmentAlgorithmPreset preset) noexcept {
    switch (preset) {
        case DevelopmentAlgorithmPreset::Legacy: return "Legacy";
        case DevelopmentAlgorithmPreset::HarmonyFirst: return "HarmonyFirst";
        case DevelopmentAlgorithmPreset::Abstract: return "Abstract";
    }
    return "Legacy";
}

inline AnalysisContext developmentContext(AnalysisContext context,
                                           DevelopmentAlgorithmPreset preset) {
    context.retrievalTimingMode = preset == DevelopmentAlgorithmPreset::HarmonyFirst
        ? RetrievalTimingMode::HarmonyFirst : RetrievalTimingMode::Legacy;
    return context;
}

inline RecommendationWeights developmentRecommendationWeights(DevelopmentAlgorithmPreset preset) {
    RecommendationWeights weights;
    // Retain choices for Show More; the UI normally shows three.
    weights.perGroup = 10;
    weights.diminishedPassingInsertionException = false;
    weights.abstractChordCompatibility = preset == DevelopmentAlgorithmPreset::Abstract;
    return weights;
}

// One entry point shared by submission and development-state diagnostics.
inline AnalysisContext developmentContext(AnalysisContext context) {
#if defined(HC_EXPERIMENT_HARMONY_FIRST_PLUGIN)
    context.retrievalTimingMode=RetrievalTimingMode::HarmonyFirst;
#endif
    return context;
}
inline RecommendationWeights developmentRecommendationWeights() {
    RecommendationWeights weights;
    // Retain choices for Show More; the UI normally shows three.
    weights.perGroup=10;
#if defined(HC_EXPERIMENT_ABSTRACT_RETRIEVAL_PLUGIN)
    weights.abstractChordCompatibility=true;
#endif
    return weights;
}
inline constexpr bool harmonyFirstBuild() {
#if defined(HC_EXPERIMENT_HARMONY_FIRST_PLUGIN)
    return true;
#else
    return false;
#endif
}
}
