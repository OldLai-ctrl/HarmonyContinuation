#include "ContinuationEngine.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace harmony {
namespace {
const KeyInterpretation* interpretationFor(const MatchQuery& query, KeySignature key) {
    for (const auto& candidate : query.interpretations)
        if (candidate.key.key.tonic == key.tonic && candidate.key.key.mode == key.mode) return &candidate;
    return nullptr;
}
int positiveMod(int x, int divisor) { return (x % divisor + divisor) % divisor; }
float clampScore(float x) { return std::clamp(x, 0.f, 1.f); }
bool higherRank(const ContinuationCandidate& a, const ContinuationCandidate& b) {
    // Sub-millipoint floating-point noise is not a musical preference. Use a
    // transitive score bucket, then expose the longer complete option on ties.
    const auto aScore = std::round(a.rankingScore * 1000.f);
    const auto bScore = std::round(b.rankingScore * 1000.f);
    if (aScore != bScore) return aScore > bScore;
    if (a.continuation.size() != b.continuation.size())
        return a.continuation.size() > b.continuation.size();
    return a.id < b.id;
}
std::string suffixFingerprint(const ContinuationCandidate& candidate, bool coarse) {
    std::ostringstream out;
    out << static_cast<int>(candidate.key.tonic) << ':' << static_cast<int>(candidate.key.mode) << ':'
        << static_cast<int>(candidate.intent) << ':';
    if (!coarse) {
        out << static_cast<int>(candidate.cadence) << ':';
        if (candidate.suggestedCurrentChordDurationQN)
            out << std::round(*candidate.suggestedCurrentChordDurationQN * 4.0) / 4.0 << ':';
    }
    double average{};
    for (const auto& event : candidate.continuation) average += event.durationQN;
    average /= std::max<std::size_t>(1, candidate.continuation.size());
    for (const auto& event : candidate.continuation) {
        if (coarse) out << event.degree.degree << '/' << event.degree.alteration << ':'
                        << (event.durationQN / std::max(0.001, average) < 0.7 ? 0 :
                            event.durationQN / std::max(0.001, average) > 1.4 ? 2 : 1);
        else out << event.label << '@' << std::round(event.durationQN * 4.0) / 4.0;
        out << '|';
        // Bass and exact colors define some V4 phrases, including diversity identity.
        if (event.harmonicData) {
            const auto tones=chordPitches(*event.harmonicData);
            out << tones.bass << ':' << tones.intervals << '|';
        }
    }
    return out.str();
}
PhraseIntent effectiveIntent(const ProgressionTemplate& item, const std::vector<MatchEvent>& suffix) {
    if (item.intent != PhraseIntent::Neutral) return item.intent;
    if (item.loopable) return PhraseIntent::Loop;
    for (const auto& event : suffix) if (hasRole(event.roles, Role::Borrowed) ||
        hasRole(event.roles, Role::SecondaryLeadingTone)) return PhraseIntent::Color;
    if (item.cadence == CadenceType::Authentic || item.cadence == CadenceType::PerfectAuthentic ||
        item.cadence == CadenceType::Plagal) return PhraseIntent::Resolve;
    return PhraseIntent::Develop;
}
ChordQuality harmonicCore(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major7: case ChordQuality::Dominant7: return ChordQuality::Major;
        case ChordQuality::Minor7: return ChordQuality::Minor;
        case ChordQuality::Diminished7: case ChordQuality::HalfDiminished7: return ChordQuality::Diminished;
        default: return quality;
    }
}
bool preservesLoopContext(const KeyInterpretation& input, const MatchResult& match,
                          const ProgressionTemplate& item) {
    // A catalogue loop certifies its own cycle, including the wrap boundary.
    // Keep both original cycle boundaries. An internal, recognized borrowed
    // major/minor variant may retain the loop; a gap or changed boundary cannot
    // inherit that evidence. No tonic ending or V-I is required.
    std::size_t paired{};
    for (const auto& step : match.alignmentTrace) {
        if (!step.queryIndex || !step.templateIndex ||
            (step.operation != AlignmentOp::Match && step.operation != AlignmentOp::Substitute)) return false;
        if (*step.queryIndex >= input.full.size() || *step.templateIndex >= item.full.size()) return false;
        const auto& actual = input.full[*step.queryIndex];
        const auto& expected = item.full[*step.templateIndex];
        if (!actual.degree || !expected.degree ||
            actual.degree->degree != expected.degree->degree ||
            actual.degree->alteration != expected.degree->alteration ||
            actual.quality == ChordQuality::Unknown) return false;
        if (harmonicCore(actual.quality) != harmonicCore(expected.quality)) {
            const auto a=harmonicCore(actual.quality),b=harmonicCore(expected.quality);
            const bool borrowedCore=(a==ChordQuality::Major&&b==ChordQuality::Minor)||
                                    (a==ChordQuality::Minor&&b==ChordQuality::Major);
            if (*step.queryIndex==0 || *step.queryIndex+1==input.full.size() || !borrowedCore ||
                !hasRole(actual.roles,Role::Borrowed) || !hasReason(step.reasons,MatchReason::BorrowedVariant)) return false;
        }
        ++paired;
    }
    return paired == input.full.size();
}
std::size_t groupIndex(PhraseIntent intent) {
    switch (intent) {
        case PhraseIntent::Resolve: return 0; case PhraseIntent::Loop: return 2;
        case PhraseIntent::Color: return 3; default: return 1;
    }
}
float styleScore(const ProgressionTemplate& item, std::optional<Style> hint, bool exactHarmony) {
    if (!hint) return 0.5f;
    for (const auto& [style, weight] : item.styleWeights) if (style == *hint) return weight;
    // Missing catalogue metadata is not negative style evidence for an exact
    // harmonic route. Keep it neutral; only a listed style earns its weight.
    return exactHarmony ? 0.5f : 0.25f;
}
float cadenceScore(PhraseIntent intent, CadenceType cadence) {
    if (intent == PhraseIntent::Loop) return cadence == CadenceType::LoopClosure ? 1.f : 0.5f;
    if (intent == PhraseIntent::Resolve) return cadence == CadenceType::Authentic ||
        cadence == CadenceType::PerfectAuthentic || cadence == CadenceType::Plagal ? 1.f : 0.4f;
    return 0.5f;
}
float rhythmNeutralEligibility(const RecommendationSubScores& s, const RecommendationWeights& w) {
    const float weights = w.match + w.skeleton + w.style + w.intent + w.cadence +
        w.continuation + w.prior + w.support;
    if (weights <= 0.f) return 0.f;
    return 100.f * (w.match*s.match + w.skeleton*s.skeleton + w.style*s.style +
        w.intent*s.intent + w.cadence*s.cadence + w.continuation*s.continuation +
        w.prior*s.prior) / weights;
}
} // namespace

std::string continuationMusicFingerprint(const ContinuationCandidate& candidate) {
    std::ostringstream out;
    out << std::setprecision(17);
    const auto hold=candidate.suggestedCurrentChordDurationQN;
    if(hold)out<<*hold;else out<<'?';
    for(const auto& event:candidate.continuation) {
        const auto chord=event.harmonicData?chordPitches(*event.harmonicData):chordPitches(event.label,event.quality);
        // The voicer uses root, bass, pitch mask, quality priority and exact-mask
        // status. Keep all of them, plus exact timing; spelling/key/intent/cadence
        // are explanations, not additional audible outputs.
        out<<'|'<<chord.root<<':'<<chord.bass<<':'<<chord.intervals<<':'<<int(chord.quality)
           <<':'<<chord.exactIntervals<<'@'<<event.durationQN;
    }
    return out.str();
}

RhythmScaleEstimate estimateRhythmScale(const MatchResult& match, const MatchQuery& query,
                                        const ProgressionTemplate& item) {
    RhythmScaleEstimate result;
    const auto* selected = interpretationFor(query, match.key);
    if (!selected) return result;
    std::vector<float> ratios, allRatios;
    for (const auto& step : match.alignmentTrace) {
        if (!step.queryIndex || !step.templateIndex ||
            (step.operation != AlignmentOp::Match && step.operation != AlignmentOp::Substitute) ||
            *step.queryIndex >= selected->full.size() || *step.templateIndex >= item.full.size()) continue;
        const auto& current = selected->full[*step.queryIndex];
        const auto& reference = item.full[*step.templateIndex];
        auto duration=current.durationQN;
        if(!duration && *step.queryIndex+1<query.source.size()) {
            const auto delta=query.source[*step.queryIndex+1].startQN-query.source[*step.queryIndex].startQN;
            if(std::isfinite(delta)&&delta>0)duration=delta;
        }
        if (duration && std::isfinite(*duration) && *duration>0 && reference.durationQN &&
            std::isfinite(*reference.durationQN) && *reference.durationQN>0) {
            const auto ratio=static_cast<float>(*duration / *reference.durationQN);
            allRatios.push_back(ratio);
            if(current.structuralWeight>=0.5f && reference.structuralWeight>=0.5f)ratios.push_back(ratio);
        }
    }
    // A short prefix ending OPEN may contain only one known matched duration.
    // Its measured ratio is stronger evidence than a silent 4QN default. Use
    // the other matched nodes only when structural evidence is insufficient.
    if(ratios.size()<2)ratios=std::move(allRatios);
    result.samples = ratios.size();
    if (ratios.empty()) {
        const auto validMeter=[](int n,int d) {return n>0&&n<=32&&d>0&&d<=32&&(d&(d-1))==0;};
        if(query.timeSigNumerator && query.timeSigDenominator &&
           validMeter(*query.timeSigNumerator,*query.timeSigDenominator) &&
           validMeter(item.meterNumerator,item.meterDenominator)) {
            // Session metadata covers the imported region. Preserve all relative
            // prototype durations; this says nothing about beat grouping.
            const double inputBar=*query.timeSigNumerator*4.0 / *query.timeSigDenominator;
            const double prototypeBar=item.meterNumerator*4.0 / item.meterDenominator;
            result.scale=std::clamp(static_cast<float>(inputBar/prototypeBar),0.25f,4.f);
            result.source="session-meter-fallback";
        }
        return result;
    }
    std::sort(ratios.begin(), ratios.end());
    const auto middle = ratios.size() / 2;
    const auto median = ratios.size() % 2 ? ratios[middle] : (ratios[middle - 1] + ratios[middle]) * 0.5f;
    if (ratios.size() == 2 && ratios.back() / std::max(0.01f, ratios.front()) > 1.3f) {
        result.source="prototype-conflicting-input";
        return result;
    }
    result.source="matched-input-QN";
    result.scale = std::clamp(median, 0.25f, 4.f);
    std::vector<float> deviation;
    for (auto ratio : ratios) deviation.push_back(std::abs(ratio - median));
    std::sort(deviation.begin(), deviation.end());
    const float spread = deviation[deviation.size() / 2] / std::max(0.01f, median);
    result.confidence = clampScore(std::min(1.f, static_cast<float>(ratios.size()) / 4.f) * (1.f - spread));
    return result;
}

ConcreteChordEvent realizeContinuation(const MatchEvent& event, KeySignature key, double durationQN) {
    constexpr int major[]{0, 2, 4, 5, 7, 9, 11};
    constexpr int minor[]{0, 2, 3, 5, 7, 8, 10};
    constexpr const char* flat[]{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    constexpr const char* sharp[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    ConcreteChordEvent concrete;
    concrete.durationQN = durationQN;
    concrete.quality = event.quality;
    concrete.roles = event.roles;
    if (!event.degree || event.degree->degree < 1 || event.degree->degree > 7) {
        concrete.label = "?"; return concrete;
    }
    concrete.degree = *event.degree;
    const auto* scale = key.mode == Mode::Major ? major : minor;
    const int pc = positiveMod(static_cast<int>(key.tonic) + scale[event.degree->degree - 1] +
                               event.degree->alteration, 12);
    const bool flatKey = key.tonic == PitchClass::Db || key.tonic == PitchClass::Eb ||
        key.tonic == PitchClass::F || key.tonic == PitchClass::Gb || key.tonic == PitchClass::Ab ||
        key.tonic == PitchClass::Bb || event.degree->alteration < 0 ||
        hasRole(event.roles, Role::Borrowed);
    concrete.label = flatKey ? flat[pc] : sharp[pc];
    switch (event.quality) {
        case ChordQuality::Minor: concrete.label += "m"; break;
        case ChordQuality::Dominant7: concrete.label += "7"; break;
        case ChordQuality::Major7: concrete.label += "maj7"; break;
        case ChordQuality::Minor7: concrete.label += "m7"; break;
        case ChordQuality::Diminished: concrete.label += "dim"; break;
        case ChordQuality::Diminished7: concrete.label += "dim7"; break;
        case ChordQuality::HalfDiminished7: concrete.label += "m7b5"; break;
        case ChordQuality::Sus2: concrete.label += "sus2"; break;
        case ChordQuality::Sus4: concrete.label += "sus4"; break;
        case ChordQuality::Augmented: concrete.label += "aug"; break;
        default: break;
    }
    if (event.bassInterval || event.intervalMask || event.colorMask || !event.displaySuffix.empty()) {
        if (!event.displaySuffix.empty()) concrete.label = std::string(flatKey ? flat[pc] : sharp[pc]) + event.displaySuffix;
        ChordEvent data; data.root=static_cast<PitchClass>(pc); data.quality=event.quality;
        if (event.bassInterval) {
            const int bass=positiveMod(pc+*event.bassInterval,12);
            data.bass=static_cast<PitchClass>(bass);
            if (bass!=pc) concrete.label += std::string("/")+(flatKey?flat[bass]:sharp[bass]);
        }
        if (event.intervalMask || event.colorMask) {
            const auto base=event.intervalMask?event.intervalMask:normalizeChord(data).intervalMask;
            std::ostringstream mask; mask << "0x" << std::hex << (base|event.colorMask);
            data.extensions.mask=mask.str();
        }
        data.name=concrete.label; data.durationQN=durationQN; data.openEnded=false;
        concrete.harmonicData=std::move(data);
    }
    return concrete;
}

DiminishedInsertionDiagnostic evaluateDiminishedPassingInsertion(
    const MatchQuery& query, const MatchResult& match, const ProgressionTemplate& item) {
    DiminishedInsertionDiagnostic d;
    d.templateId = item.id;
    d.validContinuation = match.templateMatchStart <= match.templateMatchEnd &&
        match.templateMatchEnd < item.full.size() &&
        match.continuationStartIndex == match.templateMatchEnd + 1 &&
        match.continuationStartIndex <= item.full.size() &&
        match.hasContinuation == (match.continuationStartIndex < item.full.size()) &&
        (match.continuationStartIndex < item.full.size() || (item.loopable && !item.full.empty()));
    const auto* selected = interpretationFor(query, match.key);
    if (!selected || selected->full.size() != query.source.size()) return d;
    std::size_t count{}, insertion{}, position{};
    for (std::size_t i=0;i<match.alignmentTrace.size();++i) {
        const auto& step=match.alignmentTrace[i];
        if(step.operation==AlignmentOp::QueryInsertion) {
            ++count; position=i;
            if(step.queryIndex) insertion=*step.queryIndex;
            d.alreadyExplained=hasReason(step.reasons,MatchReason::ResolvesToMatchedTarget);
        }
    }
    d.singleInsertion=count==1;
    if(!d.singleInsertion || insertion==0 || insertion+1>=query.source.size())return d;
    // Require a complete, one-to-one query trace and a contiguous Factory window.
    // A Substitute may describe borrowed function even with identical actual cores.
    if(match.alignmentTrace.size()!=query.source.size())return d;
    std::size_t nextTemplate=match.templateMatchStart;
    d.compatibleCore=true;
    for(std::size_t i=0;i<match.alignmentTrace.size();++i) {
        const auto& step=match.alignmentTrace[i];
        if(step.queryIndex!=i || selected->full[i].sourceIndex!=i)return d;
        if(i==position) {if(step.templateIndex || i!=insertion)return d;continue;}
        if((step.operation!=AlignmentOp::Match && step.operation!=AlignmentOp::Substitute) ||
           step.templateIndex!=nextTemplate || nextTemplate>=item.full.size())return d;
        const auto& expected=item.full[nextTemplate++];
        const auto actual=normalizeChord(query.source[i]);
        // Secondary targets and unknown identities need additional evidence.
        if(!expected.degree || expected.target || expected.quality==ChordQuality::Unknown || !actual.root || actual.fieldConflict ||
           actual.quality!=expected.quality) {d.compatibleCore=false;continue;}
        constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
        const auto degree=*expected.degree;
        if(degree.degree<1 || degree.degree>7) {d.compatibleCore=false;continue;}
        const int root=positiveMod(static_cast<int>(match.key.tonic)+
            (match.key.mode==Mode::Major?major:minor)[degree.degree-1]+degree.alteration,12);
        if(root!=static_cast<int>(*actual.root))d.compatibleCore=false;
    }
    d.reliableAlignment=nextTemplate==match.templateMatchEnd+1;
    const auto left=normalizeChord(query.source[insertion-1]);
    const auto mid=normalizeChord(query.source[insertion]);
    const auto right=normalizeChord(query.source[insertion+1]);
    // Half-diminished is deliberately excluded from this narrow exception.
    d.diminished=!mid.fieldConflict &&
        (mid.quality==ChordQuality::Diminished || mid.quality==ChordQuality::Diminished7);
    if(left.root && mid.root && right.root) {
        const int a=positiveMod(static_cast<int>(*mid.root)-static_cast<int>(*left.root),12);
        const int b=positiveMod(static_cast<int>(*right.root)-static_cast<int>(*mid.root),12);
        d.chromaticMotion=(a==1 || a==11) && a==b;
        const auto tones=chordPitches(query.source[insertion]);
        const auto l=chordPitches(query.source[insertion-1]),r=chordPitches(query.source[insertion+1]);
        const auto common=[&](const ChordPitchSet& side) {
            int n{};for(int pitch=0;pitch<12;++pitch)
                if((tones.intervals&(1<<positiveMod(pitch-tones.root,12))) &&
                   (side.intervals&(1<<positiveMod(pitch-side.root,12))))++n;
            return n;
        };
        d.sharedTones=common(l)>=2 || common(r)>=2;
    }
    d.eligible=d.singleInsertion && d.reliableAlignment && d.diminished && d.chromaticMotion &&
        d.sharedTones && d.compatibleCore && d.validContinuation && !d.alreadyExplained;
    return d;
}

RecommendationSet recommendContinuations(const MatchQuery& query, const CandidateIndex& index,
                                         const RecommendationRequest& request, const RecommendationWeights& w) {
    RecommendationSet result;
    result.retrievalTimingMode=query.retrievalTimingMode;
    result.phase3aEnabled=w.diminishedPassingInsertionException;
    result.abstractChordCompatibility=w.abstractChordCompatibility;
    MatchWeights matchWeights;
    matchWeights.abstractChordCompatibility=w.abstractChordCompatibility;
    result.matches = matchProgression(query, index, matchWeights, index.templates().size(), &result.stats);
    std::map<std::string, ContinuationCandidate> distinct;
    std::unordered_map<TemplateID, const ProgressionTemplate*> byId;
    byId.reserve(index.templates().size());
    for (const auto& item : index.templates()) byId.emplace(item.id, &item);
    for (auto& match : result.matches) {
        if (match.similarity < w.minimumMatch) { match.disposition="minimum_match"; continue; }
        ++result.stats.similarityPassed;
        const auto found = byId.find(match.templateId);
        if (found == byId.end()) { match.disposition="template_missing"; continue; }
        const auto& item = *found->second;
        const auto* selected = interpretationFor(query, match.key);
        if (!selected || selected->full.empty()) { match.disposition="interpretation_missing"; continue; }
        const auto lastQuery = selected->full.size() - 1;
        const bool tailAligned = std::any_of(match.alignmentTrace.begin(), match.alignmentTrace.end(),
            [&](const AlignmentStep& step) {
                return step.queryIndex == lastQuery && step.templateIndex == match.templateMatchEnd &&
                    (step.operation == AlignmentOp::Match || step.operation == AlignmentOp::Substitute);
            });
        if (!tailAligned) { match.disposition="tail_not_aligned"; continue; }
        ++result.stats.tailPassed;
        // Only explicit V4 definitions are constraints. Unknown input bass is
        // allowed; an explicitly contradictory bass must not identify this path.
        if (item.version >= 4) {
            bool conflictingBass=false;
            const auto definingConflict=[](const MatchEvent& input,const MatchEvent& expected) {
                if(expected.bassInterval && input.bassInterval && *expected.bassInterval!=*input.bassInterval)return true;
                const auto a=expected.intervalMask|expected.colorMask;
                const auto b=input.intervalMask|input.colorMask;
                for(const auto pair:{(1u<<3)|(1u<<4),(1u<<10)|(1u<<11)})
                    if((a&pair)&&(b&pair)&&!(a&b&pair))return true;
                return false;
            };
            for (const auto& step:match.alignmentTrace) {
                if (!step.queryIndex || !step.templateIndex ||
                    (step.operation!=AlignmentOp::Match && step.operation!=AlignmentOp::Substitute)) continue;
                const auto& input=selected->full[*step.queryIndex];
                const auto& expected=item.full[*step.templateIndex];
                if(definingConflict(input,expected))conflictingBass=true;
            }
            // A same-root replacement can be represented by adjacent DP gaps.
            // Preserve the same explicit identity constraints on that path;
            // otherwise a cheap deletion+insertion bypasses a conflicting third.
            const auto pitch=[&](ScaleDegree d) {
                constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
                return positiveMod((match.key.mode==Mode::Major?major:minor)[d.degree-1]+d.alteration,12);
            };
            for(std::size_t i=1;i<match.alignmentTrace.size();++i) {
                const auto& a=match.alignmentTrace[i-1];const auto& b=match.alignmentTrace[i];
                const AlignmentStep* inserted{};const AlignmentStep* deleted{};
                if(a.operation==AlignmentOp::QueryInsertion&&b.operation==AlignmentOp::TemplateDeletion){inserted=&a;deleted=&b;}
                if(b.operation==AlignmentOp::QueryInsertion&&a.operation==AlignmentOp::TemplateDeletion){inserted=&b;deleted=&a;}
                if(!inserted||!inserted->queryIndex||!deleted->templateIndex)continue;
                const auto& input=selected->full[*inserted->queryIndex];const auto& expected=item.full[*deleted->templateIndex];
                if(input.degree&&expected.degree&&pitch(*input.degree)==pitch(*expected.degree)&&definingConflict(input,expected))conflictingBass=true;
            }
            if (conflictingBass) { match.disposition="explicit_bass_or_defining_tone_conflict"; continue; }
        }
        ++result.stats.realizationPassed;
        if (!match.hasContinuation && !item.loopable) { match.disposition="no_continuation"; continue; }
        std::vector<MatchEvent> suffix;
        for (std::size_t i = match.continuationStartIndex; i < item.full.size(); ++i) suffix.push_back(item.full[i]);
        if (item.loopable) {
            // Complete one cycle up to, but not including, the query's start.
            // A prefix starting at zero already has the rest of its cycle in
            // suffix. Replaying its first chord would add a spurious extra beat
            // group. A fully consumed cycle offers the complete next round.
            const auto wrapEnd = match.templateMatchStart == 0
                ? (match.hasContinuation ? 0 : item.full.size()) : match.templateMatchStart;
            for (std::size_t i = 0; i < std::min(wrapEnd, item.full.size()); ++i) suffix.push_back(item.full[i]);
        }
        if (suffix.empty()) { match.disposition="empty_suffix"; continue; }
        ++result.stats.pathPassed;
        auto rhythm = estimateRhythmScale(match, query, item);
        match.timingSource=rhythm.source;
        ContinuationCandidate candidate;
        candidate.id = item.id;
        candidate.primaryTemplate = item.id;
        candidate.key = match.key;
        candidate.intent = effectiveIntent(item, suffix);
        candidate.cadence = item.cadence;
        if (candidate.intent == PhraseIntent::Loop && !preservesLoopContext(*selected, match, item)) {
            candidate.intent = PhraseIntent::Develop;
            candidate.cadence = CadenceType::None;
        }
        candidate.styles = item.styles;
        candidate.matchStart = match.templateMatchStart;
        candidate.matchEnd = match.templateMatchEnd;
        candidate.continuationStart = match.continuationStartIndex;
        candidate.rhythmScale = rhythm.scale;
        candidate.rhythmConfidence = rhythm.confidence;
        candidate.matchSimilarity = match.similarity;
        candidate.supportingTemplates.push_back(item.id);
        for (const auto& event : suffix)
            candidate.continuation.push_back(realizeContinuation(event, match.key,
                event.durationQN.value_or(4.0) * rhythm.scale));
        // A phrase's color chord may already be in the imported prefix. Classify
        // the actual future, while preserving explicit typed extension colors.
        const bool typedColor = std::any_of(suffix.begin(), suffix.end(), [](const auto& event) {
            return event.colorMask != 0;
        });
        if (candidate.intent == PhraseIntent::Color && !typedColor &&
            evaluateIntentCompletion(candidate).reason == CompletionReason::NoEvidence) {
            const auto& last = candidate.continuation.back();
            const bool tonicQuality = match.key.mode == Mode::Major
                ? last.quality == ChordQuality::Major || last.quality == ChordQuality::Major7
                : last.quality == ChordQuality::Minor || last.quality == ChordQuality::Minor7;
            candidate.intent = last.degree.degree == 1 && last.degree.alteration == 0 && tonicQuality
                ? PhraseIntent::Resolve : PhraseIntent::Develop;
        }
        // A repeated copy of the current open chord offers no new harmonic step.
        const auto currentLabel = realizeContinuation(selected->full.back(), match.key, 1.0).label;
        if (!candidate.continuation.empty() && candidate.continuation.front().label == currentLabel) {
            match.disposition="repeated_current_chord"; continue; }
        if (!selected->full.back().durationQN) {
            for (const auto& step : match.alignmentTrace) {
                if (step.queryIndex == lastQuery && step.templateIndex && *step.templateIndex < item.full.size() &&
                    item.full[*step.templateIndex].durationQN)
                    candidate.suggestedCurrentChordDurationQN =
                        *item.full[*step.templateIndex].durationQN * rhythm.scale;
            }
        }
        auto& s = candidate.subscores;
        s.match = match.similarity;
        s.skeleton = match.subScores.skeletonHarmony;
        s.style = styleScore(item, request.style, match.rawCost <= 0.00001f);
        s.intent = request.preferredIntent && *request.preferredIntent == candidate.intent ? 1.f : 0.5f;
        s.cadence = cadenceScore(candidate.intent, candidate.cadence);
        s.continuation = clampScore(0.45f + 0.1f * static_cast<float>(std::min<std::size_t>(5, suffix.size())));
        s.prior = item.priorWeight;
        s.rhythm = rhythm.samples >= 2 ? clampScore(0.5f * match.subScores.rhythmSimilarity +
                                                      0.5f * rhythm.confidence) : 0.5f;
        s.support = 0.f;
        candidate.rankingScore = 100.f * (w.match*s.match + w.skeleton*s.skeleton + w.style*s.style +
            w.intent*s.intent + w.cadence*s.cadence + w.continuation*s.continuation + w.prior*s.prior +
            w.rhythm*s.rhythm);
        candidate.eligibilityScore = rhythmNeutralEligibility(s, w);
        for (const auto& step : match.alignmentTrace) {
            if (step.operation == AlignmentOp::QueryInsertion &&
                !hasReason(step.reasons, MatchReason::ResolvesToMatchedTarget)) {
                candidate.rankingScore -= w.unexplainedInsertionPenalty;
                candidate.eligibilityScore -= w.unexplainedInsertionPenalty;
            } else if (step.operation == AlignmentOp::TemplateDeletion) {
                candidate.rankingScore -= w.templateDeletionPenalty;
                candidate.eligibilityScore -= w.templateDeletionPenalty;
            }
        }
        if(w.diminishedPassingInsertionException) {
            auto d=evaluateDiminishedPassingInsertion(query,match,item);
            d.scoreBefore=candidate.rankingScore;
            for(const auto& step:match.alignmentTrace)
                if(step.operation==AlignmentOp::QueryInsertion &&
                   !hasReason(step.reasons,MatchReason::ResolvesToMatchedTarget))
                    d.totalInsertionPenalty+=w.unexplainedInsertionPenalty;
            if(d.singleInsertion && !d.alreadyExplained)
                d.originalPenalty=std::max(0.f,w.unexplainedInsertionPenalty);
            // One event, one refund of the penalty actually charged. No bonus.
            d.refund=d.eligible?d.originalPenalty:0.f;
            candidate.rankingScore+=d.refund;
            candidate.eligibilityScore+=d.refund;
            d.scoreAfter=candidate.rankingScore;
            result.insertionDiagnostics.push_back(std::move(d));
        }
        if(!request.constraints.melody.empty()) {
            candidate.constraints=request.constraints;
            candidate.melodyCompatibility=evaluateMelody(continuationProgression(query.source,candidate),request.constraints);
            if(!candidate.melodyCompatibility.hardSatisfied){match.disposition="melody_constraint";continue;}
        }
        if(request.tendency!=HarmonicTendency::Balanced) {
            const auto& profile=tendencyProfile(request.tendency);
            const auto colors=std::count_if(candidate.continuation.begin(),candidate.continuation.end(),[](const auto& event) {
                return event.degree.alteration!=0||hasRole(event.roles,Role::Borrowed)||
                    hasRole(event.roles,Role::SecondaryDominant)||hasRole(event.roles,Role::SecondaryLeadingTone);
            });
            candidate.rankingScore+=profile.colorBias*static_cast<float>(colors)/
                static_cast<float>(std::max<std::size_t>(1,candidate.continuation.size()));
            candidate.eligibilityScore+=profile.colorBias*static_cast<float>(colors)/
                static_cast<float>(std::max<std::size_t>(1,candidate.continuation.size()));
        }
        match.disposition="candidate_generated";match.candidateRankingScore=candidate.rankingScore;
        match.candidateEligibilityScore=candidate.eligibilityScore;
        const auto fingerprint = suffixFingerprint(candidate, false);
        auto [it, inserted] = distinct.try_emplace(fingerprint, std::move(candidate));
        if (!inserted) {
            // A selected style can make a later source the better explanation.
            const auto previousCount = it->second.supportCount;
            auto sources = std::move(it->second.supportingTemplates);
            sources.push_back(item.id);
            if (candidate.rankingScore > it->second.rankingScore) {
                candidate.supportCount = previousCount + 1;
                candidate.supportingTemplates = std::move(sources);
                it->second = std::move(candidate);
            } else {
                it->second.supportCount = previousCount + 1;
                it->second.supportingTemplates = std::move(sources);
            }
        }
    }
    const auto mark=[&](const ContinuationCandidate& c,const char* why) {
        for(auto& m:result.matches)if(std::find(c.supportingTemplates.begin(),c.supportingTemplates.end(),m.templateId)!=c.supportingTemplates.end()){
            m.disposition=why;m.candidateRankingScore=c.rankingScore;m.candidateEligibilityScore=c.eligibilityScore;
        }
    };
    result.stats.distinct=distinct.size();
    for (auto& [key, candidate] : distinct) {
        candidate.subscores.support = clampScore(static_cast<float>(candidate.supportCount - 1) / 4.f);
        candidate.rankingScore += 100.f * w.support * candidate.subscores.support;
        const float coreWeight = w.match + w.skeleton + w.style + w.intent + w.cadence + w.continuation + w.prior + w.support;
        if (coreWeight > 0.f)
            candidate.eligibilityScore += 100.f * w.support * candidate.subscores.support / coreWeight;
        const auto gateScore = query.retrievalTimingMode == RetrievalTimingMode::HarmonyFirst
            ? candidate.eligibilityScore : candidate.rankingScore;
        if (gateScore >= w.minimumScore) {
            ++result.stats.scorePassed;
            result.groups[groupIndex(candidate.intent)].push_back(std::move(candidate));
        } else {mark(candidate,"minimum_score");
        }
    }
    // Merge accepted musical outputs before diversity and per-group limits, so
    // duplicates cannot consume More slots. Scores already include support:
    // retain the winning score and merge provenance without another bonus.
    std::map<std::string,ContinuationCandidate> music;
    for(auto& group:result.groups) {
        for(auto& candidate:group) {
            auto [it,inserted]=music.try_emplace(continuationMusicFingerprint(candidate),std::move(candidate));
            if(inserted)continue;
            auto sources=it->second.supportingTemplates;
            for(const auto& id:candidate.supportingTemplates)
                if(std::find(sources.begin(),sources.end(),id)==sources.end())sources.push_back(id);
            if(higherRank(candidate,it->second))it->second=std::move(candidate);
            it->second.supportingTemplates=std::move(sources);
            it->second.supportCount=static_cast<int>(it->second.supportingTemplates.size());
        }
        group.clear();
    }
    for(auto& [identity,candidate]:music)result.groups[groupIndex(candidate.intent)].push_back(std::move(candidate));
    for (auto& group : result.groups) {
        std::sort(group.begin(), group.end(), higherRank);
        std::vector<ContinuationCandidate> diverse;
        for (auto& candidate : group) {
            const auto coarse = suffixFingerprint(candidate, true);
            bool near{};
            for (const auto& chosen : diverse) {
                if (suffixFingerprint(chosen, true) == coarse) { near = true; break; }
                std::size_t shared{};
                const auto common = std::min(chosen.continuation.size(), candidate.continuation.size());
                while (shared < common && chosen.continuation[shared].degree.degree == candidate.continuation[shared].degree.degree &&
                       chosen.continuation[shared].degree.alteration == candidate.continuation[shared].degree.alteration)
                    ++shared;
                const auto longer = std::max(chosen.continuation.size(), candidate.continuation.size());
                // Shared openings do not make different complete endings
                // redundant. Compare a strict majority of the longer route.
                if (shared >= 2 && shared * 2 > longer) {
                    candidate.rankingScore -= w.diversityPenalty;
                    if (query.retrievalTimingMode == RetrievalTimingMode::HarmonyFirst)
                        candidate.eligibilityScore -= w.diversityPenalty;
                }
            }
            const auto candidateGateScore = query.retrievalTimingMode == RetrievalTimingMode::HarmonyFirst
                ? candidate.eligibilityScore : candidate.rankingScore;
            if (!near && candidateGateScore >= w.minimumScore) diverse.push_back(std::move(candidate));
            else mark(candidate,near?"equivalent_path_deduplication":"diversity_score");
        }
        std::sort(diverse.begin(), diverse.end(), higherRank);
        for(std::size_t i=w.perGroup;i<diverse.size();++i)mark(diverse[i],"retained_limit");
        if (diverse.size() > w.perGroup) diverse.resize(w.perGroup);
        for(const auto& c:diverse)mark(c,"TARGET_RETAINED");
        // Score only the already accepted cards: a diagnostic tie-break cannot
        // remove a valid option or pull a lower-quality option across the gate.
        for(auto& candidate:diverse)
            if(const auto* selected=interpretationFor(query,candidate.key);selected&&!selected->full.empty()) {
                const auto completion=evaluateIntentCompletion(candidate,selected->full.front().degree,
                                                                selected->full.back().degree);
                candidate.rankingScore=std::clamp(candidate.rankingScore+
                    100.f*w.intentCompletionTieBreak*(completion.score-0.5f),0.f,100.f);
            }
        for(auto& candidate:diverse)
            if(!candidate.constraints.melody.empty())
                candidate.rankingScore-=melodySoftPenaltyPoints*(1.f-candidate.melodyCompatibility.score);
        std::stable_sort(diverse.begin(),diverse.end(),higherRank);
        group = std::move(diverse);
        result.stats.output+=group.size();
    }
    return result;
}
Progression continuationProgression(const Progression& source,const ContinuationCandidate& candidate) {
    auto path=source;
    const double fallback=progressionOpenDuration(source);
    double end=source.empty()?0:source.front().startQN;
    for(std::size_t i=0;i<path.size();++i) {
        auto& e=path[i];
        if(!e.durationQN||e.openEnded)e.durationQN=i+1==path.size()?
            candidate.suggestedCurrentChordDurationQN.value_or(fallback):fallback;
        e.openEnded=false;end=std::max(end,e.startQN+*e.durationQN);
    }
    for(const auto& chord:candidate.continuation) {
        ChordEvent e=chord.harmonicData.value_or(ChordEvent{});e.name=chord.label;e.quality=chord.quality;e.startQN=end;
        e.durationQN=chord.durationQN;e.openEnded=false;path.push_back(std::move(e));end+=chord.durationQN;
    }
    return path;
}
const char* intentName(PhraseIntent value) noexcept {
    switch (value) {
        case PhraseIntent::Resolve: return "RESOLVE"; case PhraseIntent::Develop: return "DEVELOP";
        case PhraseIntent::Loop: return "LOOP"; case PhraseIntent::Color: return "COLOR";
        default: return "NEUTRAL";
    }
}
const char* cadenceName(CadenceType value) noexcept {
    switch (value) {
        case CadenceType::Authentic: return "authentic";
        case CadenceType::PerfectAuthentic: return "perfect authentic";
        case CadenceType::ImperfectAuthentic: return "imperfect authentic";
        case CadenceType::Plagal: return "plagal"; case CadenceType::Half: return "half";
        case CadenceType::Deceptive: return "deceptive"; case CadenceType::Modal: return "modal";
        case CadenceType::LoopClosure: return "loop closure";
        case CadenceType::Unknown: return "unknown"; default: return "none";
    }
}
} // namespace harmony
