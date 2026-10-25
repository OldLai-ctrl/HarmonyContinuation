#include "WishCatalog.h"
#include "WishCatalogData.h"
#include "library/ProgressionLibrary.h"
#include <exception>
#include <unordered_map>
#include <unordered_set>
namespace harmony::wish {
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
    if (boundIds.size() != factory.templates.size())
        return {{}, "unique annotation source count does not match selected Factory"};

    annotations.catalog.entries = std::move(boundEntries);
    return annotations;
}
}
