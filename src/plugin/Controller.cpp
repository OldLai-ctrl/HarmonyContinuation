#include "Controller.h"
#include "PluginIds.h"
#include "HostContextAdapter.h"
#include "DropCapture.h"
#include "VstXmlDropAdapter.h"
#include "VstXmlChordParser.h"
#include "vstgui/lib/cclipboard.h"
#include "PluginView.h"
#include "../ui/MainView.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "pluginterfaces/base/ibstream.h"
#include "session/ProductServices.h"
#if defined(_WIN32)
#include "platform/windows/ClipboardInspector.h"
#endif

#include <algorithm>
#include <bit>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <filesystem>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace harmony::plugin {
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
#if defined(_WIN32)
void moduleAnchor() {}
std::filesystem::path factoryDatabasePath() {
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&moduleAnchor), &module)) return {};
    wchar_t path[32768]{};
    const auto length = GetModuleFileNameW(module, path, 32768);
    if (!length || length >= 32768) return {};
    return std::filesystem::path(path).parent_path().parent_path() / "Resources" / "factory.db";
}
std::filesystem::path userDatabasePath() {
    wchar_t path[32768]{};
    const auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, 32768);
    if (!length || length >= 32768) return {};
    return std::filesystem::path(path) / "HarmonyContinuation" / "user.db";
}
#else
std::filesystem::path factoryDatabasePath() { return "factory.db"; }
std::filesystem::path userDatabasePath() { return "user.db"; }
#endif
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

tresult PLUGIN_API Controller::getState(IBStream* stream) {
    if (!stream) return kInvalidArgument;
    try {
        const auto bytes=harmony::session::serialize(sessionState_);
        const auto size=static_cast<std::uint32_t>(bytes.size());
        const char length[4]{static_cast<char>(size),static_cast<char>(size>>8),static_cast<char>(size>>16),static_cast<char>(size>>24)};
        int32 written{};
        if (stream->write(const_cast<char*>(length),4,&written)!=kResultOk || written!=4) return kResultFalse;
        return stream->write(const_cast<char*>(bytes.data()),static_cast<int32>(bytes.size()),&written)==kResultOk &&
               written==static_cast<int32>(bytes.size())?kResultOk:kResultFalse;
    } catch (...) { return kResultFalse; }
}
tresult PLUGIN_API Controller::setState(IBStream* stream) {
    if (!stream) return kInvalidArgument;
    try {
        char length[4]{}; int32 read{};
        if (stream->read(length,4,&read)!=kResultOk || read!=4) return kResultFalse;
        const auto size=static_cast<std::uint32_t>(static_cast<unsigned char>(length[0])) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(length[1]))<<8) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(length[2]))<<16) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(length[3]))<<24);
        if (size<4 || size>1024*1024) return kResultFalse;
        std::string bytes(size,'\0');
        if (stream->read(bytes.data(),static_cast<int32>(size),&read)!=kResultOk || read!=static_cast<int32>(size)) return kResultFalse;
        auto restored=harmony::session::deserialize(bytes);
        if (!restored) return kResultFalse;
        sessionState_=std::move(restored.state);
        importedProgression_=sessionState_.imported;
        recommendationRequest_.style=sessionState_.style;
        recommendationRequest_.preferredIntent=sessionState_.intent;
        analysis_={}; matches_.clear(); recommendations_={};
        if (view_) {
            view_->setSessionState(sessionState_);
            view_->clearSessionDirty();
            view_->setAnalysis(analysis_); view_->setMatches(matches_,"Restoring…"); view_->setRecommendations(recommendations_);
        }
        submitRecommendation();
        return kResultOk;
    } catch (...) { return kResultFalse; }
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
            view_->setSessionState(sessionState_);
            view_->clearSessionDirty();
            view_->setAnalysis(analysis_);
            view_->setMatches(matches_, matchStatus_);
            view_->setRecommendations(recommendations_);
            view_->setPlaybackPosition(lastProjectQN_, lastPlaying_);
            view_->setHostText("宿主：" + hostName_ + "\n格式：VST3 | 播放位置自动同步");
            view_->setWorkerStatus(recommendationGeneration_,lastComputationMs_,false,factoryCount_,userCount_);
            reloadLibraries();
            if (transportRateChanged_) transportRateChanged_(lastPlaying_);
        }
        catch (...) {}
    }
}

void Controller::pollRecommendation() noexcept {
    try {
        if (!recommendationWorker_) return;
        auto latest = recommendationWorker_->takeLatest();
        if (!latest) return;
        analysis_ = std::move(latest->analysis);
        recommendations_ = std::move(latest->recommendations);
        recommendationGeneration_=latest->generation;
        lastComputationMs_=latest->computationMs;
        if (latest->factoryCount) factoryCount_=latest->factoryCount;
        userCount_=latest->userCount;
        matches_ = recommendations_.matches;
        matchStatus_ = latest->error.empty()
            ? "RECOMMEND：" + std::to_string(matches_.size()) + " 条匹配已计算"
            : "RECOMMEND：" + latest->error;
        if (view_) {
            view_->setAnalysis(analysis_);
            view_->setMatches(matches_, matchStatus_);
            view_->setRecommendations(recommendations_);
            view_->setWorkerStatus(recommendationGeneration_,lastComputationMs_,false,factoryCount_,userCount_);
        }
    } catch (...) {}
}

void Controller::submitRecommendation(bool rankingOnly) {
    if (importedProgression_.events.empty()) return;
    if (!recommendationWorker_) recommendationWorker_=std::make_unique<RecommendationWorker>(factoryDatabasePath(),userDatabasePath());
    recommendationGeneration_=recommendationWorker_->submit(importedProgression_.events,sessionState_.analysisContext(),
        recommendationRequest_,rankingOnly,sessionState_.imported.revision);
    matchStatus_="Analyzing…";
    if (view_) { view_->setMatches(matches_,matchStatus_); view_->setWorkerStatus(recommendationGeneration_,lastComputationMs_,true,factoryCount_,userCount_); }
}
void Controller::applySessionState(const harmony::session::PluginSessionState& next) noexcept {
    try {
        const auto old=sessionState_;
        const auto recompute=harmony::session::recomputeScope(old,next);
        const auto keyChanged=recompute==harmony::session::RecomputeScope::Analysis;
        sessionState_=next;
        recommendationRequest_.style=next.style; recommendationRequest_.preferredIntent=next.intent;
        if (keyChanged) { analysis_={}; matches_.clear(); recommendations_={}; }
        if (view_) {
            view_->setSessionState(sessionState_);
            if (keyChanged) { view_->setAnalysis(analysis_); view_->setMatches(matches_,"Analyzing…"); view_->setRecommendations(recommendations_); }
        }
        if (recompute!=harmony::session::RecomputeScope::None)
            submitRecommendation(recompute==harmony::session::RecomputeScope::Ranking);
    } catch (...) {}
}

void Controller::setRecommendationPreferences(std::optional<harmony::Style> style,
                                               std::optional<harmony::PhraseIntent> intent) noexcept {
    auto next=sessionState_; next.style=style; next.intent=intent; applySessionState(next);
}

void Controller::reloadLibraries() {
    if (!view_) return;
    auto factory=harmony::library::loadFactory(factoryDatabasePath());
    auto user=harmony::library::UserLibrary(userDatabasePath()).loadAll();
    factoryCount_=factory.templates.size(); userCount_=user.templates.size();
    view_->setLibrary(std::move(factory.templates),std::move(user.templates),
        !factory?factory.error:!user?user.error:std::string{});
}
std::string Controller::saveRecommendation(const harmony::ContinuationCandidate& c,const harmony::session::SaveMetadata& m) noexcept {
    try {
        const auto path=userDatabasePath();
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        harmony::library::UserLibrary library(path); std::string error;
        if (!harmony::session::saveRecommendation(library,importedProgression_,c,m,error)) return "Save failed: "+error;
        if (recommendationWorker_) recommendationWorker_->invalidateLibrary();
        reloadLibraries(); submitRecommendation(); return "Saved to User Library";
    } catch (const std::exception& e) { return std::string("Save failed: ")+e.what(); }
}
std::string Controller::updateUserProgression(const harmony::ProgressionTemplate& item) noexcept {
    try {
        std::string error;
        if (!harmony::library::UserLibrary(userDatabasePath()).updateProgression(item,error)) return "Update failed: "+error;
        if (recommendationWorker_) recommendationWorker_->invalidateLibrary();
        reloadLibraries(); submitRecommendation(); return "User progression updated";
    } catch (const std::exception& e) { return std::string("Update failed: ")+e.what(); }
}
std::string Controller::deleteUserProgression(const std::string& id) noexcept {
    try {
        std::string error;
        if (!harmony::library::UserLibrary(userDatabasePath()).removeProgression(id,error)) return "Delete failed: "+error;
        if (recommendationWorker_) recommendationWorker_->invalidateLibrary();
        reloadLibraries(); submitRecommendation(); return "User progression deleted";
    } catch (const std::exception& e) { return std::string("Delete failed: ")+e.what(); }
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
            if (importedProgression_.replace(std::move(chords), harmony::TimelineCoordinateMode::AbsoluteProjectQN)) {
                sessionState_.imported=importedProgression_;
                sessionState_.pinnedCandidateIds.clear();
                sessionState_.meterNumerator=lastTimeSigNumerator_;
                sessionState_.meterDenominator=lastTimeSigDenominator_;
                analysis_ = {};
                matches_.clear();
                recommendations_ = {};
                matchStatus_ = "RECOMMEND：后台分析中…";
                submitRecommendation();
                if (view_) {
                    view_->setSessionState(sessionState_);
                    view_->setAnalysis(analysis_);
                    view_->setMatches(matches_, matchStatus_);
                    view_->setRecommendations(recommendations_);
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

        if (view_) view_->setDropReport(raw.str(), parsed.str(), false);
    } catch (...) {
        try { if (view_) view_->setDropReport("剪贴板检查失败。", "剪贴板诊断时发生异常，请查看详细信息。", false); }
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
