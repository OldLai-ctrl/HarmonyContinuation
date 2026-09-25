#include "DemoScenario.h"
#include "plugin/RecommendationWorker.h"
#include "session/ProductServices.h"
#include "ui/MainView.h"
#include "preview/OfflinePreviewRenderer.h"
#include "midi/StandardMidiFileWriter.h"
#include "snapshot/RecommendationSnapshot.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

namespace {
using namespace harmony;
std::optional<std::filesystem::path> savePath(HWND owner,std::wstring name,const wchar_t* filter,const wchar_t* extension) {
    wchar_t autosave[32768]{};
    const auto length=GetEnvironmentVariableW(L"HC_DEMO_AUTOSAVE_DIR",autosave,32768);
    if (length && length<32768) return std::filesystem::path(autosave)/name;
    wchar_t path[32768]{};
    const auto count=std::min<std::size_t>(name.size(),32766);
    std::copy_n(name.begin(),count,path);
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=owner;
    dialog.lpstrFilter=filter; dialog.lpstrFile=path; dialog.nMaxFile=32768;
    dialog.lpstrDefExt=extension; dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(path);
}
std::optional<std::filesystem::path> openSnapshotPath(HWND owner) {
    wchar_t automated[32768]{};
    const auto length=GetEnvironmentVariableW(L"HC_DEMO_SNAPSHOT_FILE",automated,32768);
    if (length && length<32768) return std::filesystem::path(automated);
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=owner;
    dialog.lpstrFilter=L"HarmonyContinuation snapshot\0*.hcrec.json\0JSON files\0*.json\0\0";
    dialog.lpstrFile=path; dialog.nMaxFile=32768; dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&dialog)) return std::nullopt;
    return std::filesystem::path(path);
}
class DemoApp {
public:
    explicit DemoApp(std::filesystem::path executable)
        : factoryPath_(executable.parent_path()/"factory.db"),
          userPath_(userPath()),worker_(factoryPath_,userPath_) {}
    ~DemoApp() { stopAudition(); if (frame_) { frame_->close(); frame_=nullptr; main_=nullptr; } }
    static std::filesystem::path userPath() {
        wchar_t value[32768]{}; const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",value,32768);
        return n && n<32768?std::filesystem::path(value)/"HarmonyContinuation"/"user.db":std::filesystem::path("user.db");
    }
    bool open(HWND host) {
        hostWindow_=GetParent(host);
        auto* factory=VSTGUI::getPlatformFactory().asWin32Factory();
        if (factory) { factory->disableDirectComposition(); factory->useD2DHardwareRenderer(false); }
        frame_=new VSTGUI::CFrame(VSTGUI::CRect(0,0,1100,900),nullptr);
        harmony::ui::MainView::Actions actions;
        actions.stateChanged=[this](const session::PluginSessionState& next){ applyState(next); };
        actions.save=[this](const ContinuationCandidate& c,const session::SaveMetadata& meta) { return save(c,meta); };
        actions.updateUser=[this](const ProgressionTemplate& t) { return update(t); };
        actions.deleteUser=[this](const std::string& id) { return remove(id); };
        actions.audition=[this](const ContinuationCandidate& c) { audition(c); };
        actions.exportMidi=[this](const ContinuationCandidate& c) { return exportRecommendation(c); };
        actions.saveSnapshot=[this](const ContinuationCandidate& c) { return saveRecommendationSnapshot(c); };
        actions.exportLibraryMidi=[this](const ProgressionTemplate& t) { return exportLibrary(t); };
        main_=new harmony::ui::MainView(VSTGUI::CRect(0,0,1100,900),std::move(actions));
        frame_->addView(main_);
        if (!frame_->open(host,VSTGUI::PlatformType::kHWND)) { frame_=nullptr; main_=nullptr; return false; }
        main_->setHostText("HarmonyContinuation Demo · 120 BPM · simulated transport");
        loadLibrary(); return true;
    }
    void resize(int width,int height) {
        if(width<1||height<1)return;
        if(frame_)frame_->setSize(width,height);
        if(main_)main_->resizeLayout(width,height);
        state_.editorWidth=static_cast<std::uint32_t>(std::clamp(width,900,2200));
        state_.editorHeight=static_cast<std::uint32_t>(std::clamp(height,640,1400));
        if(main_)main_->setEditorSizeState(static_cast<int>(state_.editorWidth),static_cast<int>(state_.editorHeight));
    }
    void simulateScale(double scale) { if(main_)main_->setSimulatedContentScale(scale); }
    bool load(char which) {
        stopAudition();
        snapshotLoaded_=false;
        if (main_) main_->setSnapshotMode(false);
        if (which<'A' || which>'H') return false;
        auto path=std::filesystem::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+static_cast<char>(which-'A'+'a')+".json");
        auto loaded=demo::loadScenario(path);
        if (!loaded) { if (main_) main_->setDropReport("Scenario error",loaded.error); return false; }
        scenario_=std::move(loaded.scenario);
        const auto width=state_.editorWidth,height=state_.editorHeight;
        state_={};state_.editorWidth=width;state_.editorHeight=height;
        state_.forcedKey=scenario_.forcedKey; state_.style=scenario_.style; state_.intent=scenario_.intent;
        state_.meterNumerator=scenario_.meterNumerator; state_.meterDenominator=scenario_.meterDenominator;
        state_.imported.replace(std::move(scenario_.chords),TimelineCoordinateMode::RelativeToSelection);
        projectQN_=state_.imported.events.front().startQN; playing_=false;
        if (main_) { main_->setSessionState(state_); main_->setPlaybackPosition(projectQN_,false);
            main_->setAnalysis({}); main_->setMatches({},"Analyzing…"); main_->setRecommendations({});
            main_->setHostText(scenario_.name+" · "+std::to_string(static_cast<int>(scenario_.tempo))+" BPM · simulated transport"); }
        submit(); return true;
    }
    void tick() {
        if (frame_) frame_->idle();
        if (auto result=worker_.takeLatest()) {
            if (!snapshotLoaded_) {
            analysis_=std::move(result->analysis); recommendations_=std::move(result->recommendations);
            if (main_) { main_->setAnalysis(analysis_); main_->setMatches(recommendations_.matches,
                result->error.empty()?"Ready":result->error); main_->setRecommendations(recommendations_);
                main_->setWorkerStatus(result->generation,result->computationMs,false,result->factoryCount,result->userCount); }
            }
        }
        const auto now=std::chrono::steady_clock::now();
        const auto seconds=std::chrono::duration<double>(now-lastTick_).count(); lastTick_=now;
        if (playing_ && !state_.imported.events.empty()) {
            projectQN_+=std::clamp(seconds,0.0,0.2)*scenario_.tempo/60.0;
            if (main_) main_->setPlaybackPosition(projectQN_,true);
        }
        if (!auditionId_.empty()) {
            const auto elapsed=std::chrono::duration<double>(now-auditionStart_).count();
            if (elapsed>=auditionSeconds_+0.15) stopAudition();
            else if (main_) main_->setPreviewPosition(auditionId_,elapsed*scenario_.tempo/60.0,auditionQN_);
        }
    }
    void seek(double qn) { projectQN_=qn; if (main_) main_->setPlaybackPosition(projectQN_,playing_); }
    bool togglePlay() { playing_=!playing_; if (main_) main_->setPlaybackPosition(projectQN_,playing_); return playing_; }
    double projectQN() const { return projectQN_; }
    void showStatus(std::string status) { if (main_) main_->setActionStatus(std::move(status)); }
    std::string exportCurrent() {
        const auto built=preview::buildSequence(state_.imported,nullptr,scenario_.tempo);
        if (!built) return "Current MIDI: "+built.error;
        const auto key=state_.forcedKey?state_.forcedKey:
            (analysis_.selectedKey?std::optional(analysis_.selectedKey->key):std::nullopt);
        return exportSequence(built.sequence,nullptr,midi::ExportScope::CurrentOnly,key);
    }
    std::string openSnapshot() {
        const auto path=openSnapshotPath(hostWindow_);
        if (!path) return "Snapshot open cancelled";
        const auto decoded=snapshot::loadFile(*path);
        if (!decoded) return "Snapshot load: "+decoded.error;
        stopAudition(); snapshotLoaded_=true; playing_=false;
        const auto& snap=decoded.value;
        const auto width=state_.editorWidth,height=state_.editorHeight;
        state_={};state_.editorWidth=width;state_.editorHeight=height;
        state_.imported=snap.imported; state_.forcedKey=snap.key;
        state_.style=snap.style; state_.intent=snap.intent;
        state_.meterNumerator=snap.meterNumerator; state_.meterDenominator=snap.meterDenominator;
        scenario_.name=path->filename().string(); scenario_.tempo=snap.tempoBPM;
        scenario_.meterNumerator=snap.meterNumerator; scenario_.meterDenominator=snap.meterDenominator;
        projectQN_=0;
        analysis_=analyzeHarmony(state_.imported.events,state_.analysisContext());
        recommendations_={};
        const auto group=snap.candidate.intent==PhraseIntent::Develop?1:snap.candidate.intent==PhraseIntent::Loop?2:
            snap.candidate.intent==PhraseIntent::Color?3:0;
        recommendations_.groups[group].push_back(snap.candidate);
        if (snap.match) recommendations_.matches.push_back(*snap.match);
        if (main_) {
            main_->setSnapshotMode(true); main_->setSessionState(state_);
            main_->setPlaybackPosition(projectQN_,false); main_->setAnalysis(analysis_);
            main_->setMatches(recommendations_.matches,"Frozen recommendation snapshot");
            main_->setRecommendations(recommendations_);
            main_->setWorkerStatus(0,0,false,0,0);
            main_->setHostText("Snapshot · "+scenario_.name+" · "+std::to_string(static_cast<int>(scenario_.tempo))+" BPM");
        }
        return "Snapshot loaded";
    }
private:
    HWND hostWindow_{};
    std::filesystem::path auditionFile_;
    std::string auditionId_;
    double auditionSeconds_{},auditionQN_{};
    std::chrono::steady_clock::time_point auditionStart_{};
    std::filesystem::path factoryPath_,userPath_;
    plugin::RecommendationWorker worker_;
    VSTGUI::CFrame* frame_{};
    harmony::ui::MainView* main_{};
    session::PluginSessionState state_;
    demo::Scenario scenario_;
    HarmonicAnalysisResult analysis_;
    RecommendationSet recommendations_;
    bool playing_{};
    bool snapshotLoaded_{};
    double projectQN_{};
    std::chrono::steady_clock::time_point lastTick_{std::chrono::steady_clock::now()};
    void stopAudition() {
        PlaySoundW(nullptr,nullptr,0);
        auditionId_.clear(); auditionSeconds_=auditionQN_=0;
        if (main_) main_->setPreviewPosition({},0,0);
        if (hostWindow_) SetTimer(hostWindow_,1,playing_?50:250,nullptr);
        if (!auditionFile_.empty()) { std::error_code ec; std::filesystem::remove(auditionFile_,ec); auditionFile_.clear(); }
    }
    void audition(const ContinuationCandidate& candidate) {
        if (candidate.id==auditionId_) { stopAudition(); return; }
        stopAudition();
        const auto built=preview::buildSequence(state_.imported,&candidate,scenario_.tempo);
        if (!built) { if (main_) main_->setDropReport("Preview",built.error,false); return; }
        const auto audio=preview::renderOffline(built.sequence,48000);
        if (audio.left.empty()) { if (main_) main_->setDropReport("Preview","Render failed",false); return; }
        auditionFile_=std::filesystem::temp_directory_path()/
            ("HarmonyContinuationDemo-"+std::to_string(GetCurrentProcessId())+".wav");
        std::string error;
        if (!preview::writeWav16(audio,auditionFile_,error) ||
            !PlaySoundW(auditionFile_.c_str(),nullptr,SND_ASYNC|SND_FILENAME|SND_NODEFAULT)) {
            if (main_) main_->setDropReport("Preview",error.empty()?"Audio playback failed":error,false);
            stopAudition(); return;
        }
        auditionId_=candidate.id; auditionQN_=built.sequence.totalQN;
        auditionSeconds_=audio.left.size()/audio.sampleRate;
        auditionStart_=std::chrono::steady_clock::now();
        if (main_) main_->setPreviewPosition(auditionId_,0,auditionQN_);
        if (hostWindow_) SetTimer(hostWindow_,1,40,nullptr);
    }
    void submit(bool rankingOnly=false) {
        if (snapshotLoaded_) return;
        if (state_.imported.events.empty()) return;
        RecommendationRequest request{state_.style,state_.intent};
        const auto generation=worker_.submit(state_.imported.events,state_.analysisContext(),request,
            rankingOnly,state_.imported.revision);
        if (main_) main_->setWorkerStatus(generation,0,true,0,0);
    }
    void applyState(const session::PluginSessionState& next) {
        const auto recompute=session::recomputeScope(state_,next);
        const auto width=state_.editorWidth,height=state_.editorHeight;
        state_=next;state_.editorWidth=width;state_.editorHeight=height;
        if (main_) main_->setSessionState(state_);
        if (!snapshotLoaded_ && recompute!=session::RecomputeScope::None) submit(recompute==session::RecomputeScope::Ranking);
    }
    std::string exportSequence(const preview::Sequence& sequence,const ContinuationCandidate* candidate,
                               midi::ExportScope scope,std::optional<KeySignature> key) {
        const auto built=midi::buildClip(sequence,midi::ArrangementMode::VoiceLed,scope,
            {scenario_.meterNumerator,scenario_.meterDenominator},key,candidate?std::optional(candidate->intent):std::nullopt);
        if (!built) return "MIDI export: "+built.error;
        const auto name=midi::suggestedFilename(candidate?std::optional(candidate->intent):std::nullopt,key,1);
        const auto path=savePath(hostWindow_,std::wstring(name.begin(),name.end()),
            L"MIDI files\0*.mid\0\0",L"mid");
        if (!path) return "MIDI export cancelled";
        const auto payload=midi::makePayload(built.sequence,name);
        std::string error;
        if (payload.smfBytes.empty()||!midi::writeToFile(payload.smfBytes,*path,error))
            return "MIDI export: "+(error.empty()?"invalid payload":error);
        return "MIDI saved: "+path->filename().string();
    }
    std::string exportRecommendation(const ContinuationCandidate& candidate) {
        const auto built=preview::buildSequence(state_.imported,&candidate,scenario_.tempo);
        if (!built) return "MIDI export: "+built.error;
        return exportSequence(built.sequence,&candidate,midi::ExportScope::FullPhrase,candidate.key);
    }
    std::string exportLibrary(const ProgressionTemplate& item) {
        const auto key=state_.forcedKey.value_or(analysis_.selectedKey?analysis_.selectedKey->key:
            KeySignature{item.mode==Mode::Major?PitchClass::C:PitchClass::A,item.mode});
        const auto preview=midi::previewFromTemplate(item,key,scenario_.tempo);
        if (!preview) return "Library MIDI: "+preview.error;
        const auto clip=midi::buildClip(preview.sequence,midi::ArrangementMode::VoiceLed,midi::ExportScope::CurrentOnly,
            {item.meterNumerator,item.meterDenominator},key,item.intent);
        if (!clip) return "Library MIDI: "+clip.error;
        const auto name=midi::suggestedFilename(item.intent,key,1);
        const auto path=savePath(hostWindow_,std::wstring(name.begin(),name.end()),L"MIDI files\0*.mid\0\0",L"mid");
        if (!path) return "Library MIDI cancelled";
        const auto payload=midi::makePayload(clip.sequence,name);
        std::string error;
        if (payload.smfBytes.empty()||!midi::writeToFile(payload.smfBytes,*path,error))
            return "Library MIDI: "+(error.empty()?"invalid payload":error);
        return "Library MIDI saved: "+path->filename().string();
    }
    std::string saveRecommendationSnapshot(const ContinuationCandidate& candidate) {
        try {
            const auto snap=snapshot::capture(state_.imported,candidate,recommendations_.matches,
                scenario_.tempo,scenario_.meterNumerator,scenario_.meterDenominator,
                state_.forcedKey?state_.forcedKey:std::optional(candidate.key),state_.style,state_.intent);
            auto name=midi::suggestedFilename(candidate.intent,candidate.key,1);
            name.replace(name.size()-4,4,".hcrec.json");
            const auto path=savePath(hostWindow_,std::wstring(name.begin(),name.end()),
                L"HarmonyContinuation snapshot\0*.hcrec.json\0JSON files\0*.json\0\0",L"json");
            if (!path) return "Snapshot save cancelled";
            std::string error;
            if (!snapshot::saveFile(snap,*path,error)) return "Snapshot save: "+error;
            return "Snapshot saved: "+path->filename().string();
        } catch (const std::exception& e) {return std::string("Snapshot save: ")+e.what();}
    }
    void loadLibrary() {
        auto factory=library::loadFactory(factoryPath_); auto user=library::UserLibrary(userPath_).loadAll();
        if (main_) main_->setLibrary(std::move(factory.templates),std::move(user.templates),
            !factory?factory.error:!user?user.error:std::string{});
    }
    std::string save(const ContinuationCandidate& c,const session::SaveMetadata& meta) {
        if (!userPath_.parent_path().empty()) std::filesystem::create_directories(userPath_.parent_path());
        library::UserLibrary library(userPath_); std::string error;
        if (!session::saveRecommendation(library,state_.imported,c,meta,error)) return "Save failed: "+error;
        worker_.invalidateLibrary(); loadLibrary(); submit(); return "Saved to User Library";
    }
    std::string update(const ProgressionTemplate& item) {
        std::string error; if (!library::UserLibrary(userPath_).updateProgression(item,error)) return "Update failed: "+error;
        worker_.invalidateLibrary(); loadLibrary(); submit(); return "User progression updated";
    }
    std::string remove(const std::string& id) {
        std::string error; if (!library::UserLibrary(userPath_).removeProgression(id,error)) return "Delete failed: "+error;
        worker_.invalidateLibrary(); loadLibrary(); submit(); return "User progression deleted";
    }
};
std::unique_ptr<DemoApp> app;
HWND slider{},playButton{},demoContent{},caseCombo{},currentButton{},snapshotButton{},statusLabel{};
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
        case WM_GETMINMAXINFO: {
            auto* limits=reinterpret_cast<MINMAXINFO*>(l);
            RECT minimum{0,0,920,705},maximum{0,0,2220,1465};
            AdjustWindowRectEx(&minimum,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
            AdjustWindowRectEx(&maximum,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
            limits->ptMinTrackSize={minimum.right-minimum.left,minimum.bottom-minimum.top};
            limits->ptMaxTrackSize={maximum.right-maximum.left,maximum.bottom-maximum.top};
            return 0;
        }
        case WM_SIZE: {
            const int width=LOWORD(l),height=HIWORD(l);
            if(width<1||height<1)return 0;
            if(demoContent)MoveWindow(demoContent,10,55,std::max(1,width-20),std::max(1,height-65),TRUE);
            const int comboWidth=std::min(285,std::max(180,width/4));
            if(caseCombo)MoveWindow(caseCombo,12,8,comboWidth,300,TRUE);
            if(playButton)MoveWindow(playButton,comboWidth+19,8,65,32,TRUE);
            const int sliderX=comboWidth+92,sliderRight=width-248;
            if(slider)MoveWindow(slider,sliderX,4,std::max(80,sliderRight-sliderX),40,TRUE);
            if(currentButton)MoveWindow(currentButton,width-240,8,112,32,TRUE);
            if(snapshotButton)MoveWindow(snapshotButton,width-122,8,112,32,TRUE);
            if(statusLabel)ShowWindow(statusLabel,width>=1080?SW_SHOW:SW_HIDE);
            if(app)app->resize(width-20,height-65);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(w)==101 && HIWORD(w)==CBN_SELCHANGE) {
                const auto index=SendMessageW(reinterpret_cast<HWND>(l),CB_GETCURSEL,0,0);
                if (app && index>=0 && app->load(static_cast<char>('A'+index))) {
                    SendMessageW(slider,TBM_SETPOS,TRUE,0); SetWindowTextW(playButton,L"Play"); SetTimer(hwnd,1,250,nullptr);
                }
            } else if (LOWORD(w)==102 && app) {
                const bool playing=app->togglePlay(); SetWindowTextW(playButton,playing?L"Pause":L"Play");
                SetTimer(hwnd,1,playing?50:250,nullptr);
            } else if (LOWORD(w)==104 && app) {
                app->showStatus(app->exportCurrent());
            } else if (LOWORD(w)==105 && app) {
                app->showStatus(app->openSnapshot());
            }
            return 0;
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(l)==slider && app) app->seek(static_cast<double>(SendMessageW(slider,TBM_GETPOS,0,0))/4.0);
            return 0;
        case WM_TIMER:
            if (app) { app->tick(); SendMessageW(slider,TBM_SETPOS,FALSE,static_cast<LPARAM>(app->projectQN()*4)); }
            return 0;
        case WM_DESTROY: KillTimer(hwnd,1); app.reset(); PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd,message,w,l);
    }
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    VSTGUI::initPlatform(instance);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSW wc{}; wc.lpfnWndProc=windowProc; wc.hInstance=instance; wc.lpszClassName=L"HarmonyContinuationDemoWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); RegisterClassW(&wc);
    auto hwnd=CreateWindowExW(0,wc.lpszClassName,L"HarmonyContinuation Demo · offline",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,1125,1000,nullptr,nullptr,instance,nullptr);
    if (!hwnd) return 1;
    auto combo=CreateWindowExW(0,L"COMBOBOX",nullptr,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
        12,8,285,300,hwnd,reinterpret_cast<HMENU>(101),instance,nullptr);
    caseCombo=combo;
    const wchar_t* cases[]{L"Case A · C → Am → Dm",L"Case B · C → Am → A7 → Dm",L"Case C · Dm7 → G7",
        L"Case D · C → F → Fm",L"Case E · C → G → Am → F",L"Case F · A minor",
        L"Case G · anomalous F#",L"Case H · ambiguous key"};
    for (auto value:cases) SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    playButton=CreateWindowExW(0,L"BUTTON",L"Play",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,305,8,65,32,hwnd,
        reinterpret_cast<HMENU>(102),instance,nullptr);
    slider=CreateWindowExW(0,TRACKBAR_CLASSW,L"Project QN",WS_CHILD|WS_VISIBLE|TBS_HORZ,380,4,305,40,hwnd,
        reinterpret_cast<HMENU>(103),instance,nullptr);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,256));
    currentButton=CreateWindowExW(0,L"BUTTON",L"Current MIDI",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,690,8,112,32,hwnd,
        reinterpret_cast<HMENU>(104),instance,nullptr);
    snapshotButton=CreateWindowExW(0,L"BUTTON",L"Open Snapshot",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,808,8,112,32,hwnd,
        reinterpret_cast<HMENU>(105),instance,nullptr);
    statusLabel=CreateWindowExW(0,L"STATIC",L"120 BPM · simulated",WS_CHILD|WS_VISIBLE,930,14,180,25,hwnd,nullptr,instance,nullptr);
    auto content=CreateWindowExW(0,L"STATIC",nullptr,WS_CHILD|WS_VISIBLE,10,55,1100,900,hwnd,nullptr,instance,nullptr);
    demoContent=content;
    int argc{}; auto args=CommandLineToArgvW(GetCommandLineW(),&argc);
    char initial='D'; int requestedWidth=1100,requestedHeight=900; double scale=1;
    std::filesystem::path resizeSmokeReport;
    for (int i=1;i+1<argc;++i) {
        const std::wstring_view key(args[i]);
        if (key==L"--case" && wcslen(args[i+1])==1)initial=static_cast<char>(args[i+1][0]);
        else if(key==L"--size") {
            int width{},height{};
            if(swscanf_s(args[i+1],L"%dx%d",&width,&height)==2) {
                requestedWidth=std::clamp(width,900,2200);requestedHeight=std::clamp(height,640,1400);
            }
        } else if(key==L"--scale") {
            double value{};if(swscanf_s(args[i+1],L"%lf",&value)==1&&std::isfinite(value))scale=std::clamp(value,1.,2.);
        } else if(key==L"--resize-smoke")resizeSmokeReport=args[i+1];
    }
    if (args) LocalFree(args);
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
    app=std::make_unique<DemoApp>(std::filesystem::path(executable));
    if (!app->open(content) || !app->load(initial)) { DestroyWindow(hwnd); VSTGUI::exitPlatform(); return 2; }
    RECT requested{0,0,requestedWidth+20,requestedHeight+65};
    AdjustWindowRectEx(&requested,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
    SetWindowPos(hwnd,nullptr,0,0,requested.right-requested.left,requested.bottom-requested.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    app->simulateScale(scale);
    SendMessageW(combo,CB_SETCURSEL,initial-'A',0);
    if(!resizeSmokeReport.empty()) {
        std::ofstream report(resizeSmokeReport,std::ios::trunc);
        if(!report){DestroyWindow(hwnd);VSTGUI::exitPlatform();return 3;}
        int passed{};
        for(char letter='A';letter<='H';++letter) {
            if(!app->load(letter)){report<<"load failed "<<letter<<'\n';DestroyWindow(hwnd);VSTGUI::exitPlatform();return 4;}
            for(const auto [width,height]:std::array<std::pair<int,int>,3>{{{900,640},{1100,900},{1800,1000}}}) {
                RECT size{0,0,width+20,height+65};
                AdjustWindowRectEx(&size,GetWindowLongW(hwnd,GWL_STYLE),FALSE,GetWindowLongW(hwnd,GWL_EXSTYLE));
                SetWindowPos(hwnd,nullptr,0,0,size.right-size.left,size.bottom-size.top,
                    SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
                for(const double factor:{1.,1.25,1.5,2.}) {
                    app->simulateScale(factor);app->tick();
                    RECT client{};GetClientRect(demoContent,&client);
                    if(client.right<900||client.bottom<640){report<<"size failed "<<letter<<' '<<width<<'x'<<height<<'\n';
                        DestroyWindow(hwnd);VSTGUI::exitPlatform();return 5;}
                    ++passed;
                }
            }
        }
        report<<"resize smoke "<<passed<<"/96 PASS\n";
        DestroyWindow(hwnd);VSTGUI::exitPlatform();return 0;
    }
    ShowWindow(hwnd,show); UpdateWindow(hwnd); SetTimer(hwnd,1,250,nullptr);
    MSG msg{}; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    VSTGUI::exitPlatform(); return static_cast<int>(msg.wParam);
}
