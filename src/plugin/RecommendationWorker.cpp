#include "RecommendationWorker.h"
#include <memory>
#include <iterator>
#include <unordered_set>
#include <stdexcept>
#include <chrono>

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
                                            RecommendationRequest request, bool rankingOnly,
                                            std::uint64_t phraseRevision) {
    std::lock_guard lock(mutex_);
    ++generation_;
    pending_ = Task{generation_, std::move(progression), std::move(context), request,rankingOnly,phraseRevision};
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
void RecommendationWorker::invalidateLibrary() {
    std::lock_guard lock(mutex_);
    reloadLibrary_=true;
}
void RecommendationWorker::run() {
    std::unique_ptr<CandidateIndex> index;
    std::string loadError;
    std::size_t factoryCount{},userCount{};
    std::optional<MatchQuery> cachedQuery;
    HarmonicAnalysisResult cachedAnalysis;
    std::uint64_t cachedPhraseRevision{};
    while (true) {
        Task task;
        bool reload{};
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || pending_.has_value(); });
            if (stop_) return;
            task = std::move(*pending_); pending_.reset();
            reload=reloadLibrary_; reloadLibrary_=false;
        }
        if (reload) { index.reset(); loadError.clear(); factoryCount=0; userCount=0; }
        WorkerResult result;
        result.generation = task.generation;
        const auto start=std::chrono::steady_clock::now();
        try {
            result.analysisReused=task.rankingOnly && task.progression.size()<=64 && cachedQuery && task.phraseRevision!=0 &&
                task.phraseRevision==cachedPhraseRevision;
            if (result.analysisReused) result.analysis=cachedAnalysis;
            else {
                result.analysis=analyzeHarmony(task.progression,task.context);
                cachedQuery.reset();
                if (task.progression.size()<=64) {
                    cachedQuery=makeMatchQuery(task.progression,task.context);
                    cachedAnalysis=result.analysis;
                    cachedPhraseRevision=task.phraseRevision;
                }
            }
            if (task.progression.size() <= 64) {
                if (!index && loadError.empty()) {
                    auto factory = library::loadFactory(factoryPath_);
                    if (!factory) loadError = "Factory library: " + factory.error;
                    else {
                        auto user = library::UserLibrary(userPath_).loadAll();
                        if (!user) loadError = "User library: " + user.error;
                        else {
                            factoryCount=factory.templates.size();
                            userCount=user.templates.size();
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
                    RecommendationWeights weights;
                    weights.perGroup=10; // Retain choices for Show More; UI normally shows three.
                    result.recommendations = recommendContinuations(*cachedQuery, *index, task.request, weights);
                } else result.error = loadError;
            } else result.error = "超过 64 个和弦；推荐暂不计算。";
        } catch (const std::exception& e) { result.error = e.what(); }
        catch (...) { result.error = "推荐计算异常"; }
        result.computationMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        result.factoryCount=factoryCount; result.userCount=userCount;
        {
            std::lock_guard lock(mutex_);
            if (!stop_ && result.generation == generation_ && !pending_) ready_ = std::move(result);
        }
    }
}
} // namespace harmony::plugin
