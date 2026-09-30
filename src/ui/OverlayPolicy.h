#pragma once
#include "ui/UILayout.h"

namespace harmony::ui {
enum class OverlayKind { None, Transient, Persistent, Modal };

inline bool dismissOnOutsideClick(OverlayKind kind, UiRect bounds, double x, double y) noexcept {
    return kind == OverlayKind::Transient && !bounds.contains(x, y);
}
inline bool dismissOnEscape(OverlayKind kind) noexcept {
    return kind == OverlayKind::Transient;
}
} // namespace harmony::ui
