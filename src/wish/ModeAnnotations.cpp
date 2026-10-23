#include "ModeAnnotations.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace harmony::wish {
namespace {
constexpr std::size_t maximumBytes = 1024u * 1024u;
constexpr std::size_t maximumRecords = 8192;
constexpr std::size_t maximumIdLength = 256;
constexpr std::size_t maximumFingerprintLength = 32768;
constexpr std::size_t maximumEvidenceLength = 8192;
constexpr std::size_t maximumReviewerLength = 512;

bool validModeValue(MusicalMode mode) noexcept {
    switch (mode) {
        case MusicalMode::Ionian: case MusicalMode::Dorian: case MusicalMode::Phrygian:
        case MusicalMode::Lydian: case MusicalMode::Mixolydian: case MusicalMode::Aeolian:
        case MusicalMode::Locrian: case MusicalMode::HarmonicMinor:
        case MusicalMode::MelodicMinorAscending: case MusicalMode::ClassicalMelodicMinor:
            return true;
    }
    return false;
}

bool validUsageValue(EditorialUsage usage) noexcept {
    switch (usage) {
        case EditorialUsage::High: case EditorialUsage::Medium: case EditorialUsage::Low:
        case EditorialUsage::Unrated: return true;
    }
    return false;
}

bool validReviewValue(ReviewStatus status) noexcept {
    switch (status) {
        case ReviewStatus::Pending: case ReviewStatus::Reviewed: case ReviewStatus::Unconfirmed:
            return true;
    }
    return false;
}

bool cleanField(const std::string& field, std::size_t maximum) noexcept {
    return field.size() <= maximum && field.find('\0') == std::string::npos &&
        field.find('\n') == std::string::npos && field.find('\r') == std::string::npos;
}
bool nonblank(const std::string& field) noexcept {
    return std::any_of(field.begin(),field.end(),[](unsigned char c){return !std::isspace(c);});
}

const char* usageName(EditorialUsage usage) noexcept {
    switch (usage) {
        case EditorialUsage::High: return "high";
        case EditorialUsage::Medium: return "medium";
        case EditorialUsage::Low: return "low";
        case EditorialUsage::Unrated: return "unrated";
    }
    return nullptr;
}

std::optional<EditorialUsage> parseUsage(std::string_view token) noexcept {
    if (token == "high") return EditorialUsage::High;
    if (token == "medium") return EditorialUsage::Medium;
    if (token == "low") return EditorialUsage::Low;
    if (token == "unrated") return EditorialUsage::Unrated;
    return std::nullopt;
}

const char* reviewName(ReviewStatus status) noexcept {
    switch (status) {
        case ReviewStatus::Pending: return "pending";
        case ReviewStatus::Reviewed: return "reviewed";
        case ReviewStatus::Unconfirmed: return "unconfirmed";
    }
    return nullptr;
}

std::optional<ReviewStatus> parseReview(std::string_view token) noexcept {
    if (token == "pending") return ReviewStatus::Pending;
    if (token == "reviewed") return ReviewStatus::Reviewed;
    if (token == "unconfirmed") return ReviewStatus::Unconfirmed;
    return std::nullopt;
}

AnnotationLoadResult failure(std::string message) {
    AnnotationLoadResult result;
    result.error = std::move(message);
    return result;
}

bool readQuoted(std::istringstream& row, std::string& value) {
    row >> std::ws;
    if (!row || row.peek() != '"') return false;
    row >> std::quoted(value);
    return static_cast<bool>(row);
}

std::string membershipKey(int tonicOffset, MusicalMode mode) {
    return std::to_string(tonicOffset) + ":" + std::to_string(static_cast<int>(mode));
}

bool validateCatalog(const AnnotationCatalog& catalog, std::string& error) {
    if (catalog.version != 1) { error = "unsupported catalog version"; return false; }
    std::size_t records{};
    std::unordered_set<std::string> ids;
    for (const auto& entry : catalog.entries) {
        if (entry.templateId.empty() || !cleanField(entry.templateId, maximumIdLength)) {
            error = "invalid template ID"; return false;
        }
        if (entry.sourceFingerprint.empty() || !cleanField(entry.sourceFingerprint, maximumFingerprintLength)) {
            error = "invalid source fingerprint"; return false;
        }
        if (!ids.insert(entry.templateId).second) { error = "duplicate template entry"; return false; }
        if (entry.memberships.empty()) { error = "template entry has no memberships"; return false; }
        std::unordered_set<std::string> membershipKeys;
        for (const auto& membership : entry.memberships) {
            if (++records > maximumRecords) { error = "too many annotation records"; return false; }
            if (!validMembership(membership, error)) return false;
            if (!membershipKeys.insert(membershipKey(membership.tonicOffset, membership.mode)).second) {
                error = "duplicate template/tonic/mode membership"; return false;
            }
        }
    }
    return true;
}

} // namespace

const char* modeName(MusicalMode mode) noexcept {
    switch (mode) {
        case MusicalMode::Ionian: return "ionian";
        case MusicalMode::Dorian: return "dorian";
        case MusicalMode::Phrygian: return "phrygian";
        case MusicalMode::Lydian: return "lydian";
        case MusicalMode::Mixolydian: return "mixolydian";
        case MusicalMode::Aeolian: return "aeolian";
        case MusicalMode::Locrian: return "locrian";
        case MusicalMode::HarmonicMinor: return "harmonic_minor";
        case MusicalMode::MelodicMinorAscending: return "melodic_minor_ascending";
        case MusicalMode::ClassicalMelodicMinor: return "classical_melodic_minor";
    }
    return "unknown";
}

std::optional<MusicalMode> parseMode(std::string_view token) noexcept {
    if (token == "ionian" || token == "natural_major") return MusicalMode::Ionian;
    if (token == "dorian") return MusicalMode::Dorian;
    if (token == "phrygian") return MusicalMode::Phrygian;
    if (token == "lydian") return MusicalMode::Lydian;
    if (token == "mixolydian") return MusicalMode::Mixolydian;
    if (token == "aeolian" || token == "natural_minor") return MusicalMode::Aeolian;
    if (token == "locrian") return MusicalMode::Locrian;
    if (token == "harmonic_minor") return MusicalMode::HarmonicMinor;
    if (token == "melodic_minor_ascending") return MusicalMode::MelodicMinorAscending;
    if (token == "classical_melodic_minor") return MusicalMode::ClassicalMelodicMinor;
    return std::nullopt;
}

std::uint16_t modePitchMask(MusicalMode mode) noexcept {
    switch (mode) {
        case MusicalMode::Ionian: return 0xAB5;
        case MusicalMode::Dorian: return 0x6AD;
        case MusicalMode::Phrygian: return 0x5AB;
        case MusicalMode::Lydian: return 0xAD5;
        case MusicalMode::Mixolydian: return 0x6B5;
        case MusicalMode::Aeolian: return 0x5AD;
        case MusicalMode::Locrian: return 0x56B;
        case MusicalMode::HarmonicMinor: return 0x9AD;
        case MusicalMode::MelodicMinorAscending: return 0xAAD;
        case MusicalMode::ClassicalMelodicMinor: return 0;
    }
    return 0;
}

bool validMembership(const ModeMembership& membership, std::string& error) {
    if (membership.tonicOffset < 0 || membership.tonicOffset > 11) {
        error = "tonic offset outside 0..11"; return false;
    }
    if (!validModeValue(membership.mode)) { error = "unknown mode enum"; return false; }
    if (!validUsageValue(membership.usage)) { error = "unknown usage enum"; return false; }
    if (!validReviewValue(membership.review)) { error = "unknown review enum"; return false; }
    if (!cleanField(membership.usageBasis, maximumEvidenceLength) ||
        !cleanField(membership.musicalEvidence, maximumEvidenceLength) ||
        !cleanField(membership.reviewer, maximumReviewerLength) ||
        !cleanField(membership.melodicContext, maximumEvidenceLength)) {
        error = "annotation field exceeds limit or contains a line break"; return false;
    }
    if (membership.review == ReviewStatus::Reviewed &&
        (!nonblank(membership.musicalEvidence) || !nonblank(membership.reviewer))) {
        error = "reviewed membership requires musical evidence and reviewer"; return false;
    }
    if (membership.usage != EditorialUsage::Unrated && !nonblank(membership.usageBasis)) {
        error = "rated usage requires a usage basis"; return false;
    }
    if (membership.mode == MusicalMode::ClassicalMelodicMinor &&
        membership.review == ReviewStatus::Reviewed && !nonblank(membership.melodicContext)) {
        error = "reviewed classical melodic minor requires melodic context"; return false;
    }
    error.clear();
    return true;
}

AnnotationLoadResult parseAnnotations(std::string_view text) {
    if (text.size() > maximumBytes) return failure("annotation file exceeds 1 MiB");
    std::istringstream input{std::string(text)};
    input.imbue(std::locale::classic());
    std::string line;
    if (!std::getline(input, line)) return failure("missing annotation header");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "HC_MODE_ANNOTATIONS 1") return failure("unsupported or missing annotation header");

    AnnotationLoadResult result;
    result.catalog.version = 1;
    std::unordered_map<std::string, std::size_t> entriesById;
    std::unordered_map<std::string, std::string> fingerprintById;
    std::unordered_set<std::string> membershipKeys;
    std::size_t records{};
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        if (++records > maximumRecords) return failure("too many records at line " + std::to_string(lineNumber));
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string id, fingerprint, modeToken, usageToken, usageBasis, evidence,
                    reviewToken, reviewer, melodicContext;
        int tonicOffset{};
        if (!readQuoted(row, id) || !(row >> tonicOffset) || !readQuoted(row, fingerprint) ||
            !readQuoted(row, modeToken) || !readQuoted(row, usageToken) ||
            !readQuoted(row, usageBasis) || !readQuoted(row, evidence) ||
            !readQuoted(row, reviewToken) || !readQuoted(row, reviewer) ||
            !readQuoted(row, melodicContext)) {
            return failure("malformed quoted record at line " + std::to_string(lineNumber));
        }
        row >> std::ws;
        if (!row.eof()) return failure("extra fields at line " + std::to_string(lineNumber));
        const auto mode = parseMode(modeToken);
        const auto usage = parseUsage(usageToken);
        const auto review = parseReview(reviewToken);
        if (!mode || !usage || !review) return failure("unknown enum token at line " + std::to_string(lineNumber));
        if (id.empty() || !cleanField(id, maximumIdLength) || fingerprint.empty() ||
            !cleanField(fingerprint, maximumFingerprintLength)) {
            return failure("invalid ID or source fingerprint at line " + std::to_string(lineNumber));
        }
        ModeMembership membership;
        membership.tonicOffset = tonicOffset;
        membership.mode = *mode;
        membership.usage = *usage;
        membership.usageBasis = std::move(usageBasis);
        membership.musicalEvidence = std::move(evidence);
        membership.review = *review;
        membership.reviewer = std::move(reviewer);
        membership.melodicContext = std::move(melodicContext);
        std::string validationError;
        if (!validMembership(membership, validationError)) {
            return failure(validationError + " at line " + std::to_string(lineNumber));
        }

        const auto fp = fingerprintById.find(id);
        if (fp != fingerprintById.end() && fp->second != fingerprint) {
            return failure("conflicting source fingerprints for template " + id);
        }
        fingerprintById.emplace(id, fingerprint);
        const auto key = std::to_string(id.size()) + ":" + id + ":" +
            membershipKey(tonicOffset, *mode);
        if (!membershipKeys.insert(key).second) return failure("duplicate template/tonic/mode record at line " + std::to_string(lineNumber));
        auto found = entriesById.find(id);
        if (found == entriesById.end()) {
            const auto index = result.catalog.entries.size();
            entriesById.emplace(id, index);
            result.catalog.entries.push_back({id, std::move(fingerprint), {}});
            found = entriesById.find(id);
        }
        result.catalog.entries[found->second].memberships.push_back(std::move(membership));
    }
    if (input.bad()) return failure("I/O error while parsing annotations");
    return result;
}

AnnotationLoadResult loadAnnotations(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return failure("cannot open annotation file");
    std::string text;
    text.reserve(8192);
    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            if (text.size() + static_cast<std::size_t>(count) > maximumBytes)
                return failure("annotation file exceeds 1 MiB");
            text.append(buffer.data(), static_cast<std::size_t>(count));
        }
    }
    if (input.bad()) return failure("I/O error while reading annotation file");
    return parseAnnotations(text);
}

std::string serializeAnnotations(const AnnotationCatalog& catalog) {
    std::string error;
    if (!validateCatalog(catalog, error)) throw std::invalid_argument(error);
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "HC_MODE_ANNOTATIONS 1\n";
    std::size_t outputBytes = std::string_view("HC_MODE_ANNOTATIONS 1\n").size();
    for (const auto& entry : catalog.entries) {
        for (const auto& membership : entry.memberships) {
            const char* usage = usageName(membership.usage);
            const char* review = reviewName(membership.review);
            if (!usage || !review) throw std::invalid_argument("unknown annotation enum");
            std::ostringstream row;
            row.imbue(std::locale::classic());
            row << std::quoted(entry.templateId) << ' ' << membership.tonicOffset << ' '
                << std::quoted(entry.sourceFingerprint) << ' ' << std::quoted(modeName(membership.mode)) << ' '
                << std::quoted(usage) << ' ' << std::quoted(membership.usageBasis) << ' '
                << std::quoted(membership.musicalEvidence) << ' ' << std::quoted(review) << ' '
                << std::quoted(membership.reviewer) << ' ' << std::quoted(membership.melodicContext) << '\n';
            const auto serializedRow = row.str();
            if (serializedRow.size() > maximumBytes - outputBytes)
                throw std::length_error("serialized annotations exceed 1 MiB");
            outputBytes += serializedRow.size();
            output << serializedRow;
        }
    }
    auto result = output.str();
    if (result.size() > maximumBytes) throw std::length_error("serialized annotations exceed 1 MiB");
    return result;
}

AnnotationResolution resolveAnnotations(const ProgressionTemplate& item, const AnnotationCatalog& catalog) {
    AnnotationResolution result;
    std::string error;
    if (!validateCatalog(catalog, error)) {
        result.state = AnnotationState::Invalid;
        result.diagnostic = std::move(error);
        return result;
    }
    const auto found = std::find_if(catalog.entries.begin(), catalog.entries.end(),
        [&](const ModeAnnotation& entry) { return entry.templateId == item.id; });
    if (found == catalog.entries.end()) return result;
    try {
        if (found->sourceFingerprint != sourceFingerprint(item)) {
            result.state = AnnotationState::SourceMismatch;
            result.diagnostic = "source fingerprint does not match exact template ID";
            return result;
        }
    } catch (const std::exception& exception) {
        result.state = AnnotationState::Invalid;
        result.diagnostic = std::string("cannot fingerprint source: ") + exception.what();
        return result;
    } catch (...) {
        result.state = AnnotationState::Invalid;
        result.diagnostic = "cannot fingerprint source";
        return result;
    }
    result.memberships = found->memberships;
    bool hasUnconfirmed{};
    bool hasPending{};
    for (const auto& membership : found->memberships) {
        hasUnconfirmed |= membership.review == ReviewStatus::Unconfirmed;
        hasPending |= membership.review == ReviewStatus::Pending;
    }
    result.state = hasUnconfirmed ? AnnotationState::Unconfirmed :
        (hasPending ? AnnotationState::Unreviewed : AnnotationState::Reviewed);
    return result;
}

bool strictlyCompatible(const preview::Sequence& sequence, MusicalMode mode, int tonicPitchClass) noexcept {
    if (sequence.events.empty() || tonicPitchClass < 0 || tonicPitchClass > 11) return false;
    const auto relativeMask = modePitchMask(mode);
    if (!relativeMask) return false;
    std::uint16_t absoluteMask{};
    for (int pitch = 0; pitch < 12; ++pitch) {
        if (relativeMask & (std::uint16_t{1} << pitch))
            absoluteMask |= std::uint16_t{1} << ((pitch + tonicPitchClass) % 12);
    }
    double previousStart = -1.0;
    for (const auto& event : sequence.events) {
        const auto& chord = event.chord;
        const auto pitchSet = static_cast<std::uint16_t>(chord.intervals | chord.colorMask);
        if (chord.root < 0 || chord.root > 11 || chord.bass < 0 || chord.bass > 11 ||
            !pitchSet || (pitchSet & ~std::uint16_t{0x0fff}) ||
            !std::isfinite(event.startQN) || !std::isfinite(event.durationQN) ||
            event.startQN < 0 || event.startQN < previousStart || event.durationQN <= 0)
            return false;
        previousStart = event.startQN;
        if (!(absoluteMask & (std::uint16_t{1} << chord.root)) ||
            !(absoluteMask & (std::uint16_t{1} << chord.bass))) return false;
        for (int interval = 0; interval < 12; ++interval) {
            if ((pitchSet & (std::uint16_t{1} << interval)) &&
                !(absoluteMask & (std::uint16_t{1} << ((chord.root + interval) % 12)))) return false;
        }
        if (event.topVoice) {
            if (*event.topVoice < 0 || *event.topVoice > 127 ||
                !(absoluteMask & (std::uint16_t{1} << (*event.topVoice % 12)))) return false;
        }
    }
    return true;
}

} // namespace harmony::wish
