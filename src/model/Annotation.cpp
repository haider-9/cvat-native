#include "cvatnative/model/Annotation.h"

#include <QJsonArray>
#include <QSet>

#include <algorithm>

namespace cvat {

namespace {

QVector<double> pointsFromJson(const QJsonArray& array)
{
    QVector<double> points;
    points.reserve(array.size());
    for (const auto& value : array) {
        points.push_back(value.toDouble());
    }
    return points;
}

QJsonArray pointsToJson(const QVector<double>& points)
{
    QJsonArray array;
    for (double point : points) {
        array.push_back(point);
    }
    return array;
}

QVector<AttributeValue> attributesFromJson(const QJsonArray& array)
{
    QVector<AttributeValue> attributes;
    attributes.reserve(array.size());
    for (const auto& value : array) {
        attributes.push_back(AttributeValue::fromJson(value.toObject()));
    }
    return attributes;
}

QJsonArray attributesToJson(const QVector<AttributeValue>& attributes)
{
    QJsonArray array;
    for (const auto& attribute : attributes) {
        array.push_back(attribute.toJson());
    }
    return array;
}

} // namespace

AttributeValue AttributeValue::fromJson(const QJsonObject& json)
{
    AttributeValue value;
    value.specId = json.value(QStringLiteral("spec_id")).toInt();
    value.value = json.value(QStringLiteral("value")).toString();
    return value;
}

QJsonObject AttributeValue::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("spec_id"), specId);
    json.insert(QStringLiteral("value"), value);
    return json;
}

Shape Shape::fromJson(const QJsonObject& json)
{
    Shape shape;
    if (json.contains(QStringLiteral("id")) && !json.value(QStringLiteral("id")).isNull()) {
        shape.id = json.value(QStringLiteral("id")).toInt();
    }
    shape.labelId = json.value(QStringLiteral("label_id")).toInt();
    shape.type = shapeTypeOrDefault(json.value(QStringLiteral("type")).toString());
    shape.points = pointsFromJson(json.value(QStringLiteral("points")).toArray());
    shape.frame = json.value(QStringLiteral("frame")).toInt();
    shape.occluded = json.value(QStringLiteral("occluded")).toBool();
    shape.outside = json.value(QStringLiteral("outside")).toBool();
    shape.zOrder = json.value(QStringLiteral("z_order")).toInt();
    shape.rotation = json.value(QStringLiteral("rotation")).toDouble();
    shape.group = json.value(QStringLiteral("group")).toInt();
    shape.source = json.value(QStringLiteral("source")).toString(QStringLiteral("manual"));
    shape.score = json.value(QStringLiteral("score")).toDouble(1.0);
    shape.attributes = attributesFromJson(json.value(QStringLiteral("attributes")).toArray());
    for (const auto& element : json.value(QStringLiteral("elements")).toArray()) {
        shape.elements.push_back(Shape::fromJson(element.toObject()));
    }
    return shape;
}

QJsonObject Shape::toJson() const
{
    QJsonObject json;
    if (id.has_value()) {
        json.insert(QStringLiteral("id"), *id);
    } else {
        json.insert(QStringLiteral("id"), QJsonValue::Null);
    }
    json.insert(QStringLiteral("label_id"), labelId);
    json.insert(QStringLiteral("type"), type);
    json.insert(QStringLiteral("points"), pointsToJson(points));
    json.insert(QStringLiteral("frame"), frame);
    json.insert(QStringLiteral("occluded"), occluded);
    json.insert(QStringLiteral("outside"), outside);
    json.insert(QStringLiteral("z_order"), zOrder);
    json.insert(QStringLiteral("rotation"), rotation);
    json.insert(QStringLiteral("group"), group);
    json.insert(QStringLiteral("source"), source);
    json.insert(QStringLiteral("score"), score);
    json.insert(QStringLiteral("attributes"), attributesToJson(attributes));
    if (!elements.isEmpty()) {
        QJsonArray array;
        for (const auto& element : elements) {
            array.push_back(element.toJson());
        }
        json.insert(QStringLiteral("elements"), array);
    }
    return json;
}

TrackedShape TrackedShape::fromJson(const QJsonObject& json)
{
    TrackedShape shape;
    if (json.contains(QStringLiteral("id")) && !json.value(QStringLiteral("id")).isNull()) {
        shape.id = json.value(QStringLiteral("id")).toInt();
    }
    shape.type = shapeTypeOrDefault(json.value(QStringLiteral("type")).toString());
    shape.points = pointsFromJson(json.value(QStringLiteral("points")).toArray());
    shape.frame = json.value(QStringLiteral("frame")).toInt();
    shape.occluded = json.value(QStringLiteral("occluded")).toBool();
    shape.outside = json.value(QStringLiteral("outside")).toBool();
    shape.zOrder = json.value(QStringLiteral("z_order")).toInt();
    shape.rotation = json.value(QStringLiteral("rotation")).toDouble();
    shape.attributes = attributesFromJson(json.value(QStringLiteral("attributes")).toArray());
    return shape;
}

QJsonObject TrackedShape::toJson() const
{
    QJsonObject json;
    if (id.has_value()) {
        json.insert(QStringLiteral("id"), *id);
    }
    json.insert(QStringLiteral("type"), type);
    json.insert(QStringLiteral("points"), pointsToJson(points));
    json.insert(QStringLiteral("frame"), frame);
    json.insert(QStringLiteral("occluded"), occluded);
    json.insert(QStringLiteral("outside"), outside);
    json.insert(QStringLiteral("z_order"), zOrder);
    json.insert(QStringLiteral("rotation"), rotation);
    json.insert(QStringLiteral("attributes"), attributesToJson(attributes));
    return json;
}

Track Track::fromJson(const QJsonObject& json)
{
    Track track;
    if (json.contains(QStringLiteral("id")) && !json.value(QStringLiteral("id")).isNull()) {
        track.id = json.value(QStringLiteral("id")).toInt();
    }
    track.labelId = json.value(QStringLiteral("label_id")).toInt();
    track.frame = json.value(QStringLiteral("frame")).toInt();
    track.group = json.value(QStringLiteral("group")).toInt();
    track.source = json.value(QStringLiteral("source")).toString(QStringLiteral("manual"));
    track.attributes = attributesFromJson(json.value(QStringLiteral("attributes")).toArray());
    for (const auto& shape : json.value(QStringLiteral("shapes")).toArray()) {
        track.shapes.push_back(TrackedShape::fromJson(shape.toObject()));
    }
    for (const auto& element : json.value(QStringLiteral("elements")).toArray()) {
        track.elements.push_back(Track::fromJson(element.toObject()));
    }
    return track;
}

QJsonObject Track::toJson() const
{
    QJsonObject json;
    if (id.has_value()) {
        json.insert(QStringLiteral("id"), *id);
    } else {
        json.insert(QStringLiteral("id"), QJsonValue::Null);
    }
    json.insert(QStringLiteral("label_id"), labelId);
    json.insert(QStringLiteral("frame"), frame);
    json.insert(QStringLiteral("group"), group);
    json.insert(QStringLiteral("source"), source);
    json.insert(QStringLiteral("attributes"), attributesToJson(attributes));
    QJsonArray shapeArray;
    for (const auto& shape : shapes) {
        shapeArray.push_back(shape.toJson());
    }
    json.insert(QStringLiteral("shapes"), shapeArray);
    if (!elements.isEmpty()) {
        QJsonArray elementArray;
        for (const auto& element : elements) {
            elementArray.push_back(element.toJson());
        }
        json.insert(QStringLiteral("elements"), elementArray);
    }
    return json;
}

const TrackedShape* Track::shapeAt(int frame) const
{
    // CVAT tracks list keyframes in ascending frame order. For the usual case
    // a reverse scan is quick; fall back to a full scan for unsorted data.
    for (auto it = shapes.crbegin(); it != shapes.crend(); ++it) {
        if (it->frame == frame) {
            return &(*it);
        }
    }
    return nullptr;
}

bool Track::visibleAt(int frame) const
{
    const TrackedShape* shape = shapeAt(frame);
    return shape != nullptr && !shape->outside;
}

std::optional<TrackedShape> Track::interpolatedAt(int frame) const
{
    if (shapes.isEmpty()) {
        return std::nullopt;
    }

    const TrackedShape* previous = nullptr;
    const TrackedShape* next = nullptr;
    for (const auto& shape : shapes) {
        if (shape.frame <= frame) {
            if (!previous || shape.frame > previous->frame) {
                previous = &shape;
            }
        }
        if (shape.frame > frame) {
            if (!next || shape.frame < next->frame) {
                next = &shape;
            }
        }
    }

    if (!previous) {
        return std::nullopt;
    }
    if (previous->outside || previous->frame == frame || !next) {
        return *previous;
    }
    if (next->outside) {
        return *previous;
    }

    const int span = next->frame - previous->frame;
    const double t = span > 0 ? static_cast<double>(frame - previous->frame) / span : 0.0;

    TrackedShape result = *previous;
    result.id.reset();
    result.frame = frame;
    if (previous->points.size() == next->points.size()) {
        result.points.resize(previous->points.size());
        for (qsizetype i = 0; i < previous->points.size(); ++i) {
            result.points[i] = previous->points[i] + (next->points[i] - previous->points[i]) * t;
        }
    }
    result.rotation = previous->rotation + (next->rotation - previous->rotation) * t;
    result.occluded = previous->occluded;
    result.outside = false;
    // Attributes are discrete: keep the previous keyframe's values rather than
    // inventing intermediate ones.
    result.attributes = previous->attributes;
    return result;
}

Tag Tag::fromJson(const QJsonObject& json)
{
    Tag tag;
    if (json.contains(QStringLiteral("id")) && !json.value(QStringLiteral("id")).isNull()) {
        tag.id = json.value(QStringLiteral("id")).toInt();
    }
    tag.labelId = json.value(QStringLiteral("label_id")).toInt();
    tag.frame = json.value(QStringLiteral("frame")).toInt();
    tag.group = json.value(QStringLiteral("group")).toInt();
    tag.source = json.value(QStringLiteral("source")).toString(QStringLiteral("manual"));
    tag.attributes = attributesFromJson(json.value(QStringLiteral("attributes")).toArray());
    return tag;
}

QJsonObject Tag::toJson() const
{
    QJsonObject json;
    if (id.has_value()) {
        json.insert(QStringLiteral("id"), *id);
    } else {
        json.insert(QStringLiteral("id"), QJsonValue::Null);
    }
    json.insert(QStringLiteral("label_id"), labelId);
    json.insert(QStringLiteral("frame"), frame);
    json.insert(QStringLiteral("group"), group);
    json.insert(QStringLiteral("source"), source);
    json.insert(QStringLiteral("attributes"), attributesToJson(attributes));
    return json;
}

LabeledData LabeledData::fromJson(const QJsonObject& json)
{
    LabeledData data;
    data.version = json.value(QStringLiteral("version")).toInt();
    for (const auto& tag : json.value(QStringLiteral("tags")).toArray()) {
        data.tags.push_back(Tag::fromJson(tag.toObject()));
    }
    for (const auto& shape : json.value(QStringLiteral("shapes")).toArray()) {
        data.shapes.push_back(Shape::fromJson(shape.toObject()));
    }
    for (const auto& track : json.value(QStringLiteral("tracks")).toArray()) {
        data.tracks.push_back(Track::fromJson(track.toObject()));
    }
    for (const auto& interval : json.value(QStringLiteral("intervals")).toArray()) {
        data.intervals.push_back(interval.toObject());
    }
    data.raw = json;
    return data;
}

QJsonObject LabeledData::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("version"), version);
    QJsonArray tagArray;
    for (const auto& tag : tags) {
        tagArray.push_back(tag.toJson());
    }
    json.insert(QStringLiteral("tags"), tagArray);
    QJsonArray shapeArray;
    for (const auto& shape : shapes) {
        shapeArray.push_back(shape.toJson());
    }
    json.insert(QStringLiteral("shapes"), shapeArray);
    QJsonArray trackArray;
    for (const auto& track : tracks) {
        trackArray.push_back(track.toJson());
    }
    json.insert(QStringLiteral("tracks"), trackArray);
    if (!intervals.isEmpty()) {
        QJsonArray intervalArray;
        for (const auto& interval : intervals) {
            intervalArray.push_back(interval);
        }
        json.insert(QStringLiteral("intervals"), intervalArray);
    } else {
        json.insert(QStringLiteral("intervals"), QJsonArray());
    }
    return json;
}

int LabeledData::shapeCount() const
{
    int count = static_cast<int>(shapes.size() + tags.size());
    for (const auto& track : tracks) {
        count += static_cast<int>(std::max<qsizetype>(1, track.shapes.size()));
    }
    return count;
}

bool LabeledData::isEmpty() const
{
    return tags.isEmpty() && shapes.isEmpty() && tracks.isEmpty() && intervals.isEmpty();
}

QString shapeTypeOrDefault(const QString& type)
{
    static const QSet<QString> supported{
        QStringLiteral("rectangle"), QStringLiteral("polygon"), QStringLiteral("polyline"),
        QStringLiteral("points"),    QStringLiteral("ellipse"), QStringLiteral("cuboid"),
        QStringLiteral("mask"),      QStringLiteral("skeleton"),
    };
    return supported.contains(type) ? type : QStringLiteral("rectangle");
}

bool shapeTypeIsClosed(const QString& type)
{
    return type == QStringLiteral("rectangle") || type == QStringLiteral("polygon") ||
        type == QStringLiteral("ellipse") || type == QStringLiteral("mask") ||
        type == QStringLiteral("cuboid") || type == QStringLiteral("skeleton");
}

} // namespace cvat
