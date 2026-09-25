#include "ui/WeightBarGeometry.h"

#include <array>
#include <cmath>
#include <iostream>

int main() {
    using harmony::ui::weightBarRect;
    for (const double scale : std::array{1.0, 1.25, 1.5, 2.0}) {
        const auto light = weightBarRect(24.5, 323.25, 383.0, 0.2, scale);
        const auto heavy = weightBarRect(24.5, 323.25, 383.0, 0.9, scale);
        const auto wholePixel = [scale](double value) {
            return std::abs(value * scale - std::round(value * scale)) < 1e-9;
        };
        const auto full = weightBarRect(24.5, 323.25, 383.0, 1.0, scale);
        const auto absent = weightBarRect(24.5, 323.25, 383.0, 0.0, scale);
        if (light.left != heavy.left || !(light.right < heavy.right) ||
            heavy.right > full.right || absent.right > absent.left ||
            light.bottom != heavy.bottom || heavy.top != light.top ||
            light.left <= 24.5 || light.right >= 323.25 || light.bottom >= 383.0 ||
            !wholePixel(light.left) || !wholePixel(light.right) ||
            !wholePixel(light.top) || !wholePixel(heavy.top) ||
            !wholePixel(light.bottom)) {
            std::cerr << "Weight bar geometry failed at scale " << scale << '\n';
            return 1;
        }
    }
    const auto tiny = weightBarRect(10.0, 13.0, 50.0, 0.5, 2.0);
    if (tiny.right > tiny.left) {
        std::cerr << "Narrow tile should omit weight bar\n";
        return 1;
    }
    std::cout << "Weight bar geometry tests passed\n";
}
