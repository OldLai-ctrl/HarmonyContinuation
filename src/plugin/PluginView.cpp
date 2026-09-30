#include "PluginView.h"

#include "Controller.h"
#include "../ui/MainView.h"
#include "../ui/UILayout.h"
#include "../ui/EffectiveScale.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/platform/iplatformframe.h"
#if defined(_WIN32)
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"
#endif

#include <memory>
#include <mutex>
#include <cstring>
#include <cmath>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace harmony::plugin {
using namespace Steinberg;
using namespace VSTGUI;

#if defined(_WIN32)
namespace {
std::once_flag configureSoftwareDrawing;

void configureSoftwareRendering() {
    std::call_once(configureSoftwareDrawing, [] {
        // VSTGUI's default Windows frame may use DirectComposition/D3D11.
        // Keep this editor on the software Direct2D path for compatibility
        // with older Intel drivers that can fault while painting child views.
        if (const auto* factory = getPlatformFactory().asWin32Factory()) {
            factory->disableDirectComposition();
            factory->useD2DHardwareRenderer(false);
        }
    });
}
} // namespace
#endif

class PluginView::Impl {
public:
    explicit Impl(Controller* controller) : controller(controller) {}

    Controller* controller{};
    ui::MainView* main{};
};

PluginView::PluginView(Controller* controller)
    : VSTGUIEditor(controller), impl_(std::make_unique<Impl>(controller)) {
    const auto [width,height]=controller->editorSize();
    setRect(ViewRect(0,0,width,height));
    // VSTGUI starts the timer on attach and stops it on removal. While stopped
    // it polls more slowly; playback snapshots arrive at about 20 Hz.
    setIdleRate(250);
}

PluginView::~PluginView() {
    close();
}

#if VSTGUI_VERSION_MAJOR > 4 || (VSTGUI_VERSION_MAJOR == 4 && VSTGUI_VERSION_MINOR >= 1)
bool PLUGIN_API PluginView::open(void* parent, const PlatformType& platform) {
#else
bool PLUGIN_API PluginView::open(void* parent) {
    const PlatformType platform = PlatformType::kDefaultNative;
#endif
    try {
        if (frame && attachedParent_==parent) return true;
        if(frame)close();
#if defined(_WIN32)
        if(!parent||!IsWindow(static_cast<HWND>(parent)))return false;
#endif
        if (platform != PlatformType::kDefaultNative && platform != PlatformType::kHWND)
            return false;

#if defined(_WIN32)
        configureSoftwareRendering();
#endif

        // VSTGUIEditor::attached reads its inherited VSTGUIEditorInterface::frame
        // immediately after open() returns. Keep the CFrame there, as the SDK does.
        ViewRect initial{}; getSize(&initial);
        const int width=static_cast<int>((initial.right-initial.left)/contentScale_);
        const int height=static_cast<int>((initial.bottom-initial.top)/contentScale_);
        frame = new CFrame(CRect(0, 0, width, height), this);
        auto* pluginController = impl_->controller;
        ui::MainView::Actions actions;
        actions.drop=[pluginController](IDataPackage* data) { pluginController->receivedDrop(data); };
        actions.refresh=[pluginController]() { pluginController->requestSnapshot(); };
        actions.clipboard=[pluginController]() { pluginController->inspectClipboard(); };
        actions.stateChanged=[pluginController](const harmony::session::PluginSessionState& state) { pluginController->applySessionState(state); };
        actions.save=[pluginController](const harmony::ContinuationCandidate& candidate,const harmony::session::SaveMetadata& metadata) {
            return pluginController->saveRecommendation(candidate,metadata); };
        actions.updateUser=[pluginController](const harmony::ProgressionTemplate& item) { return pluginController->updateUserProgression(item); };
        actions.deleteUser=[pluginController](const std::string& id) { return pluginController->deleteUserProgression(id); };
        actions.audition=[pluginController](const harmony::ContinuationCandidate& candidate) {
            pluginController->audition(candidate); };
        actions.midiPayload=[pluginController](const harmony::ContinuationCandidate& c){return pluginController->midiPayload(c);};
        actions.enrichmentMidiPayload=[pluginController](const harmony::enrichment::EnrichmentCandidate& c){return pluginController->midiPayload(c);};
        actions.saveMidiPayload=[pluginController,parent](const harmony::midi::MidiClipPayload& p){return pluginController->saveMidiPayload(p,parent);};
        actions.importMidiFiles=[pluginController](std::vector<std::filesystem::path> paths){pluginController->importMidiFiles(std::move(paths));};
        actions.hostDiagnostics=[pluginController]{return pluginController->hostDiagnostics();};
        actions.exportHostDiagnostics=[pluginController,parent]{return pluginController->exportHostDiagnostics(parent);};
        actions.observeDrop=[pluginController](bool file,bool supported){pluginController->observeDrop(file,supported);};
        actions.observeMidiDrag=[pluginController](bool generated,bool accepted){pluginController->observeMidiDrag(generated,accepted);};
        actions.importMidi=[pluginController,parent](bool open,const std::filesystem::path& p){pluginController->importMidi(open,p,parent);};
        actions.exportMidi=[pluginController,parent](const harmony::ContinuationCandidate& candidate) {
            return pluginController->exportMidi(candidate,parent); };
        actions.auditionEnrichment=[pluginController](const harmony::enrichment::EnrichmentCandidate& candidate) {
            pluginController->auditionEnrichment(candidate); };
        actions.exportEnrichmentMidi=[pluginController,parent](const harmony::enrichment::EnrichmentCandidate& candidate) {
            return pluginController->exportEnrichmentMidi(candidate,parent); };
        actions.exportLibraryMidi=[pluginController,parent](const harmony::ProgressionTemplate& item) {
            return pluginController->exportLibraryMidi(item,parent); };
        actions.saveSnapshot=[pluginController,parent](const harmony::ContinuationCandidate& candidate) {
            return pluginController->saveSnapshot(candidate,parent); };
        actions.saveEnrichmentSnapshot=[pluginController,parent](const harmony::enrichment::EnrichmentCandidate& candidate) {
            return pluginController->saveEnrichmentSnapshot(candidate,parent); };
        impl_->main = new ui::MainView(CRect(0,0,width,height),std::move(actions));
        frame->addView(impl_->main);
        if (contentScale_!=1.0) frame->setZoom(contentScale_);

        if (!frame->open(parent, platform)) {
            auto* failedFrame = frame;
            frame = nullptr;
            impl_->main = nullptr;
            failedFrame->close();
            return false;
        }

        attachedParent_=parent;
        pluginController->attach(impl_->main, [this](bool playing) { setTransportPlaying(playing); });
        pluginController->setResizeRequest([this](int w,int h) { requestEditorSize(w,h); });
        return true;
    } catch (...) {
        if (impl_->controller) impl_->controller->detach(impl_->main);
        impl_->main = nullptr;
        if (frame) {
            auto* failedFrame = frame;
            frame = nullptr;
            failedFrame->close();
        }
        return false;
    }
}

void PLUGIN_API PluginView::close() {
    if (!impl_) return;
    attachedParent_=nullptr;

    if (impl_->controller) impl_->controller->setResizeRequest({});
    if (impl_->controller) impl_->controller->detach(impl_->main);
    if(impl_->main)impl_->main->prepareForDetach();
    impl_->main = nullptr;
    if (frame) {
        auto* closingFrame = frame;
        frame = nullptr;
        closingFrame->close();
    }
}

tresult PLUGIN_API PluginView::canResize() { return kResultTrue; }
tresult PLUGIN_API PluginView::checkSizeConstraint(ViewRect* requested) {
    if (!requested) return kInvalidArgument;
    const auto width=static_cast<long long>(requested->right)-requested->left;
    const auto height=static_cast<long long>(requested->bottom)-requested->top;
    const auto clampedWidth=static_cast<int32>(std::clamp<long long>(width,
        static_cast<long long>(std::lround(ui::UILayoutResult::minimumWidth*contentScale_)),
        static_cast<long long>(std::lround(ui::UILayoutResult::maximumWidth*contentScale_))));
    const auto clampedHeight=static_cast<int32>(std::clamp<long long>(height,
        static_cast<long long>(std::lround(ui::UILayoutResult::minimumHeight*contentScale_)),
        static_cast<long long>(std::lround(ui::UILayoutResult::maximumHeight*contentScale_))));
    requested->left=requested->top=0;
    requested->right=clampedWidth;requested->bottom=clampedHeight;
    return kResultTrue;
}
tresult PLUGIN_API PluginView::onSize(ViewRect* newSize) {
    if (!newSize || newSize->right<=newSize->left || newSize->bottom<=newSize->top) return kInvalidArgument;
    const auto result=VSTGUIEditor::onSize(newSize);
    if (result==kResultTrue || result==kResultOk) {
        const int width=static_cast<int>(std::lround((newSize->right-newSize->left)/contentScale_));
        const int height=static_cast<int>(std::lround((newSize->bottom-newSize->top)/contentScale_));
        if (impl_ && impl_->main) impl_->main->resizeLayout(width,height);
        if (impl_ && impl_->main) impl_->main->setSimulatedContentScale(contentScale_);
        if (impl_ && impl_->controller) impl_->controller->editorSizeChanged(width,height);
    }
    return result;
}
tresult PluginView::requestEditorSize(int width,int height) {
    if(resizing_)return kResultFalse;
    const ui::EffectiveScale scale(contentScale_,1.);
    ViewRect wanted{0,0,static_cast<int32>(std::lround(scale.physicalEditor(width))),
        static_cast<int32>(std::lround(scale.physicalEditor(height)))};
    checkSizeConstraint(&wanted);ViewRect current{};getSize(&current);
    if(current.getWidth()==wanted.getWidth()&&current.getHeight()==wanted.getHeight())return kResultTrue;
    if(!plugFrame){setRect(wanted);return kResultTrue;}
    resizing_=true;
    const auto result=plugFrame->resizeView(this,&wanted);
    resizing_=false;
    // A host can return its accepted geometry; some hosts omit the onSize callback.
    if(result==kResultTrue||result==kResultOk){
        ViewRect actual{};getSize(&actual);
        if(actual.getWidth()==current.getWidth()&&actual.getHeight()==current.getHeight()){
            if(wanted.getWidth()>0&&wanted.getHeight()>0)onSize(&wanted);
        }
    }
    if(impl_&&impl_->controller)impl_->controller->observeEditor(contentScale_,result==kResultTrue||result==kResultOk);
    return result;
}
tresult PLUGIN_API PluginView::setContentScaleFactor(ScaleFactor factor) {
    if(resizing_)return kResultFalse;
    if (!std::isfinite(factor)||factor<0.5f||factor>4.f) return kInvalidArgument;
    if (contentScale_==factor) return kResultTrue;
    ViewRect old{}; getSize(&old);
    const int logicalWidth=static_cast<int>(std::lround((old.right-old.left)/contentScale_));
    const int logicalHeight=static_cast<int>(std::lround((old.bottom-old.top)/contentScale_));
    const double previousScale=contentScale_;
    contentScale_=factor;
    if (frame) {
        if(!frame->setZoom(contentScale_)){contentScale_=previousScale;return kResultFalse;}
        if (impl_&&impl_->main) impl_->main->setSimulatedContentScale(contentScale_);
    }
    const auto result=requestEditorSize(logicalWidth,logicalHeight);
    if(result!=kResultTrue&&result!=kResultOk) {
        contentScale_=previousScale;
        if(frame)frame->setZoom(previousScale);
        if(impl_&&impl_->main)impl_->main->setSimulatedContentScale(previousScale);
        setRect(old);
    }
    if(impl_&&impl_->controller)impl_->controller->observeEditor(contentScale_,result==kResultTrue||result==kResultOk,true);
    return result;
}
tresult PLUGIN_API PluginView::queryInterface(const TUID iid,void** obj) {
    if(!obj)return kInvalidArgument;
    if(FUnknownPrivate::iidEqual(iid,IPlugViewContentScaleSupport::iid.toTUID())) {
        *obj=static_cast<IPlugViewContentScaleSupport*>(this);addRef();return kResultOk;
    }
    return VSTGUIEditor::queryInterface(iid,obj);
}

VSTGUI::CMessageResult PluginView::notify(VSTGUI::CBaseObject* sender, const char* message) {
    if (message && std::strcmp(message, VSTGUI::CVSTGUITimer::kMsgTimer) == 0) {
        const auto result = VSTGUIEditor::notify(sender, message);
#if defined(_WIN32)
        auto* platformFrame = frame ? frame->getPlatformFrame() : nullptr;
        auto window = platformFrame ? static_cast<HWND>(platformFrame->getPlatformRepresentation()) : nullptr;
        if (!window || !IsWindowVisible(window) || IsIconic(window)) {
            setIdleRate(500);
            return result;
        }
#endif
        setIdleRate(transportPlaying_ || (impl_ && impl_->controller && impl_->controller->previewActive()) ? 50 : 250);
        if (impl_ && impl_->controller) {
            impl_->controller->pollRecommendation();
            impl_->controller->pollTransport();
            impl_->controller->pollPreview();
        }
        return result;
    }
    return VSTGUIEditor::notify(sender, message);
}

void PluginView::setTransportPlaying(bool playing) {
    transportPlaying_ = playing;
    setIdleRate(playing || (impl_ && impl_->controller && impl_->controller->previewActive()) ? 50 : 250);
}

} // namespace harmony::plugin
