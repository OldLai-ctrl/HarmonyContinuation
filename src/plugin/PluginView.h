#pragma once
#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
namespace harmony::plugin {
class Controller;
class PluginView final : public Steinberg::Vst::VSTGUIEditor, public Steinberg::IPlugViewContentScaleSupport {
public:
    explicit PluginView(Controller*);
    ~PluginView() override;
#if VSTGUI_VERSION_MAJOR > 4 || (VSTGUI_VERSION_MAJOR == 4 && VSTGUI_VERSION_MINOR >= 1)
    bool PLUGIN_API open(void*, const VSTGUI::PlatformType&) override;
#else
    bool PLUGIN_API open(void*) override;
#endif
    void PLUGIN_API close() override;
    Steinberg::tresult PLUGIN_API canResize() override;
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect*) override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect*) override;
    Steinberg::tresult requestEditorSize(int width,int height);
    Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override { return VSTGUIEditor::addRef(); }
    Steinberg::uint32 PLUGIN_API release() override { return VSTGUIEditor::release(); }
    VSTGUI::CMessageResult notify(VSTGUI::CBaseObject*, const char*) override;
    void setTransportPlaying(bool playing);
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    bool transportPlaying_{};
    double contentScale_{1};
};
}
