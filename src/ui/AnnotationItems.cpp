#include "cvatnative/ui/AnnotationItems.h"

#include "cvatnative/annotation/Geometry.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>

#include <algorithm>

namespace cvat {

AnnotationItem::AnnotationItem(QString key, Shape shape, QColor color, QString labelName,
                               QGraphicsItem* parent)
    : QGraphicsItem(parent),
      key_(std::move(key)),
      shape_(std::move(shape)),
      color_(std::move(color)),
      labelName_(std::move(labelName)),
      type_(shape_.type)
{
    setFlag(ItemIsSelectable, true);
    setAcceptHoverEvents(true);
    setZValue(shape_.zOrder);
}

QPolygonF AnnotationItem::polygon() const
{
    QPolygonF result;
    if (type_ == QStringLiteral("rectangle") || type_ == QStringLiteral("ellipse") ||
        type_ == QStringLiteral("mask")) {
        if (shape_.points.size() >= 4) {
            const RectD rect = Geometry::rectangle(shape_.points);
            result << QPointF(rect.x, rect.y)
                   << QPointF(rect.x + rect.width, rect.y)
                   << QPointF(rect.x + rect.width, rect.y + rect.height)
                   << QPointF(rect.x, rect.y + rect.height);
        }
        return result;
    }
    for (qsizetype i = 0; i + 1 < shape_.points.size(); i += 2) {
        result << QPointF(shape_.points[i], shape_.points[i + 1]);
    }
    return result;
}

bool AnnotationItem::closed() const
{
    return shapeTypeIsClosed(type_);
}

QRectF AnnotationItem::boundingRect() const
{
    const QRectF points = polygon().boundingRect();
    // Room for cosmetic pens, vertex handles and the label above the object.
    return points.adjusted(-12, -28, 12, 12);
}

QVector<QPointF> AnnotationItem::handlePoints() const
{
    return polygon();
}

int AnnotationItem::handleAt(const QPointF& position, double tolerance) const
{
    const QVector<QPointF> handles = handlePoints();
    for (qsizetype i = 0; i < handles.size(); ++i) {
        if (QLineF(position, handles[i]).length() <= tolerance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AnnotationItem::setShape(const Shape& shape)
{
    prepareGeometryChange();
    shape_ = shape;
    setZValue(shape_.zOrder);
    update();
}

void AnnotationItem::setLabelName(const QString& name, const QColor& color)
{
    labelName_ = name;
    color_ = color;
    update();
}

QPainterPath AnnotationItem::shape() const
{
    const QPolygonF points = polygon();
    if (points.isEmpty()) {
        return {};
    }
    QPainterPath path;
    if (closed()) {
        path.addPolygon(points);
        path.closeSubpath();
    } else {
        path.moveTo(points.first());
        for (qsizetype i = 1; i < points.size(); ++i) {
            path.lineTo(points[i]);
        }
        QPainterPathStroker stroker;
        stroker.setWidth(6.0);
        path = stroker.createStroke(path);
    }
    for (const auto& point : points) {
        path.addEllipse(point, 5.0, 5.0);
    }
    return path;
}

void AnnotationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool selected = option && (option->state & QStyle::State_Selected);
    const QPolygonF points = polygon();
    if (points.isEmpty()) {
        return;
    }

    QPen pen(color_);
    pen.setCosmetic(true);
    pen.setWidthF(selected ? 2.5 : 1.8);
    if (shape_.occluded) {
        pen.setStyle(Qt::DashLine);
    }
    painter->setPen(pen);
    painter->setBrush(selected ? QColor(color_.red(), color_.green(), color_.blue(), 45)
                               : QColor(color_.red(), color_.green(), color_.blue(), 18));

    if (type_ == QStringLiteral("points")) {
        painter->setBrush(Qt::NoBrush);
        for (const auto& point : points) {
            painter->drawEllipse(point, 5.0, 5.0);
        }
    } else if (closed()) {
        painter->drawPolygon(points);
    } else if (type_ == QStringLiteral("polyline")) {
        painter->setBrush(Qt::NoBrush);
        painter->drawPolyline(points);
    } else {
        painter->drawPolygon(points);
    }

    if (selected) {
        const double scale = painter->transform().m11();
        const double handle = 7.0 / std::max(0.05, scale);
        painter->setPen(QPen(QColor(30, 30, 30), 1.0 / std::max(0.05, scale)));
        painter->setBrush(Qt::white);
        for (const auto& point : points) {
            painter->drawRect(QRectF(point.x() - handle / 2.0, point.y() - handle / 2.0,
                                     handle, handle));
        }
        painter->setBrush(color_);
    }

    if (!labelName_.isEmpty()) {
        QFont font = painter->font();
        font.setPointSizeF(9.0);
        painter->setFont(font);
        const QRectF bounds = points.boundingRect();
        const QString text = labelName_;
        const QFontMetricsF metrics(font);
        const QRectF textRect(bounds.left(), bounds.top() - metrics.height() - 4,
                              metrics.horizontalAdvance(text) + 8, metrics.height() + 2);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(20, 20, 20, 180));
        painter->drawRect(textRect);
        painter->setPen(QColor(245, 245, 245));
        painter->drawText(textRect.adjusted(4, 0, -4, 0), Qt::AlignVCenter, text);
    }
}

} // namespace cvat
