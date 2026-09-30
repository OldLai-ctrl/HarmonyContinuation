#include "HostEnvironment.h"
#include "product/ProductVersion.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace harmony::host {
namespace {
const char* status(Observed v){return v==Observed::Available?"observed available":v==Observed::Unavailable?"currently unavailable":"not observed";}
Observed seen(bool v){return v?Observed::Available:Observed::Unavailable;}
}
const char* familyName(HostFamily v) noexcept {
    return v==HostFamily::Cubase?"Cubase":v==HostFamily::FLStudio?"FL Studio":"Generic VST3";
}
void HostEnvironment::identify(std::string value){
    if(value.empty()||value.size()>128||value.find_first_of("\\/:\r\n\t")!=std::string::npos)value="Unknown";
    name=std::move(value);auto lower=name;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    family=lower.find("cubase")!=std::string::npos?HostFamily::Cubase:
        lower.find("fl studio")!=std::string::npos||lower.find("flstudio")!=std::string::npos?HostFamily::FLStudio:HostFamily::GenericVst3;
    // Identification never fabricates capabilities.
}
void HostEnvironment::observeTimeline(const HostTimelineContext& value) noexcept {
    timeline=value;
    capabilities.projectTimeMusic=seen(value.projectTimeMusic.has_value());
    capabilities.tempo=seen(value.tempo.has_value());capabilities.timeSignature=seen(value.timeSignature.has_value());
    capabilities.transportState=seen(value.playing.has_value());
    if(value.hostMidiEvents)capabilities.hostMidiEvents=Observed::Available;
}
void HostEnvironment::editorAttached(bool value) noexcept {
    capabilities.editorAttached=value;
    if(value){if(editorAttachCount++)capabilities.editorDetachedLikeLifecycle=true;}
}
std::string HostEnvironment::diagnostics(bool chinese,bool exported) const {
    const auto safeName=exported?(family!=HostFamily::GenericVst3?std::string(familyName(family)):
        (name=="EditorHost"||name=="VST3PluginTestHost"||name=="HostContractHarness"?name:std::string("Generic/Unknown"))):name;
    std::ostringstream out;
    out<<(chinese?"宿主诊断\n":"Host Diagnostics\n")<<"HarmonyContinuation "<<product::version<<"\nVST3\n"
       <<"Host Name: "<<safeName<<"\nHost Family: "<<familyName(family)
       <<"\nHost Version: "<<(version?*version:"unavailable (standard VST3 API)")
       <<"\nLibrary Version: 2\nSession Schema: 5\nContent Scale Factor: "<<contentScaleFactor
       <<"\nFactory Entries: "<<factoryEntries<<"\nLibrary Source: "<<librarySource
       <<"\nUser Zoom: "<<userZoom<<"%\nEditor Size: "<<editorWidth<<'x'<<editorHeight
       <<"\nProjectTimeMusic: "<<status(capabilities.projectTimeMusic)<<"\nTempo: "<<status(capabilities.tempo)
       <<"\nTimeSignature: "<<status(capabilities.timeSignature)<<"\nTransportState: "<<status(capabilities.transportState)
       <<"\nEditorResize: "<<status(capabilities.editorResize)<<"\nContentScaleSupport: "<<status(capabilities.contentScaleFactor)
       <<"\nFilePathDragSource: "<<status(capabilities.filePathDragSource)<<"\nFilePathDrop: "<<status(capabilities.filePathDrop)
       <<"\nHostMIDIEvents: "<<status(capabilities.hostMidiEvents)<<"\nStateRestore: "<<status(capabilities.stateRestore)
       <<"\nCurrent QN: "<<(timeline.projectTimeMusic?std::to_string(*timeline.projectTimeMusic):"unavailable")
       <<"\nCurrent Tempo: "<<(timeline.tempo?std::to_string(*timeline.tempo):"unavailable")
       <<"\nCurrent Meter: "<<(timeline.timeSignature?std::to_string(timeline.timeSignature->first)+"/"+std::to_string(timeline.timeSignature->second):"unavailable")
       <<"\nCurrent Playing: "<<(timeline.playing?(*timeline.playing?"true":"false"):"unavailable")
       <<"\nEditorAttached: "<<capabilities.editorAttached<<"\nDetachedLikeLifecycleObserved: "<<capabilities.editorDetachedLikeLifecycle
       <<"\nLast Drop Type: "<<lastDropType<<"\nLast MIDI Drag Generated: "<<lastMidiDragGenerated
       <<"\nState Restore Status: "<<stateRestoreStatus<<"\n"
       <<(chinese?"Cubase：既有工作流已验证；FL Studio 20+：兼容候选，待实机验证。\n":"Cubase: established workflow verified; FL Studio 20+: compatibility candidate, real-host verification pending.\n");
    if(family==HostFamily::FLStudio)out<<(chinese?"若 FL Studio 未将文件交给插件，请检查 Wrapper 的 Accept dropped files 设置。\n":
        "If FL Studio does not pass files to the plug-in, check the Wrapper's Accept dropped files setting.\n");
    return out.str(); // Deliberately no notes, progression, library contents, paths or instance IDs.
}
std::atomic<std::uint64_t> PreviewOwnership::next_{},PreviewOwnership::active_{};
PreviewOwnership::PreviewOwnership() noexcept:id_(++next_){}
void PreviewOwnership::claim() noexcept {active_.store(id_);}
bool PreviewOwnership::release() noexcept {auto expected=id_;return active_.compare_exchange_strong(expected,0);}
bool PreviewOwnership::owns() const noexcept {return active_.load()==id_;}
}
