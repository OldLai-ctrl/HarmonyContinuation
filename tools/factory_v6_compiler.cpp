#include "TemplateJson.h"
#include "benchmark/BenchJson.h"
#include "library/ProgressionLibrary.h"
#include "library/LegacyFactoryIdResolver.h"
#include "wish/ModeAnnotations.h"
#include "wish/MusicIdentity.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace {
using namespace harmony;
using JsonValue = benchmark::json::Value;
using EntriesById = std::map<std::string, ProgressionTemplate>;

void require(bool condition, const std::string& message);
EntriesById toMap(const std::vector<ProgressionTemplate>& entries);

std::string audibleSignature(const ProgressionTemplate& item) {
    const auto rendered = preview::buildTemplateSequence(
        item, {PitchClass::C, item.mode}, preview::defaultTempoBPM);
    require(static_cast<bool>(rendered), "cannot realize complete performance for " + item.id + ": " + rendered.error);
    std::ostringstream signature;
    signature.imbue(std::locale::classic());
    signature << "audible-v1:" << item.meterNumerator << '/' << item.meterDenominator << std::hexfloat;
    for (const auto& event : rendered.sequence.events) {
        const auto& chord = event.chord;
        require(chord.root >= 0 && chord.root < 12 && chord.bass >= 0 && chord.bass < 12 &&
                ((chord.intervals | chord.colorMask) & ~0xfffu) == 0,
                "invalid realized pitch set for " + item.id);
        signature << '|' << chord.root << ':' << chord.bass << ':'
                  << (chord.intervals | chord.colorMask) << '@'
                  << event.startQN << '+' << event.durationQN << '^'
                  << event.topVoice.value_or(-1);
    }
    return signature.str();
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "cannot read " + path.string());
    return {std::istreambuf_iterator<char>{file}, {}};
}

std::vector<ProgressionTemplate> readCandidates(const std::filesystem::path& path) {
    const auto parsed = dev::parseTemplateJson(readText(path), true);
    require(static_cast<bool>(parsed), path.string() + ": " + parsed.error);
    require(!parsed.templates.empty(), "V6 candidates are empty");
    std::vector<ProgressionTemplate> result = parsed.templates;
    for (const auto& item : result) {
        require(item.sourceType == "factory", "factory sourceType required: " + item.id);
        require(item.version == 6, "V6 candidate version required: " + item.id);
        require(item.full.size() >= 2, "V6 candidate needs at least two chords: " + item.id);
        require(!item.nameZh.empty() && !item.nameEn.empty() && !item.aliases.empty() &&
                !item.builtInTags.empty(), "V6 display metadata missing: " + item.id);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return result;
}

const std::string& jsonText(const JsonValue& object, std::string_view key) {
    return object.at(key).text();
}

wish::EditorialUsage readUsage(std::string_view value) {
    if (value == "high") return wish::EditorialUsage::High;
    if (value == "medium") return wish::EditorialUsage::Medium;
    if (value == "low") return wish::EditorialUsage::Low;
    if (value == "unrated") return wish::EditorialUsage::Unrated;
    throw std::runtime_error("unknown editorial usage: " + std::string(value));
}

wish::ReviewStatus readReview(std::string_view value) {
    if (value == "pending") return wish::ReviewStatus::Pending;
    if (value == "reviewed") return wish::ReviewStatus::Reviewed;
    if (value == "unconfirmed") return wish::ReviewStatus::Unconfirmed;
    throw std::runtime_error("unknown membership review status: " + std::string(value));
}

wish::AnnotationCatalog readCandidateAnnotations(
    const std::filesystem::path& metadataPath,
    const std::vector<ProgressionTemplate>& candidates,
    const std::set<std::string>& approved) {
    const auto root = benchmark::json::parse(readText(metadataPath));
    require(root.at("version").integer(6, 6) == 6, "metadata version must be 6");
    require(jsonText(root, "baseline") == "2421c6f", "metadata baseline must identify 2421c6f");
    const auto candidateById = toMap(candidates);
    wish::AnnotationCatalog result;
    std::set<std::string> metadataIds;
    std::set<std::string> reviewedIds;
    std::set<std::string> pendingIds;
    std::set<std::string> approvedByMetadata;
    const std::set<std::string> originalPendingIds{
        "V6_B1_046", "V6_B1_057", "V6_B1_059", "V6_B1_060"};
    const std::set<std::string> originalNoMembershipIds{
        "V6_B1_024", "V6_B1_026", "V6_B1_042", "V6_B1_047", "V6_B1_048",
        "V6_B1_049", "V6_B1_050", "V6_B1_053", "V6_B1_054"};
    const std::set<std::string> representativePassIds{
        "V6_B1_001", "V6_B1_011", "V6_B1_016", "V6_B1_017", "V6_B1_025",
        "V6_B1_032", "V6_B1_045", "V6_B1_050", "V6_B1_059", "V6_B1_060"};
    for (const auto& metadata : root.at("candidates").items()) {
        const auto& id = jsonText(metadata, "id");
        require(metadataIds.insert(id).second, "duplicate metadata candidate ID: " + id);
        require(candidateById.contains(id), "metadata ID is absent from candidates.json: " + id);
        const auto& status = jsonText(metadata, "status");
        if (status == "editorial-reviewed") reviewedIds.insert(id);
        else if (status == "pending") pendingIds.insert(id);
        else throw std::runtime_error("unknown candidate admission status for " + id + ": " + status);
        const auto& grade = jsonText(metadata, "grade");
        require(grade == "A" || grade == "B" || grade == "C",
                "unknown candidate grade for " + id + ": " + grade);
        const auto* initialGradeValue = metadata.find("initialGrade");
        const auto* labelActionsValue = metadata.find("labelActions");
        if (id.starts_with("V6_B1_"))
            require(initialGradeValue != nullptr,
                    "original B1 candidate needs an initialGrade review record: " + id);
        if (initialGradeValue) {
            const auto& initialGrade = initialGradeValue->text();
            require(initialGrade == "A" || initialGrade == "B" || initialGrade == "C",
                    "unknown initial candidate grade for " + id + ": " + initialGrade);
            if (grade != initialGrade)
                require(labelActionsValue && !labelActionsValue->items().empty(),
                        "changed candidate grade requires a labelActions audit record: " + id);
        }
        if (labelActionsValue) (void)labelActionsValue->items();
        const auto& adoptionStatus = jsonText(metadata, "adoptionStatus");
        const bool qualifiesForApproval = grade == "A" && status == "editorial-reviewed";
        const std::string_view expectedAdoption = qualifiesForApproval ? "approved" : "pending";
        require(adoptionStatus == expectedAdoption,
                "adoptionStatus disagrees with grade/editorial status for " + id);
        if (qualifiesForApproval) approvedByMetadata.insert(id);

        const auto& auditionStatus = jsonText(metadata, "auditionStatus");
        const bool representativePass = auditionStatus == "user-representative-pass";
        require(representativePass || auditionStatus == "pending-human",
                "unknown V6 audition status for " + id + ": " + auditionStatus);
        require(representativePass == representativePassIds.contains(id),
                "representative audition status must match the ten explicitly sampled IDs: " + id);
        if (originalPendingIds.contains(id))
            require(status == "pending" && adoptionStatus == "pending",
                    "original pending candidate was promoted: " + id);

        wish::ModeAnnotation entry;
        entry.templateId = id;
        const auto templateIt = candidateById.find(id);
        entry.sourceFingerprint = wish::sourceFingerprint(templateIt->second);
        const auto& memberships = metadata.at("memberships").items();
        if (memberships.empty() && originalNoMembershipIds.contains(id))
            require(grade == "C" && adoptionStatus == "pending",
                    "original no-membership candidate must remain grade C and unadopted: " + id);
        for (const auto& rawMembership : memberships) {
            wish::ModeMembership membership;
            membership.tonicOffset = rawMembership.at("tonicOffset").integer(0, 11);
            const auto mode = wish::parseMode(jsonText(rawMembership, "mode"));
            require(mode.has_value(), "unsupported mode token for " + id + ": " +
                    jsonText(rawMembership, "mode"));
            membership.mode = *mode;
            membership.usage = readUsage(jsonText(rawMembership, "usage"));
            membership.usageBasis = jsonText(rawMembership, "usageBasis");
            membership.musicalEvidence = jsonText(rawMembership, "musicalEvidence");
            membership.review = readReview(jsonText(rawMembership, "review"));
            membership.reviewer = jsonText(rawMembership, "reviewer");
            membership.melodicContext = jsonText(rawMembership, "melodicContext");
            std::string validationError;
            require(wish::validMembership(membership, validationError),
                    "invalid metadata membership for " + id + ": " + validationError);
            entry.memberships.push_back(std::move(membership));
        }
        if (!entry.memberships.empty()) result.entries.push_back(std::move(entry));
    }
    require(metadataIds.size() == candidateById.size(), "metadata candidate set size differs from candidates.json");
    for (const auto& [id, item] : candidateById) {
        (void)item;
        require(metadataIds.contains(id), "candidate is absent from metadata.json: " + id);
    }
    for (const auto& id : pendingIds)
        require(!approved.contains(id), "pending candidate is in approved-ids.txt: " + id);
    for (const auto& id : originalPendingIds)
        require(metadataIds.contains(id) && pendingIds.contains(id),
                "original pending candidate is missing or no longer pending: " + id);
    require(approvedByMetadata == approved,
            "approved-ids.txt must exactly equal grade-A editorial-reviewed metadata IDs");
    require(pendingIds.size() + reviewedIds.size() == candidateById.size(),
            "metadata admission status set does not cover candidates");
    return result;
}

wish::AnnotationCatalog readFrozenIncrementAnnotations(
    const std::filesystem::path& metadataPath,
    const std::vector<ProgressionTemplate>& candidates) {
    const auto root = benchmark::json::parse(readText(metadataPath));
    require(root.at("version").integer(6, 6) == 6, "increment metadata version must be 6");
    const auto candidateById = toMap(candidates);
    wish::AnnotationCatalog result;
    std::set<std::string> metadataIds;
    for (const auto& metadata : root.at("candidates").items()) {
        const auto& id = jsonText(metadata, "id");
        require(metadataIds.insert(id).second, "duplicate increment metadata ID: " + id);
        const auto candidate = candidateById.find(id);
        require(candidate != candidateById.end(), "increment metadata ID is absent from candidates: " + id);
        require(jsonText(metadata, "status") == "independent-reviewed-candidate",
                "unexpected increment admission status for " + id);
        require(jsonText(metadata, "source") == "Original primary-agent composition; not a transcription",
                "increment provenance must remain original composition for " + id);
        require(jsonText(metadata, "auditionStatus") == "pending-human",
                "increment human audition must remain pending for " + id);
        const auto& frozenFingerprint = jsonText(metadata, "sourceFingerprint");
        require(frozenFingerprint == wish::sourceFingerprint(candidate->second),
                "frozen increment source fingerprint mismatch: " + id);

        wish::ModeAnnotation entry;
        entry.templateId = id;
        entry.sourceFingerprint = frozenFingerprint;
        const auto& memberships = metadata.at("memberships").items();
        require(memberships.size() == 1, "increment must have exactly one reviewed membership: " + id);
        const auto& rawMembership = memberships.front();
        wish::ModeMembership membership;
        membership.tonicOffset = rawMembership.at("tonicOffset").integer(0, 11);
        require(membership.tonicOffset == 0, "increment Locrian tonic offset must remain 0: " + id);
        const auto mode = wish::parseMode(jsonText(rawMembership, "mode"));
        require(mode && *mode == wish::MusicalMode::Locrian,
                "increment membership must remain Locrian: " + id);
        membership.mode = *mode;
        membership.usage = readUsage(jsonText(rawMembership, "usage"));
        require(membership.usage == wish::EditorialUsage::Unrated,
                "increment usage must remain unrated: " + id);
        membership.usageBasis = jsonText(rawMembership, "usageBasis");
        membership.musicalEvidence = jsonText(rawMembership, "musicalEvidence");
        membership.review = readReview(jsonText(rawMembership, "review"));
        require(membership.review == wish::ReviewStatus::Reviewed,
                "increment Locrian membership must remain reviewed: " + id);
        membership.reviewer = jsonText(rawMembership, "reviewer");
        membership.melodicContext = jsonText(rawMembership, "melodicContext");
        std::string validationError;
        require(wish::validMembership(membership, validationError),
                "invalid increment membership for " + id + ": " + validationError);
        entry.memberships.push_back(std::move(membership));
        result.entries.push_back(std::move(entry));
    }
    require(candidates.size() == 2, "empty-modes increment must contain exactly two candidates");
    require(metadataIds.size() == candidates.size(), "increment metadata/candidate set size mismatch");
    for (const auto& [id, item] : candidateById) {
        (void)item;
        require(metadataIds.contains(id), "increment candidate is absent from metadata: " + id);
    }
    return result;
}

std::set<std::string> readApprovedIds(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "cannot read " + path.string());
    std::set<std::string> result;
    std::string line;
    std::size_t lineNumber{};
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto begin = line.find_first_not_of(" \t");
        if (begin == std::string::npos || line[begin] == '#') continue;
        const auto end = line.find_last_not_of(" \t");
        const auto id = line.substr(begin, end - begin + 1);
        require(id.find_first_of(" \t") == std::string::npos,
                "invalid approved ID at line " + std::to_string(lineNumber));
        require(result.insert(id).second, "duplicate approved ID: " + id);
    }
    require(file.eof(), "cannot finish reading " + path.string());
    return result;
}

std::filesystem::path normalized(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal();
}

bool sameFileOrPath(const std::filesystem::path& left, const std::filesystem::path& right) {
    const auto normalizedLeft = normalized(left);
    const auto normalizedRight = normalized(right);
    if (normalizedLeft == normalizedRight) return true;
    std::error_code leftError, rightError;
    if (!std::filesystem::exists(normalizedLeft, leftError) || leftError ||
        !std::filesystem::exists(normalizedRight, rightError) || rightError) return false;
    std::error_code equivalentError;
    return std::filesystem::equivalent(normalizedLeft, normalizedRight, equivalentError) &&
           !equivalentError;
}

void checkDistinctOutputs(const std::filesystem::path& baseline,
                          const std::filesystem::path& proposal,
                          const std::filesystem::path& experiment,
                          const std::filesystem::path& annotations,
                          const std::filesystem::path& baseAnnotations,
                          const std::filesystem::path& summary,
                          const std::vector<std::filesystem::path>& additionalSources = {}) {
    std::vector<std::filesystem::path> sources{baseline, baseAnnotations};
    sources.insert(sources.end(), additionalSources.begin(), additionalSources.end());
    std::vector<std::filesystem::path> outputs{proposal, experiment, annotations};
    if (!summary.empty()) outputs.push_back(summary);
    for (std::size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex)
        for (std::size_t outputIndex = 0; outputIndex < outputs.size(); ++outputIndex)
            require(!sameFileOrPath(sources[sourceIndex], outputs[outputIndex]),
                    "V6 output must not overwrite input " + sources[sourceIndex].string() +
                    ": " + outputs[outputIndex].string());
    for (std::size_t i = 0; i < outputs.size(); ++i)
        for (std::size_t j = i + 1; j < outputs.size(); ++j)
            require(!sameFileOrPath(outputs[i], outputs[j]),
                    "V6 outputs must be separate: " + outputs[i].string() +
                    " and " + outputs[j].string());
}

EntriesById toMap(const std::vector<ProgressionTemplate>& entries) {
    EntriesById result;
    for (const auto& item : entries)
        require(result.emplace(item.id, item).second, "duplicate output ID: " + item.id);
    return result;
}

void verifyRoundTrip(const std::filesystem::path& path,
                     const std::vector<ProgressionTemplate>& expected,
                     const std::vector<ProgressionTemplate>& baseline) {
    const auto loaded = library::loadFactory(path);
    require(static_cast<bool>(loaded), "database readback failed: " + loaded.error);
    require(loaded.libraryVersion == 6 && loaded.storageSchemaVersion == 2,
            "V6 Library 6 / Schema 2 contract");
    require(loaded.templates.size() == expected.size(), "database readback count mismatch");
    const auto expectedById = toMap(expected);
    const auto loadedById = toMap(loaded.templates);
    require(expectedById.size() == loadedById.size(), "database ID set size mismatch");
    for (const auto& [id, item] : expectedById) {
        const auto found = loadedById.find(id);
        require(found != loadedById.end(), "database missing ID: " + id);
        require(library::canonicalMusicalFingerprint(item) ==
                library::canonicalMusicalFingerprint(found->second),
                "database changed musical fingerprint: " + id);
        require(dev::serializeTemplateJson(item) == dev::serializeTemplateJson(found->second),
                "database changed template payload: " + id);
    }
    for (const auto& old : baseline) {
        const auto found = loadedById.find(old.id);
        require(found != loadedById.end(), "V5 content ID missing from V6: " + old.id);
        require(library::canonicalMusicalFingerprint(old) ==
                library::canonicalMusicalFingerprint(found->second),
                "V5 musical fingerprint changed: " + old.id);
        require(dev::serializeTemplateJson(old) == dev::serializeTemplateJson(found->second),
                "V5 payload changed: " + old.id);
    }
}

void writeSummary(const std::filesystem::path& path, std::size_t baselineCount,
                  std::size_t candidateCount, const std::set<std::string>& approved,
                  const std::vector<ProgressionTemplate>& candidates,
                  const wish::AnnotationCatalog& annotations,
                  std::size_t baselineAnnotationEntries) {
    std::ofstream out(path, std::ios::binary);
    require(static_cast<bool>(out), "cannot write " + path.string());
    out << "Factory V6 isolated compiler\n"
        << "library_version=6\n"
        << "storage_schema_version=2\n"
        << "baseline_library_version=5\n"
        << "baseline_schema_version=2\n"
        << "baseline_entries=" << baselineCount << '\n'
        << "proposal_entries=" << baselineCount + candidateCount << '\n'
        << "proposal_candidates=" << candidateCount << '\n'
        << "experiment_entries=" << baselineCount + approved.size() << '\n'
        << "approved_candidate_ids=" << approved.size() << '\n'
        << "unapproved_candidate_ids=" << candidateCount - approved.size() << '\n'
        << "baseline_annotation_source_ids=" << baselineAnnotationEntries << '\n'
        << "v6_annotation_source_ids=" << annotations.entries.size() << '\n'
        << "v6_annotation_memberships=" << std::accumulate(annotations.entries.begin(), annotations.entries.end(),
            std::size_t{}, [](std::size_t count, const auto& entry) { return count + entry.memberships.size(); }) << '\n'
        << "v6_candidates_without_mode_membership=" << candidateCount - annotations.entries.size() << '\n'
        << "proposal_db=isolated review only\n"
        << "experiment_db=explicit approved-ids.txt only\n"
        << "default_v5_target_and_installer=unchanged\n"
        << "\napproved IDs\n";
    for (const auto& id : approved) out << id << '\n';
    out << "\nproposal IDs\n";
    for (const auto& item : candidates) out << item.id << '\n';
    require(static_cast<bool>(out), "cannot finish writing " + path.string());
}

std::string readAnnotationPrefix(const std::filesystem::path& path,
                                const std::vector<ProgressionTemplate>& baseline) {
    const auto originalText = readText(path);
    const auto original = wish::parseAnnotations(originalText);
    require(static_cast<bool>(original), "cannot parse V5 HCA: " + original.error);
    require(originalText.ends_with('\n'), "V5 HCA must end with a newline before append");
    const auto byId = toMap(baseline);
    for (const auto& entry : original.catalog.entries) {
        const auto found = byId.find(entry.templateId);
        require(found != byId.end(), "V5 HCA contains an ID absent from the V5 library: " + entry.templateId);
        require(entry.sourceFingerprint == wish::sourceFingerprint(found->second),
                "V5 HCA source fingerprint mismatch: " + entry.templateId);
    }
    return originalText;
}

void writeMergedAnnotations(const std::filesystem::path& outputPath,
                            const std::filesystem::path& basePath,
                            const std::vector<ProgressionTemplate>& baseline,
                            const std::vector<ProgressionTemplate>& candidates,
                            const wish::AnnotationCatalog& additions) {
    const auto originalText = readAnnotationPrefix(basePath, baseline);
    const std::string header = "HC_MODE_ANNOTATIONS 1\n";
    const auto rows = wish::serializeAnnotations(additions);
    require(rows.starts_with(header), "internal HCA serializer header mismatch");
    const auto mergedText = originalText + rows.substr(header.size());
    const auto parsed = wish::parseAnnotations(mergedText);
    require(static_cast<bool>(parsed), "generated HCA failed readback: " + parsed.error);
    require(mergedText.starts_with(originalText), "V5 HCA byte prefix changed during merge");
    require(parsed.catalog.entries.size() ==
            additions.entries.size() + wish::parseAnnotations(originalText).catalog.entries.size(),
            "merged HCA source ID count mismatch");
    const auto candidateById = toMap(candidates);
    for (const auto& entry : additions.entries) {
        const auto found = std::find_if(parsed.catalog.entries.begin(), parsed.catalog.entries.end(),
            [&](const auto& candidate) { return candidate.templateId == entry.templateId; });
        require(found != parsed.catalog.entries.end(), "merged HCA missing V6 source ID: " + entry.templateId);
        require(found->sourceFingerprint == entry.sourceFingerprint &&
                wish::serializeAnnotations({1, {*found}}) == wish::serializeAnnotations({1, {entry}}),
                "merged HCA changed V6 annotation membership: " + entry.templateId);
        const auto candidate = candidateById.find(entry.templateId);
        require(candidate != candidateById.end(), "HCA ID is absent from V6 candidates: " + entry.templateId);
        const auto resolved = wish::resolveAnnotations(candidate->second, parsed.catalog);
        require(resolved.state != wish::AnnotationState::SourceMismatch &&
                resolved.state != wish::AnnotationState::Invalid,
                "merged HCA source binding failed: " + entry.templateId);
    }
    std::ofstream output(outputPath, std::ios::binary);
    require(static_cast<bool>(output), "cannot write " + outputPath.string());
    output.write(mergedText.data(), static_cast<std::streamsize>(mergedText.size()));
    require(static_cast<bool>(output), "cannot finish writing " + outputPath.string());
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 9 && argc != 10 && argc != 12) {
        std::cerr << "usage: factory_v6_compiler factory-v5.db candidates.json metadata.json approved-ids.txt "
                     "proposal.db experiment.db factory-v5.hca merged-v6.hca [summary.txt "
                     "increment-candidates.json increment-metadata.json]\n";
        return 2;
    }
    try {
        const std::filesystem::path baselinePath = argv[1];
        const std::filesystem::path candidatesPath = argv[2];
        const std::filesystem::path metadataPath = argv[3];
        const std::filesystem::path approvedPath = argv[4];
        const std::filesystem::path proposalPath = argv[5];
        const std::filesystem::path experimentPath = argv[6];
        const std::filesystem::path baseAnnotationsPath = argv[7];
        const std::filesystem::path annotationsPath = argv[8];
        const std::filesystem::path summaryPath = argc >= 10 ? argv[9] : std::filesystem::path{};
        const bool hasIncrement = argc == 12;
        const std::filesystem::path incrementCandidatesPath = hasIncrement ? argv[10] : std::filesystem::path{};
        const std::filesystem::path incrementMetadataPath = hasIncrement ? argv[11] : std::filesystem::path{};
        std::vector<std::filesystem::path> additionalSources{candidatesPath, metadataPath, approvedPath};
        if (hasIncrement) {
            additionalSources.push_back(incrementCandidatesPath);
            additionalSources.push_back(incrementMetadataPath);
        }
        checkDistinctOutputs(baselinePath, proposalPath, experimentPath, annotationsPath,
                            baseAnnotationsPath, summaryPath, additionalSources);

        const auto baseline = library::loadFactory(baselinePath);
        require(static_cast<bool>(baseline), "cannot load V5 baseline: " + baseline.error);
        require(baseline.libraryVersion == 5 && baseline.storageSchemaVersion == 2 &&
                baseline.templates.size() == 658,
                "expected Factory V5 / Schema 2 / 658-entry baseline");

        auto candidates = readCandidates(candidatesPath);
        auto approved = readApprovedIds(approvedPath);
        auto annotations = readCandidateAnnotations(metadataPath, candidates, approved);
        if (hasIncrement) {
            auto incrementCandidates = readCandidates(incrementCandidatesPath);
            auto incrementAnnotations = readFrozenIncrementAnnotations(incrementMetadataPath, incrementCandidates);
            for (const auto& item : incrementCandidates) {
                require(!approved.contains(item.id),
                        "increment ID collides with an existing approved candidate: " + item.id);
                candidates.push_back(item);
                approved.insert(item.id);
            }
            annotations.entries.insert(annotations.entries.end(), incrementAnnotations.entries.begin(),
                                       incrementAnnotations.entries.end());
        }
        EntriesById allIds = toMap(baseline.templates);
        std::set<std::string> namesZh, namesEn, musicalFingerprints;
        for (const auto& item : baseline.templates) {
            namesZh.insert(item.nameZh);
            namesEn.insert(item.nameEn);
            musicalFingerprints.insert(audibleSignature(item));
        }
        for (const auto& item : candidates) {
            require(allIds.emplace(item.id, item).second, "V6 ID collides with baseline or candidate: " + item.id);
            require(namesZh.insert(item.nameZh).second, "duplicate Chinese display name: " + item.id);
            require(namesEn.insert(item.nameEn).second, "duplicate English display name: " + item.id);
            require(musicalFingerprints.insert(audibleSignature(item)).second,
                    "exact complete-performance duplicate: " + item.id);
        }
        for (const auto& id : approved)
            require(std::any_of(candidates.begin(), candidates.end(),
                [&](const auto& item) { return item.id == id; }),
                "approved ID is absent from candidates.json: " + id);

        std::vector<ProgressionTemplate> proposalEntries;
        proposalEntries.reserve(baseline.templates.size() + candidates.size());
        proposalEntries.insert(proposalEntries.end(), baseline.templates.begin(), baseline.templates.end());
        proposalEntries.insert(proposalEntries.end(), candidates.begin(), candidates.end());
        std::sort(proposalEntries.begin(), proposalEntries.end(),
                  [](const auto& a, const auto& b) { return a.id < b.id; });

        std::vector<ProgressionTemplate> experimentEntries;
        experimentEntries.reserve(baseline.templates.size() + approved.size());
        experimentEntries.insert(experimentEntries.end(), baseline.templates.begin(), baseline.templates.end());
        for (const auto& item : candidates)
            if (approved.contains(item.id)) experimentEntries.push_back(item);
        std::sort(experimentEntries.begin(), experimentEntries.end(),
                  [](const auto& a, const auto& b) { return a.id < b.id; });

        std::string error;
        require(library::compileFactory(proposalPath, proposalEntries, error, 6),
                "proposal database compile failed: " + error);
        error.clear();
        require(library::compileFactory(experimentPath, experimentEntries, error, 6),
                "experiment database compile failed: " + error);
        const auto baselineAnnotations = wish::parseAnnotations(readText(baseAnnotationsPath));
        require(static_cast<bool>(baselineAnnotations), "cannot parse V5 HCA: " + baselineAnnotations.error);
        writeMergedAnnotations(annotationsPath, baseAnnotationsPath, baseline.templates,
                               candidates, annotations);
        verifyRoundTrip(proposalPath, proposalEntries, baseline.templates);
        verifyRoundTrip(experimentPath, experimentEntries, baseline.templates);
        if (!summaryPath.empty())
            writeSummary(summaryPath, baseline.templates.size(), candidates.size(), approved, candidates,
                         annotations, baselineAnnotations.catalog.entries.size());

        std::cout << "Factory V6 PASS library_version=6 schema=2"
                  << " baseline=" << baseline.templates.size()
                  << " candidates=" << candidates.size()
                  << " proposal=" << proposalEntries.size()
                  << " approved=" << approved.size()
                  << " experiment=" << experimentEntries.size()
                  << " unapproved=" << candidates.size() - approved.size()
                  << " hca_v6_sources=" << annotations.entries.size() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "factory V6 compile FAIL: " << e.what() << '\n';
        return 1;
    }
}
