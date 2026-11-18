#pragma once
#include "MidiClip.h"

namespace harmony::midi {
struct WriteResult { std::vector<std::uint8_t> bytes; std::string error; explicit operator bool() const noexcept {return error.empty();} };
WriteResult writeToMemory(const ExportSequence&,ExportConfig={});
bool writeToFile(const std::vector<std::uint8_t>&,const std::filesystem::path&,std::string& error);
MidiClipPayload makePayload(const ExportSequence&,std::string suggestedName,ExportConfig={});
} // namespace harmony::midi
