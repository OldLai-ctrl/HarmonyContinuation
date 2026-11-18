#pragma once
#include "PhraseGenerator.h"
namespace harmony::generation {
const StylePrior& productStylePrior();
// Appends algorithmic results after the database Top3, preserving database order.
void appendProductGeneration(RecommendationSet&,const Progression&,const AnalysisContext&,
    const RecommendationRequest&,const std::function<bool()>& cancelled={});
}
