#include "WishWorkflow.h"

#include "color/HarmonyColorAnalyzer.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace harmony::wish {
namespace {

bool validInput(const WishInput& input, std::string& error) {
    if (input.imported.events.empty()) {
        error = "input mode requires a current progression";
        return false;
    }
    if (input.imported.events.size() > 64 ||
        input.imported.coordinateMode == TimelineCoordinateMode::Unknown) {
        error = "invalid wish input timeline";
        return false;
    }
    return true;
}

std::vector<ProgressionTemplate> styleSources(
    const std::vector<ProgressionTemplate>& sources, std::optional<Style> style) {
    if (!style) return sources;
    const auto mask = static_cast<StyleFlags>(*style);
    std::vector<ProgressionTemplate> result;
    result.reserve(sources.size());
    for (const auto& source : sources)
        if ((source.styles & mask) != 0) result.push_back(source);
    return result;
}

bool matchesColor(const WishItem& item, ColorDirection direction) {
    if (direction == ColorDirection::Any) return true;
    const auto source = color::HarmonyColorAnalyzer::prepare(item.route.imported.events);
    const auto path = color::HarmonyColorAnalyzer::analyzeContinuation(source, item.route.candidate);
    if (path.status != color::Status::Known) return false;
    switch (direction) {
        case ColorDirection::Any: return true;
        case ColorDirection::Warming: return path.warming;
        case ColorDirection::Cooling: return path.cooling;
        case ColorDirection::TensionRising: return path.tensionRising;
        case ColorDirection::TensionReleasing: return path.tensionReleasing;
    }
    return false;
}

std::string poolDiagnostic(const WishPool& pool, std::size_t factorySources,
                           std::size_t colorFiltered) {
    std::ostringstream out;
    out << "factorySources=" << factorySources
        << "; sourceMismatches=" << pool.sourceMismatches
        << "; invalidSources=" << pool.invalidSources
        << "; invalidAnnotations=" << pool.invalidAnnotations
        << "; unreviewedSources=" << pool.unreviewedSources
        << "; unannotatedSources=" << pool.unannotatedSources
        << "; unconfirmedSources=" << pool.unconfirmedSources
        << "; colorFiltered=" << colorFiltered;
    return out.str();
}

} // namespace

WishWorkflow::WishWorkflow(std::vector<ProgressionTemplate> sources,
                           AnnotationCatalog annotations)
    : sources_(std::move(sources)), annotations_(std::move(annotations)),
      published_(std::make_shared<const WishView>()) {
    worker_ = std::thread(&WishWorkflow::run, this);
}

WishWorkflow::~WishWorkflow() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        pending_.reset();
    }
    wake_.notify_one();
    if (worker_.joinable()) worker_.join();
}

std::uint64_t WishWorkflow::reset(WishSettings settings, WishInput input,
                                  std::uint64_t seed) {
    std::lock_guard lock(mutex_);
    ++revision_;
    ++displayGeneration_;
    settings_ = std::move(settings);
    input_ = settings_.useInput ? std::move(input) : WishInput{};
    seed_ = seed;
    pending_.reset();

    auto next = std::make_shared<WishView>();
    next->revision = revision_;
    next->displayGeneration = displayGeneration_;
    next->batchSerial = batchSerial_;
    next->usesInput = settings_.useInput;
    next->busy = active_;
    if (next->usesInput) validInput(input_, next->error);
    published_ = std::move(next);
    wake_.notify_one();
    return revision_;
}

bool WishWorkflow::draw(std::size_t count, bool append) {
    if (count == 0 || count > 10) return false;
    std::lock_guard lock(mutex_);
    if (stopping_ || active_ || pending_ || !published_ || published_->busy ||
        !published_->error.empty()) return false;

    auto next = std::make_shared<WishView>(*published_);
    next->busy = true;
    next->lastDraw = 0;
    published_ = std::move(next);
    pending_ = DrawTask{revision_, displayGeneration_, count, append};
    wake_.notify_one();
    return true;
}

std::uint64_t WishWorkflow::clearDisplay() {
    std::lock_guard lock(mutex_);
    ++displayGeneration_;
    auto next = std::make_shared<WishView>(published_ ? *published_ : WishView{});
    next->displayGeneration = displayGeneration_;
    next->items.clear();
    next->lastDraw = 0;
    next->lastDrawAppended = false;
    published_ = std::move(next);
    return displayGeneration_;
}

std::shared_ptr<const WishView> WishWorkflow::view() const {
    std::lock_guard lock(mutex_);
    return published_;
}

void WishWorkflow::run() {
    std::optional<WishRound> round;
    std::uint64_t roundRevision{};
    std::size_t roundPoolSize{};
    std::string diagnostic;

    for (;;) {
        DrawTask task;
        WishSettings settings;
        WishInput input;
        std::uint64_t seed{};
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [&] {
                return stopping_ || pending_.has_value() || roundRevision != revision_;
            });
            if (stopping_) return;
            if (!pending_) {
                round.reset();
                roundRevision = revision_;
                roundPoolSize = 0;
                diagnostic.clear();
                if (published_ && published_->busy && !active_) {
                    auto next = std::make_shared<WishView>(*published_);
                    next->busy = false;
                    published_ = std::move(next);
                }
                continue;
            }
            task = *pending_;
            pending_.reset();
            settings = settings_;
            if (settings.useInput) input = input_;
            seed = seed_;
            active_ = true;
        }

        std::vector<WishItem> drawn;
        std::size_t remaining{};
        std::string error;
        try {
            if (!round || roundRevision != task.revision) {
                round.reset();
                roundPoolSize = 0;
                diagnostic.clear();

                auto filteredSources = styleSources(sources_, settings.style);
                const auto factorySources = static_cast<std::size_t>(std::count_if(
                    filteredSources.begin(), filteredSources.end(),
                    [](const auto& source) { return source.sourceType == "factory"; }));
                auto pool = settings.useInput
                    ? makeInputPool(filteredSources, annotations_, input, settings.filter)
                    : makeFreePool(filteredSources, annotations_, settings.filter);
                if (!pool) {
                    error = pool.error;
                } else {
                    const auto beforeColor = pool.items.size();
                    if (settings.color != ColorDirection::Any)
                        std::erase_if(pool.items, [&](const auto& item) {
                            return !matchesColor(item, settings.color);
                        });
                    const auto colorFiltered = beforeColor - pool.items.size();
                    diagnostic = poolDiagnostic(pool, factorySources, colorFiltered);
                    roundPoolSize = pool.items.size();
                    round.emplace(std::move(pool), seed);
                    roundRevision = task.revision;
                }
            }
            if (error.empty() && round) {
                drawn = round->draw(task.count);
                remaining = round->remaining();
            } else if (error.empty()) {
                error = "wish round is unavailable";
            }
        } catch (const std::exception& exception) {
            error = exception.what();
            if (round) remaining = round->remaining();
            round.reset();
            roundRevision = task.revision;
        } catch (...) {
            error = "unknown wish workflow failure";
            if (round) remaining = round->remaining();
            round.reset();
            roundRevision = task.revision;
        }

        {
            std::lock_guard lock(mutex_);
            active_ = false;
            if (task.revision != revision_) {
                round.reset();
                roundRevision = revision_;
                roundPoolSize = 0;
                diagnostic.clear();
                if (!pending_ && published_ && published_->busy) {
                    auto next = std::make_shared<WishView>(*published_);
                    next->busy = false;
                    published_ = std::move(next);
                }
                continue;
            }

            auto next = std::make_shared<WishView>(*published_);
            next->busy = false;
            next->error = std::move(error);
            next->diagnostic = diagnostic;
            next->poolSize = roundPoolSize;
            next->remaining = remaining;
            if (drawn.size() < task.count && remaining == 0) {
                if (!next->diagnostic.empty()) next->diagnostic += "; ";
                next->diagnostic += "exhausted=1";
            }

            // A clear can happen after the worker has accepted this task. Its samples
            // stay consumed from the round, but the old display generation cannot
            // repopulate the list after the user cleared it.
            if (task.displayGeneration != displayGeneration_) {
                next->displayGeneration = displayGeneration_;
                next->lastDraw = 0;
                next->lastDrawAppended = false;
                published_ = std::move(next);
                continue;
            }

            if (!task.append) next->items.clear();
            for (auto& item : drawn)
                next->items.push_back(std::make_shared<const WishItem>(std::move(item)));
            if (!task.append || !drawn.empty()) ++displayGeneration_;
            ++batchSerial_;
            next->displayGeneration = displayGeneration_;
            next->batchSerial = batchSerial_;
            next->lastDraw = drawn.size();
            next->lastDrawAppended = task.append;
            published_ = std::move(next);
        }
    }
}

} // namespace harmony::wish
