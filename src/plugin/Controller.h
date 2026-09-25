#pragma once
#include "core/ImportedProgression.h"
#include "core/HarmonyAnalysis.h"
#include "core/ProgressionMatcher.h"
#include "core/ContinuationEngine.h"
#include "RecommendationWorker.h"
#include "session/PluginSessionState.h"
#include "session/ProductServices.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <memory>
#include <string>
namespace VSTGUI { class IDataPackage; }
namespace harmony::ui { class MainView; }
namespace harmony::plugin {
class Controller final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* create(void*) { return static_cast<Steinberg::Vst::IEditController*>(new Controller); }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage*) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
    Steinberg::tresult requestSnapshot() noexcept;
    Steinberg::tresult pollTransport() noexcept;
    void pollRecommendation() noexcept;
    void setRecommendationPreferences(std::optional<harmony::Style>, std::optional<harmony::PhraseIntent>) noexcept;
    void applySessionState(const harmony::session::PluginSessionState&) noexcept;
    std::string saveRecommendation(const harmony::ContinuationCandidate&, const harmony::session::SaveMetadata&) noexcept;
    std::string updateUserProgression(const harmony::ProgressionTemplate&) noexcept;
    std::string deleteUserProgression(const std::string&) noexcept;
    void receivedDrop(VSTGUI::IDataPackage*) noexcept;
    void inspectClipboard() noexcept;
    void attach(harmony::ui::MainView*, std::function<void(bool)> transportRateChanged = {}) noexcept;
    void detach(harmony::ui::MainView*) noexcept;
private:
    std::string hostName_{"宿主不可用"};
    harmony::ui::MainView* view_{};
    harmony::ImportedProgressionSession importedProgression_;
    harmony::session::PluginSessionState sessionState_;
    harmony::HarmonicAnalysisResult analysis_;
    std::vector<harmony::MatchResult> matches_;
    harmony::RecommendationSet recommendations_;
    harmony::RecommendationRequest recommendationRequest_;
    std::unique_ptr<RecommendationWorker> recommendationWorker_;
    std::string matchStatus_{"拖入和弦后显示匹配结果"};
    std::uint64_t lastSnapshotGeneration_{};
    unsigned unchangedTransportPolls_{};
    std::optional<double> lastProjectQN_;
    std::optional<int> lastTimeSigNumerator_;
    std::optional<int> lastTimeSigDenominator_;
    bool lastPlaying_{};
    std::function<void(bool)> transportRateChanged_;
    std::uint64_t recommendationGeneration_{};
    double lastComputationMs_{};
    std::size_t factoryCount_{}, userCount_{};
    void submitRecommendation(bool rankingOnly = false);
    void reloadLibraries();
};
}
