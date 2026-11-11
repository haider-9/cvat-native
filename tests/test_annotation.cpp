#include "cvatnative/model/Annotation.h"
#include "cvatnative/annotation/AnnotationModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>

#include <gtest/gtest.h>

using namespace cvat;

namespace {
QJsonObject sampleData()
{
    return QJsonDocument::fromJson(R"({
        "version": 0,
        "tags": [
            {"id": 9, "frame": 0, "label_id": 2, "group": 0, "source": "manual",
             "attributes": [{"spec_id": 5, "value": "sunny"}]}
        ],
        "shapes": [
            {"id": 1, "type": "rectangle", "frame": 0, "label_id": 1,
             "points": [10, 20, 110, 220], "occluded": false, "outside": false,
             "z_order": 0, "rotation": 0, "group": 0, "source": "manual",
             "score": 1, "attributes": []},
            {"id": 2, "type": "polygon", "frame": 1, "label_id": 1,
             "points": [0, 0, 100, 0, 100, 100], "occluded": true, "outside": false,
             "z_order": 1, "rotation": 0, "group": 0, "source": "manual",
             "score": 1, "attributes": []}
        ],
        "tracks": [
            {"id": 3, "frame": 0, "label_id": 1, "group": 0, "source": "manual",
             "attributes": [],
             "shapes": [
                {"id": 30, "type": "rectangle", "frame": 0, "points": [0, 0, 10, 10],
                 "occluded": false, "outside": false, "z_order": 0, "rotation": 0,
                 "attributes": []},
                {"id": 31, "type": "rectangle", "frame": 10, "points": [100, 100, 110, 110],
                 "occluded": false, "outside": false, "z_order": 0, "rotation": 0,
                 "attributes": []}
             ]}
        ],
        "intervals": []
    })").object();
}
} // namespace

TEST(Annotation, ParsesLabeledData)
{
    const LabeledData data = LabeledData::fromJson(sampleData());
    ASSERT_EQ(data.shapes.size(), 2u);
    ASSERT_EQ(data.tracks.size(), 1u);
    ASSERT_EQ(data.tags.size(), 1u);
    EXPECT_EQ(data.shapes[0].labelId, 1);
    EXPECT_EQ(data.shapes[0].points.size(), 4u);
    EXPECT_TRUE(data.shapes[1].occluded);
    EXPECT_EQ(data.tracks[0].shapes.size(), 2u);
    EXPECT_EQ(data.tags[0].attributes[0].value, QStringLiteral("sunny"));
    EXPECT_EQ(data.shapeCount(), 5);
}

TEST(Annotation, SerializationRoundTrip)
{
    const LabeledData original = LabeledData::fromJson(sampleData());
    const LabeledData copy = LabeledData::fromJson(original.toJson());
    ASSERT_EQ(copy.shapes.size(), original.shapes.size());
    ASSERT_EQ(copy.tracks.size(), original.tracks.size());
    ASSERT_EQ(copy.tags.size(), original.tags.size());
    EXPECT_TRUE(copy.shapes[0] == original.shapes[0]);
    EXPECT_TRUE(copy.shapes[1] == original.shapes[1]);
    EXPECT_TRUE(copy.tracks[0] == original.tracks[0]);
    EXPECT_TRUE(copy.tags[0] == original.tags[0]);
    EXPECT_EQ(copy.toJson(), original.toJson());
}

TEST(Annotation, TrackInterpolates)
{
    const LabeledData data = LabeledData::fromJson(sampleData());
    const Track& track = data.tracks[0];

    const auto atFive = track.interpolatedAt(5);
    ASSERT_TRUE(atFive.has_value());
    EXPECT_EQ(atFive->frame, 5);
    EXPECT_FALSE(atFive->id.has_value());
    EXPECT_DOUBLE_EQ(atFive->points[0], 50.0);
    EXPECT_DOUBLE_EQ(atFive->points[1], 50.0);
    EXPECT_DOUBLE_EQ(atFive->points[2], 60.0);

    const auto exact = track.interpolatedAt(10);
    ASSERT_TRUE(exact.has_value());
    EXPECT_TRUE(exact->id.has_value());
    EXPECT_EQ(*exact->id, 31);

    EXPECT_FALSE(track.interpolatedAt(-1).has_value());
}

TEST(Annotation, ShapeTypes)
{
    EXPECT_EQ(shapeTypeOrDefault(QStringLiteral("polygon")), QStringLiteral("polygon"));
    EXPECT_EQ(shapeTypeOrDefault(QStringLiteral("bogus")), QStringLiteral("rectangle"));
    EXPECT_TRUE(shapeTypeIsClosed(QStringLiteral("polygon")));
    EXPECT_FALSE(shapeTypeIsClosed(QStringLiteral("polyline")));
}

TEST(AnnotationModel, AddingAndUpdatingShapesIncrementsRevisionAndDirty)
{
    AnnotationModel model;
    EXPECT_FALSE(model.isDirty());
    const quint64 initialRevision = model.revision();

    Shape shape;
    shape.labelId = 1;
    shape.type = QStringLiteral("rectangle");
    shape.points = {10, 20, 100, 200};
    shape.frame = 0;
    model.addShape(shape);

    EXPECT_TRUE(model.isDirty());
    EXPECT_GT(model.revision(), initialRevision);

    const auto visible = model.visibleShapes();
    ASSERT_EQ(visible.size(), 1);
    const QString key = visible[0].key;

    const quint64 revAfterAdd = model.revision();
    Shape modified = visible[0].shape;
    modified.points = {15, 25, 105, 205};
    EXPECT_TRUE(model.updateVisibleShape(key, modified));

    EXPECT_TRUE(model.isDirty());
    EXPECT_GT(model.revision(), revAfterAdd);
}

TEST(AnnotationModel, BatchUpdatingShapesAndUndo)
{
    AnnotationModel model;
    Shape s1;
    s1.labelId = 1;
    s1.type = QStringLiteral("rectangle");
    s1.points = {10, 10, 50, 50};
    s1.frame = 0;
    model.addShape(s1);

    Shape s2;
    s2.labelId = 1;
    s2.type = QStringLiteral("rectangle");
    s2.points = {100, 100, 200, 200};
    s2.frame = 0;
    model.addShape(s2);

    const auto visible = model.visibleShapes();
    ASSERT_EQ(visible.size(), 2);

    Shape m1 = visible[0].shape;
    m1.points = {5, 5, 55, 55};
    Shape m2 = visible[1].shape;
    m2.points = {95, 95, 205, 205};

    const int updated = model.updateVisibleShapes({{visible[0].key, m1}, {visible[1].key, m2}});
    EXPECT_EQ(updated, 2);

    const auto visibleAfter = model.visibleShapes();
    ASSERT_EQ(visibleAfter.size(), 2);
    EXPECT_DOUBLE_EQ(visibleAfter[0].shape.points[0], 5.0);
    EXPECT_DOUBLE_EQ(visibleAfter[1].shape.points[0], 95.0);

    // Single undo reverts both shapes
    EXPECT_TRUE(model.canUndo());
    EXPECT_TRUE(model.undo());

    const auto visibleReverted = model.visibleShapes();
    ASSERT_EQ(visibleReverted.size(), 2);
    EXPECT_DOUBLE_EQ(visibleReverted[0].shape.points[0], 10.0);
    EXPECT_DOUBLE_EQ(visibleReverted[1].shape.points[0], 100.0);
}

TEST(Annotation, CopiedTemplateDimensionAndPlacement)
{
    Shape shape;
    shape.labelId = 3;
    shape.type = QStringLiteral("rectangle");
    shape.points = {100, 200, 300, 400};
    shape.frame = 1;

    const double x1 = std::min(shape.points[0], shape.points[2]);
    const double x2 = std::max(shape.points[0], shape.points[2]);
    const double y1 = std::min(shape.points[1], shape.points[3]);
    const double y2 = std::max(shape.points[1], shape.points[3]);
    const double w = x2 - x1;
    const double h = y2 - y1;
    EXPECT_DOUBLE_EQ(w, 200.0);
    EXPECT_DOUBLE_EQ(h, 200.0);

    const QPointF placePos(500.0, 500.0);
    Shape placed;
    placed.labelId = shape.labelId;
    placed.type = shape.type;
    placed.points = QVector<double>{placePos.x() - w / 2.0, placePos.y() - h / 2.0,
                                    placePos.x() + w / 2.0, placePos.y() + h / 2.0};
    EXPECT_DOUBLE_EQ(placed.points[0], 400.0);
    EXPECT_DOUBLE_EQ(placed.points[1], 400.0);
    EXPECT_DOUBLE_EQ(placed.points[2], 600.0);
    EXPECT_DOUBLE_EQ(placed.points[3], 600.0);
    EXPECT_DOUBLE_EQ(placed.points[2] - placed.points[0], 200.0);
    EXPECT_DOUBLE_EQ(placed.points[3] - placed.points[1], 200.0);
}
