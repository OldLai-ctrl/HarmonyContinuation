#pragma once

#include "core/ChordPitchSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace harmony::enrichment {

enum class EnrichmentProvenanceKind { Original, Modified, Replaced, Inserted, Unknown };

struct EnrichmentProvenanceMark {
    std::size_t candidateIndex{};
    std::optional<std::size_t> sourceIndex;
    EnrichmentProvenanceKind kind{EnrichmentProvenanceKind::Unknown};
};

namespace enrichment_provenance_detail {
inline bool orderedOnsets(const Progression& progression, double epsilon) {
    for (std::size_t i = 0; i < progression.size(); ++i) {
        if (!std::isfinite(progression[i].startQN)) return false;
        if (i != 0 && progression[i].startQN - progression[i - 1].startQN <= epsilon) return false;
    }
    return true;
}

inline std::vector<EnrichmentProvenanceMark> unknownMarks(std::size_t count) {
    std::vector<EnrichmentProvenanceMark> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) result.push_back({i, std::nullopt, EnrichmentProvenanceKind::Unknown});
    return result;
}

inline bool validChord(const ChordPitchSet& chord) {
    const auto intervals = static_cast<unsigned>(chord.intervals);
    const auto colors = static_cast<unsigned>(chord.colorMask);
    return chord.root >= 0 && chord.root < 12 && chord.bass >= 0 && chord.bass < 12 &&
        (intervals & 0xf000u) == 0 && (colors & 0xf000u) == 0 &&
        ((intervals | colors) & 0x0fffu & 1u) != 0 && ((intervals | colors) & 0x0fffu) != 0;
}

inline unsigned absolutePitchSet(const ChordPitchSet& chord) {
    const auto intervals = static_cast<unsigned>(chord.intervals | chord.colorMask);
    unsigned result = 1u << chord.bass;
    for (int relative = 0; relative < 12; ++relative)
        if ((intervals & (1u << relative)) != 0) result |= 1u << ((chord.root + relative) % 12);
    return result;
}

inline unsigned definingThirdAndFifth(const ChordPitchSet& chord) {
    const auto intervals = static_cast<unsigned>(chord.intervals | chord.colorMask);
    const auto fifth=(intervals&(1u<<7))?(1u<<7):intervals&((1u<<6)|(1u<<8));
    return (intervals&((1u<<3)|(1u<<4)))|fifth;
}
} // namespace enrichment_provenance_detail

// Maps route events by their stable onset. This is deliberately independent of
// matcher alignment and enrichment operation indices, which are candidate-relative.
inline std::vector<EnrichmentProvenanceMark> enrichmentProvenance(
    const Progression& source, const Progression& candidate, double epsilon = 1e-7) {
    using namespace enrichment_provenance_detail;
    if (!std::isfinite(epsilon) || epsilon < 0.0 || source.empty() ||
        !orderedOnsets(source, epsilon) || !orderedOnsets(candidate, epsilon))
        return unknownMarks(candidate.size());

    std::vector<std::optional<std::size_t>> candidateSources(candidate.size());
    for (std::size_t sourceIndex = 0; sourceIndex < source.size(); ++sourceIndex) {
        const auto onset = source[sourceIndex].startQN;
        const auto lower = std::lower_bound(candidate.begin(), candidate.end(), onset - epsilon,
            [](const ChordEvent& event, double qn) { return event.startQN < qn; });
        const auto upper = std::upper_bound(candidate.begin(), candidate.end(), onset + epsilon,
            [](double qn, const ChordEvent& event) { return qn < event.startQN; });
        if (upper - lower != 1) return unknownMarks(candidate.size());
        const auto hit = static_cast<std::size_t>(lower - candidate.begin());
        if (candidateSources[hit]) return unknownMarks(candidate.size());
        candidateSources[hit] = sourceIndex;
    }

    std::vector<EnrichmentProvenanceMark> result;
    result.reserve(candidate.size());
    for (std::size_t candidateIndex = 0; candidateIndex < candidate.size(); ++candidateIndex) {
        if (!candidateSources[candidateIndex]) {
            const auto right = std::upper_bound(source.begin(), source.end(), candidate[candidateIndex].startQN,
                [](double qn, const ChordEvent& event) { return qn < event.startQN; });
            const bool internal = right != source.begin() && right != source.end() &&
                candidate[candidateIndex].startQN > (right - 1)->startQN + epsilon &&
                candidate[candidateIndex].startQN < right->startQN - epsilon;
            result.push_back({candidateIndex, std::nullopt,
                internal ? EnrichmentProvenanceKind::Inserted : EnrichmentProvenanceKind::Unknown});
            continue;
        }

        const auto sourceIndex = *candidateSources[candidateIndex];
        const auto before = chordPitches(source[sourceIndex]);
        const auto after = chordPitches(candidate[candidateIndex]);
        auto kind = EnrichmentProvenanceKind::Unknown;
        if (validChord(before) && validChord(after)) {
            if (before.root == after.root && before.bass == after.bass &&
                absolutePitchSet(before) == absolutePitchSet(after)) {
                kind = EnrichmentProvenanceKind::Original;
            } else if (before.root != after.root ||
                definingThirdAndFifth(before) != definingThirdAndFifth(after)) {
                kind = EnrichmentProvenanceKind::Replaced;
            } else {
                kind = EnrichmentProvenanceKind::Modified;
            }
        }
        result.push_back({candidateIndex, sourceIndex, kind});
    }
    return result;
}

} // namespace harmony::enrichment
