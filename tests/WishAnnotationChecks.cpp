#include "wish/ModeAnnotations.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace harmony;
using namespace harmony::wish;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

ProgressionTemplate smallSource() {
    ProgressionTemplate item;
    item.id = "test:annotation-source";
    item.name = "annotation source fixture";
    item.mode = Mode::Major;
    item.meterNumerator = 4;
    item.meterDenominator = 4;
    MatchEvent event;
    event.degree = ScaleDegree{1, 0};
    event.quality = ChordQuality::Major;
    event.durationQN = 4.0;
    item.full.push_back(event);
    return item;
}

ModeMembership reviewedMembership() {
    ModeMembership membership;
    membership.tonicOffset = 0;
    membership.mode = MusicalMode::Dorian;
    membership.usage = EditorialUsage::Medium;
    membership.usageBasis = "fixture rubric";
    membership.musicalEvidence = "reviewed harmonic evidence";
    membership.review = ReviewStatus::Reviewed;
    membership.reviewer = "fixture reviewer";
    return membership;
}

AnnotationCatalog catalogFor(const ProgressionTemplate& item, ModeMembership membership) {
    AnnotationCatalog catalog;
    catalog.entries.push_back({item.id, sourceFingerprint(item), {std::move(membership)}});
    return catalog;
}

std::string quotedRecord(const std::string& id, const std::string& fingerprint,
                         int offset, const char* mode, const char* usage,
                         const char* basis, const char* evidence,
                         const char* review, const char* reviewer,
                         const char* context) {
    std::ostringstream row;
    row << std::quoted(id) << ' ' << offset << ' ' << std::quoted(fingerprint) << ' '
        << std::quoted(mode) << ' ' << std::quoted(usage) << ' ' << std::quoted(basis) << ' '
        << std::quoted(evidence) << ' ' << std::quoted(review) << ' '
        << std::quoted(reviewer) << ' ' << std::quoted(context) << '\n';
    return row.str();
}
} // namespace

int runWishAnnotationChecks() {
    int checks{};
    const auto verify = [&](bool condition, const char* message) {
        check(condition, message);
        ++checks;
    };

    const auto item = smallSource();
    const auto fingerprint = sourceFingerprint(item);
    const auto catalog = catalogFor(item, reviewedMembership());
    const auto encoded = serializeAnnotations(catalog);
    const auto decoded = parseAnnotations(encoded);
    verify(static_cast<bool>(decoded), "annotation round-trip parse");
    verify(decoded.catalog.entries.size() == 1 && decoded.catalog.entries.front().memberships.size() == 1,
           "annotation round-trip membership count");
    verify(serializeAnnotations(decoded.catalog) == encoded, "annotation round-trip stable serialization");
    verify(decoded.catalog.entries.front().sourceFingerprint == fingerprint, "round-trip source fingerprint");
    verify(resolveAnnotations(item, catalog).state == AnnotationState::Reviewed,
           "reviewed membership resolves as reviewed");

    const auto naturalMajor = parseMode("natural_major");
    const auto naturalMinor = parseMode("natural_minor");
    verify(naturalMajor && *naturalMajor == MusicalMode::Ionian &&
           modePitchMask(*naturalMajor) == modePitchMask(MusicalMode::Ionian),
           "natural major is an Ionian alias");
    verify(naturalMinor && *naturalMinor == MusicalMode::Aeolian &&
           modePitchMask(*naturalMinor) == modePitchMask(MusicalMode::Aeolian),
           "natural minor is an Aeolian alias");
    verify(!parseMode("major"), "major label is not a mode membership token");

    auto invalidMembership = reviewedMembership();
    invalidMembership.tonicOffset = 12;
    std::string validationError;
    verify(!validMembership(invalidMembership, validationError), "out-of-range tonic offset rejected");
    invalidMembership = reviewedMembership();
    invalidMembership.mode = static_cast<MusicalMode>(99);
    verify(!validMembership(invalidMembership, validationError), "unknown mode enum rejected");
    invalidMembership = reviewedMembership();
    invalidMembership.review = ReviewStatus::Reviewed;
    invalidMembership.musicalEvidence.clear();
    verify(!validMembership(invalidMembership, validationError), "reviewed membership needs evidence");
    invalidMembership = reviewedMembership();
    invalidMembership.usageBasis.clear();
    verify(!validMembership(invalidMembership, validationError), "rated usage needs a basis");
    auto invalidCatalog = catalog;
    invalidMembership = reviewedMembership();
    invalidMembership.tonicOffset = 12;
    invalidCatalog.entries.front().memberships.push_back(invalidMembership);
    const auto invalidResolution = resolveAnnotations(item, invalidCatalog);
    verify(invalidResolution.state == AnnotationState::Invalid && invalidResolution.memberships.empty(),
           "invalid catalog cannot bypass source binding with partially valid memberships");
    invalidMembership=reviewedMembership();invalidMembership.musicalEvidence="   ";
    verify(!validMembership(invalidMembership,validationError),"blank evidence rejected");

    const auto duplicate = std::string("HC_MODE_ANNOTATIONS 1\n") +
        quotedRecord(item.id, fingerprint, 0, "dorian", "medium", "fixture rubric",
                     "reviewed harmonic evidence", "reviewed", "fixture reviewer", "") +
        quotedRecord(item.id, fingerprint, 0, "dorian", "medium", "fixture rubric",
                     "reviewed harmonic evidence", "reviewed", "fixture reviewer", "");
    verify(!parseAnnotations(duplicate), "duplicate template/tonic/mode rejected");
    const auto conflict = std::string("HC_MODE_ANNOTATIONS 1\n") +
        quotedRecord(item.id, fingerprint, 0, "dorian", "medium", "fixture rubric",
                     "reviewed harmonic evidence", "reviewed", "fixture reviewer", "") +
        quotedRecord(item.id, "different-source", 1, "aeolian", "medium", "fixture rubric",
                     "reviewed harmonic evidence", "reviewed", "fixture reviewer", "");
    verify(!parseAnnotations(conflict), "conflicting source fingerprint rejected");
    verify(!parseAnnotations("HC_MODE_ANNOTATIONS 2\n"), "unknown format version rejected");
    const auto oversized=parseAnnotations(std::string(2u*1024u*1024u+1u,'x'));
    verify(!oversized&&oversized.error.find("2 MiB")!=std::string::npos,"annotation byte limit enforced");
    std::string many="HC_MODE_ANNOTATIONS 1\n";
    for(int i=0;i<8193;++i)many+=quotedRecord("limit-"+std::to_string(i),"source-fixture",0,"dorian","unrated","","","pending","","");
    const auto excessive=parseAnnotations(many);
    verify(!excessive&&excessive.error.find("too many")!=std::string::npos,"annotation record limit enforced");
    const auto badEnum = std::string("HC_MODE_ANNOTATIONS 1\n") +
        quotedRecord(item.id, fingerprint, 0, "unknown_mode", "unrated", "", "",
                     "pending", "", "");
    verify(!parseAnnotations(badEnum), "unknown mode token rejected");
    const auto badRange = std::string("HC_MODE_ANNOTATIONS 1\n") +
        quotedRecord(item.id, fingerprint, 12, "dorian", "unrated", "", "",
                     "pending", "", "");
    verify(!parseAnnotations(badRange), "out-of-range serialized tonic rejected");

    auto pending = reviewedMembership();
    pending.review = ReviewStatus::Pending;
    pending.musicalEvidence.clear();
    pending.reviewer.clear();
    auto pendingResolution = resolveAnnotations(item, catalogFor(item, pending));
    verify(pendingResolution.state == AnnotationState::Unreviewed && pendingResolution.memberships.size() == 1,
           "pending membership remains available as unreviewed");
    auto unconfirmed = pending;
    unconfirmed.review = ReviewStatus::Unconfirmed;
    auto unconfirmedResolution = resolveAnnotations(item, catalogFor(item, unconfirmed));
    verify(unconfirmedResolution.state == AnnotationState::Unconfirmed &&
           unconfirmedResolution.memberships.size() == 1,
           "unconfirmed membership remains available");
    auto mismatchCatalog = catalog;
    mismatchCatalog.entries.front().sourceFingerprint = "stale-source";
    const auto mismatch = resolveAnnotations(item, mismatchCatalog);
    verify(mismatch.state == AnnotationState::SourceMismatch && mismatch.memberships.empty(),
           "exact template ID with different source fingerprint mismatches");

    auto classical = reviewedMembership();
    classical.mode = MusicalMode::ClassicalMelodicMinor;
    classical.melodicContext.clear();
    verify(!validMembership(classical, validationError), "reviewed classical melodic minor needs melodic context");
    classical.review = ReviewStatus::Pending;
    verify(validMembership(classical, validationError), "pending classical melodic minor may omit melodic context");
    verify(modePitchMask(MusicalMode::ClassicalMelodicMinor) == 0,
           "classical melodic minor has no static pitch mask");

    preview::Sequence sequence;
    preview::Event event;
    event.chord.root = 0;
    event.chord.bass = 3;
    event.chord.intervals = static_cast<std::uint16_t>((1u << 0) | (1u << 3) | (1u << 7));
    event.chord.colorMask = static_cast<std::uint16_t>(1u << 9);
    event.durationQN = 1.0;
    event.topVoice = 69;
    sequence.events.push_back(event);
    verify(strictlyCompatible(sequence, MusicalMode::Dorian, 0),
           "strict compatibility includes in-mode extension, bass, and top voice");
    sequence.events.front().chord.colorMask = static_cast<std::uint16_t>(1u << 11);
    verify(!strictlyCompatible(sequence, MusicalMode::Dorian, 0), "out-of-mode extension rejected");
    sequence.events.front().chord.colorMask = static_cast<std::uint16_t>(1u << 9);
    sequence.events.front().chord.bass = 11;
    verify(!strictlyCompatible(sequence, MusicalMode::Dorian, 0), "out-of-mode bass rejected");
    sequence.events.front().chord.bass = 3;
    sequence.events.front().topVoice = 71;
    verify(!strictlyCompatible(sequence, MusicalMode::Dorian, 0), "out-of-mode top voice rejected");
    sequence.events.front().topVoice = 69;
    verify(!strictlyCompatible(sequence, MusicalMode::ClassicalMelodicMinor, 0),
           "classical melodic minor cannot pass a static compatibility test");
    sequence.events.clear();
    verify(!strictlyCompatible(sequence, MusicalMode::Dorian, 0), "empty sequence is incompatible");

    return checks;
}
