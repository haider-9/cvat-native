#include "cvatnative/ui/Icons.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

namespace cvat {

namespace {

// Every icon is designed on a 24x24 grid and scaled to the requested size.
constexpr qreal kGrid = 24.0;
constexpr int kSizes[] = {16, 20, 24, 32, 48};

void drawUndoArrow(QPainter& p)
{
    QPainterPath path;
    path.moveTo(4, 11);
    path.lineTo(14, 11);
    path.cubicTo(18.4, 11, 20, 13.5, 20, 17);
    p.drawPath(path);
    p.drawPolyline(QPolygonF{QPointF(9, 6), QPointF(4, 11), QPointF(9, 16)});
}

void drawIcon(QPainter& p, Icon icon)
{
    switch (icon) {
    case Icon::Connect:
        // Arrow entering a door: "log in".
        p.drawLine(QPointF(3, 12), QPointF(15, 12));
        p.drawPolyline(QPolygonF{QPointF(11, 8), QPointF(15, 12), QPointF(11, 16)});
        p.drawPolyline(QPolygonF{QPointF(15, 5), QPointF(20, 5), QPointF(20, 19), QPointF(15, 19)});
        break;

    case Icon::Logout:
        // Arrow leaving a door.
        p.drawLine(QPointF(10, 12), QPointF(20, 12));
        p.drawPolyline(QPolygonF{QPointF(16, 8), QPointF(20, 12), QPointF(16, 16)});
        p.drawPolyline(QPolygonF{QPointF(9, 5), QPointF(4, 5), QPointF(4, 19), QPointF(9, 19)});
        break;

    case Icon::Settings:
        // Three sliders.
        p.drawLine(QPointF(4, 6), QPointF(20, 6));
        p.drawLine(QPointF(4, 12), QPointF(20, 12));
        p.drawLine(QPointF(4, 18), QPointF(20, 18));
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(8, 6), 2.5, 2.5);
        p.drawEllipse(QPointF(16, 12), 2.5, 2.5);
        p.drawEllipse(QPointF(10, 18), 2.5, 2.5);
        p.setBrush(Qt::NoBrush);
        break;

    case Icon::Quit:
        // Power symbol.
        p.drawArc(QRectF(5, 5, 14, 14), 130 * 16, 280 * 16);
        p.drawLine(QPointF(12, 3), QPointF(12, 11));
        break;

    case Icon::Dashboard:
        p.drawRoundedRect(QRectF(4, 4, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13, 4, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(4, 13, 7, 7), 1.5, 1.5);
        p.drawRoundedRect(QRectF(13, 13, 7, 7), 1.5, 1.5);
        break;

    case Icon::About:
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawLine(QPointF(12, 11), QPointF(12, 17));
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(12, 7.5), 1.2, 1.2);
        p.setBrush(Qt::NoBrush);
        break;

    case Icon::Back:
        p.drawLine(QPointF(20, 12), QPointF(4, 12));
        p.drawPolyline(QPolygonF{QPointF(10, 6), QPointF(4, 12), QPointF(10, 18)});
        break;

    case Icon::Save:
        // Floppy disk.
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        p.drawRect(QRectF(8, 4, 8, 5));
        p.drawRect(QRectF(7.5, 13, 9, 7));
        break;

    case Icon::Tag:
        // Price tag with a hole.
        p.drawPolyline(QPolygonF{QPointF(3, 12), QPointF(11, 4), QPointF(20, 4),
                                 QPointF(20, 13), QPointF(12, 21), QPointF(3, 12)});
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(16, 8), 1.6, 1.6);
        p.setBrush(Qt::NoBrush);
        break;

    case Icon::Box:
        p.drawRect(QRectF(4, 6, 16, 12));
        break;

    case Icon::Copy:
        p.drawRoundedRect(QRectF(8, 4, 12, 12), 1.5, 1.5);
        p.drawRoundedRect(QRectF(4, 8, 12, 12), 1.5, 1.5);
        break;

    case Icon::Paste:
        // Clipboard.
        p.drawRoundedRect(QRectF(5, 5, 14, 16), 2, 2);
        p.drawRoundedRect(QRectF(9, 3, 6, 4), 1, 1);
        p.drawLine(QPointF(9, 12), QPointF(15, 12));
        p.drawLine(QPointF(9, 16), QPointF(13, 16));
        break;

    case Icon::Layers:
        p.drawPolyline(QPolygonF{QPointF(12, 3), QPointF(21, 8), QPointF(12, 13), QPointF(3, 8), QPointF(12, 3)});
        p.drawPolyline(QPolygonF{QPointF(3, 13), QPointF(12, 18), QPointF(21, 13)});
        break;

    case Icon::Undo:
        drawUndoArrow(p);
        break;

    case Icon::Redo:
        // Undo mirrored horizontally.
        p.save();
        p.translate(kGrid, 0);
        p.scale(-1, 1);
        drawUndoArrow(p);
        p.restore();
        break;

    case Icon::Trash:
        p.drawLine(QPointF(4, 6), QPointF(20, 6));
        p.drawPolyline(QPolygonF{QPointF(9, 6), QPointF(9, 3), QPointF(15, 3), QPointF(15, 6)});
        p.drawPolyline(QPolygonF{QPointF(6, 6), QPointF(7, 21), QPointF(17, 21), QPointF(18, 6)});
        p.drawLine(QPointF(10, 10), QPointF(10, 17));
        p.drawLine(QPointF(14, 10), QPointF(14, 17));
        break;

    case Icon::Previous:
        p.drawPolyline(QPolygonF{QPointF(14, 6), QPointF(8, 12), QPointF(14, 18)});
        break;

    case Icon::Next:
        p.drawPolyline(QPolygonF{QPointF(10, 6), QPointF(16, 12), QPointF(10, 18)});
        break;

    case Icon::Pointer:
        p.drawPolyline(QPolygonF{QPointF(6, 3), QPointF(6, 19), QPointF(10, 15), QPointF(13, 21),
                                 QPointF(15, 20), QPointF(12, 14), QPointF(17, 14), QPointF(6, 3)});
        break;

    case Icon::ZoomIn:
    case Icon::ZoomOut:
        p.drawEllipse(QPointF(10, 10), 7, 7);
        p.drawLine(QPointF(15, 15), QPointF(21, 21));
        p.drawLine(QPointF(7, 10), QPointF(13, 10));
        if (icon == Icon::ZoomIn) {
            p.drawLine(QPointF(10, 7), QPointF(10, 13));
        }
        break;

    case Icon::Crosshair:
        // Circle with four ticks pointing at it.
        p.drawEllipse(QPointF(12, 12), 5, 5);
        p.drawLine(QPointF(12, 2), QPointF(12, 7));
        p.drawLine(QPointF(12, 17), QPointF(12, 22));
        p.drawLine(QPointF(2, 12), QPointF(7, 12));
        p.drawLine(QPointF(17, 12), QPointF(22, 12));
        break;

    case Icon::FitView:
        p.drawPolyline(QPolygonF{QPointF(4, 9), QPointF(4, 4), QPointF(9, 4)});
        p.drawPolyline(QPolygonF{QPointF(15, 4), QPointF(20, 4), QPointF(20, 9)});
        p.drawPolyline(QPolygonF{QPointF(20, 15), QPointF(20, 20), QPointF(15, 20)});
        p.drawPolyline(QPolygonF{QPointF(9, 20), QPointF(4, 20), QPointF(4, 15)});
        break;
    }
}

} // namespace

QIcon makeIcon(Icon icon)
{
    const QColor color = QGuiApplication::palette().color(QPalette::WindowText);

    QIcon result;
    for (int size : kSizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.scale(size / kGrid, size / kGrid);
        painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        drawIcon(painter, icon);
        painter.end();

        result.addPixmap(pixmap);
    }
    return result;
}

} // namespace cvat
