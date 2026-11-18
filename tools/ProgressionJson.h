#pragma once

#include "core/Progression.h"
#include <string>
#include <string_view>

namespace harmony::dev {
struct JsonProgression {
    Progression events;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};
// Deliberately narrow development input: an array of flat chord objects.
JsonProgression parseProgressionJson(std::string_view text);
} // namespace harmony::dev
