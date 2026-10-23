#pragma once
#include "ProductVersionGenerated.h"
#include <iostream>
#include <string>
#include <string_view>

namespace harmony::product {
inline constexpr std::string_view version = HC_PRODUCT_VERSION;
inline constexpr std::string_view commit = HC_PRODUCT_GIT_COMMIT;
inline constexpr std::string_view buildType = HC_PRODUCT_BUILD_TYPE;
inline constexpr int factoryLibraryVersion = HC_PRODUCT_FACTORY_LIBRARY_VERSION;
inline constexpr int databaseSchemaVersion = 2;

inline std::string banner() {
    return "HarmonyContinuation " + std::string(version) + "\ncommit " +
        std::string(commit) + "\nbuild " + std::string(buildType) +
        "\nlibrary " + std::to_string(factoryLibraryVersion);
}
inline bool printVersionIfRequested(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]) != "--version") return false;
    std::cout << banner() << '\n';
    return true;
}
} // namespace harmony::product
