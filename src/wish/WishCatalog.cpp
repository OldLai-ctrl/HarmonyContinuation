#include "WishCatalog.h"
#include "WishCatalogData.h"
#include "library/ProgressionLibrary.h"
#include <algorithm>
#include <exception>
#include <unordered_map>
#include <unordered_set>
namespace harmony::wish {
std::size_t reviewedSourceCount(const AnnotationCatalog& catalog, MusicalMode mode) {
    return static_cast<std::size_t>(std::count_if(catalog.entries.begin(), catalog.entries.end(), [&](const auto& source) {
        return std::any_of(source.memberships.begin(), source.memberships.end(), [&](const auto& membership) {
            return membership.mode == mode && membership.review == ReviewStatus::Reviewed;
        });
    }));
}
std::string emptyModeReason(MusicalMode mode, bool strict) {
    if (mode == MusicalMode::Locrian) return "无已审校的洛克里亚中心进行";
    if (mode == MusicalMode::ClassicalMelodicMinor)
        return strict ? "严格旋律方向校验尚未实现" : "缺少可验证的旋律上下行归属";
    return "当前约束下无候选";
}
AnnotationLoadResult builtinAnnotations() {
    std::string text;
    for(const auto* line:generated::catalogLines)text+=line;
    return parseAnnotations(text);
}

AnnotationLoadResult annotationsForFactory(const std::filesystem::path& factoryDb, int version) {
    if (version <= 5) return builtinAnnotations();
    if (factoryDb.empty()) return {{}, "Factory V" + std::to_string(version) + " path is unavailable"};

    const auto factory = library::loadFactory(factoryDb);
    if (!factory) return {{}, "cannot load Factory for annotation binding: " + factory.error};
    if (factory.libraryVersion != version)
        return {{}, "Factory version changed while loading annotations"};

    auto annotations = loadAnnotations(factoryDb.parent_path() / "factory.hca");
    if (!annotations) return annotations;

    std::unordered_map<std::string, const ProgressionTemplate*> sources;
    sources.reserve(factory.templates.size());
    for (const auto& source : factory.templates) sources.emplace(source.id, &source);
    std::unordered_set<std::string> boundIds;
    boundIds.reserve(factory.templates.size());
    std::vector<ModeAnnotation> boundEntries;
    boundEntries.reserve(factory.templates.size());
    for (const auto& entry : annotations.catalog.entries) {
        const auto source = sources.find(entry.templateId);
        if (source == sources.end()) continue;
        try {
            if (entry.sourceFingerprint != sourceFingerprint(*source->second))
                return {{}, "annotation source fingerprint does not match Factory: " + entry.templateId};
        } catch (const std::exception& error) {
            return {{}, "cannot fingerprint Factory source " + entry.templateId + ": " + error.what()};
        }
        boundIds.insert(entry.templateId);
        boundEntries.push_back(entry);
    }
    // Factory V8 retains the V7 mode catalog. Its separately reviewed phrase
    // increment has no authored mode memberships.
    for (const auto& source : factory.templates) {
        if (boundIds.contains(source.id)) continue;
        const bool unannotatedPhrase = version == 8 && source.version == 8 &&
            source.id.starts_with("V8_01_") &&
            std::find(source.tags.begin(), source.tags.end(), "G8-complete") != source.tags.end();
        if (!unannotatedPhrase)
            return {{}, "annotation source is missing for selected Factory: " + source.id};
    }

    annotations.catalog.entries = std::move(boundEntries);
    return annotations;
}
}
