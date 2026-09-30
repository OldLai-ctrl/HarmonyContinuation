#include "TemplateJson.h"
#include "EmbeddedTemplates.h"
namespace harmony::dev {
JsonTemplates loadDevelopmentTemplates() { return parseTemplateJson(kDevelopmentTemplatesJson); }
}
