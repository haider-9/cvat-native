#include "cvatnative/ui/MiniMap.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <algorithm>

namespace cvat {

namespace {
constexpr int kWidth = 220;
constexpr int kHeight = 160;
constexpr int kMargin = 6;
}

MiniMap::MiniMap(QWidget* parent) : QWidget(parent)
{
    setFixedSize(kWidth, kHeight);
    setCursor(Qt::CrossCursor);
}

void MiniMap::setFrame(const QImage& frame)
{
    imageSize_ = frame.isNull() ? QSizeF() : QSizeF(frame.size());
    if (frame.isNull()) {
        thumbnail_ = QPixmap();
    } else {
        const QSize size = fittedRect().size().toSize();
        thumbnail_ = QPixmap::fromImage(frame.scaled(size, Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation));
    }
    update();
}

void MiniMap::setView(const QRectF& visible, const QSizeF& imageSize)
{
    view_ = visible;
    imageSize_ = imageSize;
    update();
}

QRectF MiniMap::fittedRect() const
{
    const QRectF area = rect().adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (imageSize_.isEmpty()) {
        return area;
    }
    const double scale = std::min(area.width() / imageSize_.width(),
                                  area.height() / imageSize_.height());
    const QSizeF size(imageSize_.width() * scale, imageSize_.height() * scale);
    return QRectF(area.center() - QPointF(size.width(), size.height()) / 2.0, size);
}

QPointF MiniMap::toImage(const QPointF& widgetPoint) const
{
    const QRectF fitted = fittedRect();
    if (fitted.width() <= 0.0 || fitted.height() <= 0.0) {
        return {};
    }
    const double x = (widgetPoint.x() - fitted.left()) / fitted.width() * imageSize_.width();
    const double y = (widgetPoint.y() - fitted.top()) / fitted.height() * imageSize_.height();
    return QPointF(std::clamp(x, 0.0, imageSize_.width()), std::clamp(y, 0.0, imageSize_.height()));
}

void MiniMap::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setPen(QPen(QColor(90, 96, 104), 1));
    painter.setBrush(QColor(20, 22, 26, 215));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    if (thumbnail_.isNull() || imageSize_.isEmpty()) {
        return;
    }
    const QRectF fitted = fittedRect();
    painter.drawPixmap(fitted, thumbnail_, QRectF(thumbnail_.rect()));

    const double scale = fitted.width() / imageSize_.width();
    QRectF viewRect(fitted.left() + view_.left() * scale, fitted.top() + view_.top() * scale,
                    view_.width() * scale, view_.height() * scale);
    viewRect = viewRect.intersected(fitted);
    painter.setPen(QPen(QColor(230, 30, 30), 1.5));
    painter.setBrush(QColor(230, 30, 30, 40));
    painter.drawRect(viewRect);
}

void MiniMap::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = true;
    emit panRequested(toImage(event->position()));
}

void MiniMap::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        emit panRequested(toImage(event->position()));
    }
}

} // namespace cvat
