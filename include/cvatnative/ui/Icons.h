#pragma once

#include <QIcon>

namespace cvat {

// Monochrome line icons drawn in code, so they need no image files or Qt Svg
// plugin and work the same on Linux and Windows. They use the application's
// window text colour, so they follow light and dark palettes.
enum class Icon {
    Connect,
    Logout,
    Settings,
    Quit,
    Dashboard,
    About,
    Back,
    Save,
    Tag,
    Box,
    Copy,
    Paste,
    Layers,
    Undo,
    Redo,
    Trash,
    Previous,
    Next,
    Pointer,
    ZoomIn,
    ZoomOut,
    FitView,
};

// Requires a QGuiApplication (the palette is read when the icon is made).
QIcon makeIcon(Icon icon);

} // namespace cvat
