#pragma once

#include <algorithm>
#include <cmath>

namespace harmony::ui {

struct WeightBarRect {
    double left{};
    double top{};
    double right{};
    double bottom{};
};

// Keep the bar inside the tile and snap every edge to physical pixels.
// Length carries the structural weight; thickness remains constant.
inline WeightBarRect weightBarRect(double tileLeft, double tileRight, double tileBottom,
                                   double weight, double scaleFactor) {
    const double scale = std::isfinite(scaleFactor) && scaleFactor > 0.0 ? scaleFactor : 1.0;
    const double clampedWeight = std::isfinite(weight) ? std::clamp(weight, 0.0, 1.0) : 0.0;
    const double left = std::ceil((tileLeft + 2.0) * scale);
    const double maxRight = std::floor((tileRight - 2.0) * scale);
    const double bottom = std::floor((tileBottom - 2.0) * scale);
    if (maxRight <= left || clampedWeight <= 0.0) return {};
    const double width = std::max(1.0, std::round((maxRight - left) * clampedWeight));
    const double right = std::min(maxRight, left + width);
    const double thickness = std::max(1.0, std::round(3.0 * scale));
    return {left / scale, (bottom - thickness) / scale, right / scale, bottom / scale};
}

} // namespace harmony::ui
