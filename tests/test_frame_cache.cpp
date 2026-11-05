#include "cvatnative/media/FrameCache.h"

#include <QImage>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace cvat;

namespace {
QImage image(int width, int height)
{
    QImage result(width, height, QImage::Format_RGB32);
    result.fill(Qt::red);
    return result;
}
} // namespace

TEST(FrameCache, MemoryLruEviction)
{
    FrameCache cache;
    cache.setMemoryLimit(2 * 64 * 64 * 4);

    FrameCache::Key first{1, 0, QStringLiteral("compressed")};
    FrameCache::Key second{1, 1, QStringLiteral("compressed")};
    FrameCache::Key third{1, 2, QStringLiteral("compressed")};

    cache.insert(first, image(64, 64));
    cache.insert(second, image(64, 64));
    EXPECT_EQ(cache.count(), 2);
    EXPECT_TRUE(cache.contains(first));

    // Touch the first entry so the second becomes the least recently used.
    ASSERT_TRUE(cache.get(first).has_value());
    cache.insert(third, image(64, 64));

    EXPECT_EQ(cache.count(), 2);
    EXPECT_TRUE(cache.contains(first));
    EXPECT_TRUE(cache.contains(third));
    EXPECT_FALSE(cache.contains(second));
}

TEST(FrameCache, DiskRoundTripAndJobRemoval)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    FrameCache cache(directory.path());
    cache.setDiskLimit(1024 * 1024);

    FrameCache::Key key{42, 7, QStringLiteral("original")};
    const QByteArray payload("not-a-real-jpeg");
    ASSERT_TRUE(cache.writeDisk(key, payload));

    const auto read = cache.readDisk(key);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, payload);

    cache.removeJobDisk(42);
    EXPECT_FALSE(cache.readDisk(key).has_value());
}

TEST(FrameCache, SeparatesQualityNamespaces)
{
    FrameCache cache;
    cache.setMemoryLimit(1024 * 1024);
    FrameCache::Key compressed{1, 2, QStringLiteral("compressed")};
    FrameCache::Key original{1, 2, QStringLiteral("original")};
    cache.insert(compressed, image(8, 8));
    EXPECT_TRUE(cache.contains(compressed));
    EXPECT_FALSE(cache.contains(original));
}
