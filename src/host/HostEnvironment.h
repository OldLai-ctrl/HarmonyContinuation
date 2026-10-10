#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace harmony::host {
enum class HostFamily { GenericVst3, Cubase, FLStudio };
enum class Observed { Unknown, Available, Unavailable };
struct HostCapabilities {
    Observed projectTimeMusic{},tempo{},timeSignature{},transportState{},editorResize{},contentScaleFactor{};
    Observed filePathDragSource{},filePathDrop{},hostMidiEvents{},stateRestore{};
    bool editorAttached{},editorDetachedLikeLifecycle{};
};
struct HostTimelineContext {
    bool available{};
    std::optional<double> projectTimeMusic,tempo;
    std::optional<std::pair<int,int>> timeSignature;
    std::optional<bool> playing;
    bool hostMidiEvents{};
};
struct HostEnvironment {
    HostFamily family{HostFamily::GenericVst3};
    std::string name{"Unknown"};
    std::optional<std::string> version; // Standard IHostApplication supplies no version API.
    HostCapabilities capabilities;
    HostTimelineContext timeline;
    double contentScaleFactor{1.};
    unsigned factoryEntries{};std::string librarySource{"not loaded"};
    unsigned userZoom{100};int editorWidth{1100},editorHeight{900};
    std::string lastDropType{"none"},stateRestoreStatus{"not requested"};
    bool lastMidiDragGenerated{};
    unsigned editorAttachCount{};
    void identify(std::string hostName);
    void observeTimeline(const HostTimelineContext&) noexcept;
    void editorAttached(bool value) noexcept;
    std::string diagnostics(bool localizedChinese=false,bool forExport=false) const;
};
const char* familyName(HostFamily) noexcept;
// Windows PlaySound is process-wide. Only the current owner may stop it.
class PreviewOwnership {
public:
    PreviewOwnership() noexcept;
    void claim() noexcept;
    bool release() noexcept;
    bool owns() const noexcept;
private:
    std::uint64_t id_{};
    static std::atomic<std::uint64_t> next_,active_;
};
}
