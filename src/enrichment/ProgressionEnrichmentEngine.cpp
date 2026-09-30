#include "ProgressionEnrichmentEngine.h"
#include "preview/VoiceLeadingMetrics.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace harmony::enrichment {
namespace {
int pc(int n) { return (n % 12 + 12) % 12; }
constexpr const char* spellings[]{"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
std::string name(int root, ChordQuality quality) {
    const char* suffix = "";
    switch (quality) {
    case ChordQuality::Minor: suffix = "m"; break;
    case ChordQuality::Major7: suffix = "maj7"; break;
    case ChordQuality::Minor7: suffix = "m7"; break;
    case ChordQuality::Dominant7: suffix = "7"; break;
    case ChordQuality::Diminished: suffix = "dim"; break;
    case ChordQuality::Diminished7: suffix = "dim7"; break;
    default: break;
    }
    return std::string(spellings[pc(root)]) + suffix;
}
std::optional<int> root(const ChordEvent& event) {
    const auto normalized = normalizeChord(event);
    if (!normalized.root || normalized.quality == ChordQuality::Unknown) return {};
    return static_cast<int>(*normalized.root);
}
ChordEvent replacement(const ChordEvent& old, int pitch, ChordQuality quality) {
    ChordEvent next = old;
    next.root = static_cast<PitchClass>(pc(pitch));
    next.bass = next.root;
    next.keyNoteValue.reset(); next.bassNoteValue.reset();
    next.rawName.reset(); next.rawKeyNote.reset(); next.rawBassNote.reset();
    next.extensions = {};
    next.quality = quality;
    next.name = name(pitch, quality);
    return next;
}
void record(EnrichmentCandidate& candidate, OperationType type, TechniqueID technique,
            std::size_t index, std::string before, std::string after, std::string reason) {
    candidate.operations.push_back({type, technique, index, std::move(before), std::move(after), std::move(reason)});
    if (std::find(candidate.techniques.begin(), candidate.techniques.end(), technique) == candidate.techniques.end())
        candidate.techniques.push_back(technique);
}
bool insertBefore(EnrichmentCandidate& candidate, std::size_t target, int pitch,
                  ChordQuality quality, TechniqueID technique, std::string reason,
                  const EnrichmentConfig& config) {
    if (target == 0 || target >= candidate.progression.size()) return false;
    auto& previous = candidate.progression[target - 1];
    auto& next = candidate.progression[target];
    const double available = next.startQN - previous.startQN;
    if (!std::isfinite(available) || available < config.minimumSplitQN * 2) return false;
    const double split = available / 2;
    if (previous.durationQN && !previous.openEnded && *previous.durationQN + 1e-7 < available) return false;
    const auto before = previous.name + " → " + next.name;
    ChordEvent inserted = replacement(previous, pitch, quality);
    inserted.startQN = previous.startQN + split;
    inserted.durationQN = split;
    inserted.openEnded = false;
    previous.durationQN = split;
    previous.openEnded = false;
    const auto insertedName = inserted.name;
    candidate.progression.insert(candidate.progression.begin() + static_cast<std::ptrdiff_t>(target), std::move(inserted));
    record(candidate, OperationType::InsertChord, technique, target - 1, before,
           candidate.progression[target - 1].name + " → " + insertedName + " → " + candidate.progression[target + 1].name,
           std::move(reason));
    return true;
}
bool polish(EnrichmentCandidate& candidate, const HarmonicAnalysisResult& analysis,
            std::optional<Style> style, const EnrichmentConfig& config) {
    for (std::size_t i = 0; i < candidate.progression.size(); ++i) {
        auto& event = candidate.progression[i];
        const auto normalized = normalizeChord(event);
        if (!normalized.root) continue;
        ChordQuality newQuality = ChordQuality::Unknown;
        if (normalized.quality == ChordQuality::Minor) newQuality = ChordQuality::Minor7;
        else if (normalized.quality == ChordQuality::Major) {
            const bool dominant = i < analysis.full.size() &&
                analysis.full[i].function == HarmonicFunction::Dominant;
            newQuality = dominant ? ChordQuality::Dominant7 : ChordQuality::Major7;
            if (style == Style::Rock && !dominant) continue;
        }
        if (newQuality == ChordQuality::Unknown) continue;
        const auto before = event.name;
        event = replacement(event, static_cast<int>(*normalized.root), newQuality);
        record(candidate, OperationType::UpgradeQuality, TechniqueID::SeventhColor, i,
               before, event.name, "增加七和弦色彩，保留原和弦根音和时间位置");
        return true;
    }
    for (std::size_t i = 0; i < candidate.progression.size(); ++i) {
        auto& event = candidate.progression[i];
        const auto normalized = normalizeChord(event);
        if (!normalized.root || (normalized.colorMask & (1u << 2))) continue;
        const auto quality = normalized.quality;
        if (quality != ChordQuality::Major7 && quality != ChordQuality::Minor7 &&
            quality != ChordQuality::Dominant7) continue;
        if (style == Style::Rock) continue;
        const auto before = event.name;
        event = replacement(event, static_cast<int>(*normalized.root), quality);
        event.name = std::string(spellings[static_cast<int>(*normalized.root)]) +
            (quality == ChordQuality::Major7 ? "maj9" : quality == ChordQuality::Minor7 ? "m9" : "9");
        record(candidate, OperationType::UpgradeQuality, TechniqueID::NinthColor, i,
               before, event.name, "在已有七和弦上加入九度色彩");
        return true;
    }
    return !candidate.operations.empty() && static_cast<int>(candidate.operations.size()) <= config.maxOperations[0];
}
bool improveInversion(EnrichmentCandidate& candidate,const EnrichmentConfig& config) {
    const auto baseline=preview::measureVoiceLeading(candidate.progression);
    if(!baseline)return false;
    std::optional<std::size_t> bestIndex;
    float bestGain{};
    for (std::size_t i = 1; i + 1 < candidate.progression.size(); ++i) {
        const auto mid = root(candidate.progression[i]);
        if (!mid) continue;
        const auto q = normalizeChord(candidate.progression[i]).quality;
        if (q != ChordQuality::Major && q != ChordQuality::Minor) continue;
        const int third = pc(*mid + (q == ChordQuality::Major ? 4 : 3));
        auto trial=candidate.progression;
        trial[i]=replacement(trial[i],*mid,q);
        trial[i].bass=static_cast<PitchClass>(third);
        trial[i].name+="/"+std::string(spellings[third]);
        const auto measured=preview::measureVoiceLeading(trial);
        if(!measured)continue;
        const int bassGain=baseline->bassMotionSemitones-measured->bassMotionSemitones;
        if(bassGain<config.minimumBassImprovementSemitones ||
           measured->score+config.toleratedVoiceLeadingLoss<baseline->score)continue;
        const float gain=measured->score-baseline->score+0.02f*static_cast<float>(bassGain);
        if(!bestIndex || gain>bestGain){bestIndex=i;bestGain=gain;}
    }
    if(!bestIndex)return false;
    auto& event=candidate.progression[*bestIndex];
    const auto before=event.name;
    const auto q=normalizeChord(event).quality;
    const int mid=*root(event);
    const int third=pc(mid+(q==ChordQuality::Major?4:3));
    event=replacement(event,mid,q);
    event.bass=static_cast<PitchClass>(third);
    event.name+="/"+std::string(spellings[third]);
    record(candidate,OperationType::ChangeInversion,TechniqueID::Inversion,*bestIndex,
           before,event.name,"实际发声的低音移动更短，且整体声部连接保持自然");
    return true;
}
struct Option { TechniqueID technique; std::size_t target; int pitch; ChordQuality quality; const char* reason; };
std::vector<Option> options(const Progression& progression, const HarmonicAnalysisResult& analysis,
                            const EnrichmentConfig& config) {
    std::vector<Option> result;
    const auto key = analysis.selectedKey ? std::optional(analysis.selectedKey->key) : std::nullopt;
    for (std::size_t target = 1; target < progression.size(); ++target) {
        const auto left = root(progression[target - 1]);
        const auto right = root(progression[target]);
        if (!left || !right || progression[target].startQN - progression[target - 1].startQN <
                               config.minimumSplitQN * 2) continue;
        if (pc(*right - *left) == 2)
            result.push_back({TechniqueID::PassingDiminished, target, pc(*left + 1),
                              ChordQuality::Diminished7, "半音经过减七连接两个上行级进和弦"});
        const bool diatonicTarget=target < analysis.full.size() && analysis.full[target].degree &&
            analysis.full[target].degree->alteration==0;
        if (key && diatonicTarget && *right != static_cast<int>(key->tonic) && pc(*right - *left) != 0) {
            result.push_back({TechniqueID::SecondaryDominant, target, pc(*right + 7),
                              ChordQuality::Dominant7, "插入目标和弦的属七和弦，明确向目标解决"});
            result.push_back({TechniqueID::SecondaryLeadingTone, target, pc(*right - 1),
                              ChordQuality::Diminished7, "插入目标和弦的导音减七，半音引向目标"});
        }
        if (key && key->mode == Mode::Major && pc(*left - static_cast<int>(key->tonic)) == 5 &&
            pc(*right - static_cast<int>(key->tonic)) == 7)
            result.push_back({TechniqueID::PredominantSubstitution, target,
                              pc(static_cast<int>(key->tonic) + 2), ChordQuality::Minor7,
                              "在 IV 与 V 之间加入 ii，扩展属前功能"});
        if (key && target > 1 && pc(*left - static_cast<int>(key->tonic)) == 7 &&
            *right == static_cast<int>(key->tonic))
            result.push_back({TechniqueID::CadentialExpansion, target - 1,
                              pc(static_cast<int>(key->tonic) + 2), ChordQuality::Minor7,
                              "在 V–I 终止前加入 ii，形成 ii–V–I"});
    }
    return result;
}
float styleFit(std::optional<Style> style, const std::vector<TechniqueID>& techniques) {
    if (!style) return 0.75f;
    float value = 0.72f;
    for (auto technique : techniques) {
        if ((technique == TechniqueID::SecondaryDominant || technique == TechniqueID::CadentialExpansion) &&
            (*style == Style::Jazz || *style == Style::CityPop || *style == Style::Functional)) value += 0.10f;
        if (technique == TechniqueID::PassingDiminished &&
            (*style == Style::Rnb || *style == Style::Jazz || *style == Style::CityPop)) value += 0.10f;
        if (technique == TechniqueID::SeventhColor && *style == Style::Rock) value -= 0.08f;
    }
    return std::clamp(value, 0.f, 1.f);
}
std::string fingerprint(const Progression& progression) {
    // Stable across processes and independent of std::hash implementation.
    std::uint64_t hash = 14695981039346656037ULL;
    auto add = [&hash](unsigned char byte) { hash = (hash ^ byte) * 1099511628211ULL; };
    for (const auto& event : progression) {
        for (unsigned char byte : event.name) add(byte);
        add(0);
        const auto tick = static_cast<std::int64_t>(std::llround(event.startQN * 480.0));
        for (int n = 0; n < 8; ++n) add(static_cast<unsigned char>(tick >> (n * 8)));
    }
    std::ostringstream out; out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}
void finalize(EnrichmentCandidate& candidate, const Progression& source,
              const HarmonicAnalysisResult& analysis, std::optional<Style> style) {
    int inserted = 0, replaced = 0;
    for (const auto& operation : candidate.operations) {
        inserted += operation.type == OperationType::InsertChord;
        replaced += operation.type == OperationType::ReplaceChord;
    }
    // Compare the rendered progression with the source at each original start.
    // Structural chords count twice; inserted passing chords do not erase the
    // source identity, while a changed or missing structural root is costly.
    float retained = 0.f, possible = 0.f;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const bool structural = std::find(analysis.skeletonIndices.begin(), analysis.skeletonIndices.end(), i)
                                != analysis.skeletonIndices.end();
        const float weight = structural ? 2.f : 1.f;
        possible += weight;
        const auto found = std::find_if(candidate.progression.begin(), candidate.progression.end(),
            [&](const ChordEvent& chord) { return std::abs(chord.startQN - source[i].startQN) < 1e-7; });
        if (found == candidate.progression.end()) continue;
        const auto oldRoot = root(source[i]), newRoot = root(*found);
        if (oldRoot && newRoot && *oldRoot == *newRoot) {
            retained += 0.88f * weight;
            if (normalizeChord(source[i]).quality == normalizeChord(*found).quality)
                retained += 0.12f * weight;
        }
    }
    candidate.skeletonPreservation = possible > 0.f ? retained / possible : 0.f;
    candidate.complexityScore = std::clamp(0.12f + 0.16f * static_cast<float>(candidate.operations.size()) +
                                            0.12f * inserted + 0.12f * replaced, 0.f, 1.f);
    candidate.styleCompatibility = styleFit(style, candidate.techniques);
    candidate.score = 100.f * (0.55f * candidate.skeletonPreservation +
                               0.30f * candidate.styleCompatibility +
                               0.15f * candidate.complexityScore);
    candidate.fingerprint = fingerprint(candidate.progression);
    candidate.id = std::string(groupName(candidate.group)) + "-" + candidate.fingerprint;
}
} // namespace

const char* techniqueName(TechniqueID technique) noexcept {
    switch (technique) {
    case TechniqueID::ChordExtension: return "ChordExtension";
    case TechniqueID::SeventhColor: return "SeventhColor";
    case TechniqueID::NinthColor: return "NinthColor";
    case TechniqueID::Inversion: return "Inversion";
    case TechniqueID::BassConnection: return "BassConnection";
    case TechniqueID::SecondaryDominant: return "SecondaryDominant";
    case TechniqueID::SecondaryLeadingTone: return "SecondaryLeadingTone";
    case TechniqueID::PassingDiminished: return "PassingDiminished";
    case TechniqueID::ChromaticApproach: return "ChromaticApproach";
    case TechniqueID::BorrowedChord: return "BorrowedChord";
    case TechniqueID::PredominantSubstitution: return "PredominantSubstitution";
    case TechniqueID::CadentialExpansion: return "CadentialExpansion";
    case TechniqueID::Turnaround: return "Turnaround";
    }
    return "Unknown";
}
const char* groupName(Group group) noexcept {
    switch (group) {
    case Group::Polish: return "Polish";
    case Group::Rich: return "Rich";
    case Group::Advanced: return "Advanced";
    }
    return "Unknown";
}
EnrichmentResult enrichProgression(const Progression& source, const HarmonicAnalysisResult& analysis,
                                   std::optional<Style> style, const EnrichmentConfig& inputConfig) {
    auto config=inputConfig;
    const auto& profile=tendencyProfile(config.tendency);
    if(config.tendency!=HarmonicTendency::Balanced) {
        config.maxOperations=profile.operations;config.maxInsertions=profile.insertions;
        config.maxSubstitutions=profile.substitutions;config.minimumSkeletonPreservation=profile.skeletonMinimum;
    }
    EnrichmentResult result;
    if(!validConstraints(config.constraints)){result.error="invalid melody constraints";return result;}
    if (source.empty() || source.size() > 64) { result.error = "invalid progression length"; return result; }
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (!std::isfinite(source[i].startQN) || (i && source[i].startQN <= source[i - 1].startQN)) {
            result.error = "invalid chord timeline"; return result;
        }
    }
    auto available = options(source, analysis, config);
    for (const auto& option : available) {
        if (std::none_of(result.opportunities.begin(), result.opportunities.end(), [&](const auto& existing) {
                return existing.afterIndex == option.target - 1; }))
            result.opportunities.push_back({option.technique, option.target - 1, option.reason});
    }
    EnrichmentCandidate polishCandidate;
    polishCandidate.group = Group::Polish;
    polishCandidate.complexity = ComplexityLevel::Basic;
    polishCandidate.progression = source;
    if (!polish(polishCandidate, analysis, style, config) && !improveInversion(polishCandidate,config))
        return result;
    if(static_cast<int>(polishCandidate.operations.size())>config.maxOperations[0])return result;
    finalize(polishCandidate, source, analysis, style);
    result.groups[0].push_back(polishCandidate);

    std::unordered_set<std::string> fingerprints{polishCandidate.fingerprint};
    EnrichmentCandidate inversion;
    inversion.group=Group::Polish;
    inversion.complexity=ComplexityLevel::Basic;
    inversion.progression=source;
    if (config.maxOperations[0]>0 && improveInversion(inversion,config)) {
        finalize(inversion,source,analysis,style);
        if (fingerprints.insert(inversion.fingerprint).second)
            result.groups[0].push_back(std::move(inversion));
    }
    for (const auto& option : available) {
        if(config.maxInsertions[1]<1)break;
        EnrichmentCandidate rich = polishCandidate;
        rich.group = Group::Rich;
        rich.complexity = ComplexityLevel::Rich;
        if (static_cast<int>(rich.operations.size()) >= config.maxOperations[1] ||
            !insertBefore(rich, option.target, option.pitch, option.quality, option.technique,
                          option.reason, config)) continue;
        finalize(rich, source, analysis, style);
        if (fingerprints.insert(rich.fingerprint).second)
            result.groups[1].push_back(std::move(rich));
        if (result.groups[1].size() == 3) break;
    }
    if (result.groups[1].empty()) {
        EnrichmentCandidate rich = polishCandidate;
        rich.group = Group::Rich;
        rich.complexity = ComplexityLevel::Rich;
        // A second color is still a meaningful Rich variant when insertion is impossible.
        for (std::size_t i = 0; i < rich.progression.size(); ++i) {
            auto& event = rich.progression[i];
            const auto normalized = normalizeChord(event);
            if (!normalized.root || (normalized.quality != ChordQuality::Major &&
                                     normalized.quality != ChordQuality::Minor)) continue;
            const auto before = event.name;
            event = replacement(event, static_cast<int>(*normalized.root),
                                normalized.quality == ChordQuality::Minor ? ChordQuality::Minor7 : ChordQuality::Major7);
            record(rich, OperationType::UpgradeQuality, TechniqueID::SeventhColor, i,
                   before, event.name, "为另一处和弦增加七和弦色彩");
            break;
        }
        if (rich.operations.size() > polishCandidate.operations.size() &&
            static_cast<int>(rich.operations.size())<=config.maxOperations[1]) {
            finalize(rich, source, analysis, style);
            if (fingerprints.insert(rich.fingerprint).second)
                result.groups[1].push_back(std::move(rich));
        }
    }
    std::stable_sort(result.groups[1].begin(),result.groups[1].end(),
        [](const auto& a,const auto& b){return a.score>b.score;});

    EnrichmentCandidate advanced = result.groups[1].empty() ? polishCandidate : result.groups[1].front();
    advanced.group = Group::Advanced;
    advanced.complexity = ComplexityLevel::Advanced;
    const auto key = analysis.selectedKey ? std::optional(analysis.selectedKey->key) : std::nullopt;
    if (key && key->mode == Mode::Major && config.maxSubstitutions[2]>0 &&
        static_cast<int>(advanced.operations.size()) < config.maxOperations[2]) {
        for (std::size_t i = 0; i + 1 < advanced.progression.size(); ++i) {
            const auto current = root(advanced.progression[i]);
            const auto next = root(advanced.progression[i + 1]);
            if (!current || !next || pc(*current - static_cast<int>(key->tonic)) != 5 ||
                *next != static_cast<int>(key->tonic) ||
                (normalizeChord(advanced.progression[i]).quality != ChordQuality::Major &&
                 normalizeChord(advanced.progression[i]).quality != ChordQuality::Major7)) continue;
            auto& event = advanced.progression[i];
            const auto before = event.name;
            event = replacement(event, *current, normalizeChord(event).quality==ChordQuality::Major7?
                                ChordQuality::Minor7:ChordQuality::Minor);
            record(advanced, OperationType::ReplaceChord, TechniqueID::BorrowedChord, i,
                   before, event.name, "借用小调 iv 回到主和弦");
            break;
        }
    }
    const auto baseOperationCount = result.groups[1].empty() ? polishCandidate.operations.size() :
        result.groups[1].front().operations.size();
    if (advanced.operations.size() == baseOperationCount &&
        static_cast<int>(advanced.operations.size())<config.maxOperations[2] &&
        config.maxInsertions[2] > std::count_if(advanced.operations.begin(), advanced.operations.end(),
            [](const EnrichmentOperation& operation) { return operation.type == OperationType::InsertChord; })) {
        for (auto it = available.rbegin(); it != available.rend(); ++it) {
            if (it->technique != TechniqueID::SecondaryLeadingTone &&
                it->technique != TechniqueID::CadentialExpansion &&
                it->technique != TechniqueID::PredominantSubstitution) continue;
            const auto targetRoot = root(source[it->target]);
            if (!targetRoot) continue;
            if (std::any_of(advanced.operations.begin(), advanced.operations.end(), [&](const auto& operation) {
                    return operation.type == OperationType::InsertChord &&
                           operation.sourceIndex == it->target - 1; })) continue;
            auto target = std::find_if(advanced.progression.begin(), advanced.progression.end(),
                [&](const ChordEvent& event) { return event.startQN == source[it->target].startQN && root(event) == targetRoot; });
            if (target != advanced.progression.end() &&
                insertBefore(advanced, static_cast<std::size_t>(target - advanced.progression.begin()),
                             it->pitch, it->quality, it->technique, it->reason, config)) break;
        }
    }
    if (advanced.operations.size() > baseOperationCount) {
        finalize(advanced, source, analysis, style);
        if (advanced.skeletonPreservation >= config.minimumSkeletonPreservation &&
            fingerprints.insert(advanced.fingerprint).second)
            result.groups[2].push_back(std::move(advanced));
    }
    // Score the completed set of paths. Scoring Rich earlier would also change
    // which Rich path becomes the Advanced candidate.
    const auto sourceVoice = preview::measureVoiceLeading(source);
    if (sourceVoice) {
        for (auto& group : result.groups) {
            for (auto& candidate : group) {
                if (const auto candidateVoice = preview::measureVoiceLeading(candidate.progression)) {
                    const auto delta = std::clamp(candidateVoice->score - sourceVoice->score, -0.5f, 0.5f);
                    candidate.score = std::clamp(candidate.score + 100.f * config.voiceLeadingWeight * delta, 0.f, 100.f);
                }
            }
            std::stable_sort(group.begin(), group.end(),
                [](const auto& a, const auto& b) { return a.score > b.score; });
        }
    }
    for(auto& group:result.groups) {
        for(auto& candidate:group) {
            if(!config.constraints.melody.empty()) {
                candidate.constraints=config.constraints;
                candidate.melodyCompatibility=evaluateMelody(candidate.progression,config.constraints);
                candidate.score-=melodySoftPenaltyPoints*(1.f-candidate.melodyCompatibility.score);
            }
            if(config.tendency!=HarmonicTendency::Balanced) {
                const auto chromatic=std::count_if(candidate.techniques.begin(),candidate.techniques.end(),[](auto t) {
                    return t==TechniqueID::BorrowedChord||t==TechniqueID::SecondaryDominant||
                        t==TechniqueID::SecondaryLeadingTone||t==TechniqueID::PassingDiminished||
                        t==TechniqueID::ChromaticApproach;
                });
                candidate.score=std::clamp(candidate.score+profile.chromaticBias*static_cast<float>(chromatic),0.f,100.f);
            }
        }
        std::erase_if(group,[&](const auto& c) { return !c.melodyCompatibility.hardSatisfied||
            (config.tendency!=HarmonicTendency::Balanced&&c.skeletonPreservation<config.minimumSkeletonPreservation); });
        if(!config.constraints.melody.empty()||config.tendency!=HarmonicTendency::Balanced)
            std::stable_sort(group.begin(),group.end(),[](const auto& a,const auto& b){return a.score>b.score;});
    }
    return result;
}
} // namespace harmony::enrichment
