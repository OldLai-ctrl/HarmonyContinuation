#include "PluginView.h"

#include "Controller.h"
#include "../ui/MainView.h"
#include "../ui/UILayout.h"
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
        if (frame) return true;
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

    if (impl_->controller) impl_->controller->setResizeRequest({});
    if (impl_->controller) impl_->controller->detach(impl_->main);
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
    ViewRect wanted{0,0,static_cast<int32>(std::lround(width*contentScale_)),
        static_cast<int32>(std::lround(height*contentScale_))};
    checkSizeConstraint(&wanted);
    if (plugFrame) return plugFrame->resizeView(this,&wanted);
    setRect(wanted); // before attach, getSize must advertise the requested size
    return kResultTrue;
}
tresult PLUGIN_API PluginView::setContentScaleFactor(ScaleFactor factor) {
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
        setIdleRate(transportPlaying_ ? 50 : 250);
        if (impl_ && impl_->controller) {
            impl_->controller->pollRecommendation();
            impl_->controller->pollTransport();
        }
        return result;
    }
    return VSTGUIEditor::notify(sender, message);
}

void PluginView::setTransportPlaying(bool playing) {
    transportPlaying_ = playing;
    setIdleRate(playing ? 50 : 250);
}

} // namespace harmony::plugin
