#pragma once
#include "core/ImportedProgression.h"
#include "core/HarmonyAnalysis.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
namespace VSTGUI { class IDataPackage; }
namespace harmony::ui { class MainView; }
namespace harmony::plugin {
class Controller final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IEditController*>(new Controller); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
    Steinberg::tresult requestSnapshot() noexcept;
    Steinberg::tresult pollTransport() noexcept;
    void receivedDrop(VSTGUI::IDataPackage*) noexcept;
    void inspectClipboard() noexcept;
    void attach(harmony::ui::MainView*, std::function<void(bool)> transportRateChanged = {}) noexcept;
    void detach(harmony::ui::MainView*) noexcept;
private:
    std::string hostName_{"宿主不可用"};
    harmony::ui::MainView* view_{};
    harmony::ImportedProgressionSession importedProgression_;
    harmony::HarmonicAnalysisResult analysis_;
    std::uint64_t lastSnapshotGeneration_{};
    unsigned unchangedTransportPolls_{};
    std::optional<double> lastProjectQN_;
    std::optional<int> lastTimeSigNumerator_;
    std::optional<int> lastTimeSigDenominator_;
    bool lastPlaying_{};
    std::function<void(bool)> transportRateChanged_;
};
}
