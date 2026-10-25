#pragma once
#include "ModeAnnotations.h"
#include <filesystem>
namespace harmony::wish {
AnnotationLoadResult builtinAnnotations();
AnnotationLoadResult annotationsForFactory(const std::filesystem::path& factoryDb, int version);
}
