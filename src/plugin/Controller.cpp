#include "Controller.h"
#include "PluginIds.h"
#include "HostContextAdapter.h"
#include "DropCapture.h"
#include "VstXmlDropAdapter.h"
#include "VstXmlChordParser.h"
#include "vstgui/lib/cclipboard.h"
#include "PluginView.h"
#include "../ui/MainView.h"
#include "TemplateJson.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#if defined(_WIN32)
#include "platform/windows/ClipboardInspector.h"
#endif

#include <algorithm>
#include <bit>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace harmony::plugin {
using namespace Steinberg;
using namespace Steinberg::Vst;

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

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    const auto result = EditController::initialize(context);
    if (result != kResultOk) return result;
    FUnknownPtr<IHostApplication> host(context);
    if (host) {
        String128 name{};
        if (host->getName(name) == kResultOk) hostName_ = StringConvert::convert(name, 128);
    }
    return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
    try { return name && std::strcmp(name, ViewType::kEditor) == 0 ? new PluginView(this) : nullptr; }
    catch (...) { return nullptr; }
}

tresult Controller::requestSnapshot() noexcept {
    try {
        auto message = owned(allocateMessage());
        if (!message) return kResultFalse;
        message->setMessageID("HC.RequestSnapshot");
        return sendMessage(message);
    } catch (...) { return kResultFalse; }
}

tresult Controller::pollTransport() noexcept {
    try {
        auto message = owned(allocateMessage());
        if (!message) return kResultFalse;
        message->setMessageID("HC.PollTransport");
        return sendMessage(message);
    } catch (...) { return kResultFalse; }
}

void Controller::attach(harmony::ui::MainView* view, std::function<void(bool)> transportRateChanged) noexcept {
    view_ = view;
    transportRateChanged_ = std::move(transportRateChanged);
    if (view_) {
        try {
            view_->setProgressionSession(importedProgression_);
            view_->setAnalysis(analysis_);
            view_->setMatches(matches_, matchStatus_);
            view_->setPlaybackPosition(lastProjectQN_, lastPlaying_);
            view_->setHostText("宿主：" + hostName_ + "\n格式：VST3 | 播放位置自动同步");
            if (transportRateChanged_) transportRateChanged_(lastPlaying_);
        }
        catch (...) {}
    }
}

void Controller::detach(harmony::ui::MainView* view) noexcept {
    if (view_ == view) {
        view_ = nullptr;
        transportRateChanged_ = {};
    }
}

void Controller::receivedDrop(VSTGUI::IDataPackage* package) noexcept {
    try {
        auto report = inspectDrop(package, "Cubase 拖放（VSTGUI IDataPackage）");
        harmony::Progression chords;
        std::ostringstream parsed;
        parsed << "最近一次拖放 / VSTGUI IDataPackage\n数据项数：" << report.itemCount
               << "；已检查数据项的声明总大小：" << report.payloadBytes << " 字节\n";
        parsed << parseDropItems(report.items, harmony::ChordSource::CubaseDrop, chords);
        if (!chords.empty()) {
            harmony::AnalysisContext analysisContext;
            analysisContext.timeSigNumerator = lastTimeSigNumerator_;
            analysisContext.timeSigDenominator = lastTimeSigDenominator_;
            auto nextAnalysis = harmony::analyzeHarmony(chords, analysisContext);
            if (importedProgression_.replace(std::move(chords), harmony::TimelineCoordinateMode::AbsoluteProjectQN)) {
                analysis_ = std::move(nextAnalysis);
                matches_.clear();
                if (importedProgression_.events.size() > 64) {
                    matchStatus_ = "MATCH：当前导入超过 64 个和弦，开发视图暂不计算；分析与时间轴仍可用。";
                } else {
                    // Embedded test templates and matching initialize on import,
                    // never on editor open or a transport update.
                    try {
                        static const auto fixture = harmony::dev::loadDevelopmentTemplates();
                        if (fixture) {
                            static const harmony::CandidateIndex index(fixture.templates);
                            const auto query = harmony::makeMatchQuery(importedProgression_.events, analysisContext);
                            matches_ = harmony::matchProgression(query, index, {}, 5);
                            matchStatus_ = "MATCH：开发测试模板 " + std::to_string(index.templates().size()) + " 条";
                        } else matchStatus_ = "MATCH 测试模板加载失败：" + fixture.error;
                    } catch (...) { matchStatus_ = "MATCH 计算失败；和声分析与时间轴仍可使用。"; }
                }
                if (view_) {
                    view_->setProgressionSession(importedProgression_);
                    view_->setAnalysis(analysis_);
                    view_->setMatches(matches_, matchStatus_);
                }
            } else {
                parsed << "\n导入失败：和弦顺序或工程时间无效，保留上一次有效进行。\n";
            }
        }
        if (view_) view_->setDropReport(std::move(report.summary), parsed.str());
    } catch (...) {
        try { if (view_) view_->setDropReport("拖放检查时发生异常。", "拖放失败，请展开诊断面板查看详情。"); }
        catch (...) {}
    }
}

void Controller::inspectClipboard() noexcept {
    try {
        std::ostringstream raw;
        std::ostringstream parsed;
        harmony::Progression vguiChords;
        harmony::Progression nativeChords;

        raw << "A. VSTGUI IDataPackage 剪贴板视图\n";
        auto package = VSTGUI::CClipboard::get();
        if (!package) {
            raw << "VSTGUI 未提供可读取的数据项；这不代表 Windows 剪贴板为空。\n";
            parsed << "VSTGUI 剪贴板：未暴露抽象数据项。\n";
        } else {
            auto report = inspectDrop(package, "用户通过 VSTGUI CClipboard 主动检查");
            raw << report.summary << '\n';
            parsed << "VSTGUI 剪贴板解析结果：\n"
                   << parseDropItems(report.items, harmony::ChordSource::Clipboard, vguiChords);
        }

#if defined(_WIN32)
        raw << "\nB. Windows 原生剪贴板格式\n";
        const auto native = windows::inspectNativeClipboard();
        raw << native.summary << '\n';
        parsed << "\nWindows 原生剪贴板解析结果：\n"
               << parseNativeClipboardItems(native, nativeChords);
#else
        raw << "\nB. 原生剪贴板检查器：仅支持 Windows。\n";
#endif

        if (view_) view_->setDropReport(raw.str(), parsed.str());
    } catch (...) {
        try { if (view_) view_->setDropReport("剪贴板检查失败。", "剪贴板诊断时发生异常，请查看详细信息。"); }
        catch (...) {}
    }
}

tresult PLUGIN_API Controller::notify(IMessage* message) {
    try {
        if (message && message->getMessageID() &&
            (std::strcmp(message->getMessageID(), "HC.Snapshot") == 0 ||
             std::strcmp(message->getMessageID(), "HC.TransportSnapshot") == 0)) {
            const bool manualRefresh = std::strcmp(message->getMessageID(), "HC.Snapshot") == 0;
            const void* data{};
            uint32 size{};
            if (!message->getAttributes() || message->getAttributes()->getBinary("snapshot", data, size) != kResultTrue ||
                !data || size != sizeof(HostSnapshot)) return kResultFalse;
            HostSnapshot snapshot{};
            std::memcpy(&snapshot, data, sizeof(snapshot));
            const bool changed = snapshot.generation != lastSnapshotGeneration_;
            if (!manualRefresh && !changed) {
                if (unchangedTransportPolls_ < 3) ++unchangedTransportPolls_;
                if (unchangedTransportPolls_ >= 3 && lastPlaying_) {
                    lastPlaying_ = false;
                    if (view_) view_->setPlaybackPosition(lastProjectQN_, false);
                    if (transportRateChanged_) transportRateChanged_(false);
                }
                return kResultOk;
            }
            if (changed) {
                unchangedTransportPolls_ = 0;
                lastSnapshotGeneration_ = snapshot.generation;
                const auto flags = snapshot.words[1];
                if (snapshot.words[0] && (flags & ProcessContext::kProjectTimeMusicValid))
                    lastProjectQN_ = std::bit_cast<double>(snapshot.words[4]);
                else
                    lastProjectQN_.reset();
                if (snapshot.words[0] && (flags & ProcessContext::kTimeSigValid) &&
                    snapshot.words[5] > 0 && snapshot.words[6] > 0 &&
                    snapshot.words[5] <= 64 && snapshot.words[6] <= 64) {
                    lastTimeSigNumerator_ = static_cast<int>(snapshot.words[5]);
                    lastTimeSigDenominator_ = static_cast<int>(snapshot.words[6]);
                } else {
                    lastTimeSigNumerator_.reset();
                    lastTimeSigDenominator_.reset();
                }
                lastPlaying_ = (flags & ProcessContext::kPlaying) != 0;
                if (view_) view_->setPlaybackPosition(lastProjectQN_, lastPlaying_);
                if (transportRateChanged_) transportRateChanged_(lastPlaying_);
            }
            if (view_ && manualRefresh) {
                auto history = HostContextAdapter::summarize(snapshot);
                view_->setHostText("宿主：" + hostName_ + "\n" + HostContextAdapter::describe(snapshot), std::move(history));
            }
            return kResultOk;
        }
        return EditController::notify(message);
    } catch (...) { return kResultFalse; }
}
} // namespace harmony::plugin
