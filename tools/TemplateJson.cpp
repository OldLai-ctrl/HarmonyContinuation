#include "TemplateJson.h"
#include "EmbeddedTemplates.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <unordered_set>

namespace harmony::dev {
namespace {
struct Input {
    std::string_view text;
    std::size_t at{};
    void space() { while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at; }
    bool take(char c) { space(); if (at < text.size() && text[at] == c) { ++at; return true; } return false; }
    void need(char c) { if (!take(c)) throw std::runtime_error(std::string("expected '") + c + "'"); }
    std::string string() {
        need('"'); std::string value;
        while (at < text.size()) {
            const auto c = text[at++];
            if (c == '"') return value;
            if (c == '\\' && at < text.size()) {
                const auto escape = text[at++];
                if (escape == '"' || escape == '\\' || escape == '/') value += escape;
                else if (escape == 'n') value += '\n';
                else throw std::runtime_error("unsupported escape");
            } else value += c;
        }
        throw std::runtime_error("unterminated string");
    }
    bool boolean() {
        space();
        if (text.substr(at, 4) == "true") { at += 4; return true; }
        if (text.substr(at, 5) == "false") { at += 5; return false; }
        throw std::runtime_error("expected boolean");
    }
    double number() {
        space(); double result{};
        const auto parsed = std::from_chars(text.data() + at, text.data() + text.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr == text.data() + at || !std::isfinite(result))
            throw std::runtime_error("invalid finite number");
        at = static_cast<std::size_t>(parsed.ptr - text.data()); return result;
    }
};
Style styleFromName(std::string_view name) {
    if (name == "pop") return Style::Pop;
    if (name == "rock") return Style::Rock;
    if (name == "rnb") return Style::Rnb;
    if (name == "jazz") return Style::Jazz;
    if (name == "citypop") return Style::CityPop;
    if (name == "functional") return Style::Functional;
    throw std::runtime_error("unknown style: " + std::string(name));
}
PhraseIntent intentFromName(std::string_view name) {
    if (name.empty() || name == "neutral") return PhraseIntent::Neutral;
    if (name == "resolve" || name == "cadence") return PhraseIntent::Resolve;
    if (name == "develop" || name == "departure") return PhraseIntent::Develop;
    if (name == "loop") return PhraseIntent::Loop;
    if (name == "color") return PhraseIntent::Color;
    throw std::runtime_error("unknown intent");
}
CadenceType cadenceFromName(std::string_view name) {
    if (name.empty() || name == "none") return CadenceType::None;
    if (name == "authentic") return CadenceType::Authentic;
    if (name == "perfect_authentic") return CadenceType::PerfectAuthentic;
    if (name == "imperfect_authentic") return CadenceType::ImperfectAuthentic;
    if (name == "plagal") return CadenceType::Plagal;
    if (name == "half") return CadenceType::Half;
    if (name == "deceptive") return CadenceType::Deceptive;
    if (name == "modal") return CadenceType::Modal;
    if (name == "loop_closure") return CadenceType::LoopClosure;
    if (name == "unknown") return CadenceType::Unknown;
    throw std::runtime_error("unknown cadence");
}
std::vector<std::string> split(std::string_view text, char delimiter) {
    std::vector<std::string> out;
    for (std::size_t at = 0; at < text.size();) {
        const auto end = text.find(delimiter, at);
        auto part = text.substr(at, end == std::string_view::npos ? end : end - at);
        if (!part.empty()) out.emplace_back(part);
        if (end == std::string_view::npos) break;
        at = end + 1;
    }
    return out;
}
int romanDegree(std::string_view roman) {
    if (roman == "I" || roman == "i") return 1;
    if (roman == "II" || roman == "ii") return 2;
    if (roman == "III" || roman == "iii") return 3;
    if (roman == "IV" || roman == "iv") return 4;
    if (roman == "V" || roman == "v") return 5;
    if (roman == "VI" || roman == "vi") return 6;
    if (roman == "VII" || roman == "vii") return 7;
    throw std::runtime_error("invalid degree: " + std::string(roman));
}
std::pair<ScaleDegree, std::size_t> degreePrefix(std::string_view value) {
    std::size_t pos{}; int alteration{};
    if (value.starts_with('b')) { alteration = -1; ++pos; }
    else if (value.starts_with('#')) { alteration = 1; ++pos; }
    const auto start = pos;
    while (pos < value.size() && (value[pos] == 'I' || value[pos] == 'V' || value[pos] == 'i' || value[pos] == 'v')) ++pos;
    if (pos == start) throw std::runtime_error("missing degree");
    return {{romanDegree(value.substr(start, pos - start)), alteration}, pos};
}
std::vector<double> rhythm(std::string_view text, std::size_t count) {
    std::vector<double> result;
    if (text.empty()) return std::vector<double>(count, 4.0);
    std::istringstream input{std::string(text)};
    double duration{};
    while (input >> duration) {
        if (!std::isfinite(duration) || duration <= 0 || duration > 128) throw std::runtime_error("invalid rhythm");
        result.push_back(duration);
    }
    if (!input.eof() || result.size() != count) throw std::runtime_error("rhythm count mismatch");
    return result;
}
std::vector<MatchEvent> events(std::string_view notation, Mode mode, std::string_view rhythmText) {
    std::istringstream input{std::string(notation)};
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) tokens.push_back(token);
    if (tokens.empty() || tokens.size() > 64) throw std::runtime_error("template needs 1..64 events");
    const auto durations = rhythm(rhythmText, tokens.size());
    std::vector<MatchEvent> result;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        auto event = parseNotationEvent(tokens[i], mode, durations[i]);
        event.sourceIndex = i;
        result.push_back(event);
    }
    if (result.size() >= 2 && result.back().degree && result.back().degree->degree == 1 &&
        result[result.size() - 2].degree && result[result.size() - 2].degree->degree == 5) {
        result.back().roles |= flag(Role::Cadential);
        result[result.size() - 2].roles |= flag(Role::Cadential);
    }
    return result;
}
} // namespace

MatchEvent parseNotationEvent(std::string_view token, Mode mode, double durationQN) {
    if (!std::isfinite(durationQN) || durationQN <= 0) throw std::runtime_error("invalid duration");
    MatchEvent e;
    e.durationQN = durationQN;
    const auto slash = token.find('/');
    const auto body = token.substr(0, slash);
    const auto [degree, pos] = degreePrefix(body);
    e.degree = degree;
    const auto suffix = body.substr(pos);
    const auto lower = pos > 0 && std::islower(static_cast<unsigned char>(body[pos - 1]));
    if (suffix == "maj7") e.quality = ChordQuality::Major7;
    else if (suffix == "7") e.quality = lower ? ChordQuality::Minor7 : ChordQuality::Dominant7;
    else if (suffix == "dim") e.quality = ChordQuality::Diminished;
    else if (suffix == "dim7") e.quality = ChordQuality::Diminished7;
    else if (suffix == "m7b5") e.quality = ChordQuality::HalfDiminished7;
    else if (suffix == "m7") e.quality = ChordQuality::Minor7;
    else if (suffix.empty()) e.quality = lower ? ChordQuality::Minor : ChordQuality::Major;
    else throw std::runtime_error("unsupported quality: " + std::string(suffix));
    if (slash != std::string_view::npos) {
        const auto target = degreePrefix(token.substr(slash + 1));
        if (slash + 1 + target.second != token.size()) throw std::runtime_error("invalid target");
        e.target = target.first;
        // V/ii is an A-root chord in C major: its global degree is VI, not V.
        // The Roman prefix names its local function; alignment uses the
        // actual root degree relative to the candidate key.
        constexpr int majorTones[]{0, 2, 4, 5, 7, 9, 11};
        constexpr int minorTones[]{0, 2, 3, 5, 7, 8, 10};
        const auto* scale = mode == Mode::Major ? majorTones : minorTones;
        const bool leading = e.quality == ChordQuality::Diminished || e.quality == ChordQuality::Diminished7;
        const auto targetPitch = scale[target.first.degree - 1] + target.first.alteration;
        const auto rootPitch = (targetPitch + (leading ? 11 : 7)) % 12;
        int bestDegree = 1, bestAlteration = 0, bestDistance = 12;
        for (int degreeIndex = 0; degreeIndex < 7; ++degreeIndex) {
            const auto alteration = (rootPitch - scale[degreeIndex] + 18) % 12 - 6;
            if (std::abs(alteration) < bestDistance) {
                bestDistance = std::abs(alteration);
                bestDegree = degreeIndex + 1;
                bestAlteration = alteration;
            }
        }
        e.degree = ScaleDegree{bestDegree, bestAlteration};
        e.roles |= flag(e.quality == ChordQuality::Diminished || e.quality == ChordQuality::Diminished7
                            ? Role::SecondaryLeadingTone : Role::SecondaryDominant);
        e.function = HarmonicFunction::ChromaticDominant;
        e.structuralWeight = durationQN < 2 ? 0.4f : 0.58f;
    } else if (degree.degree == 5 || degree.degree == 7) e.function = HarmonicFunction::Dominant;
    else if (degree.degree == 2 || degree.degree == 4) e.function = HarmonicFunction::Predominant;
    else if (degree.degree == 1) e.function = HarmonicFunction::Tonic;
    else e.function = HarmonicFunction::TonicProlongation;
    if (!e.target && degree.alteration != 0) {
        e.roles |= flag(Role::Borrowed);
        e.function = HarmonicFunction::ChromaticPredominant;
    }
    if (!e.target && mode == Mode::Major && degree.degree == 4 && lower) {
        e.roles |= flag(Role::Borrowed);
        e.function = HarmonicFunction::ChromaticPredominant;
    }
    if (!e.target && degree.degree == 1 && degree.alteration == 1 &&
        (e.quality == ChordQuality::Diminished || e.quality == ChordQuality::Diminished7)) {
        // #I°7 approaches ii in the major-key fixtures. Phase 1 recognizes
        // the same sounding chord as vii°7/ii, so carry that target as well.
        e.roles &= ~flag(Role::Borrowed);
        e.roles |= flag(Role::SecondaryLeadingTone) | flag(Role::Approach);
        e.target = ScaleDegree{2, 0};
        e.function = HarmonicFunction::ChromaticDominant;
        e.structuralWeight = 0.25f;
    }
    if (e.structuralWeight == 0.f) e.structuralWeight = durationQN < 1 ? 0.52f : 0.78f;
    if (e.structuralWeight >= 0.55f) e.roles |= flag(Role::Structural);
    else e.roles |= flag(Role::Embellishing);
    return e;
}

JsonTemplates parseTemplateJson(std::string_view text, bool requireFactoryMetadata) {
    JsonTemplates result;
    try {
        if (text.size() > 2 * 1024 * 1024) throw std::runtime_error("template JSON exceeds 2 MiB");
        Input input{text}; input.need('[');
        std::unordered_set<std::string> ids;
        if (!input.take(']')) while (true) {
            if (result.templates.size() >= 4096) throw std::runtime_error("too many templates");
            input.need('{');
            ProgressionTemplate item;
            std::string sequence, rhythmText, modeText, cadenceText, intentText, stylesText, tagsText, meterText,
                        skeletonText;
            bool idSeen{}, sequenceSeen{}, nameSeen{}, modeSeen{}, intentSeen{}, cadenceSeen{},
                 loopSeen{}, meterSeen{}, stylesSeen{}, sourceSeen{}, priorSeen{}, complexitySeen{},
                 versionSeen{}, phraseSeen{};
            std::unordered_set<std::string> seenFields;
            while (!input.take('}')) {
                const auto key = input.string(); input.need(':');
                if (!seenFields.insert(key).second) throw std::runtime_error("duplicate JSON field: " + key);
                if (key == "id") { item.id = input.string(); idSeen = true; }
                else if (key == "name") { item.name = input.string(); nameSeen = true; }
                else if (key == "mode") { modeText = input.string(); modeSeen = true; }
                else if (key == "sequence") { sequence = input.string(); sequenceSeen = true; }
                else if (key == "rhythm") rhythmText = input.string();
                else if (key == "cadence") { cadenceText = input.string(); cadenceSeen = true; }
                else if (key == "intent") { intentText = input.string(); intentSeen = true; }
                else if (key == "loopable") { item.loopable = input.boolean(); loopSeen = true; }
                else if (key == "styles") { stylesText = input.string(); stylesSeen = true; }
                else if (key == "tags") tagsText = input.string();
                else if (key == "skeleton") skeletonText = input.string();
                else if (key == "meter") { meterText = input.string(); meterSeen = true; }
                else if (key == "sourceType") { item.sourceType = input.string(); sourceSeen = true; }
                else if (key == "priorWeight") { item.priorWeight = static_cast<float>(input.number()); priorSeen = true; }
                else if (key == "complexity") { item.complexity = static_cast<float>(input.number()); complexitySeen = true; }
                else if (key == "version") {
                    const auto value = input.number();
                    if (std::floor(value) != value || value < 1 || value > 1000) throw std::runtime_error("invalid version");
                    item.version = static_cast<int>(value); versionSeen = true;
                }
                else if (key == "phraseLength") {
                    const auto value = input.number();
                    if (std::floor(value) != value || value < 2 || value > 64) throw std::runtime_error("invalid phraseLength");
                    item.phraseLength = static_cast<std::size_t>(value); phraseSeen = true;
                }
                else if (key == "secondaryIntents") {
                    const auto value = input.number();
                    if (std::floor(value) != value || value < 0 || value > 255) throw std::runtime_error("invalid secondaryIntents");
                    item.secondaryIntents = static_cast<StyleFlags>(value);
                }
                else throw std::runtime_error("unsupported template field: " + key);
                if (input.take('}')) break;
                input.need(',');
            }
            if (!idSeen || item.id.empty() || !sequenceSeen) throw std::runtime_error("template needs id and sequence");
            if (requireFactoryMetadata && (!nameSeen || item.name.empty() || !modeSeen || !intentSeen ||
                !cadenceSeen || !loopSeen || !meterSeen || !stylesSeen || stylesText.empty() || !sourceSeen ||
                !priorSeen || !complexitySeen || !versionSeen || !phraseSeen))
                throw std::runtime_error("factory schema required metadata missing");
            if (!ids.insert(item.id).second) throw std::runtime_error("duplicate template id");
            if (modeText == "major") item.mode = Mode::Major;
            else if (modeText == "minor") item.mode = Mode::Minor;
            else throw std::runtime_error("mode must be major or minor");
            item.cadence = cadenceFromName(cadenceText);
            item.intent = intentFromName(intentText);
            if (meterText.size() == 3 && meterText[1] == '/' && meterText[0] >= '1' && meterText[0] <= '9' &&
                (meterText[2] == '4' || meterText[2] == '8')) {
                item.meterNumerator = meterText[0] - '0'; item.meterDenominator = meterText[2] - '0';
            } else if (!meterText.empty()) throw std::runtime_error("unsupported meter");
            for (const auto& part : split(stylesText, ',')) {
                const auto colon = part.find(':');
                const auto style = styleFromName(std::string_view(part).substr(0, colon));
                float weight = 1.f;
                if (colon != std::string::npos) {
                    const auto value = std::string_view(part).substr(colon + 1);
                    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), weight);
                    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                        !std::isfinite(weight) || weight <= 0.f || weight > 1.f) throw std::runtime_error("invalid style weight");
                }
                if (item.styles & static_cast<StyleFlags>(style)) throw std::runtime_error("duplicate style");
                item.styles |= static_cast<StyleFlags>(style); item.styleWeights.emplace_back(style, weight);
            }
            item.tags = split(tagsText, ',');
            if (item.priorWeight < 0.f || item.priorWeight > 1.f || item.complexity < 0.f || item.complexity > 1.f ||
                item.version < 1 || (item.sourceType != "factory" && item.sourceType != "user"))
                throw std::runtime_error("invalid metadata range");
            item.full = events(sequence, item.mode, rhythmText);
            for (const auto& value : split(skeletonText, ',')) {
                std::size_t index{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), index);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    index >= item.full.size() || (!item.skeletonIndices.empty() && index <= item.skeletonIndices.back()))
                    throw std::runtime_error("invalid skeleton index");
                item.skeletonIndices.push_back(index);
            }
            if (item.full.size() < 2) throw std::runtime_error("template needs at least two events");
            if (!item.phraseLength) item.phraseLength = item.full.size();
            if (item.phraseLength != item.full.size()) throw std::runtime_error("phrase boundary must be template end");
            if (item.name.empty()) item.name = sequence;
            prepareTemplate(item);
            result.templates.push_back(std::move(item));
            if (input.take(']')) break;
            input.need(',');
        }
        input.space();
        if (input.at != text.size()) throw std::runtime_error("trailing JSON content");
        if (result.templates.empty()) throw std::runtime_error("empty template library");
    } catch (const std::exception& e) { result.templates.clear(); result.error = e.what(); }
    return result;
}
JsonTemplates loadDevelopmentTemplates() { return parseTemplateJson(kDevelopmentTemplatesJson); }
std::string serializeTemplateJson(const ProgressionTemplate& item) {
    auto quoted = [](std::string_view value) {
        std::string out{"\""};
        for (char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else out += c;
        }
        out += '"'; return out;
    };
    auto cadenceName = [](CadenceType value) -> const char* {
        switch (value) {
            case CadenceType::Authentic: return "authentic";
            case CadenceType::PerfectAuthentic: return "perfect_authentic";
            case CadenceType::ImperfectAuthentic: return "imperfect_authentic";
            case CadenceType::Plagal: return "plagal";
            case CadenceType::Half: return "half";
            case CadenceType::Deceptive: return "deceptive";
            case CadenceType::Modal: return "modal";
            case CadenceType::LoopClosure: return "loop_closure";
            case CadenceType::Unknown: return "unknown";
            default: return "none";
        }
    };
    auto intentName = [](PhraseIntent value) -> const char* {
        switch (value) {
            case PhraseIntent::Develop: return "develop";
            case PhraseIntent::Resolve: return "resolve";
            case PhraseIntent::Loop: return "loop";
            case PhraseIntent::Color: return "color";
            default: return "neutral";
        }
    };
    auto styleName = [](Style value) -> const char* {
        switch (value) {
            case Style::Pop: return "pop"; case Style::Rock: return "rock";
            case Style::Rnb: return "rnb"; case Style::Jazz: return "jazz";
            case Style::CityPop: return "citypop"; default: return "functional";
        }
    };
    std::ostringstream sequence, rhythm, styles, tags, skeleton;
    for (std::size_t i = 0; i < item.full.size(); ++i) {
        if (i) { sequence << ' '; rhythm << ' '; }
        sequence << formatMatchEvent(item.full[i]);
        rhythm << std::setprecision(9) << item.full[i].durationQN.value_or(4.0);
    }
    for (std::size_t i = 0; i < item.styleWeights.size(); ++i) {
        if (i) styles << ',';
        styles << styleName(item.styleWeights[i].first) << ':' << item.styleWeights[i].second;
    }
    for (std::size_t i = 0; i < item.tags.size(); ++i) { if (i) tags << ','; tags << item.tags[i]; }
    for (std::size_t i = 0; i < item.skeletonIndices.size(); ++i) {
        if (i) skeleton << ',';
        skeleton << item.skeletonIndices[i];
    }
    std::ostringstream out;
    out << '{' << "\"id\":" << quoted(item.id) << ",\"name\":" << quoted(item.name)
        << ",\"mode\":" << quoted(item.mode == Mode::Major ? "major" : "minor")
        << ",\"sequence\":" << quoted(sequence.str()) << ",\"rhythm\":" << quoted(rhythm.str())
        << ",\"cadence\":" << quoted(cadenceName(item.cadence))
        << ",\"intent\":" << quoted(intentName(item.intent))
        << ",\"loopable\":" << (item.loopable ? "true" : "false")
        << ",\"styles\":" << quoted(styles.str()) << ",\"tags\":" << quoted(tags.str())
        << ",\"skeleton\":" << quoted(skeleton.str())
        << ",\"meter\":" << quoted(std::to_string(item.meterNumerator) + "/" + std::to_string(item.meterDenominator))
        << ",\"sourceType\":" << quoted(item.sourceType)
        << ",\"priorWeight\":" << item.priorWeight << ",\"complexity\":" << item.complexity
        << ",\"version\":" << item.version << ",\"phraseLength\":" << item.phraseLength
        << ",\"secondaryIntents\":" << item.secondaryIntents << '}';
    return out.str();
}
} // namespace harmony::dev
