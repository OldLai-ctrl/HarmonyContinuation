#include "MidiImportWorkflow.h"
#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#endif
namespace harmony::midi {
ImportResult importFile(const std::filesystem::path& path,bool openEnded) {
    ImportResult result;result.midi=readFromFile(path);
    if(result.midi){MidiHarmonyExtractionConfig config;config.openEnded=openEnded;result.extraction=extractHarmony(result.midi.file,config);}
    return result;
}
std::optional<std::filesystem::path> chooseMidiFile(void* owner) {
#if defined(_WIN32)
    wchar_t filename[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=static_cast<HWND>(owner);dialog.lpstrFile=filename;dialog.nMaxFile=32768;
    dialog.lpstrFilter=L"MIDI files\0*.mid;*.midi\0\0";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&dialog))return std::filesystem::path(filename);
#else
    (void)owner;
#endif
    return {};
}
}
