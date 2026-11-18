#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace harmony::io {
struct FileDropRoute {bool hasFiles{};std::vector<std::filesystem::path> midiFiles;};
// No filesystem access: routing only validates a bounded UTF-8 file path.
FileDropRoute routeFiles(const std::vector<std::string>&);
}
