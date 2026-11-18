#include "plugin/VstXmlChordParser.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace harmony;
using namespace harmony::plugin;

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string fixture(const char* name) {
    std::ifstream file(std::string(HC_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open VST-XML fixture");
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string oneChord(std::string time, std::string domain = "quarterNotes") {
    return "<?xml version=\"1.0\"?><vst-xml version=\"1.3\"><sourceApp>Test</sourceApp>"
           "<chord id=\"1\"><name>C</name><projectTime domain=\"" + domain + "\">" + time +
           "</projectTime><pitches>C;E;G;</pitches><keyNote>60</keyNote><mask>0x91</mask></chord></vst-xml>";
}
}

int main() {
    try {
        const VstXmlChordParser parser;

        // Case 1: parse a four-chord Clipboard VST-XML 1.3 document and retain its structured fields.
        const auto normal = parser.parse(fixture("vstxml-1.3-normal.xml"));
        check(normal.ok() && normal.chords.size() == 4, "Case 1: four VST-XML chord events");
        check(normal.sourceApp == "ParserTestFixture", "Case 1: sourceApp retained");
        check(normal.chords[0].rawId == "1" && normal.chords[0].rawName == "Cmaj7" && normal.chords[0].name == "Cmaj7",
              "Case 1: id and display/raw name retained");
        check(normal.chords[0].keyNoteValue == 60 && normal.chords[0].bassNoteValue == 24 &&
              normal.chords[0].rawKeyNote == "60" && normal.chords[0].rawBassNote == "24",
              "Case 1: VST-XML note values preserved without pitch-class guessing");
        check(normal.chords[0].extensions.pitches == "C1;E2;G2;B2;" && normal.chords[0].extensions.mask == "0x891",
              "Case 1: raw pitches and mask retained");
        check(normal.chords[0].quality == ChordQuality::Unknown && normal.chords[0].source == ChordSource::CubaseDrop,
              "Case 1: no quality inference; drop source marked");
        check(!normal.chords[0].root && !normal.chords[0].bass,
              "Case 1: VST-XML note values do not fabricate pitch-class fields");

        const auto currentSchema = parser.parse(fixture("vstxml-1.4-chord.xml"));
        check(currentSchema.ok() && currentSchema.chords.size() == 1 &&
              currentSchema.sourceApp == "Cubase" && currentSchema.detail.find("1.4") != std::string::npos,
              "Case 10: current Clipboard VST-XML 1.4 is accepted");
        check(currentSchema.chords[0].name == "Cmaj7" && currentSchema.chords[0].keyNoteValue == 60 &&
              currentSchema.chords[0].extensions.color == "#91959bff" &&
              currentSchema.chords[0].extensions.mask == "0x891",
              "Case 10: VST-XML 1.4 fields and optional chord color are retained");

        // Case 2: QN spacing 0/4/6/7 gives durations 4/2/1 and an open final chord.
        check(normal.chords[0].startQN == 0 && normal.chords[1].startQN == 4 &&
              normal.chords[2].startQN == 6 && normal.chords[3].startQN == 7,
              "Case 2: quarter-note start values");
        check(normal.chords[0].durationQN == 4 && normal.chords[1].durationQN == 2 &&
              normal.chords[2].durationQN == 1 && !normal.chords[3].durationQN && normal.chords[3].openEnded,
              "Case 2: inferred durations and open final chord");

        // Case 3: source XML order is not assumed to be project order.
        const auto unordered = parser.parse(fixture("vstxml-1.3-unordered.xml"));
        check(unordered.ok() && unordered.chords.size() == 4, "Case 3: unordered fixture parsed");
        check(unordered.chords[0].name == "Cmaj7" && unordered.chords[1].name == "Am7" &&
              unordered.chords[2].name == "A7" && unordered.chords[3].name == "Dm7",
              "Case 3: stable QN ordering");
        check(unordered.chords[0].durationQN == 4 && unordered.chords[1].durationQN == 2 &&
              unordered.chords[2].durationQN == 1 && unordered.chords[3].openEnded,
              "Case 3: durations are derived after sorting");

        // Case 4/5/6: optional bass/name fields and unknown masks remain lossless.
        const auto optional = parser.parse(fixture("vstxml-1.3-optional-fields.xml"));
        check(optional.ok() && optional.chords.size() == 2, "Cases 4-6: optional-field fixture parsed");
        check(!optional.chords[0].bassNoteValue && !optional.chords[0].rawBassNote,
              "Case 4: absent bass remains absent");
        check(!optional.chords[1].rawName && optional.chords[1].name == "(unnamed)" && optional.chords[1].keyNoteValue == 65,
              "Case 5: missing name does not lose keyNote");
        check(optional.chords[1].extensions.mask == "0xF00FFF" && optional.chords[1].quality == ChordQuality::Unknown,
              "Case 6: unknown mask is preserved and not interpreted");

        // Case 7: malformed XML fails atomically and does not return partial chords.
        const auto malformed = parser.parse("<vst-xml version=\"1.3\"><sourceApp>x</sourceApp><chord>");
        check(malformed.status == VstXmlStatus::InvalidXml && malformed.chords.empty(),
              "Case 7: malformed XML rejected without partial output");

        // Case 8: seconds are reported with their original value, never guessed into QN.
        const auto seconds = parser.parse(oneChord("3.5", "seconds"));
        check(seconds.status == VstXmlStatus::UnsupportedTimeDomain && seconds.chords.empty() &&
              seconds.rawTimeDomain == "seconds" && seconds.rawTimeValue == "3.5",
              "Case 8: unsupported domain and raw value retained");

        // Case 9: one chord is a normal open-ended event.
        const auto single = parser.parse(fixture("vstxml-1.3-single.xml"));
        check(single.ok() && single.chords.size() == 1 && single.chords[0].openEnded && !single.chords[0].durationQN,
              "Case 9: one chord is open-ended");
        check(single.chords[0].name == "D♭m7 & color" && single.chords[0].extensions.mask == "0xFFFFFFFF",
              "UTF-8, XML entity decoding, and raw mask handling");

        std::string utf16leXml = "\xFF\xFE";
        const auto utf8Xml = oneChord("0");
        for (const auto byte : utf8Xml) {
            utf16leXml.push_back(byte);
            utf16leXml.push_back('\0');
        }
        check(parser.parse(utf16leXml).ok(), "Expat recognizes BOM-marked UTF-16LE VST-XML");

        const auto base = oneChord("0");
        const auto rootStart = base.find("<vst-xml");
        const std::string withExternalDtd = base.substr(0, rootStart) +
                                            "<!DOCTYPE vst-xml SYSTEM \"https://example.invalid/vst-xml.dtd\">" + base.substr(rootStart);
        check(parser.parse(withExternalDtd).ok(), "External DTD declaration is ignored without network/entity resolution");
        const std::string withInternalEntity = "<!DOCTYPE vst-xml [<!ENTITY x 'x'>]>" + oneChord("0");
        check(parser.parse(withInternalEntity).status == VstXmlStatus::InvalidXml,
              "Internal entity declarations are rejected");
        check(parser.parse(oneChord("nan")).status == VstXmlStatus::InvalidChord, "Non-finite time rejected");
        const std::string missingTime = "<vst-xml version=\"1.3\"><sourceApp>x</sourceApp>"
                                        "<chord id=\"missing-time\"><keyNote>60</keyNote><pitches>C;</pitches><mask>0</mask></chord></vst-xml>";
        check(parser.parse(missingTime).status == VstXmlStatus::MissingProjectTime,
              "Optional VST-XML projectTime is rejected when no absolute event start can be derived");
        check(parser.parse(std::string(VstXmlChordParser::maxPayloadBytes + 1, 'x')).status == VstXmlStatus::ResourceLimit,
              "Payload size bound");
        auto missingId = oneChord("0");
        missingId.replace(missingId.find("id=\"1\""), 6, "");
        check(parser.parse(missingId).status == VstXmlStatus::InvalidChord,
              "Required chord id enforced");

        std::string tooMany = "<vst-xml version=\"1.3\"><sourceApp>x</sourceApp>";
        for (std::size_t i = 0; i <= VstXmlChordParser::maxChords; ++i) {
            tooMany += "<chord id=\"" + std::to_string(i) + "\"><projectTime domain=\"quarterNotes\">0</projectTime>"
                       "<pitches>C;</pitches><keyNote>60</keyNote><mask>0</mask></chord>";
        }
        tooMany += "</vst-xml>";
        check(parser.parse(tooMany).status == VstXmlStatus::ResourceLimit, "Chord count bound");

        std::string deep = "<vst-xml version=\"1.3\"><sourceApp>x</sourceApp>";
        for (int i = 0; i < 36; ++i) deep += "<x>";
        for (int i = 0; i < 36; ++i) deep += "</x>";
        deep += "</vst-xml>";
        check(parser.parse(deep).status == VstXmlStatus::ResourceLimit, "XML depth bound");

        std::cout << "All Clipboard VST-XML 1.3/1.4 parser cases passed. Live Cubase re-drop remains required.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "FAIL: unexpected exception\n";
        return 1;
    }
}
