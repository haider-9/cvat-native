#pragma once

#include "cvatnative/model/Annotation.h"

#include <QColor>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QString>
#include <QVector>

namespace cvat {

// One CVAT object rendered in image-pixel scene coordinates. Geometry lives in
// the item itself so dragging/resizing is smooth; the model is only updated on
// mouse release, which also produces a single undo step.
class AnnotationItem : public QGraphicsItem {
public:
    AnnotationItem(QString key, Shape shape, QColor color, QString labelName,
                   QGraphicsItem* parent = nullptr);

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    [[nodiscard]] QPainterPath shape() const override;

    [[nodiscard]] const QString& key() const { return key_; }
    [[nodiscard]] const Shape& annotationShape() const { return shape_; }
    void setShape(const Shape& shape);
    void setLabelName(const QString& name, const QColor& color);

    [[nodiscard]] int handleAt(const QPointF& position, double tolerance) const;
    [[nodiscard]] QVector<QPointF> handlePoints() const;

private:
    [[nodiscard]] QPolygonF polygon() const;
    [[nodiscard]] bool closed() const;

    QString key_;
    Shape shape_;
    QColor color_;
    QString labelName_;
    QString type_;
};

} // namespace cvat
