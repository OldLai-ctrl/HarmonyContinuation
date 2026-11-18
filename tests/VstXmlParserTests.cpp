#include "plugin/VstXmlDropAdapter.h"
#include "ui/ProgressionTimeline.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <utility>

using namespace harmony;
using namespace harmony::plugin;
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // Never compiled out in Release.
}
std::string fixture(const char* name) {
    std::ifstream file(std::string(HC_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open fixture");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
std::string wrap(std::string body) {
    return "<harmony-spike-fixture schema=\"synthetic-v1\">" + body + "</harmony-spike-fixture>";
}
std::string chord(std::string time = "0", std::string attributes = "keyNote=\"0\"", std::string domain = "quarterNotes") {
    return "<chord " + attributes + "><projectTime domain=\"" + domain + "\">" + time + "</projectTime></chord>";
}
}
int main() {
    try {
        const VstXmlDropAdapter parser;
        const auto normal = parser.parseSyntheticV1(fixture("normal.xml"));
        check(normal.ok() && normal.chords.size() == 4, "Case 1: four events");
        check(normal.chords[0].root == PitchClass::C && normal.chords[0].bass == PitchClass::C,
              "Structured root and bass");
        check(normal.chords[0].name == "Cmaj7" && normal.chords[0].quality == ChordQuality::Major7,
              "Name and structured type");
        check(normal.chords[0].durationQN == 4 && normal.chords[1].durationQN == 2 && normal.chords[2].durationQN == 1,
              "Case 2: differing durations");
        check(!normal.chords[3].durationQN && normal.chords[3].openEnded && !normal.chords[0].openEnded,
              "Case 3: last event OPEN");
        const auto unordered = parser.parseSyntheticV1(fixture("unordered.xml"));
        check(unordered.ok() && unordered.chords.size() == 4, "Case 4: unordered input");
        for (std::size_t i = 0; i < 4; ++i) {
            check(unordered.chords[i].startQN == normal.chords[i].startQN, "Case 4: sorted positions");
            check(unordered.chords[i].durationQN == normal.chords[i].durationQN, "Case 4: sorted durations");
        }
        check(!unordered.chords[0].bass, "Case 5: missing bass remains unknown");
        const auto malformed = parser.parseSyntheticV1("<a><b></a>");
        check(malformed.status == ParseStatus::InvalidXml && malformed.chords.empty(), "Case 6: malformed XML");
        const auto unknown = parser.parseSyntheticV1(fixture("unknown.xml"));
        check(unknown.ok() && unknown.chords[0].quality == ChordQuality::Unknown &&
              unknown.chords[0].extensions.mask == "producer-defined" &&
              unknown.chords[0].extensions.pitches == "unverified", "Case 7: preserve unknown structure");
        check(parser.parseSyntheticV1(wrap(chord("10", "keyNote=\"0\"", "seconds"))).status == ParseStatus::UnsupportedTimeDomain,
              "Reject unsupported time domain");
        for (const auto value : {"nan", "inf", "1abc", "1e999", ""})
            check(!parser.parseSyntheticV1(wrap(chord(value))).ok(), "Reject invalid time");
        for (const auto value : {"-1", "12", "1.5", "C"})
            check(!parser.parseSyntheticV1(wrap(chord("0", std::string("keyNote=\"") + value + "\""))).ok(), "Reject invalid root");
        const auto atomic = parser.parseSyntheticV1(wrap(chord() + chord("bad")));
        check(!atomic.ok() && atomic.chords.empty() && !atomic.detail.empty(), "No misleading partial results");
        check(parser.parseSyntheticV1("<actualCubaseFormat/>").status == ParseStatus::UnsupportedSchema,
              "Never claim synthetic schema is real VST-XML");
        check(!parser.parseSyntheticV1(wrap(chord("0", "keyNote=\"0\" keyNote=\"1\""))).ok(), "Duplicate attribute");
        check(!parser.parseSyntheticV1("<!DOCTYPE x SYSTEM 'file:///test'>" + wrap(chord())).ok(), "External entities forbidden");
        check(parser.parseSyntheticV1(std::string(VstXmlDropAdapter::maxPayloadBytes + 1, 'x')).status == ParseStatus::ResourceLimit,
              "Payload size bound");
        std::string many;
        for (std::size_t i = 0; i <= VstXmlDropAdapter::maxChords; ++i) many += chord();
        check(parser.parseSyntheticV1(wrap(many)).status == ParseStatus::ResourceLimit, "Event count bound");
        const auto simultaneous = parser.parseSyntheticV1(wrap(chord("-1.25") + chord("-1.25")));
        check(simultaneous.ok() && simultaneous.chords[0].durationQN == 0, "Negative and simultaneous times");
        using Q = ChordQuality;
        const std::pair<const char*, Q> qualities[] = {
            {"Major", Q::Major}, {"Minor", Q::Minor}, {"7", Q::Dominant7}, {"Maj7", Q::Major7},
            {"m7", Q::Minor7}, {"dim", Q::Diminished}, {"dim7", Q::Diminished7},
            {"m7b5", Q::HalfDiminished7}, {"sus2", Q::Sus2}, {"sus4", Q::Sus4}, {"aug", Q::Augmented}
        };
        for (const auto& [name, expected] : qualities) {
            const auto parsed = parser.parseSyntheticV1(wrap(chord("0", std::string("keyNote=\"0\" type=\"") + name + "\"")));
            check(parsed.ok() && parsed.chords[0].quality == expected, "Common quality mapping");
        }
        const auto blocks = ui::layoutTimeline(normal.chords, 900);
        check(blocks.size() == 4 && blocks[0].width == 400 && blocks[1].width == 200 &&
              blocks[2].width == 100 && blocks[3].openRightEdge, "Proportional timeline with OPEN edge");
        check(ui::layoutTimeline({}, 900).empty(), "Empty timeline");
        std::cout << "All standalone synthetic-fixture checks passed. No Cubase test was performed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    } catch (...) {
        std::cerr << "FAIL: unexpected exception\n"; return 1;
    }
}
