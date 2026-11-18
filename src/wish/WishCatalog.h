#pragma once
#include "ModeAnnotations.h"
#include "library/ProgressionLibrary.h"
#include <filesystem>
#include <cstddef>
namespace harmony::wish {
std::size_t reviewedSourceCount(const AnnotationCatalog&, MusicalMode);
std::string emptyModeReason(MusicalMode, bool strict);
AnnotationLoadResult builtinAnnotations();
// Binds the authored mode catalog to one Factory. Overload taking an already loaded
// Factory avoids a second full parse of the same database (UI attach loads it once).
AnnotationLoadResult annotationsForFactory(const library::LoadResult& factory,
    const std::filesystem::path& factoryDb, int version);
AnnotationLoadResult annotationsForFactory(const std::filesystem::path& factoryDb, int version);
}
