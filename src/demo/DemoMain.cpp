#include "DemoScenario.h"
#include "plugin/RecommendationWorker.h"
#include "session/ProductServices.h"
#include "ui/MainView.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace {
using namespace harmony;
class DemoApp {
public:
    explicit DemoApp(std::filesystem::path executable)
        : factoryPath_(executable.parent_path()/"factory.db"),
          userPath_(userPath()),worker_(factoryPath_,userPath_) {}
    ~DemoApp() { if (frame_) { frame_->close(); frame_=nullptr; main_=nullptr; } }
    static std::filesystem::path userPath() {
        wchar_t value[32768]{}; const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",value,32768);
        return n && n<32768?std::filesystem::path(value)/"HarmonyContinuation"/"user.db":std::filesystem::path("user.db");
    }
    bool open(HWND host) {
        auto* factory=VSTGUI::getPlatformFactory().asWin32Factory();
        if (factory) { factory->disableDirectComposition(); factory->useD2DHardwareRenderer(false); }
        frame_=new VSTGUI::CFrame(VSTGUI::CRect(0,0,1100,900),nullptr);
        harmony::ui::MainView::Actions actions;
        actions.stateChanged=[this](const session::PluginSessionState& next){ applyState(next); };
        actions.save=[this](const ContinuationCandidate& c,const session::SaveMetadata& meta) { return save(c,meta); };
        actions.updateUser=[this](const ProgressionTemplate& t) { return update(t); };
        actions.deleteUser=[this](const std::string& id) { return remove(id); };
        main_=new harmony::ui::MainView(VSTGUI::CRect(0,0,1100,900),std::move(actions));
        frame_->addView(main_);
        if (!frame_->open(host,VSTGUI::PlatformType::kHWND)) { frame_=nullptr; main_=nullptr; return false; }
        main_->setHostText("HarmonyContinuation Demo · 120 BPM · simulated transport");
        loadLibrary(); return true;
    }
    bool load(char which) {
        if (which<'A' || which>'H') return false;
        auto path=std::filesystem::path(HC_DEMO_FIXTURE_DIR)/(std::string("case_")+static_cast<char>(which-'A'+'a')+".json");
        auto loaded=demo::loadScenario(path);
        if (!loaded) { if (main_) main_->setDropReport("Scenario error",loaded.error); return false; }
        scenario_=std::move(loaded.scenario);
        state_={}; state_.forcedKey=scenario_.forcedKey; state_.style=scenario_.style; state_.intent=scenario_.intent;
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
            analysis_=std::move(result->analysis); recommendations_=std::move(result->recommendations);
            if (main_) { main_->setAnalysis(analysis_); main_->setMatches(recommendations_.matches,
                result->error.empty()?"Ready":result->error); main_->setRecommendations(recommendations_);
                main_->setWorkerStatus(result->generation,result->computationMs,false,result->factoryCount,result->userCount); }
        }
        const auto now=std::chrono::steady_clock::now();
        const auto seconds=std::chrono::duration<double>(now-lastTick_).count(); lastTick_=now;
        if (playing_ && !state_.imported.events.empty()) {
            projectQN_+=std::clamp(seconds,0.0,0.2)*scenario_.tempo/60.0;
            if (main_) main_->setPlaybackPosition(projectQN_,true);
        }
    }
    void seek(double qn) { projectQN_=qn; if (main_) main_->setPlaybackPosition(projectQN_,playing_); }
    bool togglePlay() { playing_=!playing_; if (main_) main_->setPlaybackPosition(projectQN_,playing_); return playing_; }
    double projectQN() const { return projectQN_; }
private:
    std::filesystem::path factoryPath_,userPath_;
    plugin::RecommendationWorker worker_;
    VSTGUI::CFrame* frame_{};
    harmony::ui::MainView* main_{};
    session::PluginSessionState state_;
    demo::Scenario scenario_;
    HarmonicAnalysisResult analysis_;
    RecommendationSet recommendations_;
    bool playing_{};
    double projectQN_{};
    std::chrono::steady_clock::time_point lastTick_{std::chrono::steady_clock::now()};
    void submit(bool rankingOnly=false) {
        if (state_.imported.events.empty()) return;
        RecommendationRequest request{state_.style,state_.intent};
        const auto generation=worker_.submit(state_.imported.events,state_.analysisContext(),request,
            rankingOnly,state_.imported.revision);
        if (main_) main_->setWorkerStatus(generation,0,true,0,0);
    }
    void applyState(const session::PluginSessionState& next) {
        const auto recompute=session::recomputeScope(state_,next);
        state_=next;
        if (main_) main_->setSessionState(state_);
        if (recompute!=session::RecomputeScope::None) submit(recompute==session::RecomputeScope::Ranking);
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
HWND slider{},playButton{};
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
        case WM_COMMAND:
            if (LOWORD(w)==101 && HIWORD(w)==CBN_SELCHANGE) {
                const auto index=SendMessageW(reinterpret_cast<HWND>(l),CB_GETCURSEL,0,0);
                if (app && index>=0 && app->load(static_cast<char>('A'+index))) {
                    SendMessageW(slider,TBM_SETPOS,TRUE,0); SetWindowTextW(playButton,L"Play"); SetTimer(hwnd,1,250,nullptr);
                }
            } else if (LOWORD(w)==102 && app) {
                const bool playing=app->togglePlay(); SetWindowTextW(playButton,playing?L"Pause":L"Play");
                SetTimer(hwnd,1,playing?50:250,nullptr);
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
    auto hwnd=CreateWindowExW(0,wc.lpszClassName,L"HarmonyContinuation Demo · offline",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,1125,1000,nullptr,nullptr,instance,nullptr);
    if (!hwnd) return 1;
    auto combo=CreateWindowExW(0,L"COMBOBOX",nullptr,WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,
        12,8,330,300,hwnd,reinterpret_cast<HMENU>(101),instance,nullptr);
    const wchar_t* cases[]{L"Case A · C → Am → Dm",L"Case B · C → Am → A7 → Dm",L"Case C · Dm7 → G7",
        L"Case D · C → F → Fm",L"Case E · C → G → Am → F",L"Case F · A minor",
        L"Case G · anomalous F#",L"Case H · ambiguous key"};
    for (auto value:cases) SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    playButton=CreateWindowExW(0,L"BUTTON",L"Play",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,355,8,85,32,hwnd,
        reinterpret_cast<HMENU>(102),instance,nullptr);
    slider=CreateWindowExW(0,TRACKBAR_CLASSW,L"Project QN",WS_CHILD|WS_VISIBLE|TBS_HORZ,455,4,490,40,hwnd,
        reinterpret_cast<HMENU>(103),instance,nullptr);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(0,256));
    CreateWindowExW(0,L"STATIC",L"120 BPM · simulated",WS_CHILD|WS_VISIBLE,950,14,160,25,hwnd,nullptr,instance,nullptr);
    auto content=CreateWindowExW(0,L"STATIC",nullptr,WS_CHILD|WS_VISIBLE,10,55,1100,900,hwnd,nullptr,instance,nullptr);
    int argc{}; auto args=CommandLineToArgvW(GetCommandLineW(),&argc);
    char initial='D'; for (int i=1;i+1<argc;++i) if (std::wstring_view(args[i])==L"--case" && wcslen(args[i+1])==1)
        initial=static_cast<char>(args[i+1][0]);
    if (args) LocalFree(args);
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
    app=std::make_unique<DemoApp>(std::filesystem::path(executable));
    if (!app->open(content) || !app->load(initial)) { DestroyWindow(hwnd); VSTGUI::exitPlatform(); return 2; }
    SendMessageW(combo,CB_SETCURSEL,initial-'A',0);
    ShowWindow(hwnd,show); UpdateWindow(hwnd); SetTimer(hwnd,1,250,nullptr);
    MSG msg{}; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    VSTGUI::exitPlatform(); return static_cast<int>(msg.wParam);
}
