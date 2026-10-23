#include "WishCatalog.h"
#include "WishCatalogData.h"
namespace harmony::wish {
AnnotationLoadResult builtinAnnotations() {
    std::string text;
    for(const auto* line:generated::catalogLines)text+=line;
    return parseAnnotations(text);
}
}
