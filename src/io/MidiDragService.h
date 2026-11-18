#pragma once
#include "midi/MidiWorkflow.h"
namespace harmony::io {
class MidiDragService {
public:
    midi::DragFileResult prepare(const midi::MidiClipPayload& payload,const std::filesystem::path& directory=
        std::filesystem::temp_directory_path()/"HarmonyContinuation"/"MidiDrag") {
        return midi::createDragFile(payload,directory);
    }
}; // Host adapters never own the temporary file or the export profile.
}
