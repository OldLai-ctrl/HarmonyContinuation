#pragma once
#include "ContinuationEngine.h"

namespace harmony {
void appendContextualContinuations(const MatchQuery&, const CandidateIndex&,
    const RecommendationRequest&, RecommendationSet&);
void rankCreativeContinuations(std::vector<ContinuationCandidate>&);
void assessInspirationStructure(const MatchQuery&,const CandidateIndex&,RecommendationSet&);
void prepareContextualDiversity(const MatchQuery&,RecommendationSet&,std::size_t appendedAfter=0);
void prepareContinuationBranching(const MatchQuery&,const CandidateIndex&,RecommendationSet&);
float creativeRouteSimilarity(const ContinuationCandidate&,const ContinuationCandidate&) noexcept;
std::size_t inspirationFirstPageSize(const RecommendationSet&,std::size_t maximum=10);
const char* recommendationChannelName(RecommendationChannel) noexcept;
int recommendationQualityTier(const ContinuationCandidate&) noexcept;
std::vector<RecommendationPosition> mixedInspirationOrder(const RecommendationSet&);
CreativeDirection inspirationDirection(const ContinuationCandidate&,std::optional<KeySignature> inputCenter = {});
bool unterminatedEnding(PhraseEnding) noexcept;
const char* phraseEndingName(PhraseEnding) noexcept;
void annotatePhraseDevelopment(ContinuationCandidate&,const ProgressionTemplate&,
    const CandidateIndex&,KeySignature inputCenter);
}
