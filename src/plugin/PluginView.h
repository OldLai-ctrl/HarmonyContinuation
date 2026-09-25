#pragma once
#include "public.sdk/source/vst/vstguieditor.h"
namespace harmony::plugin {
class Controller;
class PluginView final : public Steinberg::Vst::VSTGUIEditor {
public:
    explicit PluginView(Controller*);
    ~PluginView() override;
#if VSTGUI_VERSION_MAJOR > 4 || (VSTGUI_VERSION_MAJOR == 4 && VSTGUI_VERSION_MINOR >= 1)
    bool PLUGIN_API open(void*, const VSTGUI::PlatformType&) override;
#else
    bool PLUGIN_API open(void*) override;
#endif
    void PLUGIN_API close() override;
    VSTGUI::CMessageResult notify(VSTGUI::CBaseObject*, const char*) override;
    void setTransportPlaying(bool playing);
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    bool transportPlaying_{};
};
}
