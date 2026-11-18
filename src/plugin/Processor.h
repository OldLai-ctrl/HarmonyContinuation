#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "HostContextAdapter.h"
#include "preview/PreviewExchange.h"
namespace VSTGUI { class IDataPackage; }
namespace harmony::plugin {
class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();
    static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement*, Steinberg::int32, Steinberg::Vst::SpeakerArrangement*, Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API notifyDrop(VSTGUI::IDataPackage*) noexcept;
private:
    HostContextAdapter context_;
    preview::PreviewMailbox previewCommands_;
    preview::PreviewReceiver preview_;
    preview::PreviewFeedback previewFeedback_;
    preview::TransportRequest previewRequest_;
    std::atomic<bool> processing_{},previewInFlight_{};
    std::atomic<std::uint64_t> previewGate_{};
    std::uint64_t previewBlocks_{},lastPreviewRequest_{};
    preview::AudioError previewError_{preview::AudioError::None};
    double previewRate_{48000};
    Steinberg::int32 maximumBlock_{4096};
};
}
