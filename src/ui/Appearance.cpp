#include "cvatnative/ui/Appearance.h"

#include "cvatnative/core/Settings.h"
#include "cvatnative/ui/Icons.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>

#include <memory>

namespace cvat {

namespace {

struct SystemAppearance {
    QString style;
    QPalette palette;
    QFont font;
};

const SystemAppearance& systemAppearance()
{
    static const SystemAppearance saved{
        QApplication::style()->name(),
        QApplication::palette(),
        QApplication::font(),
    };
    return saved;
}

QPalette darkPalette()
{
    const QColor window(53, 53, 53);
    const QColor base(35, 35, 35);
    const QColor text(235, 235, 235);
    const QColor highlight(42, 130, 218);
    const QColor disabled(127, 127, 127);

    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::ToolTipBase, window);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, window);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::black);
    palette.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    return palette;
}

QPalette lightPalette()
{
    // The Fusion style's standard palette is a consistent light scheme that
    // does not depend on the desktop environment.
    const std::unique_ptr<QStyle> fusion(QStyleFactory::create(QStringLiteral("Fusion")));
    return fusion ? fusion->standardPalette() : QPalette();
}

} // namespace

void applyAppearance()
{
    const SystemAppearance& system = systemAppearance();
    const auto& settings = Settings::instance();
    const QString mode = settings.themeMode();

    // setStyle resets the palette, so the palette is set afterwards.
    if (mode == QLatin1String("light") || mode == QLatin1String("dark")) {
        QApplication::setStyle(QStringLiteral("Fusion"));
        QApplication::setPalette(mode == QLatin1String("dark") ? darkPalette() : lightPalette());
    } else {
        QApplication::setStyle(system.style);
        QApplication::setPalette(system.palette);
    }

    QFont font = system.font;
    if (!settings.fontFamily().isEmpty()) {
        font.setFamily(settings.fontFamily());
    }
    if (settings.fontPointSize() > 0) {
        font.setPointSize(settings.fontPointSize());
    }
    QApplication::setFont(font);

    // Icons are drawn with the palette's text colour, so redraw them.
    for (QWidget* window : QApplication::topLevelWidgets()) {
        refreshIcons(window);
    }
}

} // namespace cvat
