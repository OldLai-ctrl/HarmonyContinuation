#pragma once
#include "demo/DemoScenario.h"
#include "snapshot/RecommendationSnapshot.h"
#include <filesystem>
#include <string>
#include <vector>

namespace harmony::benchmark {
struct Case {
    std::string id,name,category,notes;
    std::vector<std::string> expectedCharacteristics;
    demo::Scenario scenario;
};
Case loadCase(const std::filesystem::path&);
std::vector<std::filesystem::path> caseFiles(const std::filesystem::path& directory);

inline constexpr int matchingConfigVersion=1;
inline constexpr int recommendationConfigVersion=1;
struct Rating {
    int schemaVersion{1};
    std::string benchmarkId,candidateFingerprint,verdict{"QUESTIONABLE"},issueCategory{"Other"},note;
    int naturalness{3},intentFit{3},rhythmFit{3},distinctiveness{3},usability{3};
    int libraryVersion{2},sessionSchemaVersion{session::PluginSessionState::currentSchemaVersion};
    int matchingVersion{matchingConfigVersion},recommendationVersion{recommendationConfigVersion};
    snapshot::RecommendationSnapshot recommendation;
};
std::string ratingFilename(const Rating&);
bool saveRating(const Rating&,const std::filesystem::path& directory,std::string& error);
Rating loadRating(const std::filesystem::path&);
} // namespace harmony::benchmark
