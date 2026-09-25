#include "PluginView.h"

#include "Controller.h"
#include "../ui/MainView.h"
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
        frame = new CFrame(CRect(0, 0, 1100, 900), this);
        auto* pluginController = impl_->controller;
        impl_->main = new ui::MainView(
            CRect(0, 0, 1100, 900),
            [pluginController](IDataPackage* data) { pluginController->receivedDrop(data); },
            [pluginController]() { pluginController->requestSnapshot(); },
            [pluginController]() { pluginController->inspectClipboard(); },
            [pluginController](std::optional<harmony::Style> style, std::optional<harmony::PhraseIntent> intent) {
                pluginController->setRecommendationPreferences(style, intent);
            });
        frame->addView(impl_->main);

        if (!frame->open(parent, platform)) {
            auto* failedFrame = frame;
            frame = nullptr;
            impl_->main = nullptr;
            failedFrame->close();
            return false;
        }

        pluginController->attach(impl_->main, [this](bool playing) { setTransportPlaying(playing); });
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

    if (impl_->controller) impl_->controller->detach(impl_->main);
    impl_->main = nullptr;
    if (frame) {
        auto* closingFrame = frame;
        frame = nullptr;
        closingFrame->close();
    }
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
