#include "cvatnative/storage/RecoveryStore.h"

#include <QJsonDocument>

namespace cvat {

RecoveryStore::RecoveryStore(LocalStore* store) : store_(store) {}

bool RecoveryStore::saveSnapshot(int jobId, int frame, const LabeledData& data)
{
    return store_ && store_->saveSnapshot(jobId, frame, data.toJson());
}

std::optional<LabeledData> RecoveryStore::loadSnapshot(int jobId, int frame) const
{
    if (!store_) {
        return std::nullopt;
    }
    const auto json = store_->loadSnapshot(jobId, frame);
    if (!json) {
        return std::nullopt;
    }
    return LabeledData::fromJson(*json);
}

QDateTime RecoveryStore::snapshotTime(int jobId, int frame) const
{
    return store_ ? store_->snapshotUpdatedAt(jobId, frame) : QDateTime();
}

qint64 RecoveryStore::queuePatch(int jobId, int frame, const QString& action,
                                 const LabeledData& data)
{
    if (!store_) {
        return -1;
    }
    return store_->addPendingEdit(jobId, frame, action, data.toJson());
}

QVector<RecoveryStore::Record> RecoveryStore::pending(int jobId) const
{
    QVector<Record> records;
    if (!store_) {
        return records;
    }
    for (const auto& edit : store_->pendingEdits(jobId)) {
        Record record;
        record.id = edit.id;
        record.jobId = edit.jobId;
        record.frame = edit.frame;
        record.action = edit.action;
        record.data = LabeledData::fromJson(edit.payload);
        record.createdAt = edit.createdAt;
        records.push_back(record);
    }
    return records;
}

bool RecoveryStore::removePatch(qint64 id)
{
    return store_ && store_->removePendingEdit(id);
}

bool RecoveryStore::clearJob(int jobId)
{
    return store_ && store_->clearPendingEdits(jobId);
}

int RecoveryStore::pendingCount(int jobId) const
{
    return store_ ? store_->pendingEditCount(jobId) : 0;
}

} // namespace cvat
