#pragma once

#include <QIcon>
#include <QString>

class QAction;
class QColor;
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
    SelectionBox,
    Sidebar,
    ZoomIn,
    ZoomOut,
    FitView,
    Crosshair,
    Update,
};

// Requires a QGuiApplication (the palette is read when the icon is made).
// makeCheckedIcon() uses the highlighted-text colour so checked buttons stay
// legible on the highlighted background.
QIcon makeIcon(Icon icon);
QIcon makeIcon(Icon icon, const QColor& color);
QIcon makeCheckedIcon(Icon icon);

// Actions and buttons made with these helpers remember their icon, so
// refreshIcons() can redraw them after the palette changes. Checkable helpers
// also swap to the high-contrast icon when checked.
QAction* makeAction(Icon icon, const QString& text, QObject* parent);
QAction* makeCheckableAction(Icon icon, const QString& text, QObject* parent);
QAction* addIconAction(QWidget* widget, Icon icon, const QString& text);
void setButtonIcon(QToolButton* button, Icon icon);
void setCheckableButtonIcon(QToolButton* button, Icon icon);
void refreshIcon(QAction* action);
void refreshIcon(QToolButton* button);
void refreshIcons(QWidget* root);

} // namespace cvat
