#pragma once

#include <QJsonObject>
#include <QVector>

#include <optional>

namespace cvat {

struct AttributeValue {
    int specId = 0;
    QString value;

    static AttributeValue fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const AttributeValue&) const = default;
};

struct Shape {
    std::optional<int> id;
    int labelId = 0;
    QString type = QStringLiteral("rectangle");
    QVector<double> points;
    int frame = 0;
    bool occluded = false;
    bool outside = false;
    int zOrder = 0;
    double rotation = 0.0;
    int group = 0;
    QString source = QStringLiteral("manual");
    double score = 1.0;
    QVector<AttributeValue> attributes;
    QVector<Shape> elements;

    static Shape fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const Shape&) const = default;
};

struct TrackedShape {
    std::optional<int> id;
    QString type = QStringLiteral("rectangle");
    QVector<double> points;
    int frame = 0;
    bool occluded = false;
    bool outside = false;
    int zOrder = 0;
    double rotation = 0.0;
    QVector<AttributeValue> attributes;

    static TrackedShape fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const TrackedShape&) const = default;
};

struct Track {
    std::optional<int> id;
    int labelId = 0;
    int frame = 0;
    int group = 0;
    QString source = QStringLiteral("manual");
    QVector<AttributeValue> attributes;
    QVector<TrackedShape> shapes;
    QVector<Track> elements;

    static Track fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const Track&) const = default;

    [[nodiscard]] const TrackedShape* shapeAt(int frame) const;
    [[nodiscard]] bool visibleAt(int frame) const;
    // Returns the keyframe at `frame`, or a linear interpolation between the
    // surrounding keyframes. Returns nullopt outside the track's lifetime.
    [[nodiscard]] std::optional<TrackedShape> interpolatedAt(int frame) const;
};

struct Tag {
    std::optional<int> id;
    int labelId = 0;
    int frame = 0;
    int group = 0;
    QString source = QStringLiteral("manual");
    QVector<AttributeValue> attributes;

    static Tag fromJson(const QJsonObject& json);
    QJsonObject toJson() const;
    bool operator==(const Tag&) const = default;
};

struct LabeledData {
    int version = 0;
    QVector<Tag> tags;
    QVector<Shape> shapes;
    QVector<Track> tracks;
    QVector<QJsonObject> intervals;
    QJsonObject raw;

    static LabeledData fromJson(const QJsonObject& json);
    QJsonObject toJson() const;

    [[nodiscard]] int shapeCount() const;
    [[nodiscard]] bool isEmpty() const;
};

QString shapeTypeOrDefault(const QString& type);
bool shapeTypeIsClosed(const QString& type);

} // namespace cvat
