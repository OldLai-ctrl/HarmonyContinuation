#include "HostAdapters.h"
#include "VstXmlChordParser.h"
#include <iomanip>
#include <sstream>
namespace harmony::plugin {
namespace {
const char* qualityName(harmony::ChordQuality q) {
    using Q = harmony::ChordQuality;
    switch (q) {
        case Q::Major: return "大三和弦"; case Q::Minor: return "小三和弦";
        case Q::Dominant7: return "7"; case Q::Major7: return "Maj7";
        case Q::Minor7: return "m7"; case Q::Diminished: return "dim";
        case Q::Diminished7: return "dim7"; case Q::HalfDiminished7: return "m7b5";
        case Q::Sus2: return "sus2"; case Q::Sus4: return "sus4";
        case Q::Augmented: return "增三和弦"; default: return "未知";
    }
}

const char* parseStatusName(VstXmlStatus status) {
    switch (status) {
        case VstXmlStatus::Success: return "解析成功";
        case VstXmlStatus::InvalidXml: return "XML 格式错误";
        case VstXmlStatus::UnsupportedSchema: return "不支持的 VST-XML 结构";
        case VstXmlStatus::UnsupportedTimeDomain: return "不支持的时间单位";
        case VstXmlStatus::InvalidChord: return "和弦数据无效";
        case VstXmlStatus::MissingProjectTime: return "缺少工程位置";
        case VstXmlStatus::ResourceLimit: return "超过安全限制";
        default: return "内部错误";
    }
}

std::string chordSummary(const harmony::Progression& events) {
    std::ostringstream out;
    for (const auto& chord : events) {
        out << chord.name << " @ " << std::fixed << std::setprecision(3) << chord.startQN << " QN";
        if (chord.keyNoteValue) out << "  keyNote=" << *chord.keyNoteValue;
        else if (chord.root) out << "  rootPC=" << unsigned(*chord.root);
        else out << "  根音=未知";
        out << "  低音=";
        if (chord.bassNoteValue) out << *chord.bassNoteValue;
        else if (chord.bass) out << "PC " << unsigned(*chord.bass);
        else out << "未知";
        out << "  性质=" << qualityName(chord.quality) << "  时长=";
        if (chord.durationQN) out << *chord.durationQN << " QN"; else out << "延续至后续编辑";
        if (chord.extensions.mask) out << "  原始 mask=" << *chord.extensions.mask;
        if (chord.extensions.pitches) out << "  原始 pitches=" << *chord.extensions.pitches;
        if (chord.extensions.color) out << "  颜色=" << *chord.extensions.color;
        out << '\n';
    }
    return out.str();
}

std::string parseDropItems(const std::vector<DropItemReport>& items, harmony::ChordSource source,
                           harmony::Progression& events) {
    std::ostringstream parsed;
    const VstXmlChordParser parser;
    bool anyCandidate{};
    for (const auto& item : items) {
        parsed << "数据项 " << item.index << " 解析：";
        if (!item.looksLikeXml) {
            parsed << (item.containsVstXml ? "VST-XML 标记位于数据内部；未尝试猜测并截取片段。\n"
                                          : "数据开头不是 XML，未调用解析器。\n");
            continue;
        }
        anyCandidate = true;
        if (!item.fullPayloadReadable) {
            parsed << "已跳过：只能读取受限预览，完整数据未交给解析器。\n";
            continue;
        }
        const auto result = parser.parse(item.decodedText, source);
        parsed << parseStatusName(result.status) << ": " << result.detail;
        if (!result.rawTimeDomain.empty() || !result.rawTimeValue.empty())
            parsed << "；原始 projectTime domain='" << result.rawTimeDomain << "' value='" << result.rawTimeValue << "'";
        if (result.ok()) {
            parsed << "；来源应用='" << result.sourceApp << "'；和弦数=" << result.chords.size();
            events.insert(events.end(), result.chords.begin(), result.chords.end());
        }
        parsed << '\n';
    }
    if (!anyCandidate) parsed << "没有可交给 VST-XML 解析器的完整 XML 数据项。\n";
    if (!events.empty()) {
        if (!harmony::sortAndInferDurations(events)) {
            events.clear();
            parsed << "合并和弦时长推导失败，未保留时间轴数据。\n";
        } else parsed << "解析出的和弦时间轴：\n" << chordSummary(events);
    }
    return parsed.str();
}

#if defined(_WIN32)
std::string parseNativeClipboardItems(const windows::ClipboardInspection& report,
                                     harmony::Progression& events) {
    std::ostringstream parsed;
    const VstXmlChordParser parser;
    bool anyCandidate{};
    for (const auto& item : report.formats) {
        if (!item.looksLikeXml) {
            if (item.containsVstXml)
                parsed << "Windows 格式 " << item.formatId << "：VST-XML 标记不在文档开头，未猜测截取。\n";
            continue;
        }
        anyCandidate = true;
        parsed << "Windows 格式 " << item.formatId << "（" << item.formatName << "）解析：";
        if (!item.fullPayloadReadable) { parsed << "已跳过：只能读取受限预览。\n"; continue; }
        const auto result = parser.parse(item.decodedText, harmony::ChordSource::Clipboard);
        parsed << parseStatusName(result.status) << ": " << result.detail;
        if (!result.rawTimeDomain.empty() || !result.rawTimeValue.empty())
            parsed << "；原始 projectTime domain='" << result.rawTimeDomain << "' value='" << result.rawTimeValue << "'";
        if (result.ok()) {
            parsed << "；来源应用='" << result.sourceApp << "'；和弦数=" << result.chords.size();
            events.insert(events.end(), result.chords.begin(), result.chords.end());
        }
        parsed << '\n';
    }
    if (!anyCandidate) parsed << "没有可供解析器读取的原生剪贴板 XML 数据。\n";
    if (!events.empty()) {
        if (!harmony::sortAndInferDurations(events)) { events.clear(); parsed << "时间轴推导失败。\n"; }
        else parsed << "解析出的和弦时间轴：\n" << chordSummary(events);
    }
    return parsed.str();
}
#endif
} // namespace
std::string CubaseHostAdapter::parseXml(const std::vector<DropItemReport>& items,ChordSource source,Progression& events){return parseDropItems(items,source,events);}
#if defined(_WIN32)
std::string CubaseHostAdapter::parseClipboard(const windows::ClipboardInspection& report,Progression& events){return parseNativeClipboardItems(report,events);}
#endif
HostChordInput CubaseHostAdapter::chordInput(const DropReport& report) const {HostChordInput input;input.details=parseXml(report.items,ChordSource::CubaseDrop,input.chords);return input;}
HostChordInput GenericVst3HostAdapter::chordInput(const DropReport& report) const {
    for(const auto& item:report.items)if(item.looksLikeXml&&item.containsVstXml)return CubaseHostAdapter{}.chordInput(report);
    return {{},"No supported chord XML; use MIDI File Import."};
}
HostChordInput FLStudioHostAdapter::chordInput(const DropReport&) const {return {{},"Use standard MIDI File Import or a .mid/.midi file drop."};}
std::unique_ptr<HostAdapter> makeHostAdapter(host::HostFamily family){
    if(family==host::HostFamily::Cubase)return std::make_unique<CubaseHostAdapter>();
    if(family==host::HostFamily::FLStudio)return std::make_unique<FLStudioHostAdapter>();
    return std::make_unique<GenericVst3HostAdapter>();
}
}
