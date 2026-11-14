#pragma once

#include <QIcon>
#include <QString>

class QAction;
class QObject;
class QToolButton;
class QWidget;

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
    Crosshair,
};

// Requires a QGuiApplication (the palette is read when the icon is made).
QIcon makeIcon(Icon icon);

// Actions and buttons made with these helpers remember their icon, so
// refreshIcons() can redraw them after the palette changes.
QAction* makeAction(Icon icon, const QString& text, QObject* parent);
QAction* addIconAction(QWidget* widget, Icon icon, const QString& text);
void setButtonIcon(QToolButton* button, Icon icon);
void refreshIcons(QWidget* root);

} // namespace cvat
