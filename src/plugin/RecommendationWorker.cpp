#include "RecommendationWorker.h"
#include "library/LibraryStore.h"
#include <memory>
#include <iterator>
#include <unordered_set>
#include <stdexcept>
#include <chrono>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#endif

namespace {
struct FileSha256Result {
    std::string value{"unavailable"};
    std::string error;
};

#if defined(_WIN32)
std::string bcryptStatus(const char* operation, NTSTATUS status) {
    std::ostringstream message;
    message << operation << " failed (NTSTATUS 0x" << std::uppercase << std::hex
            << static_cast<unsigned long>(status) << ')';
    return message.str();
}

FileSha256Result sha256File(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    const auto cleanup = [&](std::string& error) {
        if (hash) {
            const auto status = BCryptDestroyHash(hash);
            hash = nullptr;
            if (status < 0) error += (error.empty() ? "" : "; ") + bcryptStatus("BCryptDestroyHash", status);
        }
        if (algorithm) {
            const auto status = BCryptCloseAlgorithmProvider(algorithm, 0);
            algorithm = nullptr;
            if (status < 0) error += (error.empty() ? "" : "; ") + bcryptStatus("BCryptCloseAlgorithmProvider", status);
        }
    };
    const auto failed = [&](std::string error) {
        cleanup(error);
        return FileSha256Result{"unavailable", std::move(error)};
    };

    std::ifstream input(path, std::ios::binary);
    if (!input) return failed("selected Factory file cannot be opened for SHA-256");

    auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0) return failed(bcryptStatus("BCryptOpenAlgorithmProvider", status));

    ULONG objectLength{}, hashLength{}, returned{};
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength), static_cast<ULONG>(sizeof(objectLength)), &returned, 0);
    if (status < 0) return failed(bcryptStatus("BCryptGetProperty(BCRYPT_OBJECT_LENGTH)", status));
    if (returned != static_cast<ULONG>(sizeof(objectLength)) || objectLength == 0)
        return failed("BCrypt returned an invalid SHA-256 object length");

    returned = 0;
    status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hashLength), static_cast<ULONG>(sizeof(hashLength)), &returned, 0);
    if (status < 0) return failed(bcryptStatus("BCryptGetProperty(BCRYPT_HASH_LENGTH)", status));
    if (returned != static_cast<ULONG>(sizeof(hashLength)) || hashLength != 32)
        return failed("BCrypt returned an invalid SHA-256 digest length");

    std::vector<UCHAR> object(objectLength);
    status = BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0);
    if (status < 0) return failed(bcryptStatus("BCryptCreateHash", status));

    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                    static_cast<ULONG>(count), 0);
            if (status < 0) return failed(bcryptStatus("BCryptHashData", status));
        }
    }
    if (input.bad() || (!input.eof() && input.fail()))
        return failed("selected Factory file could not be read for SHA-256");

    std::array<UCHAR, 32> digest{};
    status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    if (status < 0) return failed(bcryptStatus("BCryptFinishHash", status));

    std::ostringstream encoded;
    encoded << std::hex << std::setfill('0');
    for (const auto byte : digest) encoded << std::setw(2) << static_cast<unsigned int>(byte);
    std::string cleanupError;
    cleanup(cleanupError);
    if (!cleanupError.empty()) return {"unavailable", std::move(cleanupError)};
    return {encoded.str(), {}};
}
#else
FileSha256Result sha256File(const std::filesystem::path&) {
    return {"unavailable (Windows BCrypt is unavailable on this platform)",
            "SHA-256 is unavailable because this build does not have Windows BCrypt"};
}
#endif
} // namespace

namespace harmony::plugin {
RecommendationWorker::RecommendationWorker(std::filesystem::path factoryPath, std::filesystem::path userPath)
    : factoryPath_(std::move(factoryPath)), userPath_(std::move(userPath)) { thread_=std::thread([this] { run(); }); }
RecommendationWorker::~RecommendationWorker() {
    { std::lock_guard lock(mutex_); stop_ = true; pending_.reset(); }
    wake_.notify_one();
    if (thread_.joinable()) thread_.join();
}
std::uint64_t RecommendationWorker::submit(Progression progression, AnalysisContext context,
                                            RecommendationRequest request, bool rankingOnly,
                                            std::uint64_t phraseRevision) {
    return submitImpl(std::move(progression), std::move(context), request, rankingOnly,
                      phraseRevision, std::nullopt);
}
std::uint64_t RecommendationWorker::submitPreset(Progression progression, AnalysisContext context,
                                                  DevelopmentAlgorithmPreset preset,
                                                  RecommendationRequest request, bool rankingOnly,
                                                  std::uint64_t phraseRevision) {
    return submitImpl(std::move(progression), std::move(context), request, rankingOnly,
                      phraseRevision, preset);
}
std::uint64_t RecommendationWorker::submitImpl(Progression progression, AnalysisContext context,
                                                RecommendationRequest request, bool rankingOnly,
                                                std::uint64_t phraseRevision,
                                                std::optional<DevelopmentAlgorithmPreset> preset) {
    std::lock_guard lock(mutex_);
    ++generation_;
    pending_ = Task{generation_, std::move(progression), std::move(context), request,
                    rankingOnly, phraseRevision, preset};
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
void RecommendationWorker::cancel(){
    std::lock_guard lock(mutex_);++generation_;pending_.reset();ready_.reset();
}
void RecommendationWorker::invalidateLibrary() {
    std::lock_guard lock(mutex_);
    reloadLibrary_=true;
}
void RecommendationWorker::run() {
    std::unique_ptr<CandidateIndex> index;
    std::string loadError;
    std::size_t factoryCount{},userCount{};
    int factoryVersion{};
    std::string factorySha256{"unavailable"};
    std::optional<MatchQuery> cachedQuery;
    HarmonicAnalysisResult cachedAnalysis;
    std::uint64_t cachedPhraseRevision{};
    std::optional<AnalysisContext> cachedContext;
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
        if (reload) {
            index.reset(); loadError.clear(); factoryCount=0; userCount=0;
            factoryVersion=0; factorySha256="unavailable";
        }
        WorkerResult result;
        result.generation = task.generation;
        result.algorithmPreset = task.algorithmPreset;
        const auto start=std::chrono::steady_clock::now();
        try {
            const bool metadataOnly=task.progression.empty();
            if (metadataOnly) {
                // An empty progression is used to resolve the selected Factory identity only.
                // Drop any prior query so a later ranking-only task cannot reuse stale input.
                cachedQuery.reset();
                cachedAnalysis={};
                cachedPhraseRevision=0;
                cachedContext.reset();
            } else {
                const auto effectiveContext = task.algorithmPreset
                    ? developmentContext(task.context, *task.algorithmPreset) : task.context;
                result.analysisReused=task.rankingOnly && task.progression.size()<=64 && cachedQuery && task.phraseRevision!=0 &&
                    task.phraseRevision==cachedPhraseRevision && cachedContext &&
                    cachedContext->forcedKey.has_value()==effectiveContext.forcedKey.has_value() &&
                    (!cachedContext->forcedKey || (cachedContext->forcedKey->tonic==effectiveContext.forcedKey->tonic &&
                        cachedContext->forcedKey->mode==effectiveContext.forcedKey->mode)) &&
                    cachedContext->timeSigNumerator==effectiveContext.timeSigNumerator &&
                    cachedContext->timeSigDenominator==effectiveContext.timeSigDenominator &&
                    cachedContext->retrievalTimingMode==effectiveContext.retrievalTimingMode;
                if (result.analysisReused) result.analysis=cachedAnalysis;
                else {
                    cachedQuery.reset();
                    result.analysis=analyzeHarmony(task.progression,effectiveContext);
                    if (task.progression.size()<=64) {
                        cachedQuery=makeMatchQuery(task.progression,effectiveContext);
                        cachedAnalysis=result.analysis;
                        cachedPhraseRevision=task.phraseRevision;
                        cachedContext=effectiveContext;
                    }
                }
            }
            if (!metadataOnly && task.progression.size() <= 64) {
                enrichment::EnrichmentConfig config;
                config.constraints=task.request.constraints;config.tendency=task.request.tendency;
                result.enrichments=enrichment::enrichProgression(task.progression,result.analysis,task.request.style,config);
            }
            if (task.progression.size() <= 64) {
                if (!index && loadError.empty()) {
                    auto selectedFactory = library::loadAvailableFactory(factoryPath_);
                    auto& factory = selectedFactory.library;
                    if (!factory) loadError = "Factory library: " + factory.error;
                    else {
                        factoryVersion=factory.libraryVersion;
                        const auto fileHash=sha256File(selectedFactory.path);
                        if (!fileHash.error.empty()) {
#if defined(_WIN32)
                            loadError = "Factory SHA-256: " + fileHash.error;
#endif
                            factorySha256=fileHash.value;
                        } else factorySha256=fileHash.value;
                        if (loadError.empty()) {
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
                }
                if (metadataOnly) {
                    if (!index) result.error=loadError;
                } else if (index && cachedQuery) {
                    const auto weights=task.algorithmPreset
                        ? developmentRecommendationWeights(*task.algorithmPreset)
                        : developmentRecommendationWeights();
                    result.recommendations = recommendContinuations(*cachedQuery, *index, task.request, weights);
                } else result.error = index ? "推荐查询不可用。" : loadError;
            } else result.error = "超过 64 个和弦；推荐暂不计算。";
        } catch (const std::exception& e) { result.error = e.what(); }
        catch (...) { result.error = "推荐计算异常"; }
        result.computationMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        result.factoryCount=factoryCount; result.userCount=userCount;
        result.factoryVersion=factoryVersion; result.factorySha256=factorySha256;
        {
            std::lock_guard lock(mutex_);
            if (!stop_ && result.generation == generation_ && !pending_) ready_ = std::move(result);
        }
    }
}
} // namespace harmony::plugin
