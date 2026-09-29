#pragma once
#include "core/ProgressionMatcher.h"
#include <string>
#include <string_view>

namespace harmony::dev {
struct JsonTemplates {
    std::vector<ProgressionTemplate> templates;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};
JsonTemplates parseTemplateJson(std::string_view, bool requireFactoryMetadata = false);
JsonTemplates loadDevelopmentTemplates(); // embedded, test-only library for MATCH view
MatchEvent parseNotationEvent(std::string_view token, Mode mode, double durationQN = 4.0);
std::string serializeTemplateJson(const ProgressionTemplate&);
} // namespace harmony::dev
