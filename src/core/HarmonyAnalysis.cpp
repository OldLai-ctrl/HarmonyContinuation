#include "HarmonyAnalysis.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <sstream>

namespace harmony {
namespace {
int number(PitchClass pitch) { return static_cast<int>(pitch); }
int mod12(int value) { return (value % 12 + 12) % 12; }
constexpr std::array<int, 7> majorScale{0, 2, 4, 5, 7, 9, 11};
constexpr std::array<int, 7> minorScale{0, 2, 3, 5, 7, 8, 10};
const std::array<int, 7>& scale(Mode mode) { return mode == Mode::Major ? majorScale : minorScale; }

std::optional<ScaleDegree> degreeOf(PitchClass pitch, KeySignature key) {
    const auto offset = mod12(number(pitch) - number(key.tonic));
    const auto& notes = scale(key.mode);
    for (int i = 0; i < 7; ++i) if (notes[i] == offset) return ScaleDegree{i + 1, 0};
    // At this stage only a one-semitone chromatic alteration is represented.
    // Prefer flats for ambiguous roots; secondary roles retain their own target.
    for (int i = 0; i < 7; ++i)
        if (mod12(notes[i] - 1) == offset) return ScaleDegree{i + 1, -1};
    for (int i = 0; i < 7; ++i)
        if (mod12(notes[i] + 1) == offset) return ScaleDegree{i + 1, 1};
    return std::nullopt;
}

bool minorQuality(ChordQuality q) {
    return q == ChordQuality::Minor || q == ChordQuality::Minor7;
}
bool majorQuality(ChordQuality q) {
    return q == ChordQuality::Major || q == ChordQuality::Major7;
}
bool dominantQuality(ChordQuality q) {
    return q == ChordQuality::Major || q == ChordQuality::Dominant7;
}
bool diminishedQuality(ChordQuality q) {
    return q == ChordQuality::Diminished || q == ChordQuality::Diminished7 ||
           q == ChordQuality::HalfDiminished7;
}
ChordQuality expectedQuality(KeySignature key, int degree) {
    if (key.mode == Mode::Major) {
        constexpr std::array<ChordQuality, 7> qualities{
            ChordQuality::Major, ChordQuality::Minor, ChordQuality::Minor,
            ChordQuality::Major, ChordQuality::Major, ChordQuality::Minor,
            ChordQuality::Diminished};
        return qualities[degree - 1];
    }
    constexpr std::array<ChordQuality, 7> qualities{
        ChordQuality::Minor, ChordQuality::Diminished, ChordQuality::Major,
        ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major,
        ChordQuality::Major};
    return qualities[degree - 1];
}

bool diatonicQuality(ChordQuality q, KeySignature key, int degree) {
    if (q == ChordQuality::Unknown) return false;
    if (key.mode == Mode::Major && degree == 5) return dominantQuality(q);
    if (key.mode == Mode::Major && degree == 7) return diminishedQuality(q);
    if (key.mode == Mode::Minor && degree == 2) return diminishedQuality(q);
    if (key.mode == Mode::Minor && degree == 5) return minorQuality(q) || dominantQuality(q);
    const auto expected = expectedQuality(key, degree);
    if (expected == ChordQuality::Major) return majorQuality(q);
    if (expected == ChordQuality::Minor) return minorQuality(q);
    return diminishedQuality(q);
}

bool borrowed(const NormalizedChord& chord, KeySignature key, ScaleDegree degree) {
    if (key.mode != Mode::Major) return false;
    if (degree.degree == 4 && degree.alteration == 0 && minorQuality(chord.quality)) return true;
    if (degree.alteration == -1 && majorQuality(chord.quality))
        return degree.degree == 3 || degree.degree == 6 || degree.degree == 7;
    return false;
}

std::optional<ScaleDegree> secondaryTarget(const std::vector<NormalizedChord>& chords,
                                           std::size_t i, KeySignature key, bool leadingTone) {
    if (i + 1 >= chords.size() || !chords[i].root || !chords[i + 1].root) return std::nullopt;
    const auto target = degreeOf(*chords[i + 1].root, key);
    if (!target || target->alteration != 0 || target->degree == 1) return std::nullopt;
    if (!diatonicQuality(chords[i + 1].quality, key, target->degree)) return std::nullopt;
    const auto interval = mod12(number(*chords[i].root) - number(*chords[i + 1].root));
    const auto sourceDegree = degreeOf(*chords[i].root, key);
    if (sourceDegree && sourceDegree->alteration == 0 &&
        diatonicQuality(chords[i].quality, key, sourceDegree->degree)) return std::nullopt;
    if (leadingTone ? (diminishedQuality(chords[i].quality) && interval == 11)
                    : (dominantQuality(chords[i].quality) && interval == 7)) return target;
    return std::nullopt;
}

bool primaryDominant(const NormalizedChord& chord, KeySignature key) {
    return chord.root && mod12(number(*chord.root) - number(key.tonic)) == 7 &&
           dominantQuality(chord.quality);
}
bool tonic(const NormalizedChord& chord, KeySignature key) {
    return chord.root && *chord.root == key.tonic &&
           (key.mode == Mode::Major ? majorQuality(chord.quality) : minorQuality(chord.quality));
}

float medianDuration(const Progression& events) {
    std::vector<float> durations;
    for (const auto& event : events)
        if (event.durationQN && std::isfinite(*event.durationQN) && *event.durationQN > 0)
            durations.push_back(static_cast<float>(*event.durationQN));
    if (durations.empty()) return 2.0f;
    std::sort(durations.begin(), durations.end());
    return durations[durations.size() / 2];
}

float durationFactor(const ChordEvent& event, float median) {
    if (!event.durationQN || !std::isfinite(*event.durationQN) || *event.durationQN <= 0) return 1.0f;
    return std::clamp(static_cast<float>(*event.durationQN) / median, 0.5f, 1.5f);
}

float scoreKey(const Progression& events, const std::vector<NormalizedChord>& chords,
               KeySignature key, const HarmonyAnalysisWeights& w, float median) {
    float score{};
    for (std::size_t i = 0; i < chords.size(); ++i) {
        const auto& chord = chords[i];
        if (!chord.root) { score += w.chromaticPenalty; continue; }
        const auto degree = degreeOf(*chord.root, key);
        float evidence{};
        if (degree && degree->alteration == 0 && diatonicQuality(chord.quality, key, degree->degree))
            evidence = w.exactDiatonic;
        else if (degree && borrowed(chord, key, *degree)) evidence = w.borrowedChord;
        else if (secondaryTarget(chords, i, key, false)) evidence = w.secondaryDominant;
        else if (secondaryTarget(chords, i, key, true)) evidence = w.secondaryLeadingTone;
        else if (degree && degree->alteration == 0) {
            evidence = chord.quality == ChordQuality::Unknown ? w.inScaleRoot : w.compatibleDiatonic * 0.35f;
        } else evidence = w.chromaticPenalty;
        evidence *= durationFactor(events[i], median);
        score += evidence;
        if (tonic(chord, key)) {
            score += w.tonicEvidence * 0.5f;
            if (i == 0) score += w.firstTonic;
            if (i + 1 == chords.size() && !events[i].openEnded) score += w.lastClosedTonic;
        }
        if (i + 1 < chords.size() && primaryDominant(chord, key) && tonic(chords[i + 1], key))
            score += w.dominantResolution;
        if (i + 2 < chords.size() && degree && degree->degree == 2 && degree->alteration == 0 &&
            primaryDominant(chords[i + 1], key) && tonic(chords[i + 2], key)) score += w.cadence;
    }
    return score / std::max(1.0f, static_cast<float>(chords.size()));
}

std::vector<KeyCandidate> candidateKeys(const Progression& events,
                                        const std::vector<NormalizedChord>& chords,
                                        const HarmonyAnalysisWeights& weights, float median) {
    std::vector<KeyCandidate> result;
    result.reserve(24);
    for (int root = 0; root < 12; ++root)
        for (Mode mode : {Mode::Major, Mode::Minor}) {
            const KeySignature key{static_cast<PitchClass>(root), mode};
            result.push_back({key, scoreKey(events, chords, key, weights, median), 0});
        }
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });
    const float best = result.front().score;
    float total{};
    for (auto& candidate : result) {
        candidate.confidence = std::exp((candidate.score - best) * 2.0f);
        total += candidate.confidence;
    }
    for (auto& candidate : result) candidate.confidence /= total;
    return result;
}

HarmonicFunction baseFunction(const HarmonicEvent& event, KeySignature key) {
    if (!event.degree || !event.chord.root) return HarmonicFunction::Unknown;
    if (hasRole(event.roles, Role::SecondaryDominant) || hasRole(event.roles, Role::SecondaryLeadingTone))
        return HarmonicFunction::ChromaticDominant;
    if (hasRole(event.roles, Role::Borrowed))
        return event.degree->degree == 4 ? HarmonicFunction::ChromaticPredominant
                                         : HarmonicFunction::Unknown;
    if (event.degree->alteration != 0) return HarmonicFunction::Unknown;
    const auto d = event.degree->degree;
    if (!diatonicQuality(event.chord.quality, key, d)) return HarmonicFunction::Unknown;
    if (d == 1) return HarmonicFunction::Tonic;
    if (d == 3 || d == 6) return HarmonicFunction::TonicProlongation;
    if (d == 2 || d == 4) return HarmonicFunction::Predominant;
    if (d == 5 || d == 7) return HarmonicFunction::Dominant;
    return HarmonicFunction::Unknown;
}

float structuralWeight(const ChordEvent& source, const HarmonicEvent& event,
                       std::size_t index, std::size_t count, float median,
                       const AnalysisContext& context) {
    const auto& w = context.weights;
    float weight = w.structuralBase;
    const bool shortChord = source.durationQN && *source.durationQN > 0 &&
                            *source.durationQN < median * w.shortDurationRatio;
    if (source.durationQN && *source.durationQN > 0 && std::isfinite(*source.durationQN))
        weight += std::clamp(std::log2(static_cast<float>(*source.durationQN) / median), -1.0f, 1.0f) * w.relativeDurationEffect;
    if (event.function == HarmonicFunction::Tonic) weight += w.structuralTonicEffect;
    else if (event.function != HarmonicFunction::Unknown) weight += w.structuralFunctionEffect;
    else weight -= w.structuralUnknownPenalty;
    if (index == 0 || index + 1 == count) weight += w.structuralEndpointEffect;
    if (hasRole(event.roles, Role::Cadential)) weight += w.structuralCadenceEffect;
    if (hasRole(event.roles, Role::Borrowed)) weight += w.structuralBorrowedEffect;
    if (shortChord && hasRole(event.roles, Role::SecondaryDominant)) weight -= w.shortSecondaryDominantPenalty;
    if (shortChord && hasRole(event.roles, Role::SecondaryLeadingTone)) weight -= w.shortSecondaryLeadingPenalty;
    if (shortChord && hasRole(event.roles, Role::Approach)) weight -= w.shortApproachPenalty;
    if (context.timeSigNumerator && context.timeSigDenominator &&
        *context.timeSigNumerator > 0 && *context.timeSigDenominator > 0) {
        const double barQN = *context.timeSigNumerator * 4.0 / *context.timeSigDenominator;
        if (barQN > 0 && std::remainder(source.startQN, barQN) == 0) weight += w.barStartEffect;
    }
    return std::clamp(weight, 0.0f, 1.0f);
}

std::string roman(ScaleDegree degree, ChordQuality quality) {
    constexpr std::array<const char*, 7> upper{"I", "II", "III", "IV", "V", "VI", "VII"};
    constexpr std::array<const char*, 7> lower{"i", "ii", "iii", "iv", "v", "vi", "vii"};
    if (degree.degree < 1 || degree.degree > 7) return "?";
    std::string text;
    if (degree.alteration < 0) text.append(static_cast<std::size_t>(-degree.alteration), 'b');
    if (degree.alteration > 0) text.append(static_cast<std::size_t>(degree.alteration), '#');
    text += (minorQuality(quality) || diminishedQuality(quality)) ? lower[degree.degree - 1]
                                                           : upper[degree.degree - 1];
    if (diminishedQuality(quality)) text += quality == ChordQuality::HalfDiminished7 ? "ø" : "°";
    if (quality == ChordQuality::Major7) text += "maj7";
    else if (quality == ChordQuality::Minor7 || quality == ChordQuality::Dominant7 ||
             quality == ChordQuality::Diminished7 || quality == ChordQuality::HalfDiminished7) text += '7';
    else if (quality == ChordQuality::Sus2) text += "sus2";
    else if (quality == ChordQuality::Sus4) text += "sus4";
    else if (quality == ChordQuality::Augmented) text += '+';
    return text;
}

std::string targetRoman(ScaleDegree degree, ChordQuality quality) {
    if (diminishedQuality(quality)) return roman(degree, ChordQuality::Diminished);
    return roman(degree, minorQuality(quality) ? ChordQuality::Minor : ChordQuality::Major);
}
} // namespace

HarmonicAnalysisResult analyzeHarmony(const Progression& events, const AnalysisContext& context) {
    HarmonicAnalysisResult result;
    if (events.empty()) { result.warnings.emplace_back("empty progression"); return result; }
    if (!std::is_sorted(events.begin(), events.end(), [](const auto& a, const auto& b) {
            return a.startQN < b.startQN;
        })) { result.warnings.emplace_back("events must be sorted by startQN"); return result; }
    std::vector<NormalizedChord> chords;
    chords.reserve(events.size());
    for (const auto& event : events) {
        if (!std::isfinite(event.startQN)) {
            result.warnings.emplace_back("non-finite event position"); return result;
        }
        chords.push_back(normalizeChord(event));
    }
    const auto median = medianDuration(events);
    result.keyCandidates = candidateKeys(events, chords, context.weights, median);
    if (context.forcedKey) {
        const auto found = std::find_if(result.keyCandidates.begin(), result.keyCandidates.end(), [&](const auto& candidate) {
            return candidate.key.tonic == context.forcedKey->tonic && candidate.key.mode == context.forcedKey->mode;
        });
        if (found != result.keyCandidates.end()) result.selectedKey = *found;
    } else result.selectedKey = result.keyCandidates.front();
    const auto key = result.selectedKey->key;
    result.full.reserve(chords.size());
    for (std::size_t i = 0; i < chords.size(); ++i) {
        HarmonicEvent harmonic;
        harmonic.chord = std::move(chords[i]);
        harmonic.sourceChordIndex = i;
        if (harmonic.chord.root) harmonic.degree = degreeOf(*harmonic.chord.root, key);
        if (harmonic.chord.inversion) harmonic.roles |= flag(Role::Inversion);
        if (harmonic.chord.colorMask) harmonic.roles |= flag(Role::Extension);
        result.full.push_back(std::move(harmonic));
    }
    // Role decisions use the selected key and the actual following chord.
    std::vector<NormalizedChord> normalized;
    normalized.reserve(result.full.size());
    for (const auto& event : result.full) normalized.push_back(event.chord);
    for (std::size_t i = 0; i < result.full.size(); ++i) {
        auto& event = result.full[i];
        if (event.degree && borrowed(event.chord, key, *event.degree)) event.roles |= flag(Role::Borrowed);
        if (const auto target = secondaryTarget(normalized, i, key, false)) {
            event.roles |= flag(Role::SecondaryDominant);
            event.target = target;
            event.targetQuality = normalized[i + 1].quality;
        } else if (const auto leadingTarget = secondaryTarget(normalized, i, key, true)) {
            event.roles |= flag(Role::SecondaryLeadingTone);
            event.target = leadingTarget;
            event.targetQuality = normalized[i + 1].quality;
        }
        if (i + 1 < events.size() && event.target && events[i].durationQN &&
            *events[i].durationQN < median * context.weights.shortDurationRatio)
            event.roles |= flag(Role::Approach);
        event.function = baseFunction(event, key);
    }
    for (std::size_t i = 0; i + 1 < result.full.size(); ++i) {
        if (primaryDominant(result.full[i].chord, key) && tonic(result.full[i + 1].chord, key)) {
            result.full[i].roles |= flag(Role::Cadential);
            result.full[i + 1].roles |= flag(Role::Cadential);
            if (i > 0 && result.full[i - 1].degree && result.full[i - 1].degree->degree == 2 &&
                result.full[i - 1].degree->alteration == 0)
                result.full[i - 1].roles |= flag(Role::Cadential);
        }
    }
    float confidenceSum{};
    for (std::size_t i = 0; i < result.full.size(); ++i) {
        auto& event = result.full[i];
        event.structuralWeight = structuralWeight(events[i], event, i, events.size(), median, context);
        event.analysisConfidence = event.chord.confidence;
        if (event.function == HarmonicFunction::Unknown) event.analysisConfidence *= 0.48f;
        if (!event.chord.diagnostics.empty()) {
            result.warnings.push_back("chord " + std::to_string(i) + ": " + event.chord.diagnostics.front());
        }
        confidenceSum += event.analysisConfidence;
        // Preserve endpoints and cadence-bearing functional links. A short
        // chromatic approach can disappear without erasing its target.
        if (i == 0 || i + 1 == events.size() || hasRole(event.roles, Role::Cadential) ||
            event.structuralWeight >= context.skeletonThreshold) {
            event.roles |= flag(Role::Structural);
            result.skeletonIndices.push_back(i);
        } else event.roles |= flag(Role::Embellishing);
    }
    const auto keyCertainty = context.forcedKey ? 1.0f : std::clamp(result.keyCandidates.front().confidence * 3.0f, 0.0f, 1.0f);
    result.overallConfidence = keyCertainty * confidenceSum / static_cast<float>(events.size());
    if (!context.forcedKey && result.keyCandidates.front().confidence < 0.35f)
        result.warnings.emplace_back("key is ambiguous; review alternate candidates");
    return result;
}

std::string formatKey(KeySignature key) {
    constexpr std::array<const char*, 12> names{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    return std::string(names[number(key.tonic)]) + (key.mode == Mode::Major ? " Major" : " Minor");
}

std::string formatDegree(const HarmonicEvent& event) {
    if (event.target && hasRole(event.roles, Role::SecondaryDominant))
        return std::string(event.chord.quality == ChordQuality::Dominant7 ? "V7/" : "V/") +
               targetRoman(*event.target, event.targetQuality.value_or(ChordQuality::Unknown));
    if (event.target && hasRole(event.roles, Role::SecondaryLeadingTone))
        return std::string(event.chord.quality == ChordQuality::Diminished7 ? "vii°7/" : "vii°/") +
               targetRoman(*event.target, event.targetQuality.value_or(ChordQuality::Unknown));
    return event.degree ? roman(*event.degree, event.chord.quality) : "?";
}

std::string formatFunction(HarmonicFunction function) {
    switch (function) {
        case HarmonicFunction::Tonic: return "T";
        case HarmonicFunction::TonicProlongation: return "T-prol";
        case HarmonicFunction::Predominant: return "PD";
        case HarmonicFunction::Dominant: return "D";
        case HarmonicFunction::ChromaticPredominant: return "Chr-PD";
        case HarmonicFunction::ChromaticDominant: return "Chr-D";
        default: return "?";
    }
}

} // namespace harmony
