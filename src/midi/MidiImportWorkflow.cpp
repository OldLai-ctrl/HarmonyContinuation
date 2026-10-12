#include "MidiImportWorkflow.h"
#include <algorithm>
#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#endif
namespace harmony::midi {
std::optional<std::pair<int,int>> uniformImportedMeter(const ImportResult& result) {
    if(!result || result.extraction.chords.empty())return {};
    const auto first=result.extraction.chords.front().startQN;
    const auto& last=result.extraction.chords.back();
    double end=last.startQN+last.durationQN.value_or(0.);
    for(const auto& slice:result.extraction.slices)
        end=(std::max)(end,slice.startQN+slice.durationQN);
    std::optional<std::pair<int,int>> active;
    for(const auto& meter:result.midi.file.meters) {
        if(meter.qn>first && meter.qn>=end)break;
        const bool valid=meter.numerator>0 && meter.numerator<=32 &&
            meter.denominator>0 && meter.denominator<=32 &&
            (meter.denominator&(meter.denominator-1))==0;
        const auto value=valid?std::optional(std::pair(meter.numerator,meter.denominator)):std::nullopt;
        if(meter.qn<=first)active=value;
        else if(!active || active!=value)return {};
    }
    return active;
}
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
