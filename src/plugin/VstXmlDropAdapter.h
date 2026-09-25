#pragma once
#include "core/Progression.h"
#include <string_view>

namespace harmony::plugin {
enum class ParseStatus { Success, InvalidXml, UnsupportedSchema, UnsupportedTimeDomain,
                         InvalidChord, ResourceLimit, InternalError };
struct ParseResult {
    ParseStatus status{ParseStatus::InternalError};
    Progression chords;
    std::string detail;
    bool ok() const noexcept { return status == ParseStatus::Success; }
};
// Experimental synthetic-v1 schema ONLY. Not a verified Cubase VST-XML reader.
// Call on a non-realtime thread. ASCII subset, bounded payload; no files or external entities.
class VstXmlDropAdapter {
public:
    static constexpr std::size_t maxPayloadBytes = 1024 * 1024;
    static constexpr std::size_t maxChords = 4096;
    ParseResult parseSyntheticV1(std::string_view xml) const noexcept;
};
}
