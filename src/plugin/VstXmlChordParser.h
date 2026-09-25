#pragma once

#include "core/Progression.h"
#include <cstddef>
#include <string>
#include <string_view>

namespace harmony::plugin {

enum class VstXmlStatus {
    Success,
    InvalidXml,
    UnsupportedSchema,
    UnsupportedTimeDomain,
    InvalidChord,
    MissingProjectTime,
    ResourceLimit,
    InternalError
};

struct VstXmlParseResult {
    VstXmlStatus status{VstXmlStatus::InternalError};
    Progression chords;
    std::string detail;
    std::string sourceApp;
    std::string rawTimeDomain;
    std::string rawTimeValue;
    std::size_t failedChordIndex{};
    bool ok() const noexcept { return status == VstXmlStatus::Success; }
};

// Bounded Clipboard VST-XML 1.3/1.4 chord reader backed by Expat. It accepts
// the XML encodings supported by Expat, rejects internal DTD subsets, does not
// resolve external entities, and must only be called off the realtime thread.
class VstXmlChordParser {
public:
    static constexpr std::size_t maxPayloadBytes = 1024 * 1024;
    static constexpr std::size_t maxChords = 4096;
    VstXmlParseResult parse(std::string_view xml,
                            ChordSource source = ChordSource::CubaseDrop) const noexcept;
};

} // namespace harmony::plugin
