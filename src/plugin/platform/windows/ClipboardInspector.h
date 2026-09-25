#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace harmony::plugin::windows {

struct ClipboardFormatReport {
    unsigned int formatId{};
    std::string formatName;
    std::string payloadType;
    std::size_t payloadBytes{};
    bool dataAvailable{};
    bool fullPayloadReadable{};
    std::string readableAs{"否"};
    bool looksLikeXml{};
    bool containsVstXml{};
    std::string printablePreview;
    std::string hexPreview;
    std::string decodedText;
    std::string error;
};

struct ClipboardInspection {
    bool opened{};
    std::string summary;
    std::vector<ClipboardFormatReport> formats;
};

// Must only be called in direct response to the user's explicit Inspect Clipboard action.
ClipboardInspection inspectNativeClipboard() noexcept;

} // namespace harmony::plugin::windows
