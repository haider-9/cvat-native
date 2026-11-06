#include "cvatnative/storage/LocalStore.h"
#include "cvatnative/storage/RecoveryStore.h"

#include <QJsonObject>
#include <QSqlDatabase>

#include <gtest/gtest.h>

using namespace cvat;

namespace {
QJsonObject object()
{
    return QJsonObject{{QStringLiteral("name"), QStringLiteral("demo")},
                       {QStringLiteral("count"), 2}};
}
} // namespace

TEST(LocalStore, InMemoryRoundTrip)
{
    LocalStore store;
    QString error;
    ASSERT_TRUE(store.open(QStringLiteral(":memory:"), &error)) << error.toStdString();
    ASSERT_TRUE(store.isOpen());

    ASSERT_TRUE(store.saveJson(QStringLiteral("projects/1"), object()));
    const auto loaded = store.loadJson(QStringLiteral("projects/1"));
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->value(QStringLiteral("name")).toString(), QStringLiteral("demo"));

    EXPECT_FALSE(store.loadJson(QStringLiteral("missing")).has_value());
    EXPECT_TRUE(store.removeJson(QStringLiteral("projects/1")));
    EXPECT_FALSE(store.loadJson(QStringLiteral("projects/1")).has_value());
}

TEST(LocalStore, SnapshotAndPendingEdits)
{
    LocalStore store;
    ASSERT_TRUE(store.open(QStringLiteral(":memory:")));

    ASSERT_TRUE(store.saveSnapshot(7, 3, object()));
    const auto snapshot = store.loadSnapshot(7, 3);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->value(QStringLiteral("count")).toInt(), 2);
    EXPECT_TRUE(store.snapshotUpdatedAt(7, 3).isValid());

    const qint64 first = store.addPendingEdit(7, 3, QStringLiteral("update"), object());
    const qint64 second = store.addPendingEdit(7, 4, QStringLiteral("create"), object());
    ASSERT_GE(first, 1);
    ASSERT_GE(second, 1);
    EXPECT_EQ(store.pendingEditCount(7), 2);

    const auto edits = store.pendingEdits(7);
    ASSERT_EQ(edits.size(), 2);
    EXPECT_EQ(edits[0].action, QStringLiteral("update"));
    EXPECT_EQ(edits[1].frame, 4);

    EXPECT_TRUE(store.removePendingEdit(first));
    EXPECT_EQ(store.pendingEditCount(7), 1);
    EXPECT_TRUE(store.clearPendingEdits(7));
    EXPECT_EQ(store.pendingEditCount(), 0);
}

TEST(LocalStore, PrunesOldPendingEdits)
{
    LocalStore store(QStringLiteral("test-prune-pending-edits"));
    ASSERT_TRUE(store.open(QStringLiteral(":memory:")));
    ASSERT_GT(store.addPendingEdit(9, 4, QStringLiteral("update"),
                                   QJsonObject{{QStringLiteral("value"), 1}}), 0);

    ASSERT_TRUE(store.prunePendingEdits(-1));
    EXPECT_EQ(store.pendingEditCount(9), 0);
}

TEST(RecoveryStore, WrapsSnapshots)
{
    LocalStore local;
    ASSERT_TRUE(local.open(QStringLiteral(":memory:")));
    RecoveryStore recovery(&local);

    LabeledData data;
    Shape shape;
    shape.id = 1;
    shape.labelId = 2;
    shape.points = {0, 0, 10, 10};
    data.shapes.push_back(shape);

    ASSERT_TRUE(recovery.saveSnapshot(5, 2, data));
    const auto loaded = recovery.loadSnapshot(5, 2);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->shapes.size(), 1u);
    EXPECT_EQ(loaded->shapes[0].labelId, 2);

    EXPECT_GE(recovery.queuePatch(5, 2, QStringLiteral("update"), data), 1);
    EXPECT_EQ(recovery.pendingCount(5), 1);
    const auto pending = recovery.pending(5);
    ASSERT_EQ(pending.size(), 1);
    EXPECT_EQ(pending[0].data.shapes.size(), 1u);
    EXPECT_EQ(pending[0].action, QStringLiteral("update"));
}
