#pragma once
#include "ContinuationEngine.h"

namespace harmony {
// Product callers share one policy. Historical core experiments may still use
// explicit contexts/weights, but never select the production worker's behavior.
inline AnalysisContext recommendationContext(AnalysisContext context) {
    context.retrievalTimingMode=RetrievalTimingMode::Unified;
    return context;
}
inline RecommendationWeights recommendationWeights() {
    RecommendationWeights weights;
    weights.perGroup=10;
    weights.abstractChordCompatibility=true;
    return weights;
}
}
