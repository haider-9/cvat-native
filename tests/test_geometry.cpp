#include "cvatnative/annotation/Geometry.h"
#include "cvatnative/model/Annotation.h"

#include <gtest/gtest.h>

using namespace cvat;

TEST(Geometry, BoundingRect)
{
    const auto rect = Geometry::boundingRect({10, 20, 110, 220, 40, -5});
    EXPECT_DOUBLE_EQ(rect.x, 10);
    EXPECT_DOUBLE_EQ(rect.y, -5);
    EXPECT_DOUBLE_EQ(rect.width, 100);
    EXPECT_DOUBLE_EQ(rect.height, 225);
}

TEST(Geometry, RectangleUsesOppositeCorners)
{
    const auto rect = Geometry::rectangle({110, 220, 10, 20});
    EXPECT_DOUBLE_EQ(rect.x, 10);
    EXPECT_DOUBLE_EQ(rect.y, 20);
    EXPECT_DOUBLE_EQ(rect.width, 100);
    EXPECT_DOUBLE_EQ(rect.height, 200);
}

TEST(Geometry, PointInPolygon)
{
    const QVector<double> triangle{0, 0, 100, 0, 50, 100};
    EXPECT_TRUE(Geometry::pointInPolygon(triangle, 50, 40));
    EXPECT_FALSE(Geometry::pointInPolygon(triangle, 5, 90));
    EXPECT_FALSE(Geometry::pointInPolygon(triangle, 200, 200));
}

TEST(Geometry, DistanceToSegment)
{
    EXPECT_DOUBLE_EQ(Geometry::distanceToSegment(5, 3, 0, 0, 10, 0), 3);
    EXPECT_DOUBLE_EQ(Geometry::distanceToSegment(-3, 4, 0, 0, 10, 0), 5);
}

TEST(Geometry, HitTestRectangleAndPolygon)
{
    Shape rectangle;
    rectangle.type = QStringLiteral("rectangle");
    rectangle.points = {0, 0, 100, 50};
    EXPECT_TRUE(Geometry::hitTest(rectangle, 50, 25, 2));
    EXPECT_TRUE(Geometry::hitTest(rectangle, 101, 25, 2));
    EXPECT_FALSE(Geometry::hitTest(rectangle, 110, 25, 2));

    Shape polygon;
    polygon.type = QStringLiteral("polygon");
    polygon.points = {0, 0, 100, 0, 100, 100, 0, 100};
    EXPECT_TRUE(Geometry::hitTest(polygon, 50, 50, 1));
    EXPECT_FALSE(Geometry::hitTest(polygon, 150, 50, 1));
}

TEST(Geometry, TranslateAndScale)
{
    QVector<double> points{0, 0, 10, 10};
    Geometry::translate(points, 5, -5);
    EXPECT_EQ(points, (QVector<double>{5, -5, 15, 5}));
    Geometry::scale(points, RectD{5, -5, 10, 10}, RectD{0, 0, 20, 40});
    EXPECT_EQ(points, (QVector<double>{0, 0, 20, 40}));
}

TEST(Geometry, RotatedRectangle)
{
    const auto corners = Geometry::rotatedRectangleCorners(RectD{0, 0, 10, 10}, 90);
    ASSERT_EQ(corners.size(), 8);
    EXPECT_NEAR(corners[0], 10, 1e-9);
    EXPECT_NEAR(corners[1], 0, 1e-9);
    EXPECT_NEAR(corners[2], 10, 1e-9);
    EXPECT_NEAR(corners[3], 10, 1e-9);
}
