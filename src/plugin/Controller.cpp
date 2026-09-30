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
#include "library/LibraryStore.h"
#include "preview/OfflinePreviewRenderer.h"
#include "midi/StandardMidiFileWriter.h"
#include "midi/MidiImportWorkflow.h"
#include "snapshot/RecommendationSnapshot.h"
#include "snapshot/EnrichmentSnapshot.h"
#if defined(_WIN32)
#include "platform/windows/ClipboardInspector.h"
#endif

#include <algorithm>
#include <bit>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <cmath>
#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
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
std::optional<std::filesystem::path> savePath(void* owner, const std::string& suggestedName,
                                               const wchar_t* filter,const wchar_t* extension) {
    std::wstring filename(suggestedName.begin(),suggestedName.end());
    wchar_t path[32768]{};
    std::copy_n(filename.begin(),std::min<std::size_t>(filename.size(),32766),path);
    OPENFILENAMEW dialog{};
    dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=static_cast<HWND>(owner);
    dialog.lpstrFilter=filter;
    dialog.lpstrFile=path;
    dialog.nMaxFile=32768;
    dialog.lpstrDefExt=extension;
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(path);
}
#else
std::filesystem::path factoryDatabasePath() { return "factory.db"; }

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
        stopPreview();
        sessionState_=std::move(restored.state);
        importedProgression_=sessionState_.imported;
        recommendationRequest_.style=sessionState_.style;
        recommendationRequest_.preferredIntent=sessionState_.intent;
        recommendationRequest_.constraints=sessionState_.constraints;
        recommendationRequest_.tendency=sessionState_.tendency;
        analysis_={}; matches_.clear(); recommendations_={};
        if (view_) {
            view_->setSessionState(sessionState_);
            view_->clearSessionDirty();
            view_->setAnalysis(analysis_); view_->setMatches(matches_,"Restoring…"); view_->setRecommendations(recommendations_);
        }
        if (resizeRequest_) resizeRequest_(static_cast<int>(sessionState_.editorWidth),static_cast<int>(sessionState_.editorHeight));
        submitRecommendation();
        return kResultOk;
    } catch (...) { return kResultFalse; }
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
    try { return name && std::strcmp(name, ViewType::kEditor) == 0 ? new PluginView(this) : nullptr; }
    catch (...) { return nullptr; }
}

void Controller::editorSizeChanged(int width,int height) noexcept {
    if (width<900 || width>2200 || height<640 || height>1400) return;
    sessionState_.editorWidth=static_cast<std::uint32_t>(width);
    sessionState_.editorHeight=static_cast<std::uint32_t>(height);
    if(view_)view_->setEditorSizeState(width,height);
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
            view_->setEnrichments(latest->enrichments);
            view_->setWorkerStatus(recommendationGeneration_,lastComputationMs_,false,factoryCount_,userCount_);
            if(!midiImportSummary_.empty()) {
                view_->setActionStatus(midiImportSummary_+" · "+std::string(harmony::localization::text(sessionState_.locale,"midi.key"))+" "+
                    (analysis_.selectedKey?harmony::formatKey(analysis_.selectedKey->key):"?"));midiImportSummary_.clear();
            }
        }
    } catch (...) {}
}

void Controller::submitRecommendation(bool rankingOnly) {
    if (importedProgression_.events.empty()) return;
    if (!recommendationWorker_) recommendationWorker_=std::make_unique<RecommendationWorker>(factoryDatabasePath(),harmony::library::userDatabasePath());
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
        if (recompute!=harmony::session::RecomputeScope::None) stopPreview();
        sessionState_=next;
        sessionState_.editorWidth=old.editorWidth;
        sessionState_.editorHeight=old.editorHeight;
        recommendationRequest_.style=next.style; recommendationRequest_.preferredIntent=next.intent;
        recommendationRequest_.constraints=next.constraints;recommendationRequest_.tendency=next.tendency;
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
    auto selected=harmony::library::loadAvailableFactory(factoryDatabasePath());
    auto& factory=selected.library;
    if(factory) sessionState_.factoryLibraryVersion=factory.libraryVersion;
    view_->setSessionState(sessionState_);
    auto user=harmony::library::UserLibrary(harmony::library::userDatabasePath()).loadAll();
    factoryCount_=factory.templates.size(); userCount_=user.templates.size();
    view_->setLibrary(std::move(factory.templates),std::move(user.templates),
        !factory?factory.error:!user?user.error:selected.warning);
}
std::string Controller::saveRecommendation(const harmony::ContinuationCandidate& c,const harmony::session::SaveMetadata& m) noexcept {
    try {
        const auto path=harmony::library::userDatabasePath();
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
        if (!harmony::library::UserLibrary(harmony::library::userDatabasePath()).updateProgression(item,error)) return "Update failed: "+error;
        if (recommendationWorker_) recommendationWorker_->invalidateLibrary();
        reloadLibraries(); submitRecommendation(); return "User progression updated";
    } catch (const std::exception& e) { return std::string("Update failed: ")+e.what(); }
}
std::string Controller::deleteUserProgression(const std::string& id) noexcept {
    try {
        std::string error;
        if (!harmony::library::UserLibrary(harmony::library::userDatabasePath()).removeProgression(id,error)) return "Delete failed: "+error;
        if (recommendationWorker_) recommendationWorker_->invalidateLibrary();
        reloadLibraries(); submitRecommendation(); return "User progression deleted";
    } catch (const std::exception& e) { return std::string("Delete failed: ")+e.what(); }
}

void Controller::stopPreview() noexcept {
#if defined(_WIN32)
    if (!previewCandidateId_.empty()) PlaySoundW(nullptr,nullptr,0);
#endif
    previewCandidateId_.clear();
    previewTotalQN_=previewSeconds_=0;
    if (view_) view_->setPreviewPosition({},0,0);
    if (!previewFile_.empty()) {
        std::error_code error;
        std::filesystem::remove(previewFile_,error);
        previewFile_.clear();
    }
}

void Controller::playPreview(std::string id,const harmony::preview::BuildResult& built) noexcept {
    try {
        stopPreview();
        if (!built) { if (view_) view_->setActionStatus("试听失败："+built.error); return; }
#if defined(_WIN32)
        const auto audio=harmony::preview::renderOffline(built.sequence,48000);
        if (audio.left.empty() || audio.sampleRate<=0) {
            if (view_) view_->setActionStatus("试听失败：音频生成失败");
            return;
        }
        previewFile_=std::filesystem::temp_directory_path()/
            (L"HarmonyContinuation-"+std::to_wstring(GetCurrentProcessId())+L"-"+
             std::to_wstring(reinterpret_cast<std::uintptr_t>(this))+L".wav");
        std::string error;
        if (!harmony::preview::writeWav16(audio,previewFile_,error) ||
            !PlaySoundW(previewFile_.c_str(),nullptr,SND_ASYNC|SND_FILENAME|SND_NODEFAULT)) {
            if (view_) view_->setActionStatus("试听失败："+(error.empty()?"系统播放设备不可用":error));
            stopPreview();
            return;
        }
        previewCandidateId_=std::move(id);
        previewTotalQN_=built.sequence.totalQN;
        previewSeconds_=static_cast<double>(audio.left.size())/audio.sampleRate;
        previewStarted_=std::chrono::steady_clock::now();
        if (view_) {
            view_->setPreviewPosition(previewCandidateId_,0,previewTotalQN_);
            view_->setActionStatus("正在通过 Windows 默认播放设备试听");
        }
#else
        if (view_) view_->setActionStatus("当前平台不支持试听");
#endif
    } catch (const std::exception& e) {
        stopPreview();
        if (view_) view_->setActionStatus(std::string("试听失败：")+e.what());
    } catch (...) {
        stopPreview();
        if (view_) view_->setActionStatus("试听失败");
    }
}

void Controller::audition(const harmony::ContinuationCandidate& candidate) noexcept {
    try {
        if (previewCandidateId_==candidate.id) {
            stopPreview();
            if (view_) view_->setActionStatus("试听已停止");
            return;
        }
        playPreview(candidate.id,harmony::preview::buildSequence(importedProgression_,&candidate,lastTempoBPM_));
    } catch (const std::exception& e) {
        if (view_) view_->setActionStatus(std::string("试听失败：")+e.what());
    } catch (...) {
        if (view_) view_->setActionStatus("试听失败");
    }
}

void Controller::auditionEnrichment(const harmony::enrichment::EnrichmentCandidate& candidate) noexcept {
    try {
        if (previewCandidateId_==candidate.id) {
            stopPreview();
            if (view_) view_->setActionStatus("试听已停止");
            return;
        }
        harmony::ImportedProgressionSession transformed;
        if (!transformed.replace(candidate.progression,harmony::TimelineCoordinateMode::RelativeToSelection)) {
            if (view_) view_->setActionStatus("试听失败：升级进行时间轴无效");
            return;
        }
        playPreview(candidate.id,harmony::preview::buildSequence(transformed,nullptr,lastTempoBPM_,candidate.constraints));
    } catch (const std::exception& e) {
        if (view_) view_->setActionStatus(std::string("试听失败：")+e.what());
    } catch (...) {
        if (view_) view_->setActionStatus("试听失败");
    }
}

void Controller::pollPreview() noexcept {
    if (previewCandidateId_.empty()) return;
    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-previewStarted_).count();
    if (elapsed>=previewSeconds_) { stopPreview(); return; }
    if (view_ && previewSeconds_>0)
        view_->setPreviewPosition(previewCandidateId_,previewTotalQN_*elapsed/previewSeconds_,previewTotalQN_);
}

namespace {
std::string saveMidiSequence(const harmony::preview::Sequence& sequence,
                             harmony::midi::ExportScope scope,harmony::midi::Meter meter,
                             std::optional<harmony::KeySignature> key,
                             std::optional<harmony::PhraseIntent> intent,void* owner) {
    const auto clip=harmony::midi::buildClip(sequence,harmony::midi::ArrangementMode::VoiceLed,scope,meter,key,intent);
    if (!clip) return "MIDI 导出失败："+clip.error;
    const auto name=harmony::midi::suggestedFilename(intent,key,1);
#if defined(_WIN32)
    const auto path=savePath(owner,name,L"MIDI files\0*.mid\0\0",L"mid");
    if (!path) return "已取消 MIDI 导出";
    const auto payload=harmony::midi::makePayload(clip.sequence,name);
    std::string error;
    if (payload.smfBytes.empty() || !harmony::midi::writeToFile(payload.smfBytes,*path,error))
        return "MIDI 导出失败："+(error.empty()?"文件内容无效":error);
    return "MIDI 已保存："+path->filename().string();
#else
    (void)owner; (void)name;
    return "当前平台不支持 MIDI 保存对话框";
#endif
}
}

namespace {
std::string saveCandidatePayload(const harmony::midi::PayloadResult& result,void* owner,harmony::session::Locale locale) {
    const auto text=[locale](const char* key){return std::string(harmony::localization::text(locale,key));};
    if(!result)return text("midi.saveFailed");
#if defined(_WIN32)
    const auto path=savePath(owner,result.payload.suggestedFilename,L"MIDI files\0*.mid\0\0",L"mid");
    if(!path)return text("midi.cancelled");
    std::string error;if(!harmony::midi::writeToFile(result.payload.smfBytes,*path,error))return text("midi.saveFailed");
    return text("midi.saved");
#else
    (void)owner;return text("midi.saveFailed");
#endif
}
}
harmony::midi::PayloadResult Controller::midiPayload(const harmony::ContinuationCandidate& candidate) const {
    return harmony::midi::candidatePayload({importedProgression_,lastTempoBPM_,
        {sessionState_.meterNumerator.value_or(4),sessionState_.meterDenominator.value_or(4)}},candidate);
}
harmony::midi::PayloadResult Controller::midiPayload(const harmony::enrichment::EnrichmentCandidate& candidate) const {
    const auto key=sessionState_.forcedKey?sessionState_.forcedKey:analysis_.selectedKey?std::optional(analysis_.selectedKey->key):std::nullopt;
    return harmony::midi::candidatePayload({importedProgression_,lastTempoBPM_,
        {sessionState_.meterNumerator.value_or(4),sessionState_.meterDenominator.value_or(4)},key},candidate);
}
std::string Controller::exportMidi(const harmony::ContinuationCandidate& candidate,void* owner) noexcept {
    try{return saveCandidatePayload(midiPayload(candidate),owner,sessionState_.locale);}catch(...){return std::string(harmony::localization::text(sessionState_.locale,"midi.saveFailed"));}
}
std::string Controller::exportEnrichmentMidi(const harmony::enrichment::EnrichmentCandidate& candidate,void* owner) noexcept {
    try{return saveCandidatePayload(midiPayload(candidate),owner,sessionState_.locale);}catch(...){return std::string(harmony::localization::text(sessionState_.locale,"midi.saveFailed"));}
}
void Controller::importMidi(bool openEnded,const std::filesystem::path& supplied,void* owner) noexcept {
    try {
        const auto path=supplied.empty()?harmony::midi::chooseMidiFile(owner):std::optional(supplied);
        if(!path)return;
        const auto loaded=harmony::midi::importFile(*path,openEnded);
        const auto text=[&](const char* key){return std::string(harmony::localization::text(sessionState_.locale,key));};
        if(!loaded){if(view_){view_->setDropReport(text("midi.failed"),loaded.midi.error+"\n"+loaded.extraction.error,false);view_->setActionStatus(text(loaded.midi.status==harmony::midi::ReadStatus::Unsupported?"midi.unsupported":"midi.failed"));}return;}
        auto next=sessionState_;if(!harmony::midi::applyImport(loaded,next.imported))return;
        next.tab=harmony::session::Tab::Recommend;
        if(!loaded.midi.file.meters.empty()){const auto meter=loaded.midi.file.meters.front();
            if(meter.numerator<=32&&meter.denominator<=32){next.meterNumerator=meter.numerator;next.meterDenominator=meter.denominator;}}
        importedProgression_=next.imported;
        if(view_)view_->setSnapshotMode(false);
        applySessionState(next);
        if(view_) {
            std::string summary=text("midi.recognized")+" "+std::to_string(loaded.extraction.chords.size())+" · "+text("midi.track")+" "+loaded.extraction.selectedTrackName;
            const auto warnings=loaded.midi.file.warnings.size()+loaded.extraction.warnings.size();
            if(warnings)summary+=" · "+text("midi.uncertain")+" "+std::to_string(warnings);
            std::ostringstream details;details<<"SMF "<<loaded.midi.file.format<<" PPQ "<<loaded.midi.file.ppq<<'\n';
            for(const auto& t:loaded.extraction.tracks)details<<"Track "<<t.track<<" "<<t.name<<" score "<<t.chordLikeness<<'\n';
            for(const auto& w:loaded.midi.file.warnings)details<<w<<'\n';
            for(const auto& w:loaded.extraction.warnings)details<<w<<'\n';
            for(std::size_t i=0;i<std::min<std::size_t>(256,loaded.extraction.slices.size());++i) {
                const auto& slice=loaded.extraction.slices[i];details<<slice.startQN<<" "<<slice.chord<<" confidence="<<slice.confidence<<'\n';
            }
            midiImportSummary_=summary;
            view_->setDropReport(summary,details.str(),false);view_->setActionStatus(summary);
        }
    } catch(...){if(view_)view_->setActionStatus(std::string(harmony::localization::text(sessionState_.locale,"midi.failed")));}
}
std::string Controller::saveMidiPayload(const harmony::midi::MidiClipPayload& payload,void* owner) noexcept {
    try {return saveCandidatePayload({payload,{}},owner,sessionState_.locale);}catch(...){return std::string(harmony::localization::text(sessionState_.locale,"midi.saveFailed"));}
}
std::string Controller::exportLibraryMidi(const harmony::ProgressionTemplate& item,void* owner) noexcept {
    try {
        const auto key=sessionState_.forcedKey.value_or(analysis_.selectedKey?analysis_.selectedKey->key:
            harmony::KeySignature{item.mode==harmony::Mode::Major?harmony::PitchClass::C:harmony::PitchClass::A,item.mode});
        const auto built=harmony::midi::previewFromTemplate(item,key,lastTempoBPM_);
        if (!built) return "MIDI 导出失败："+built.error;
        return saveMidiSequence(built.sequence,harmony::midi::ExportScope::CurrentOnly,
            {item.meterNumerator,item.meterDenominator},key,item.intent,owner);
    } catch (const std::exception& e) { return std::string("MIDI 导出失败：")+e.what(); }
    catch (...) { return "MIDI 导出失败"; }
}

std::string Controller::saveSnapshot(const harmony::ContinuationCandidate& candidate,void* owner) noexcept {
    try {
        const auto snapshot=harmony::snapshot::capture(importedProgression_,candidate,matches_,lastTempoBPM_,
            sessionState_.meterNumerator.value_or(4),sessionState_.meterDenominator.value_or(4),
            sessionState_.forcedKey?sessionState_.forcedKey:std::optional(candidate.key),
            sessionState_.style,sessionState_.intent);
#if defined(_WIN32)
        const auto path=savePath(owner,candidate.id+".hcrec.json",
            L"Recommendation snapshot\0*.hcrec.json\0JSON files\0*.json\0\0",L"json");
        if (!path) return "已取消快照保存";
        std::string error;
        if (!harmony::snapshot::saveFile(snapshot,*path,error)) return "快照保存失败："+error;
        return "快照已保存："+path->filename().string();
#else
        (void)owner; (void)snapshot;
        return "当前平台不支持快照保存对话框";
#endif
    } catch (const std::exception& e) { return std::string("快照保存失败：")+e.what(); }
    catch (...) { return "快照保存失败"; }
}

std::string Controller::saveEnrichmentSnapshot(const harmony::enrichment::EnrichmentCandidate& candidate,
                                               void* owner) noexcept {
    try {
        const auto key=sessionState_.forcedKey?sessionState_.forcedKey:
            analysis_.selectedKey?std::optional(analysis_.selectedKey->key):std::nullopt;
        const auto snapshot=harmony::snapshot::captureEnrichment(importedProgression_,candidate,lastTempoBPM_,
            sessionState_.meterNumerator.value_or(4),sessionState_.meterDenominator.value_or(4),key,sessionState_.style);
#if defined(_WIN32)
        const auto path=savePath(owner,candidate.id+".hcenrich.json",
            L"Enrichment snapshot\0*.hcenrich.json\0JSON files\0*.json\0\0",L"json");
        if (!path) return "已取消快照保存";
        std::string error;
        if (!harmony::snapshot::saveFile(snapshot,*path,error)) return "快照保存失败："+error;
        return "快照已保存："+path->filename().string();
#else
        (void)owner; (void)snapshot;
        return "当前平台不支持快照保存对话框";
#endif
    } catch (const std::exception& e) { return std::string("快照保存失败：")+e.what(); }
    catch (...) { return "快照保存失败"; }
}

void Controller::detach(harmony::ui::MainView* view) noexcept {
    if (view_ == view) {
        stopPreview();
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
            stopPreview();
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
                if (snapshot.words[0] && (flags & ProcessContext::kTempoValid)) {
                    const auto tempo=std::bit_cast<double>(snapshot.words[3]);
                    if (std::isfinite(tempo) && tempo>=20.0 && tempo<=400.0) lastTempoBPM_=tempo;
                }
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
