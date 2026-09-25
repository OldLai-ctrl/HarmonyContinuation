#include "RecommendationWorker.h"
#include <memory>
#include <iterator>
#include <unordered_set>
#include <stdexcept>

namespace harmony::plugin {
RecommendationWorker::RecommendationWorker(std::filesystem::path factoryPath, std::filesystem::path userPath)
    : factoryPath_(std::move(factoryPath)), userPath_(std::move(userPath)),
      thread_([this] { run(); }) {}
RecommendationWorker::~RecommendationWorker() {
    { std::lock_guard lock(mutex_); stop_ = true; pending_.reset(); }
    wake_.notify_one();
    if (thread_.joinable()) thread_.join();
}
std::uint64_t RecommendationWorker::submit(Progression progression, AnalysisContext context,
                                            RecommendationRequest request) {
    std::lock_guard lock(mutex_);
    ++generation_;
    pending_ = Task{generation_, std::move(progression), std::move(context), request};
    ready_.reset();
    wake_.notify_one();
    return generation_;
}
std::optional<WorkerResult> RecommendationWorker::takeLatest() {
    std::lock_guard lock(mutex_);
    if (!ready_ || ready_->generation != generation_) return std::nullopt;
    auto result = std::move(ready_);
    ready_.reset();
    return result;
}
void RecommendationWorker::run() {
    std::unique_ptr<CandidateIndex> index;
    std::string loadError;
    while (true) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || pending_.has_value(); });
            if (stop_) return;
            task = std::move(*pending_); pending_.reset();
        }
        WorkerResult result;
        result.generation = task.generation;
        try {
            result.analysis = analyzeHarmony(task.progression, task.context);
            if (task.progression.size() <= 64) {
                if (!index && loadError.empty()) {
                    auto factory = library::loadFactory(factoryPath_);
                    if (!factory) loadError = "Factory library: " + factory.error;
                    else {
                        auto user = library::UserLibrary(userPath_).loadAll();
                        if (!user) loadError = "User library: " + user.error;
                        else {
                            std::unordered_set<TemplateID> ids;
                            for (const auto& item : factory.templates) ids.insert(item.id);
                            for (const auto& item : user.templates)
                                if (!ids.insert(item.id).second)
                                    throw std::runtime_error("duplicate factory/user template ID: " + item.id);
                            factory.templates.insert(factory.templates.end(),
                                std::make_move_iterator(user.templates.begin()),
                                std::make_move_iterator(user.templates.end()));
                            index = std::make_unique<CandidateIndex>(std::move(factory.templates));
                        }
                    }
                }
                if (index) {
                    const auto query = makeMatchQuery(task.progression, task.context);
                    result.recommendations = recommendContinuations(query, *index, task.request);
                } else result.error = loadError;
            } else result.error = "超过 64 个和弦；推荐暂不计算。";
        } catch (const std::exception& e) { result.error = e.what(); }
        catch (...) { result.error = "推荐计算异常"; }
        {
            std::lock_guard lock(mutex_);
            if (!stop_ && result.generation == generation_ && !pending_) ready_ = std::move(result);
        }
    }
}
} // namespace harmony::plugin
