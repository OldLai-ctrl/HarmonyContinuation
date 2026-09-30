#pragma once
#include "core/ImportedProgression.h"
#include "core/HarmonyAnalysis.h"
#include "core/ProgressionMatcher.h"
#include "core/ContinuationEngine.h"
#include "RecommendationWorker.h"
#include "HostAdapters.h"
#include "io/AsyncMidiImport.h"
#include "ui/MainView.h"
#include "session/PluginSessionState.h"
#include "session/ProductServices.h"
#include "enrichment/ProgressionEnrichmentEngine.h"
#include "preview/PreviewSequence.h"
#include "midi/MidiWorkflow.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <cstdint>
#include <chrono>
#include <filesystem>
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
    Steinberg::tresult PLUGIN_API terminate() override;
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
    void audition(const harmony::ContinuationCandidate&) noexcept;
    void auditionEnrichment(const harmony::enrichment::EnrichmentCandidate&) noexcept;
    void pollPreview() noexcept;
    void stopPreview() noexcept;
    bool previewActive() const noexcept { return !previewCandidateId_.empty(); }
    std::string exportMidi(const harmony::ContinuationCandidate&, void* owner) noexcept;
    std::string exportEnrichmentMidi(const harmony::enrichment::EnrichmentCandidate&, void* owner) noexcept;
    harmony::midi::PayloadResult midiPayload(const harmony::ContinuationCandidate&) const;
    harmony::midi::PayloadResult midiPayload(const harmony::enrichment::EnrichmentCandidate&) const;
    void importMidiFiles(std::vector<std::filesystem::path>,bool openEnded=false) noexcept;
    std::string hostDiagnostics() const;
    std::string exportHostDiagnostics(void* owner) noexcept;
    void observeEditor(double scale, bool resizeAccepted,bool scaleObserved=false) noexcept;
    void observeDrop(bool file, bool supported) noexcept;
    void observeMidiDrag(bool generated, bool accepted) noexcept;
    void importMidi(bool openEnded,const std::filesystem::path&,void* owner) noexcept;
    std::string saveMidiPayload(const harmony::midi::MidiClipPayload&,void* owner) noexcept;
    std::string exportLibraryMidi(const harmony::ProgressionTemplate&, void* owner) noexcept;
    std::string saveSnapshot(const harmony::ContinuationCandidate&, void* owner) noexcept;
    std::string saveEnrichmentSnapshot(const harmony::enrichment::EnrichmentCandidate&, void* owner) noexcept;
    void receivedDrop(VSTGUI::IDataPackage*) noexcept;
    void inspectClipboard() noexcept;
    void attach(harmony::ui::MainView*, std::function<void(bool)> transportRateChanged = {}) noexcept;
    void detach(harmony::ui::MainView*) noexcept;
    std::pair<int,int> editorSize() const noexcept { return {static_cast<int>(sessionState_.editorWidth),static_cast<int>(sessionState_.editorHeight)}; }
    void editorSizeChanged(int width,int height) noexcept;
    void setResizeRequest(std::function<void(int,int)> request) { resizeRequest_=std::move(request); }
private:
    host::HostEnvironment hostEnvironment_;
    std::unique_ptr<HostAdapter> hostAdapter_{makeHostAdapter(host::HostFamily::GenericVst3)};
    std::unique_ptr<io::AsyncMidiImport> midiImportWorker_;
    host::PreviewOwnership previewOwnership_;
    harmony::enrichment::EnrichmentResult enrichments_;
    std::optional<harmony::ui::MainView::EditorUiState> editorUiState_;
    void pollMidiImport();
    void applyMidiImport(const harmony::midi::ImportResult&);
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
    std::string midiImportSummary_;
    std::uint64_t lastSnapshotGeneration_{};
    unsigned unchangedTransportPolls_{};
    std::optional<double> lastProjectQN_;
    double lastTempoBPM_{120.0};
    std::optional<int> lastTimeSigNumerator_;
    std::optional<int> lastTimeSigDenominator_;
    bool lastPlaying_{};
    std::function<void(bool)> transportRateChanged_;
    std::function<void(int,int)> resizeRequest_;
    std::uint64_t recommendationGeneration_{};
    double lastComputationMs_{};
    std::size_t factoryCount_{}, userCount_{};
    std::filesystem::path previewFile_;
    std::string previewCandidateId_;
    double previewTotalQN_{}, previewSeconds_{};
    std::chrono::steady_clock::time_point previewStarted_{};
    void playPreview(std::string id, const harmony::preview::BuildResult&) noexcept;
    void submitRecommendation(bool rankingOnly = false);
    void reloadLibraries();
};
}
