#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

namespace cvat {

// Small SQLite cache. It caches list/detail JSON so the dashboard can render
// offline, stores per-frame annotation snapshots for crash recovery, and holds
// queued annotation patches that have not reached the server yet.
//
// Access is expected from the GUI thread. All operations are synchronous and
// deliberately short; large media payloads belong in FrameCache instead.
class LocalStore {
public:
    struct PendingEdit {
        qint64 id = 0;
        int jobId = 0;
        int frame = -1;
        QString action;
        QJsonObject payload;
        QDateTime createdAt;
    };

    explicit LocalStore(QString connectionName = {});
    ~LocalStore();

    LocalStore(const LocalStore&) = delete;
    LocalStore& operator=(const LocalStore&) = delete;

    bool open(const QString& path, QString* error = nullptr);
    void close();
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] QString databasePath() const { return db_.databaseName(); }
    [[nodiscard]] QString lastError() const { return error_; }

    bool saveJson(const QString& key, const QJsonObject& value,
                  const QDateTime& updatedAt = QDateTime::currentDateTimeUtc());
    [[nodiscard]] std::optional<QJsonObject> loadJson(const QString& key) const;
    [[nodiscard]] QDateTime jsonUpdatedAt(const QString& key) const;
    bool removeJson(const QString& key);

    bool saveSnapshot(int jobId, int frame, const QJsonObject& data);
    [[nodiscard]] std::optional<QJsonObject> loadSnapshot(int jobId, int frame) const;
    bool removeSnapshot(int jobId, int frame);
    QDateTime snapshotUpdatedAt(int jobId, int frame) const;

    qint64 addPendingEdit(int jobId, int frame, const QString& action,
                          const QJsonObject& payload);
    [[nodiscard]] QVector<PendingEdit> pendingEdits(int jobId) const;
    [[nodiscard]] QVector<PendingEdit> allPendingEdits() const;
    bool removePendingEdit(qint64 id);
    bool clearPendingEdits(int jobId);
    [[nodiscard]] int pendingEditCount(int jobId = -1) const;

    bool pruneSnapshots(int olderThanDays, int keepPerJob);
    bool prunePendingEdits(int olderThanDays);
    bool transaction();

private:
    bool exec(const QString& sql);
    bool migrate();

    QSqlDatabase db_;
    QString connectionName_;
    QString error_;
};

} // namespace cvat
