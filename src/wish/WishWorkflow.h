#pragma once

#include "WishSampler.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace harmony::wish {

enum class ColorDirection { Any, Warming, Cooling, TensionRising, TensionReleasing };

struct WishSettings {
    bool useInput{};
    WishFilter filter;
    std::optional<Style> style;
    ColorDirection color{ColorDirection::Any};
};

struct WishView {
    std::uint64_t revision{};
    std::vector<std::shared_ptr<const WishItem>> items;
    std::size_t remaining{}, poolSize{}, lastDraw{};
    bool busy{}, usesInput{};
    std::string error;
    std::string diagnostic;
};

// Owns the wish-only settings, input association and no-replacement round.
// All sampling and color analysis run on the worker thread.
class WishWorkflow final {
public:
    WishWorkflow(std::vector<ProgressionTemplate>, AnnotationCatalog);
    ~WishWorkflow();
    WishWorkflow(const WishWorkflow&) = delete;
    WishWorkflow& operator=(const WishWorkflow&) = delete;

    std::uint64_t reset(WishSettings, WishInput = {},
                        std::uint64_t seed = 424242);
    bool draw(std::size_t count);
    std::shared_ptr<const WishView> view() const;

private:
    struct DrawTask {
        std::uint64_t revision{};
        std::size_t count{};
    };

    void run();

    const std::vector<ProgressionTemplate> sources_;
    const AnnotationCatalog annotations_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    WishSettings settings_;
    WishInput input_;
    std::uint64_t seed_{424242}, revision_{};
    std::optional<DrawTask> pending_;
    std::shared_ptr<const WishView> published_;
    bool stopping_{}, active_{};
    std::thread worker_;
};

} // namespace harmony::wish
