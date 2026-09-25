#include "VstXmlDropAdapter.h"
#include <charconv>
#include <map>
#include <stdexcept>
#include <utility>

namespace harmony::plugin {
namespace {
struct Failure { ParseStatus status; const char* message; };
[[noreturn]] void fail(ParseStatus status, const char* message) { throw Failure{status, message}; }
struct Element {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<Element> children;
    std::string text;
};
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool nameStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool nameChar(char c) { return nameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.'; }

// A deliberately restricted XML subset for fixtures, not a general XML implementation.
// Rejects declarations, DTD, CDATA, namespaces and non-predefined entity references.
class FixtureXml {
public:
    explicit FixtureXml(std::string_view input) : input_(input) {}
    Element read() {
        whitespace();
        auto root = element(0);
        whitespace();
        if (position_ != input_.size()) invalid();
        return root;
    }
private:
    std::string_view input_;
    std::size_t position_{};
    std::size_t nodes_{};
    [[noreturn]] void invalid() { fail(ParseStatus::InvalidXml, "Malformed or unsupported fixture XML syntax"); }
    void whitespace() { while (position_ < input_.size() && space(input_[position_])) ++position_; }
    bool take(std::string_view token) {
        if (input_.substr(position_, token.size()) != token) return false;
        position_ += token.size(); return true;
    }
    std::string name() {
        if (position_ == input_.size() || !nameStart(input_[position_])) invalid();
        const auto start = position_++;
        while (position_ < input_.size() && nameChar(input_[position_])) ++position_;
        return std::string(input_.substr(start, position_ - start));
    }
    std::string decoded(std::string_view raw) {
        std::string result;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const auto c = static_cast<unsigned char>(raw[i]);
            // Fixture grammar is ASCII; actual encoding must be learned from host payloads.
            if ((c < 32 && !space(static_cast<char>(c))) || c >= 127 || c == '<') invalid();
            if (c != '&') { result += static_cast<char>(c); continue; }
            const auto end = raw.find(';', i);
            if (end == std::string_view::npos) invalid();
            const auto entity = raw.substr(i, end - i + 1);
            if (entity == "&amp;") result += '&';
            else if (entity == "&lt;") result += '<';
            else if (entity == "&gt;") result += '>';
            else if (entity == "&quot;") result += '"';
            else if (entity == "&apos;") result += '\'';
            else invalid();
            i = end;
        }
        return result;
    }
    Element element(unsigned depth) {
        if (depth > 8 || ++nodes_ > VstXmlDropAdapter::maxChords * 2 + 1)
            fail(ParseStatus::ResourceLimit, "XML depth/node limit exceeded");
        if (!take("<")) invalid();
        Element node;
        node.name = name();
        while (true) {
            const auto before = position_;
            whitespace();
            if (take("/>")) return node;
            if (take(">")) break;
            if (before == position_) invalid();
            auto key = name(); whitespace();
            if (!take("=")) invalid();
            whitespace();
            if (position_ == input_.size()) invalid();
            const auto quote = input_[position_++];
            if (quote != '\'' && quote != '"') invalid();
            const auto end = input_.find(quote, position_);
            if (end == std::string_view::npos) invalid();
            auto value = decoded(input_.substr(position_, end - position_));
            if (!node.attributes.emplace(std::move(key), std::move(value)).second) invalid();
            position_ = end + 1;
        }
        while (true) {
            if (take("</")) {
                if (name() != node.name) invalid();
                whitespace(); if (!take(">")) invalid();
                return node;
            }
            if (position_ == input_.size()) invalid();
            if (input_[position_] == '<') node.children.push_back(element(depth + 1));
            else {
                const auto end = input_.find('<', position_);
                if (end == std::string_view::npos) invalid();
                const auto raw = input_.substr(position_, end - position_);
                if (raw.find("]]>") != std::string_view::npos) invalid();
                node.text += decoded(raw);
                position_ = end;
            }
        }
    }
};
std::optional<std::string> attribute(const Element& node, const char* key) {
    const auto it = node.attributes.find(key);
    if (it == node.attributes.end()) return std::nullopt;
    return it->second;
}
std::string required(const Element& node, const char* key) {
    auto value = attribute(node, key);
    if (!value || value->empty()) fail(ParseStatus::InvalidChord, "Missing required chord/time attribute");
    return *value;
}
PitchClass pitch(const std::string& value) {
    int number{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || number < 0 || number > 11)
        fail(ParseStatus::InvalidChord, "Pitch class must be an integer in [0,11]; encoding is not guessed");
    return static_cast<PitchClass>(number);
}
ChordQuality quality(std::string_view type) {
    using Q = ChordQuality;
    if (type == "Major") return Q::Major;
    if (type == "Minor") return Q::Minor;
    if (type == "7") return Q::Dominant7;
    if (type == "Maj7") return Q::Major7;
    if (type == "m7") return Q::Minor7;
    if (type == "dim") return Q::Diminished;
    if (type == "dim7") return Q::Diminished7;
    if (type == "m7b5") return Q::HalfDiminished7;
    if (type == "sus2") return Q::Sus2;
    if (type == "sus4") return Q::Sus4;
    if (type == "aug") return Q::Augmented;
    return Q::Unknown;
}
bool blank(const std::string& text) { return std::all_of(text.begin(), text.end(), space); }
}

ParseResult VstXmlDropAdapter::parseSyntheticV1(std::string_view xml) const noexcept {
    // Even allocation failure cannot escape this public adapter boundary.
    ParseResult result;
    try {
        if (xml.size() > maxPayloadBytes) fail(ParseStatus::ResourceLimit, "Payload exceeds 1 MiB limit");
        const auto root = FixtureXml(xml).read();
        if (root.name != "harmony-spike-fixture" || attribute(root, "schema") != "synthetic-v1" || !blank(root.text))
            fail(ParseStatus::UnsupportedSchema, "Not synthetic-v1; actual Cubase VST-XML schema is not verified");
        if (root.children.empty()) fail(ParseStatus::InvalidChord, "No chord events");
        if (root.children.size() > maxChords) fail(ParseStatus::ResourceLimit, "Too many chord events");
        for (const auto& node : root.children) {
            if (node.name != "chord" || node.children.size() != 1 || !blank(node.text))
                fail(ParseStatus::UnsupportedSchema, "Expected chord with one projectTime child");
            const auto& time = node.children.front();
            if (time.name != "projectTime" || !time.children.empty())
                fail(ParseStatus::UnsupportedSchema, "Expected projectTime leaf");
            if (required(time, "domain") != "quarterNotes")
                fail(ParseStatus::UnsupportedTimeDomain, "Unsupported time domain (only quarterNotes accepted)");
            std::string_view value(time.text);
            while (!value.empty() && space(value.front())) value.remove_prefix(1);
            while (!value.empty() && space(value.back())) value.remove_suffix(1);
            if (value.empty()) fail(ParseStatus::InvalidChord, "Empty projectTime");
            ChordEvent chord;
            const auto converted = std::from_chars(value.data(), value.data() + value.size(), chord.startQN);
            if (converted.ec != std::errc{} || converted.ptr != value.data() + value.size() || !std::isfinite(chord.startQN))
                fail(ParseStatus::InvalidChord, "Invalid or non-finite projectTime");
            chord.root = pitch(required(node, "keyNote"));
            if (const auto bass = attribute(node, "bassNote")) chord.bass = pitch(*bass);
            chord.name = attribute(node, "name").value_or("(unnamed)");
            chord.extensions = {attribute(node, "pitches"), attribute(node, "mask"), attribute(node, "type")};
            chord.quality = quality(chord.extensions.type.value_or(""));
            result.chords.push_back(std::move(chord));
        }
        if (!sortAndInferDurations(result.chords)) fail(ParseStatus::InvalidChord, "Duration overflow");
        result.status = ParseStatus::Success;
        result.detail = "Synthetic fixture parsed; this does not verify Cubase interoperability";
    } catch (const Failure& error) {
        result.chords.clear(); result.status = error.status;
        try { result.detail = error.message; } catch (...) {}
    } catch (...) {
        result.chords.clear(); result.status = ParseStatus::InternalError;
        try { result.detail = "Internal parser error or allocation failure"; } catch (...) {}
    }
    return result;
}
} // namespace harmony::plugin
