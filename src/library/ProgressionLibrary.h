#pragma once
#include "core/ProgressionMatcher.h"
#include <filesystem>
#include <string>
#include <vector>

namespace harmony::library {
constexpr int schemaVersion = 1;
struct LoadResult {
    std::vector<ProgressionTemplate> templates;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};
bool compileFactory(const std::filesystem::path& output,
                    const std::vector<ProgressionTemplate>& templates, std::string& error);
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
