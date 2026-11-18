#pragma once
#include "session/PluginSessionState.h"
#include "LocalizationGenerated.h"
#include <string_view>

namespace harmony::localization {
inline std::string_view text(session::Locale locale, std::string_view key) noexcept {
    for (const auto& entry : generated::entries)
        if (entry.key == key) return locale == session::Locale::EnUS ? entry.enUS : entry.zhCN;
    return key;
}
inline constexpr std::size_t keyCount = generated::entries.size();
} // namespace harmony::localization
