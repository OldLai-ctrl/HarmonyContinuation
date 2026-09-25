#include "HarmonyAnalysis.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <string_view>

namespace harmony {
namespace {
constexpr std::uint16_t bit(int semitone) { return std::uint16_t(1u << semitone); }
int pc(int note) { return (note % 12 + 12) % 12; }

std::optional<PitchClass> spelledRoot(std::string_view name, std::string& spelling) {
    if (name.empty()) return std::nullopt;
    int value;
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())))) {
        case 'C': value = 0; break; case 'D': value = 2; break;
        case 'E': value = 4; break; case 'F': value = 5; break;
        case 'G': value = 7; break; case 'A': value = 9; break;
        case 'B': value = 11; break; default: return std::nullopt;
    }
    spelling.assign(name.substr(0, 1));
    if (name.size() > 1 && (name[1] == '#' || name[1] == 'b')) {
        value += name[1] == '#' ? 1 : -1;
        spelling.push_back(name[1]);
    }
    return static_cast<PitchClass>(pc(value));
}

ChordQuality namedQuality(std::string_view name, std::size_t rootLength) {
    name.remove_prefix(std::min(rootLength, name.size()));
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower.starts_with("m7b5") || lower.starts_with("ø")) return ChordQuality::HalfDiminished7;
    if (lower.starts_with("dim7") || lower.starts_with("o7")) return ChordQuality::Diminished7;
    if (lower.starts_with("dim") || lower.starts_with("o")) return ChordQuality::Diminished;
    if (lower.starts_with("sus2")) return ChordQuality::Sus2;
    if (lower.starts_with("sus4") || lower.starts_with("sus")) return ChordQuality::Sus4;
    if (lower.starts_with("aug") || lower.starts_with("+")) return ChordQuality::Augmented;
    if (lower.starts_with("maj9") || lower.starts_with("maj7") || lower.starts_with("ma7") || lower.starts_with("Δ7")) return ChordQuality::Major7;
    if (lower.starts_with("maj")) return ChordQuality::Major;
    if (lower.starts_with("min9") || lower.starts_with("m9") || lower.starts_with("min7") ||
        lower.starts_with("m7") || lower.starts_with("-7")) return ChordQuality::Minor7;
    if (lower.starts_with("min") || lower.starts_with("m") || lower.starts_with("-")) return ChordQuality::Minor;
    if (lower.starts_with("7") || lower.starts_with("9") || lower.starts_with("13")) return ChordQuality::Dominant7;
    if (lower.empty() || lower.starts_with("6")) return ChordQuality::Major;
    return ChordQuality::Unknown;
}

std::uint16_t namedColor(std::string_view name, std::size_t rootLength) {
    name.remove_prefix(std::min(rootLength, name.size()));
    std::string lower;
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower.find("b9") != std::string::npos) return bit(1);
    if (lower.find("#9") != std::string::npos) return bit(3);
    if (lower.find("13") != std::string::npos || lower == "6") return bit(9);
    if (lower.find('9') != std::string::npos) return bit(2);
    return 0;
}

std::optional<std::uint16_t> rawMask(std::string_view text) {
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    unsigned value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value > 0xFFF)
        return std::nullopt;
    return static_cast<std::uint16_t>(value);
}

std::optional<std::uint16_t> pitchIntervals(std::string_view text, int root) {
    std::uint16_t mask{};
    bool any{};
    while (!text.empty()) {
        const auto end = text.find(';');
        const auto token = text.substr(0, end);
        if (!token.empty()) {
            std::string spelling;
            const auto note = spelledRoot(token, spelling);
            if (!note) return std::nullopt;
            mask |= bit(pc(static_cast<int>(*note) - root));
            any = true;
        }
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return any ? std::optional(mask) : std::nullopt;
}

ChordQuality classify(std::uint16_t mask) {
    const auto contains = [mask](int a, int b, int c) {
        return (mask & (bit(a) | bit(b) | bit(c))) == (bit(a) | bit(b) | bit(c));
    };
    if (contains(0, 3, 6)) {
        if (mask & bit(9)) return ChordQuality::Diminished7;
        if (mask & bit(10)) return ChordQuality::HalfDiminished7;
        return ChordQuality::Diminished;
    }
    if (contains(0, 4, 8)) return ChordQuality::Augmented;
    if (contains(0, 4, 7)) {
        if (mask & bit(11)) return ChordQuality::Major7;
        if (mask & bit(10)) return ChordQuality::Dominant7;
        return ChordQuality::Major;
    }
    if (contains(0, 3, 7)) return mask & bit(10) ? ChordQuality::Minor7 : ChordQuality::Minor;
    if (contains(0, 2, 7)) return ChordQuality::Sus2;
    if (contains(0, 5, 7)) return ChordQuality::Sus4;
    return ChordQuality::Unknown;
}

std::uint16_t essential(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major: return bit(0)|bit(4)|bit(7);
        case ChordQuality::Minor: return bit(0)|bit(3)|bit(7);
        case ChordQuality::Dominant7: return bit(0)|bit(4)|bit(7)|bit(10);
        case ChordQuality::Major7: return bit(0)|bit(4)|bit(7)|bit(11);
        case ChordQuality::Minor7: return bit(0)|bit(3)|bit(7)|bit(10);
        case ChordQuality::Diminished: return bit(0)|bit(3)|bit(6);
        case ChordQuality::Diminished7: return bit(0)|bit(3)|bit(6)|bit(9);
        case ChordQuality::HalfDiminished7: return bit(0)|bit(3)|bit(6)|bit(10);
        case ChordQuality::Sus2: return bit(0)|bit(2)|bit(7);
        case ChordQuality::Sus4: return bit(0)|bit(5)|bit(7);
        case ChordQuality::Augmented: return bit(0)|bit(4)|bit(8);
        default: return 0;
    }
}
} // namespace

NormalizedChord normalizeChord(const ChordEvent& event) {
    NormalizedChord result;
    std::string spelling;
    const auto fromName = spelledRoot(event.name, spelling);
    if (fromName) result.rootSpelling = spelling;
    if (event.root) result.root = event.root;
    else if (event.keyNoteValue) result.root = static_cast<PitchClass>(pc(*event.keyNoteValue));
    else result.root = fromName;
    if (event.bass) result.bass = event.bass;
    else if (event.bassNoteValue) result.bass = static_cast<PitchClass>(pc(*event.bassNoteValue));
    else result.bass = result.root;
    if (result.root && fromName && *result.root != *fromName)
        result.diagnostics.push_back("name and structured root disagree");
    if (!result.root) {
        result.diagnostics.push_back("root could not be determined");
        return result;
    }
    const auto expected = namedQuality(event.name, spelling.size());
    std::optional<std::uint16_t> rawIntervalMask;
    if (event.extensions.mask) {
        rawIntervalMask = rawMask(*event.extensions.mask);
        if (!rawIntervalMask) result.diagnostics.push_back("invalid interval mask");
    }
    std::optional<std::uint16_t> pitches;
    if (event.extensions.pitches) {
        pitches = pitchIntervals(*event.extensions.pitches, static_cast<int>(*result.root));
        if (!pitches) result.diagnostics.push_back("pitches could not be decoded");
    }
    // Real Cubase triads can use a root-omitted mask: 0x48 accompanies
    // C;E;G; (0x091 relative to C), while seventh chords can use 0x891
    // directly. Resolve the encoding against the explicit pitch list.
    const bool rootOmittedMask = rawIntervalMask && pitches && *rawIntervalMask <= 0x7ff &&
        static_cast<std::uint16_t>((*rawIntervalMask << 1) | 1u) == *pitches;
    if (rawIntervalMask && pitches && *rawIntervalMask != *pitches && !rootOmittedMask)
        result.diagnostics.push_back("mask and pitches disagree; using pitches");
    const auto maskQuality = rawIntervalMask ? classify(*rawIntervalMask) : ChordQuality::Unknown;
    const auto pitchQuality = pitches ? classify(*pitches) : ChordQuality::Unknown;
    std::optional<std::uint16_t> intervals;
    if (pitches && pitchQuality != ChordQuality::Unknown) intervals = pitches;
    else if (rawIntervalMask && maskQuality != ChordQuality::Unknown) intervals = rawIntervalMask;
    else if (pitches) intervals = pitches;
    else intervals = rawIntervalMask;
    result.intervalMask = intervals.value_or(0);
    result.quality = intervals ? classify(*intervals) : event.quality;
    if (result.quality == ChordQuality::Unknown) {
        if (event.quality != ChordQuality::Unknown) result.quality = event.quality;
        else if (!intervals) result.quality = expected;
    }
    if (intervals && result.quality == ChordQuality::Unknown)
        result.diagnostics.push_back("interval structure is not recognized");
    if (expected != ChordQuality::Unknown && result.quality != ChordQuality::Unknown && expected != result.quality)
        result.diagnostics.push_back("name and interval structure disagree");
    if (event.quality != ChordQuality::Unknown && intervals && event.quality != result.quality)
        result.diagnostics.push_back("input quality and interval structure disagree");
    if (!intervals) result.intervalMask = essential(result.quality) | namedColor(event.name, spelling.size());
    result.colorMask = result.intervalMask & ~essential(result.quality);
    result.inversion = result.bass && *result.bass != *result.root;
    result.confidence = intervals ? 0.96f : (event.quality != ChordQuality::Unknown ? 0.88f : 0.66f);
    result.confidence = std::max(0.15f, result.confidence - 0.20f * static_cast<float>(result.diagnostics.size()));
    if (result.quality == ChordQuality::Unknown) result.confidence = std::min(result.confidence, 0.35f);
    return result;
}

} // namespace harmony
