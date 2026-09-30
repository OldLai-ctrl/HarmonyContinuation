#pragma once
#include "ProgressionLibrary.h"

namespace harmony::library {
// Machine factory packages and per-user creations have independent lifecycles.
std::filesystem::path factoryStoreDirectory();
std::filesystem::path userDatabasePath();
struct FactorySelection {
    LoadResult library;
    std::filesystem::path path;
    std::string warning;
};
FactorySelection loadAvailableFactory(const std::filesystem::path& bundled,
    const std::filesystem::path& store = factoryStoreDirectory());
// Immutable version directory + atomic active pointer. Previous packages remain local.
bool installFactoryPackage(const std::filesystem::path& source, const std::filesystem::path& store,
    std::string& error);
} // namespace harmony::library
