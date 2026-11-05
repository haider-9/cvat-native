#include "cvatnative/annotation/Geometry.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace cvat {

RectD Geometry::boundingRect(const QVector<double>& points)
{
    if (points.size() < 2) {
        return {};
    }
    double minX = points[0];
    double maxX = points[0];
    double minY = points[1];
    double maxY = points[1];
    for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
        minX = std::min(minX, points[i]);
        maxX = std::max(maxX, points[i]);
        minY = std::min(minY, points[i + 1]);
        maxY = std::max(maxY, points[i + 1]);
    }
    return RectD{minX, minY, maxX - minX, maxY - minY};
}

RectD Geometry::rectangle(const QVector<double>& points)
{
    if (points.size() < 4) {
        return {};
    }
    const double x1 = points[0];
    const double y1 = points[1];
    const double x2 = points[2];
    const double y2 = points[3];
    return RectD{std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1), std::abs(y2 - y1)};
}

bool Geometry::pointInPolygon(const QVector<double>& polygon, double x, double y)
{
    if (polygon.size() < 6) {
        return false;
    }
    bool inside = false;
    const qsizetype count = polygon.size() / 2;
    for (qsizetype i = 0, j = count - 1; i < count; j = i++) {
        const double xi = polygon[i * 2];
        const double yi = polygon[i * 2 + 1];
        const double xj = polygon[j * 2];
        const double yj = polygon[j * 2 + 1];
        const bool intersects = ((yi > y) != (yj > y)) &&
            (x < (xj - xi) * (y - yi) / ((yj - yi) == 0.0 ? 1e-12 : (yj - yi)) + xi);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

double Geometry::distanceToSegment(double px, double py, double x1, double y1,
                                   double x2, double y2)
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1e-12) {
        return std::hypot(px - x1, py - y1);
    }
    double t = ((px - x1) * dx + (py - y1) * dy) / lengthSquared;
    t = clamp(t, 0.0, 1.0);
    return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
}

double Geometry::distanceToShape(const Shape& shape, double x, double y)
{
    const double tolerance = 1e9;
    if (shape.points.size() < 2) {
        return tolerance;
    }
    const QString type = shape.type;
    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") ||
        type == QStringLiteral("cuboid") || type == QStringLiteral("mask")) {
        const RectD rect = rectangle(shape.points);
        if (x >= rect.x && x <= rect.x + rect.width && y >= rect.y &&
            y <= rect.y + rect.height) {
            return 0.0;
        }
        const double left = std::abs(x - rect.x);
        const double right = std::abs(x - (rect.x + rect.width));
        const double top = std::abs(y - rect.y);
        const double bottom = std::abs(y - (rect.y + rect.height));
        const double dx = std::min(left, right);
        const double dy = std::min(top, bottom);
        return std::min(dx, dy);
    }

    const bool closed = type == QStringLiteral("polygon") ||
        type == QStringLiteral("skeleton");
    if (closed && pointInPolygon(shape.points, x, y)) {
        return 0.0;
    }
    double best = tolerance;
    const qsizetype count = shape.points.size() / 2;
    const qsizetype segments = closed ? count : count - 1;
    for (qsizetype i = 0; i < segments; ++i) {
        const qsizetype next = (i + 1) % count;
        best = std::min(best, distanceToSegment(x, y, shape.points[i * 2],
                                                shape.points[i * 2 + 1],
                                                shape.points[next * 2],
                                                shape.points[next * 2 + 1]));
    }
    if (type == QStringLiteral("points")) {
        for (qsizetype i = 0; i + 1 < shape.points.size(); i += 2) {
            best = std::min(best, std::hypot(x - shape.points[i], y - shape.points[i + 1]));
        }
    }
    return best;
}

bool Geometry::hitTest(const Shape& shape, double x, double y, double tolerance)
{
    return distanceToShape(shape, x, y) <= tolerance;
}

void Geometry::translate(QVector<double>& points, double dx, double dy)
{
    for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
        points[i] += dx;
        points[i + 1] += dy;
    }
}

void Geometry::scale(QVector<double>& points, const RectD& from, const RectD& to)
{
    if (from.width <= 0.0 || from.height <= 0.0) {
        return;
    }
    const double sx = to.width / from.width;
    const double sy = to.height / from.height;
    for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
        points[i] = to.x + (points[i] - from.x) * sx;
        points[i + 1] = to.y + (points[i + 1] - from.y) * sy;
    }
}

void Geometry::rotate(QVector<double>& points, double angleDegrees, QPointF center)
{
    const double radians = qDegreesToRadians(angleDegrees);
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
        const double dx = points[i] - center.x();
        const double dy = points[i + 1] - center.y();
        points[i] = center.x() + dx * cosine - dy * sine;
        points[i + 1] = center.y() + dx * sine + dy * cosine;
    }
}

QVector<double> Geometry::rectanglePoints(double x, double y, double width, double height)
{
    return {x, y, x + width, y, x + width, y + height, x, y + height};
}

QVector<double> Geometry::rectanglePoints(const RectD& rect)
{
    return rectanglePoints(rect.x, rect.y, rect.width, rect.height);
}

QVector<double> Geometry::rotatedRectangleCorners(const RectD& rect, double angleDegrees)
{
    QVector<double> points = rectanglePoints(rect);
    rotate(points, angleDegrees, rect.center());
    return points;
}

QRectF Geometry::bounds(const QVector<double>& points)
{
    return boundingRect(points).toRectF();
}

double Geometry::clamp(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

QPointF Geometry::closestPointOnSegment(double px, double py, double x1, double y1,
                                        double x2, double y2)
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1e-12) {
        return QPointF(x1, y1);
    }
    double t = ((px - x1) * dx + (py - y1) * dy) / lengthSquared;
    t = clamp(t, 0.0, 1.0);
    return QPointF(x1 + t * dx, y1 + t * dy);
}

} // namespace cvat
