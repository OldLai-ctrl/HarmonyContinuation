#include "ProgressionJson.h"

#include <charconv>
#include <cmath>
#include <stdexcept>

namespace harmony::dev {
namespace {
struct Input {
    std::string_view text;
    std::size_t at{};
    void space() { while (at < text.size() && (text[at] == ' ' || text[at] == '\n' || text[at] == '\r' || text[at] == '\t')) ++at; }
    bool take(char c) { space(); if (at < text.size() && text[at] == c) { ++at; return true; } return false; }
    void need(char c) { if (!take(c)) throw std::runtime_error(std::string("expected '") + c + "'"); }
    std::string string() {
        space(); need('"');
        std::string out;
        while (at < text.size()) {
            const char c = text[at++];
            if (c == '"') return out;
            if (c == '\\') {
                if (at == text.size()) break;
                const char escaped = text[at++];
                if (escaped == '"' || escaped == '\\' || escaped == '/') out += escaped;
                else if (escaped == 'n') out += '\n';
                else if (escaped == 't') out += '\t';
                else throw std::runtime_error("unsupported JSON escape");
            } else out += c;
        }
        throw std::runtime_error("unterminated JSON string");
    }
    double number() {
        space();
        double value{};
        const auto parsed = std::from_chars(text.data() + at, text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr == text.data() + at || !std::isfinite(value))
            throw std::runtime_error("invalid finite number");
        at = static_cast<std::size_t>(parsed.ptr - text.data());
        return value;
    }
};
} // namespace

JsonProgression parseProgressionJson(std::string_view text) {
    JsonProgression result;
    try {
        if (text.size() > 1024 * 1024) throw std::runtime_error("JSON input exceeds 1 MiB");
        Input input{text};
        input.need('[');
        while (!input.take(']')) {
            if (result.events.size() >= 4096) throw std::runtime_error("too many chords");
            input.need('{');
            ChordEvent event;
            bool nameSeen{}, startSeen{}, durationSeen{};
            while (!input.take('}')) {
                const auto key = input.string();
                input.need(':');
                if (key == "name") { event.name = input.string(); nameSeen = true; }
                else if (key == "start") { event.startQN = input.number(); startSeen = true; }
                else if (key == "duration") {
                    const auto duration = input.number();
                    if (duration <= 0) throw std::runtime_error("duration must be positive");
                    event.durationQN = duration; durationSeen = true;
                } else if (key == "keyNote") {
                    const auto value = input.number();
                    if (std::floor(value) != value || value < -100000 || value > 100000) throw std::runtime_error("invalid keyNote");
                    event.keyNoteValue = static_cast<std::int32_t>(value);
                } else if (key == "bassNote") {
                    const auto value = input.number();
                    if (std::floor(value) != value || value < -100000 || value > 100000) throw std::runtime_error("invalid bassNote");
                    event.bassNoteValue = static_cast<std::int32_t>(value);
                } else if (key == "mask") event.extensions.mask = input.string();
                else if (key == "pitches") event.extensions.pitches = input.string();
                else throw std::runtime_error("unsupported JSON field: " + key);
                if (input.take('}')) break;
                input.need(',');
            }
            if (!nameSeen || !startSeen) throw std::runtime_error("every chord needs name and start");
            event.openEnded = !durationSeen;
            result.events.push_back(std::move(event));
            if (input.take(']')) break;
            input.need(',');
        }
        input.space();
        if (input.at != text.size()) throw std::runtime_error("trailing JSON content");
        if (result.events.empty()) throw std::runtime_error("empty progression");
        for (std::size_t i = 1; i < result.events.size(); ++i)
            if (result.events[i].startQN < result.events[i - 1].startQN)
                throw std::runtime_error("chords must be ordered by start");
    } catch (const std::exception& error) {
        result.events.clear(); result.error = error.what();
    }
    return result;
}
} // namespace harmony::dev
