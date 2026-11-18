#include "wish/WishSampler.h"
#include "core/RecommendationPolicy.h"
#include "library/ProgressionLibrary.h"
#include "midi/MidiWorkflow.h"
#include "snapshot/RecommendationSnapshot.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace harmony;
using namespace harmony::wish;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

MatchEvent event(int degree, ChordQuality quality, double duration,
                 std::uint16_t intervals = 0, int bass = -1,
                 std::uint16_t colors = 0) {
    MatchEvent result;
    result.degree = ScaleDegree{degree, 0};
    result.quality = quality;
    // Complete the synthetic functional projection, as the Factory parser does.
    result.function=degree==1?HarmonicFunction::Tonic:
        (degree==3||degree==6)?HarmonicFunction::TonicProlongation:
        (degree==2||degree==4)?HarmonicFunction::Predominant:HarmonicFunction::Dominant;
    result.roles = flag(Role::Structural);
    result.structuralWeight = 0.9f;
    result.durationQN = duration;
    result.intervalMask = intervals;
    if (bass >= 0) result.bassInterval = bass;
    result.colorMask = colors;
    return result;
}

ProgressionTemplate source(std::string id) {
    ProgressionTemplate result;
    result.id = std::move(id);
    result.name = "wish sampler fixture";
    result.mode = Mode::Major;
    result.intent = PhraseIntent::Develop;
    result.cadence = CadenceType::None;
    result.meterNumerator = 4;
    result.meterDenominator = 4;
    result.full = {
        event(1, ChordQuality::Major7, 1.25, (1u<<0)|(1u<<4)|(1u<<7)|(1u<<11), 4),
        event(5, ChordQuality::Dominant7, 2.5, (1u<<0)|(1u<<4)|(1u<<7)|(1u<<10), 7, 1u<<2),
        event(6, ChordQuality::Minor7, 0.75, (1u<<0)|(1u<<3)|(1u<<7)|(1u<<10), 0),
        event(4, ChordQuality::Major, 3.5, (1u<<0)|(1u<<4)|(1u<<7), 0)
    };
    result.skeletonIndices = {0, 1, 2, 3};
    prepareTemplate(result);
    return result;
}

ProgressionTemplate minorSource(std::string id, int bassInterval = 0,
                                std::uint16_t colorMask = 0) {
    ProgressionTemplate result;
    result.id = std::move(id);
    result.name = "minor wish fixture";
    result.mode = Mode::Minor;
    result.intent = PhraseIntent::Develop;
    result.meterNumerator = 4;
    result.meterDenominator = 4;
    result.full = {event(1, ChordQuality::Minor, 2.0,
                         (1u<<0)|(1u<<3)|(1u<<7), bassInterval, colorMask)};
    result.skeletonIndices = {0};
    prepareTemplate(result);
    return result;
}

ModeMembership membership(MusicalMode mode, int offset = 0,
                          ReviewStatus status = ReviewStatus::Reviewed) {
    ModeMembership result;
    result.tonicOffset = offset;
    result.mode = mode;
    result.usage = EditorialUsage::Medium;
    result.usageBasis = "focused sampler fixture";
    result.musicalEvidence = "manually specified test membership";
    result.review = status;
    result.reviewer = "test";
    if (status != ReviewStatus::Reviewed) {
        result.musicalEvidence.clear();
        result.reviewer.clear();
    }
    return result;
}

AnnotationCatalog catalogFor(const ProgressionTemplate& item,
                             std::vector<ModeMembership> memberships) {
    AnnotationCatalog result;
    result.entries.push_back({item.id, sourceFingerprint(item), std::move(memberships)});
    return result;
}

std::string performance(const preview::Sequence& sequence, int n = 4, int d = 4) {
    return performanceFingerprint(sequence, n, d);
}

std::set<std::string> directEligibleIdentities(const ProgressionTemplate& item,
                                               const WishInput& input) {
    const auto query = makeMatchQuery(input.imported.events,
        recommendationContext(input.analysis));
    const auto set = recommendContinuations(query, CandidateIndex({item}), input.request,
                                             recommendationWeights());
    std::set<std::string> result;
    for (const auto& group : set.groups) for (const auto& candidate : group) {
        if (!candidate.baseQualified || !candidate.hardConstraintsPassed ||
            !std::isfinite(candidate.rankingScore)) continue;
        const auto route = preview::sourceRoute(input.imported, candidate, item);
        if (!route) continue;
        const auto built = preview::buildSequence(route.imported, &route.candidate,
                                                   preview::defaultTempoBPM);
        require(bool(built), "direct input candidate previews");
        result.insert(performance(built.sequence, route.meterNumerator,
                                  route.meterDenominator));
    }
    return result;
}

std::vector<preview::SourceRouteResult> directRoutesForAlignment(
    const ProgressionTemplate& item, const WishInput& input,
    std::size_t matchStart, std::size_t matchEnd, std::size_t continuationStart) {
    const auto query = makeMatchQuery(input.imported.events,
        recommendationContext(input.analysis));
    const auto set = recommendContinuations(query, CandidateIndex({item}), input.request,
                                             recommendationWeights());
    std::vector<preview::SourceRouteResult> result;
    for (const auto& group : set.groups) for (const auto& candidate : group) {
        if (!candidate.baseQualified || !candidate.hardConstraintsPassed ||
            !std::isfinite(candidate.rankingScore) || candidate.primaryTemplate != item.id ||
            candidate.matchStart != matchStart || candidate.matchEnd != matchEnd ||
            candidate.continuationStart != continuationStart) continue;
        auto route = preview::sourceRoute(input.imported, candidate, item);
        if (route) result.push_back(std::move(route));
    }
    return result;
}

WishInput prefixInput(const ProgressionTemplate& item) {
    const auto complete = preview::templateRoute(item, {PitchClass::C, item.mode});
    require(bool(complete) && complete.imported.events.size() >= 3,
            "input fixture has a prefix and continuation");
    Progression prefix(complete.imported.events.begin(), complete.imported.events.begin() + 2);
    prefix.back().durationQN.reset();
    prefix.back().openEnded = true;
    WishInput result;
    require(result.imported.replace(std::move(prefix),
                TimelineCoordinateMode::RelativeToSelection), "input fixture timeline");
    result.analysis.forcedKey = KeySignature{PitchClass::C, item.mode};
    result.request.preferredIntent = item.intent;
    return result;
}

void roundTripAndMidi(const WishItem& item, const library::LoadResult& factory,
                      bool expectSuffix) {
    require(bool(item.route), "round-trip source route exists");
    require(item.route.candidate.continuation.empty() != expectSuffix,
            "route has the requested continuation shape");
    const auto before = preview::buildSequence(item.route.imported,
        &item.route.candidate, preview::defaultTempoBPM);
    require(bool(before), "route previews before snapshot");
    require(performance(before.sequence, item.route.meterNumerator,
                        item.route.meterDenominator) == item.musicIdentity,
            "route full music identity matches sampler item");

    std::vector<MatchResult> matches;
    if (item.match) matches.push_back(*item.match);
    auto saved = snapshot::capture(item.route.imported, item.route.candidate, matches,
        preview::defaultTempoBPM, item.route.meterNumerator,
        item.route.meterDenominator, item.route.candidate.key);
    saved.factoryLibraryVersion = factory.libraryVersion;
    const auto encoded = snapshot::serialize(saved);
    const auto decoded = snapshot::deserialize(encoded, &factory);
    require(bool(decoded), "snapshot v3 deserialize");
    const auto after = preview::buildSequence(decoded.value.imported,
        &decoded.value.candidate, decoded.value.tempoBPM);
    require(bool(after), "route previews after snapshot");
    require(performance(after.sequence, decoded.value.meterNumerator,
                        decoded.value.meterDenominator) == item.musicIdentity,
            "snapshot round-trip preserves full music identity");

    midi::CandidateExportContext originalContext;
    originalContext.imported = item.route.imported;
    originalContext.tempo = preview::defaultTempoBPM;
    originalContext.meter = {item.route.meterNumerator, item.route.meterDenominator};
    originalContext.key = item.route.candidate.key;
    midi::CandidateExportContext decodedContext = originalContext;
    decodedContext.imported = decoded.value.imported;
    decodedContext.tempo = decoded.value.tempoBPM;
    decodedContext.meter = {decoded.value.meterNumerator, decoded.value.meterDenominator};
    decodedContext.key = decoded.value.key;
    const auto midiBefore = midi::candidatePayload(originalContext, item.route.candidate);
    const auto midiAfter = midi::candidatePayload(decodedContext, decoded.value.candidate);
    require(bool(midiBefore) && bool(midiAfter), "MIDI candidate payloads build");
    require(midiBefore.payload.smfBytes == midiAfter.payload.smfBytes,
            "snapshot round-trip preserves MIDI candidate payload bytes");
}
} // namespace

int runWishSamplerChecks() {
    int checks{};
    const auto verify = [&](bool condition, const char* message) {
        require(condition, message);
        ++checks;
    };
    const AnnotationCatalog emptyCatalog;
    const auto base = source("TEST_WISH_BASE");

    // Free sampling is independent of, and cannot mutate, any WishInput state.
    WishInput first, second;
    first.request.preferredIntent = PhraseIntent::Loop;
    first.analysis.forcedKey = KeySignature{PitchClass::F, Mode::Minor};
    first.imported.replace({ChordEvent{}}, TimelineCoordinateMode::RelativeToSelection);
    second.request.preferredIntent = PhraseIntent::Resolve;
    second.analysis.forcedKey = KeySignature{PitchClass::D, Mode::Major};
    second.imported.replace({ChordEvent{}}, TimelineCoordinateMode::RelativeToSelection);
    const auto free = makeFreePool(std::span<const ProgressionTemplate>(&base, 1), emptyCatalog);
    verify(bool(free) && free.items.size() == 12, "unannotated valid source enters 12-tonic free pool");
    verify(first.imported.events.size() == 1 && second.imported.events.size() == 1,
           "free pool has no WishInput dependency or side effect");
    first.clear();
    verify(first.imported.events.empty() && first.imported.coordinateMode == TimelineCoordinateMode::Unknown &&
           first.analysis.forcedKey == std::nullopt && !first.request.preferredIntent,
           "WishInput clear resets its own input, context, and request");
    verify(second.imported.events.size() == 1 && second.request.preferredIntent == PhraseIntent::Resolve &&
           second.analysis.forcedKey &&
           second.analysis.forcedKey->tonic == PitchClass::D &&
           second.analysis.forcedKey->mode == Mode::Major,
           "clearing one WishInput leaves another untouched");

    // All 12 roots preserve inversion, extension data and authored, unequal QN timing.
    std::set<int> roots;
    for (const auto& item : free.items) {
        const auto built = preview::buildSequence(item.route.imported, &item.route.candidate, 120);
        verify(bool(built) && built.sequence.events.size() == base.full.size(),
               "transposed route realizes every source chord");
        if (!built) continue;
        roots.insert(built.sequence.events.front().chord.root);
        for (std::size_t i = 0; i < base.full.size(); ++i) {
            const auto expected = realizeContinuation(base.full[i], item.route.candidate.key,
                                                       *base.full[i].durationQN);
            const auto actual = built.sequence.events[i];
            const auto chord = expected.harmonicData ? chordPitches(*expected.harmonicData)
                                                     : chordPitches(expected.label, expected.quality);
            verify(actual.chord.root == chord.root && actual.chord.bass == chord.bass &&
                   actual.chord.intervals == chord.intervals && actual.chord.colorMask == chord.colorMask,
                   "transposition preserves root, bass, intervals, and extensions");
            verify(std::abs(actual.durationQN - *base.full[i].durationQN) < 1e-9,
                   "transposition preserves authored nonuniform QN duration");
        }
    }
    verify(roots.size() == 12, "free pool covers all 12 tonic transpositions");

    // Multiple labels are metadata, not extra tickets; mode selection is an OR union.
    const auto minor = minorSource("TEST_WISH_MULTI");
    auto multiCatalog = catalogFor(minor, {membership(MusicalMode::Dorian),
                                           membership(MusicalMode::Aeolian)});
    const auto multiAll = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), multiCatalog);
    verify(bool(multiAll) && multiAll.items.size() == 12,
           "multiple reviewed labels do not multiply free music tickets");
    WishFilter oneMode; oneMode.fixedTonic = PitchClass::C; oneMode.modes = {MusicalMode::Dorian};
    const auto dorian = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), multiCatalog, oneMode);
    oneMode.modes = {MusicalMode::Aeolian};
    const auto aeolian = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), multiCatalog, oneMode);
    oneMode.modes = {MusicalMode::Dorian, MusicalMode::Aeolian};
    const auto either = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), multiCatalog, oneMode);
    verify(dorian.items.size() == 1 && aeolian.items.size() == 1 && either.items.size() == 1,
           "multi-mode filtering is an OR union over matching memberships");
    verify(either.items.front().memberships.size() == 2,
           "OR-selected route retains both matching memberships");

    WishFilter strict; strict.fixedTonic = PitchClass::C;
    strict.modes = {MusicalMode::Dorian}; strict.strict = true;
    const auto inScale = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1),
                                      catalogFor(minor, {membership(MusicalMode::Dorian)}), strict);
    verify(inScale.items.size() == 1, "strict mode accepts fully in-scale minor triad");
    const auto badBass = minorSource("TEST_WISH_BAD_BASS", 11);
    const auto bassRejected = makeFreePool(std::span<const ProgressionTemplate>(&badBass, 1),
        catalogFor(badBass, {membership(MusicalMode::Dorian)}), strict);
    verify(bassRejected.items.empty(), "strict mode rejects out-of-scale bass");
    const auto badExtension = minorSource("TEST_WISH_BAD_EXTENSION", 0, 1u<<11);
    const auto extensionRejected = makeFreePool(std::span<const ProgressionTemplate>(&badExtension, 1),
        catalogFor(badExtension, {membership(MusicalMode::Dorian)}), strict);
    verify(extensionRejected.items.empty(), "strict mode rejects out-of-scale extension");

    for (const auto status : {ReviewStatus::Pending, ReviewStatus::Unconfirmed}) {
        const auto pendingCatalog = catalogFor(minor, {membership(MusicalMode::Dorian, 0, status)});
        const auto unrestricted = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), pendingCatalog);
        strict.strict = false;
        const auto filtered = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), pendingCatalog, strict);
        verify(unrestricted.items.size() == 12 && filtered.items.empty(),
               "pending and unconfirmed labels do not filter mode but do not remove free music");
    }
    strict.strict = false;
    auto mismatchCatalog = catalogFor(minor, {membership(MusicalMode::Dorian)});
    mismatchCatalog.entries.front().sourceFingerprint = "stale-source";
    const auto mismatchFree = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), mismatchCatalog);
    strict.modes = {MusicalMode::Dorian};
    const auto mismatchFiltered = makeFreePool(std::span<const ProgressionTemplate>(&minor, 1), mismatchCatalog, strict);
    verify(mismatchFree.items.size() == 12 && mismatchFree.sourceMismatches == 1 &&
           mismatchFiltered.items.empty(), "source mismatch remains free music but cannot supply a mode filter");

    // Fingerprints describe the complete performed route, not labels or prefixes.
    preview::Sequence identity;
    preview::Event identityEvent;
    identityEvent.chord.root = 0; identityEvent.chord.bass = 4;
    identityEvent.chord.quality = ChordQuality::Major7;
    identityEvent.chord.intervals = (1u<<0)|(1u<<4)|(1u<<7)|(1u<<11);
    identityEvent.chord.colorMask = 1u<<11; identityEvent.chord.exactIntervals = true;
    identityEvent.startQN = 0; identityEvent.durationQN = 1.25; identityEvent.topVoice = 71;
    identity.events = {identityEvent, identityEvent};
    identity.events[1].startQN = 1.25; identity.events[1].durationQN = 2.5;
    const auto identityBase = performance(identity);
    auto changed = identity; changed.events.pop_back();
    verify(performance(changed) != identityBase, "fingerprint distinguishes a prefix");
    changed = identity; changed.events[0].chord.bass = 5;
    verify(performance(changed) != identityBase, "fingerprint distinguishes bass");
    changed = identity; changed.events[0].chord.colorMask ^= 1u<<2;
    verify(performance(changed) != identityBase, "fingerprint distinguishes extensions");
    changed = identity; changed.events[1].startQN += 0.25;
    verify(performance(changed) != identityBase, "fingerprint distinguishes relative onset");
    changed = identity; changed.events[1].durationQN += 0.25;
    verify(performance(changed) != identityBase, "fingerprint distinguishes duration");
    changed = identity; changed.events[0].topVoice = 72;
    verify(performance(changed) != identityBase, "fingerprint distinguishes top voice");
    changed = identity; changed.sourceRecommendation = "different explanatory label";
    verify(performance(changed) == identityBase, "fingerprint ignores recommendation labels");
    changed = identity; changed.events[0].chord.label = "alternate chord spelling";
    verify(performance(changed) == identityBase, "fingerprint ignores chord spelling labels");

    // Source IDs deduplicate only exact music; origins remain available on the item.
    auto duplicate = base; duplicate.id = "TEST_WISH_BASE_COPY";
    prepareTemplate(duplicate);
    const std::vector<ProgressionTemplate> duplicateSources{base, duplicate};
    const auto deduped = makeFreePool(duplicateSources, emptyCatalog);
    verify(deduped.items.size() == 12, "same music from different source IDs is deduplicated");
    verify(std::all_of(deduped.items.begin(), deduped.items.end(), [](const auto& item) {
        return item.sources.size() == 2 && item.sources[0].id != item.sources[1].id;
    }), "deduplicated music retains both source IDs");

    WishRound batches(free, 0xA17EULL);
    const auto firstDraw = batches.draw(1);
    const auto secondDraw = batches.draw(10);
    const auto thirdDraw = batches.draw(10);
    std::set<std::string> batchIdentities;
    for (const auto& item : firstDraw) batchIdentities.insert(item.musicIdentity);
    for (const auto& item : secondDraw) batchIdentities.insert(item.musicIdentity);
    for (const auto& item : thirdDraw) batchIdentities.insert(item.musicIdentity);
    verify(firstDraw.size() == 1 && secondDraw.size() == 10 && thirdDraw.size() == 1,
           "one plus ten plus ten returns actual remaining count without refill");
    verify(batchIdentities.size() == 12 && batches.remaining() == 0,
           "one WishRound draws without replacement and exhausts once");
    bool oversizedRejected{};
    try { batches.draw(11); } catch (const std::invalid_argument&) { oversizedRejected = true; }
    verify(oversizedRejected, "WishRound rejects a batch over ten");

    auto deterministicCatalog = catalogFor(base, {membership(MusicalMode::Ionian)});
    const auto duplicateCatalog = catalogFor(duplicate, {membership(MusicalMode::Ionian)});
    deterministicCatalog.entries.push_back(duplicateCatalog.entries.front());
    auto reversedCatalog = deterministicCatalog;
    std::reverse(reversedCatalog.entries.begin(), reversedCatalog.entries.end());
    const auto stableA = makeFreePool(duplicateSources, deterministicCatalog);
    const std::vector<ProgressionTemplate> reversedSources{duplicate, base};
    const auto stableB = makeFreePool(reversedSources, reversedCatalog);
    WishRound stableRoundA(stableA, 45123), stableRoundB(stableB, 45123);
    const auto stableDrawA = stableRoundA.draw(10), stableDrawB = stableRoundB.draw(10);
    verify(stableDrawA.size() == stableDrawB.size() &&
           std::equal(stableDrawA.begin(), stableDrawA.end(), stableDrawB.begin(),
               [](const auto& a, const auto& b) { return a.musicIdentity == b.musicIdentity; }),
           "same seed is stable under source and annotation order reversal");

    // A coarse family is only a soft bucket: different performed rhythms survive.
    auto rhythmVariant = base; rhythmVariant.id = "TEST_WISH_RHYTHM_VARIANT";
    rhythmVariant.full[0].durationQN = 1.5; prepareTemplate(rhythmVariant);
    const std::vector<ProgressionTemplate> familySources{base, rhythmVariant};
    WishFilter fixed; fixed.fixedTonic = PitchClass::C;
    const auto familyPool = makeFreePool(familySources, emptyCatalog, fixed);
    verify(familyPool.items.size() == 2 &&
           familyPool.items[0].familyIdentity == familyPool.items[1].familyIdentity &&
           familyPool.items[0].musicIdentity != familyPool.items[1].musicIdentity,
           "same soft family retains distinct complete music");
    WishRound familyRound(familyPool, 991);
    verify(familyRound.draw(10).size() == 2, "soft family does not discard a different performance");

    // Twelve transpositions in one family must not outweigh a family with one
    // instance at the family-selection stage.
    WishFilter fixedC; fixedC.fixedTonic = PitchClass::C;
    const auto singleInstancePool = makeFreePool(
        std::span<const ProgressionTemplate>(&minor, 1), emptyCatalog, fixedC);
    verify(singleInstancePool.items.size() == 1 &&
           singleInstancePool.items.front().familyIdentity != free.items.front().familyIdentity,
           "family balance fixture has one instance in a distinct family");
    WishPool familyBalance = free;
    familyBalance.items.push_back(singleInstancePool.items.front());
    std::size_t singleFamilyFirstDraws{};
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        WishRound round(familyBalance, seed);
        const auto picked = round.draw(1);
        if (picked.size() == 1 &&
            picked.front().familyIdentity == singleInstancePool.items.front().familyIdentity)
            ++singleFamilyFirstDraws;
    }
    verify(singleFamilyFirstDraws >= 12 && singleFamilyFirstDraws <= 52,
           "fixed-seed first draws select the single-instance family despite 12 transpositions");

    // Input recommendations are the production query's hard/base-qualified routes.
    const auto input = prefixInput(base);
    const auto expectedInput = directEligibleIdentities(base, input);
    const auto inputPool = makeInputPool(std::span<const ProgressionTemplate>(&base, 1),
                                         emptyCatalog, input);
    std::set<std::string> actualInput;
    for (const auto& item : inputPool.items) actualInput.insert(item.musicIdentity);
    if(expectedInput.empty()||!inputPool||actualInput!=expectedInput)
    {
        const auto q=makeMatchQuery(input.imported.events,recommendationContext(input.analysis));
        const auto diagnostics=recommendContinuations(q,CandidateIndex({base}),input.request,recommendationWeights());
        std::ostringstream why;for(const auto& m:diagnostics.matches)why<<" similarity="<<m.similarity<<" gate="<<m.candidateEligibilityScore<<" reason="<<m.disposition;
        throw std::runtime_error("input comparison: direct="+std::to_string(expectedInput.size())+
            " sampled="+std::to_string(actualInput.size())+" invalid="+std::to_string(inputPool.invalidSources)+
            " aligned="+std::to_string(inputPool.unifiedStats.aligned)+" qualified="+std::to_string(inputPool.unifiedStats.scorePassed)+
            " error="+inputPool.error+why.str());
    }
    verify(bool(inputPool) && !expectedInput.empty() && actualInput == expectedInput,
           "input pool identities equal direct base and hard-qualified recommendations");

    // Two sources share the matched input and continuation, but have different
    // complete-prefix music before the match. Both direct Unified routes must
    // survive input-pool deduplication by full performance identity.
    auto prefixA = source("TEST_WISH_PREFIX_A");
    prefixA.full = {
        event(4, ChordQuality::Major7, 1.25, (1u<<0)|(1u<<4)|(1u<<7)|(1u<<11), 4),
        base.full[0], base.full[1], base.full[2]
    };
    prepareTemplate(prefixA);
    auto prefixB = source("TEST_WISH_PREFIX_B");
    prefixB.full = {
        event(2, ChordQuality::Minor7, 1.25, (1u<<0)|(1u<<3)|(1u<<7)|(1u<<10), 3),
        base.full[0], base.full[1], base.full[2]
    };
    prepareTemplate(prefixB);
    const auto sharedInput = prefixInput(base);
    const auto directA = directRoutesForAlignment(prefixA, sharedInput, 1, 2, 3);
    const auto directB = directRoutesForAlignment(prefixB, sharedInput, 1, 2, 3);
    verify(!directA.empty() && !directB.empty(),
           "both single-source Unified queries qualify the intended shifted match");
    if (!directA.empty() && !directB.empty()) {
        const auto seqA = preview::buildSequence(directA.front().imported,
            &directA.front().candidate, preview::defaultTempoBPM);
        const auto seqB = preview::buildSequence(directB.front().imported,
            &directB.front().candidate, preview::defaultTempoBPM);
        verify(bool(seqA) && bool(seqB), "both shifted direct routes realize");
        const auto identityA = performance(seqA.sequence, directA.front().meterNumerator,
                                           directA.front().meterDenominator);
        const auto identityB = performance(seqB.sequence, directB.front().meterNumerator,
                                           directB.front().meterDenominator);
        verify(identityA != identityB &&
               continuationMusicFingerprint(directA.front().candidate) ==
                   continuationMusicFingerprint(directB.front().candidate),
               "different pre-match prefixes retain distinct full identities with the same suffix");
        const std::vector<ProgressionTemplate> shiftedSources{prefixA, prefixB};
        const auto shiftedPool = makeInputPool(shiftedSources, emptyCatalog, sharedInput);
        const auto hasSourceIdentity = [&](const std::string& identity,
                                           const TemplateID& sourceId) {
            return std::any_of(shiftedPool.items.begin(), shiftedPool.items.end(),
                [&](const auto& item) {
                    return item.musicIdentity == identity &&
                        std::any_of(item.sources.begin(), item.sources.end(),
                            [&](const auto& sourceRef) { return sourceRef.id == sourceId; });
                });
        };
        verify(bool(shiftedPool) && hasSourceIdentity(identityA, prefixA.id) &&
               hasSourceIdentity(identityB, prefixB.id),
               "combined input pool preserves both full-prefix identities instead of suffix-deduplicating");
    }
    WishInput noInput;
    verify(makeInputPool(std::span<const ProgressionTemplate>(&base, 1), emptyCatalog,
                         noInput).items.empty(), "empty input produces an empty input pool");
    auto hardFailure = input;
    MelodyConstraint incompatible;
    incompatible.startQN = 0; incompatible.durationQN = 1;
    incompatible.pitchClass = 1; incompatible.strictness = ConstraintStrictness::Hard;
    hardFailure.request.constraints.melody.push_back(incompatible);
    verify(directEligibleIdentities(base, hardFailure).empty() &&
           makeInputPool(std::span<const ProgressionTemplate>(&base, 1), emptyCatalog,
                         hardFailure).items.empty(),
           "hard melody conflict cannot be relaxed by input sampling");

    // Exercise the installed compile-time V5 database through the real sampler,
    // then round-trip both an empty free suffix and a nonempty input suffix.
#ifndef HC_WISH_FACTORY_DB_PATH
    throw std::runtime_error("HC_WISH_FACTORY_DB_PATH is not defined for WishTests");
#else
    const auto factory = library::loadFactory(HC_WISH_FACTORY_DB_PATH);
    verify(bool(factory) && factory.libraryVersion == 5 && factory.storageSchemaVersion == 2 &&
           factory.templates.size() == 658, "real V5 Factory database loads 658 templates");
    std::size_t validFingerprints{};
    for (const auto& item : factory.templates) {
        const auto fingerprint = sourceFingerprint(item);
        verify(!fingerprint.empty(), "real Factory source has a valid source fingerprint");
        ++validFingerprints;
    }
    verify(validFingerprints == 658, "all real Factory sources fingerprint successfully");
    const auto realFree = makeFreePool(factory.templates, emptyCatalog);
    verify(bool(realFree) && !realFree.items.empty() && realFree.unannotatedSources > 0,
           "real V5 unrestricted free pool is nonempty without mode annotations");
    WishRound realRound(realFree, 20261023);
    const auto realDraw = realRound.draw(10);
    verify(realDraw.size() == 10, "real V5 pool draws ten unique performances");
    for (std::size_t i = 0; i < realDraw.size(); ++i)
        for (std::size_t j = i + 1; j < realDraw.size(); ++j)
            verify(realDraw[i].musicIdentity != realDraw[j].musicIdentity,
                   "real V5 draw has no repeated complete performance");
    const auto freeItem = std::find_if(realFree.items.begin(), realFree.items.end(),
        [](const auto& item) { return item.route.candidate.continuation.empty(); });
    verify(freeItem != realFree.items.end(), "real free draw has an empty continuation suffix");
    if (freeItem != realFree.items.end()) roundTripAndMidi(*freeItem, factory, false);

    const auto realPrefixSource = std::find_if(factory.templates.begin(), factory.templates.end(),
        [](const auto& item) { return item.id.starts_with("V3_DUSK_") && item.full.size() >= 3; });
    verify(realPrefixSource != factory.templates.end(), "real input prefix fixture is present");
    if (realPrefixSource != factory.templates.end()) {
        const auto realInput = prefixInput(*realPrefixSource);
        const auto realInputPool = makeInputPool(factory.templates, emptyCatalog, realInput);
        const auto withSuffix = std::find_if(realInputPool.items.begin(), realInputPool.items.end(),
            [](const auto& item) { return !item.route.candidate.continuation.empty(); });
        verify(bool(realInputPool) && withSuffix != realInputPool.items.end(),
               "real V5 input pool contains a continuation suffix");
        if (withSuffix != realInputPool.items.end()) roundTripAndMidi(*withSuffix, factory, true);
    }
#endif
    return checks;
}
