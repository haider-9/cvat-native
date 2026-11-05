#include "cvatnative/model/Annotation.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
