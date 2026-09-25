#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "HostContextAdapter.h"
namespace VSTGUI { class IDataPackage; }
namespace harmony::plugin {
class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();
    static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement*, Steinberg::int32, Steinberg::Vst::SpeakerArrangement*, Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API notifyDrop(VSTGUI::IDataPackage*) noexcept;
private:
    HostContextAdapter context_;
};
}
