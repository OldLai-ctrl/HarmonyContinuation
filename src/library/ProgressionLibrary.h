#pragma once
#include "core/ProgressionMatcher.h"
#include <filesystem>
#include <string>
#include <vector>

namespace harmony::library {
constexpr int schemaVersion = 1;
constexpr int factorySchemaVersion = 2;
constexpr int userSchemaVersion = 2;
struct LoadResult {
    std::vector<ProgressionTemplate> templates;
    std::string error;
    int libraryVersion{};
    int storageSchemaVersion{1};
    explicit operator bool() const noexcept { return error.empty(); }
};
bool compileFactory(const std::filesystem::path& output,
                    const std::vector<ProgressionTemplate>& templates, std::string& error,
                    int libraryVersion = 2);
LoadResult loadFactory(const std::filesystem::path& path);

class UserLibrary {
public:
    explicit UserLibrary(std::filesystem::path path) : path_(std::move(path)) {}
    bool addProgression(const ProgressionTemplate&, std::string& error);
    bool updateProgression(const ProgressionTemplate&, std::string& error);
    bool removeProgression(const TemplateID&, std::string& error);
    LoadResult listProgressions() const;
    LoadResult loadAll() const { return listProgressions(); }
private:
    std::filesystem::path path_;
};
} // namespace harmony::library
