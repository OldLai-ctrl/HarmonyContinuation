#pragma once
#include "ContinuationEngine.h"

namespace harmony {
void appendContextualContinuations(const MatchQuery&, const CandidateIndex&,
    const RecommendationRequest&, RecommendationSet&);
void rankCreativeContinuations(std::vector<ContinuationCandidate>&);
void assessInspirationStructure(const MatchQuery&,const CandidateIndex&,RecommendationSet&);
const char* recommendationChannelName(RecommendationChannel) noexcept;
int recommendationQualityTier(const ContinuationCandidate&) noexcept;
std::vector<RecommendationPosition> mixedInspirationOrder(const RecommendationSet&);
CreativeDirection inspirationDirection(const ContinuationCandidate&,std::optional<KeySignature> inputCenter = {});
bool unterminatedEnding(PhraseEnding) noexcept;
const char* phraseEndingName(PhraseEnding) noexcept;
void annotatePhraseDevelopment(ContinuationCandidate&,const ProgressionTemplate&,
    const CandidateIndex&,KeySignature inputCenter);
}
