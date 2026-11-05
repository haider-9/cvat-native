#pragma once

#include "cvatnative/model/Annotation.h"
#include "cvatnative/storage/LocalStore.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace cvat {

// Annotation-specific recovery on top of LocalStore. Snapshots are written
// before a remote save and after local edits; queued patches are replayed
// explicitly by the user, never silently at startup.
class RecoveryStore {
public:
    struct Record {
        qint64 id = 0;
        int jobId = 0;
        int frame = -1;
        QString action;
        LabeledData data;
        QDateTime createdAt;
    };

    explicit RecoveryStore(LocalStore* store);

    bool saveSnapshot(int jobId, int frame, const LabeledData& data);
    [[nodiscard]] std::optional<LabeledData> loadSnapshot(int jobId, int frame) const;
    [[nodiscard]] QDateTime snapshotTime(int jobId, int frame) const;

    qint64 queuePatch(int jobId, int frame, const QString& action, const LabeledData& data);
    [[nodiscard]] QVector<Record> pending(int jobId) const;
    bool removePatch(qint64 id);
    bool clearJob(int jobId);
    [[nodiscard]] int pendingCount(int jobId = -1) const;
    [[nodiscard]] bool hasPending(int jobId) const { return pendingCount(jobId) > 0; }

private:
    LocalStore* store_;
};

} // namespace cvat
