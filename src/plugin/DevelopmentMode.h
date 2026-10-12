#pragma once
#include "core/HarmonyAnalysis.h"
#include "core/ContinuationEngine.h"
namespace harmony::plugin {
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
