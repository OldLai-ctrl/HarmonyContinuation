#include "TemplateJson.h"
#include "EmbeddedTemplates.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
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
};
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

JsonTemplates parseTemplateJson(std::string_view text) {
    JsonTemplates result;
    try {
        if (text.size() > 2 * 1024 * 1024) throw std::runtime_error("template JSON exceeds 2 MiB");
        Input input{text}; input.need('[');
        std::unordered_set<std::string> ids;
        if (!input.take(']')) while (true) {
            if (result.templates.size() >= 4096) throw std::runtime_error("too many templates");
            input.need('{');
            ProgressionTemplate item;
            std::string sequence, rhythmText, modeText, cadenceText, intentText;
            bool idSeen{}, sequenceSeen{};
            while (!input.take('}')) {
                const auto key = input.string(); input.need(':');
                if (key == "id") { item.id = input.string(); idSeen = true; }
                else if (key == "name") item.name = input.string();
                else if (key == "mode") modeText = input.string();
                else if (key == "sequence") { sequence = input.string(); sequenceSeen = true; }
                else if (key == "rhythm") rhythmText = input.string();
                else if (key == "cadence") cadenceText = input.string();
                else if (key == "intent") intentText = input.string();
                else if (key == "loopable") item.loopable = input.boolean();
                else throw std::runtime_error("unsupported template field: " + key);
                if (input.take('}')) break;
                input.need(',');
            }
            if (!idSeen || item.id.empty() || !sequenceSeen) throw std::runtime_error("template needs id and sequence");
            if (!ids.insert(item.id).second) throw std::runtime_error("duplicate template id");
            if (modeText == "major") item.mode = Mode::Major;
            else if (modeText == "minor") item.mode = Mode::Minor;
            else throw std::runtime_error("mode must be major or minor");
            if (cadenceText == "authentic") item.cadence = CadenceType::Authentic;
            else if (cadenceText == "plagal") item.cadence = CadenceType::Plagal;
            else if (cadenceText == "half") item.cadence = CadenceType::Half;
            else if (cadenceText == "deceptive") item.cadence = CadenceType::Deceptive;
            else if (!cadenceText.empty()) throw std::runtime_error("unknown cadence");
            if (intentText == "cadence") item.intent = PhraseIntent::Cadence;
            else if (intentText == "loop") item.intent = PhraseIntent::Loop;
            else if (intentText == "departure") item.intent = PhraseIntent::Departure;
            else if (!intentText.empty()) throw std::runtime_error("unknown intent");
            item.full = events(sequence, item.mode, rhythmText);
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
} // namespace harmony::dev
