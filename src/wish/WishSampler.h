#pragma once
#include "ModeAnnotations.h"
#include <random>

namespace harmony::wish {
// Runtime-only state. Clearing wish input never edits the normal recommendation session.
struct WishInput {
    ImportedProgressionSession imported;
    AnalysisContext analysis;
    RecommendationRequest request;
    void clear() {*this=WishInput{};}
};
struct WishFilter {
    std::optional<PitchClass> fixedTonic;
    std::vector<MusicalMode> modes; // empty = no mode restriction; otherwise reviewed-membership union
    bool strict{}; // additionally require every sounding pitch in the selected static scale
};
struct RealizedMembership {
    TemplateID source;
    ModeMembership membership;
    int tonicPitchClass{};
};
struct WishSource {
    TemplateID id;
    std::string fingerprint;
    AnnotationState annotationState{AnnotationState::Unannotated};
};
struct WishItem {
    preview::SourceRouteResult route;
    std::string musicIdentity, familyIdentity, variantIdentity;
    std::vector<WishSource> sources;
    std::vector<RealizedMembership> memberships;
    std::optional<MatchResult> match; // absent for free draws; never fabricated
};
struct WishPool {
    std::vector<WishItem> items;
    std::size_t sourceMismatches{}, invalidSources{}, invalidAnnotations{}, unreviewedSources{}, unannotatedSources{}, unconfirmedSources{};
    MatchRunStats unifiedStats;
    std::string error;
    explicit operator bool() const noexcept {return error.empty();}
};
// UI/control worker only: synchronous, bounded by the supplied catalog, no audio callbacks or shared state.
// Without a mode restriction, fixedTonic means the template reference tonic (not an inferred modal center).
WishPool makeFreePool(std::span<const ProgressionTemplate>,const AnnotationCatalog&,const WishFilter& = {});
WishPool makeInputPool(std::span<const ProgressionTemplate>,const AnnotationCatalog&,const WishInput&,const WishFilter& = {});

// One seed + one immutable pool define a round. draw(1), draw(10), draw(10) continue without replacement.
// Exhaustion returns fewer results, with no refill or constraint relaxation.
class WishRound {
public:
    WishRound(WishPool,std::uint64_t seed);
    std::vector<WishItem> draw(std::size_t count);
    std::size_t remaining() const noexcept;
private:
    struct Unit {std::string identity;std::vector<WishItem> items;std::size_t drawn{};};
    struct Family {std::string identity;std::vector<Unit> units;std::size_t drawn{};};
    std::vector<Family> families_;
    std::mt19937_64 random_;
    std::size_t remaining_{};
    std::uint64_t bounded(std::uint64_t);
};
}
