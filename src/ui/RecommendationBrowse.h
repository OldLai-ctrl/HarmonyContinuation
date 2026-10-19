#pragma once

#include "session/ProductServices.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace harmony::ui {

constexpr std::size_t recommendationPageSize = 10;

struct BrowsePage {
    std::size_t page{}, pages{}, begin{}, end{}, total{};
};

inline BrowsePage browsePage(std::size_t total, std::size_t requested) noexcept {
    if (total == 0) return {};
    const auto pages = total / recommendationPageSize +
        (total % recommendationPageSize != 0);
    const auto page = std::min(requested, pages - 1);
    const auto begin = page * recommendationPageSize;
    const auto count = std::min(recommendationPageSize, total - begin);
    return {page, pages, begin, begin + count, total};
}

struct BrowseEntry {
    std::size_t group{}, index{};
    bool priority{}, exploratory{};
};

inline std::vector<std::size_t> qualifiedBrowseOrder(
    const std::vector<ContinuationCandidate>& group,
    const std::vector<std::size_t>& preferredOrder = {}) {
    const auto access = session::continuationAccess(group);
    std::vector<bool> qualified(group.size());
    std::vector<bool> emitted(group.size());
    std::vector<std::size_t> order;
    order.reserve(access.more.size());

    for (const auto index : access.more)
        if (index < group.size()) qualified[index] = true;

    const auto append = [&](std::size_t index) {
        if (index >= group.size() || !qualified[index] || emitted[index]) return;
        order.push_back(index);
        emitted[index] = true;
    };
    for (const auto index : preferredOrder) append(index);
    for (const auto index : access.more) append(index);
    return order;
}

} // namespace harmony::ui
