#pragma once

#include "cvatnative/model/Annotation.h"

#include <QPointF>
#include <QRectF>
#include <QVector>

namespace cvat {

struct RectD {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    [[nodiscard]] QRectF toRectF() const { return QRectF(x, y, width, height); }
    [[nodiscard]] QPointF center() const { return QPointF(x + width / 2.0, y + height / 2.0); }
    [[nodiscard]] bool valid() const { return width > 0.0 && height > 0.0; }
};

// Stateless geometry used by both the canvas and unit tests. Coordinates are
// always CVAT image pixels, never widget coordinates.
class Geometry {
public:
    static RectD boundingRect(const QVector<double>& points);
    static RectD rectangle(const QVector<double>& points);

    static bool pointInPolygon(const QVector<double>& polygon, double x, double y);
    static double distanceToSegment(double px, double py, double x1, double y1,
                                    double x2, double y2);

    // Distance from a point to the shape outline/area. Returns a large value
    // for non-hit shapes; callers compare against a pixel tolerance.
    static double distanceToShape(const Shape& shape, double x, double y);
    static bool hitTest(const Shape& shape, double x, double y, double tolerance);

    static void translate(QVector<double>& points, double dx, double dy);
    static void scale(QVector<double>& points, const RectD& from, const RectD& to);
    static void rotate(QVector<double>& points, double angleDegrees, QPointF center);

    static QVector<double> rectanglePoints(double x, double y, double width, double height);
    static QVector<double> rectanglePoints(const RectD& rect);
    static QVector<double> rotatedRectangleCorners(const RectD& rect, double angleDegrees);

    static QRectF bounds(const QVector<double>& points);
    static double clamp(double value, double minimum, double maximum);
    static QPointF closestPointOnSegment(double px, double py, double x1, double y1,
                                         double x2, double y2);
};

} // namespace cvat
