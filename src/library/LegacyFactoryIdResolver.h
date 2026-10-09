#pragma once
#include "core/ProgressionMatcher.h"
#include <array>
#include <span>
#include <string_view>

namespace harmony::library {
struct LegacyMapping { std::string_view requested, canonical; };
inline constexpr std::array<LegacyMapping,6> legacyFactoryIds{{
    {"RNB_SOUL_005","CITYPOP_JPOP_003"}, {"FUNCTIONAL_001","COMMON_MAJOR_016"},
    {"FUNCTIONAL_005","COMMON_MAJOR_018"}, {"POP_010","COMMON_MAJOR_020"},
    {"ROCK_001","COMMON_MAJOR_020"}, {"ROCK_004","COMMON_MAJOR_021"}
}};
constexpr bool validRedirects(std::span<const LegacyMapping> mappings) {
    for(std::size_t i=0;i<mappings.size();++i) {
        const auto& m=mappings[i];if(m.requested.empty()||m.canonical.empty()||m.requested==m.canonical)return false;
        for(std::size_t j=0;j<mappings.size();++j) {
            if(m.canonical==mappings[j].requested)return false; // includes cycles and chains
            if(i!=j&&m.requested==mappings[j].requested)return false;
        }
    }
    return true;
}
static_assert(validRedirects(legacyFactoryIds));
std::string canonicalFactoryId(std::string_view);
struct FactoryReference {
    std::string requested, resolved;
    const ProgressionTemplate* entry{};
    explicit operator bool() const noexcept {return entry!=nullptr;}
};
FactoryReference resolveFactoryId(std::string_view,std::span<const ProgressionTemplate>);
bool validateFactoryRedirectTargets(std::span<const ProgressionTemplate>,std::string& error);
// Content identity only: never use the matcher's ranking fingerprint for dedup.
std::string canonicalMusicalFingerprint(const ProgressionTemplate&);
void mergeFactoryMetadata(ProgressionTemplate& target,const ProgressionTemplate& source);
std::vector<ProgressionTemplate> canonicalFactoryEntries(std::vector<ProgressionTemplate>);
std::string resolveFingerprint(std::string_view,int libraryVersion);
} // namespace harmony::library
