#pragma once
#include "ModeAnnotations.h"
#include <filesystem>
#include <cstddef>
namespace harmony::wish {
std::size_t reviewedSourceCount(const AnnotationCatalog&, MusicalMode);
std::string emptyModeReason(MusicalMode, bool strict);
AnnotationLoadResult builtinAnnotations();
AnnotationLoadResult annotationsForFactory(const std::filesystem::path& factoryDb, int version);
}
