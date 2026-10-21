#pragma once
#include "PreviewExchange.h"
#include <memory>
namespace harmony::preview {
// Native output and all driver calls belong to a dedicated worker. The UI
// submits bounded desired states and never waits for device control calls.
class WindowsPreview {
public:
    WindowsPreview();
    ~WindowsPreview();
    WindowsPreview(const WindowsPreview&)=delete;
    WindowsPreview& operator=(const WindowsPreview&)=delete;
    bool submit(const TransportRequest&) noexcept;
    TransportFeedback feedback() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
