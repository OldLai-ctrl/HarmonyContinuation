#pragma once
#include "core/ContinuationEngine.h"
#include "library/ProgressionLibrary.h"
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

namespace harmony::plugin {
struct WorkerResult {
    std::uint64_t generation{};
    HarmonicAnalysisResult analysis;
    RecommendationSet recommendations;
    std::string error;
    double computationMs{};
    std::size_t factoryCount{}, userCount{};
    bool analysisReused{};
};
class RecommendationWorker {
public:
    RecommendationWorker(std::filesystem::path factoryPath, std::filesystem::path userPath);
    ~RecommendationWorker();
    RecommendationWorker(const RecommendationWorker&) = delete;
    RecommendationWorker& operator=(const RecommendationWorker&) = delete;
    std::uint64_t submit(Progression, AnalysisContext, RecommendationRequest = {},
                         bool rankingOnly = false, std::uint64_t phraseRevision = 0);
    void invalidateLibrary();
    std::optional<WorkerResult> takeLatest();
private:
    struct Task {
        std::uint64_t generation{};
        Progression progression;
        AnalysisContext context;
        RecommendationRequest request;
        bool rankingOnly{};
        std::uint64_t phraseRevision{};
    };
    void run();
    std::filesystem::path factoryPath_, userPath_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_{};
    std::uint64_t generation_{};
    std::optional<Task> pending_;
    std::optional<WorkerResult> ready_;
    bool reloadLibrary_{};
};
} // namespace harmony::plugin
