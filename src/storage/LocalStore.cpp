#include "cvatnative/storage/LocalStore.h"

#include "cvatnative/core/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <atomic>

namespace cvat {

namespace {
std::atomic<int> g_connectionCounter{0};

QString newConnectionName()
{
    return QStringLiteral("cvat-native-store-%1").arg(++g_connectionCounter);
}
} // namespace

LocalStore::LocalStore(QString connectionName)
    : connectionName_(connectionName.isEmpty() ? newConnectionName() : std::move(connectionName))
{
}

LocalStore::~LocalStore()
{
    close();
}

bool LocalStore::open(const QString& path, QString* error)
{
    close();
    error_.clear();

    if (path.isEmpty()) {
        error_ = QStringLiteral("The database path is empty");
        if (error) {
            *error = error_;
        }
        return false;
    }

    if (path != QStringLiteral(":memory:")) {
        const QString directory = QFileInfo(path).absolutePath();
        if (!QDir().mkpath(directory)) {
            error_ = QStringLiteral("Cannot create the data directory: %1").arg(directory);
            if (error) {
                *error = error_;
            }
            return false;
        }
    }

    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db_.setDatabaseName(path);
    if (!db_.open()) {
        error_ = db_.lastError().text();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName_);
        if (error) {
            *error = error_;
        }
        return false;
    }

    exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    exec(QStringLiteral("PRAGMA busy_timeout=5000"));
    if (!migrate()) {
        if (error) {
            *error = error_;
        }
        return false;
    }
    qCInfo(logStore) << "opened local database" << path;
    return true;
}

void LocalStore::close()
{
    if (!db_.isOpen() && connectionName_.isEmpty()) {
        return;
    }
    if (db_.isOpen()) {
        db_.close();
    }
    const QString name = connectionName_;
    db_ = QSqlDatabase();
    if (!name.isEmpty()) {
        QSqlDatabase::removeDatabase(name);
    }
}

bool LocalStore::isOpen() const
{
    return db_.isOpen();
}

bool LocalStore::exec(const QString& sql)
{
    QSqlQuery query(db_);
    if (!query.exec(sql)) {
        error_ = query.lastError().text();
        qCWarning(logStore) << "SQL failed:" << query.lastError().text() << "for" << sql;
        return false;
    }
    return true;
}

bool LocalStore::migrate()
{
    return exec(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS meta ("
               " key TEXT PRIMARY KEY,"
               " json TEXT NOT NULL,"
               " updated_at TEXT NOT NULL)"))
        && exec(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS snapshots ("
               " job_id INTEGER NOT NULL,"
               " frame INTEGER NOT NULL,"
               " json TEXT NOT NULL,"
               " updated_at TEXT NOT NULL,"
               " PRIMARY KEY (job_id, frame))"))
        && exec(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS pending_edits ("
               " id INTEGER PRIMARY KEY AUTOINCREMENT,"
               " job_id INTEGER NOT NULL,"
               " frame INTEGER NOT NULL DEFAULT -1,"
               " action TEXT NOT NULL,"
               " payload TEXT NOT NULL,"
               " created_at TEXT NOT NULL)"))
        && exec(QStringLiteral(
               "CREATE INDEX IF NOT EXISTS idx_pending_job ON pending_edits(job_id)"));
}

bool LocalStore::saveJson(const QString& key, const QJsonObject& value,
                          const QDateTime& updatedAt)
{
    if (!isOpen()) {
        error_ = QStringLiteral("Database is not open");
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO meta(key, json, updated_at) VALUES(?, ?, ?) "
        "ON CONFLICT(key) DO UPDATE SET json=excluded.json, updated_at=excluded.updated_at"));
    query.addBindValue(key);
    query.addBindValue(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
    query.addBindValue(updatedAt.toUTC().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        error_ = query.lastError().text();
        return false;
    }
    return true;
}

std::optional<QJsonObject> LocalStore::loadJson(const QString& key) const
{
    if (!isOpen()) {
        return std::nullopt;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT json FROM meta WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    const QJsonDocument document =
        QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    if (!document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

QDateTime LocalStore::jsonUpdatedAt(const QString& key) const
{
    if (!isOpen()) {
        return {};
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT updated_at FROM meta WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return QDateTime::fromString(query.value(0).toString(), Qt::ISODateWithMs);
}

bool LocalStore::removeJson(const QString& key)
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM meta WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        error_ = query.lastError().text();
        return false;
    }
    return true;
}

bool LocalStore::saveSnapshot(int jobId, int frame, const QJsonObject& data)
{
    if (!isOpen()) {
        error_ = QStringLiteral("Database is not open");
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO snapshots(job_id, frame, json, updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(job_id, frame) DO UPDATE SET json=excluded.json, "
        "updated_at=excluded.updated_at"));
    query.addBindValue(jobId);
    query.addBindValue(frame);
    query.addBindValue(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        error_ = query.lastError().text();
        return false;
    }
    return true;
}

std::optional<QJsonObject> LocalStore::loadSnapshot(int jobId, int frame) const
{
    if (!isOpen()) {
        return std::nullopt;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT json FROM snapshots WHERE job_id = ? AND frame = ?"));
    query.addBindValue(jobId);
    query.addBindValue(frame);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    const QJsonDocument document =
        QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    if (!document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

bool LocalStore::removeSnapshot(int jobId, int frame)
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM snapshots WHERE job_id = ? AND frame = ?"));
    query.addBindValue(jobId);
    query.addBindValue(frame);
    return query.exec();
}

QDateTime LocalStore::snapshotUpdatedAt(int jobId, int frame) const
{
    if (!isOpen()) {
        return {};
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT updated_at FROM snapshots WHERE job_id = ? AND frame = ?"));
    query.addBindValue(jobId);
    query.addBindValue(frame);
    if (!query.exec() || !query.next()) {
        return {};
    }
    return QDateTime::fromString(query.value(0).toString(), Qt::ISODateWithMs);
}

qint64 LocalStore::addPendingEdit(int jobId, int frame, const QString& action,
                                  const QJsonObject& payload)
{
    if (!isOpen()) {
        return -1;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO pending_edits(job_id, frame, action, payload, created_at) "
        "VALUES(?, ?, ?, ?, ?)"));
    query.addBindValue(jobId);
    query.addBindValue(frame);
    query.addBindValue(action);
    query.addBindValue(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        error_ = query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

QVector<LocalStore::PendingEdit> LocalStore::pendingEdits(int jobId) const
{
    QVector<PendingEdit> edits;
    if (!isOpen()) {
        return edits;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT id, job_id, frame, action, payload, created_at FROM pending_edits "
        "WHERE job_id = ? ORDER BY id"));
    query.addBindValue(jobId);
    if (!query.exec()) {
        return edits;
    }
    while (query.next()) {
        PendingEdit edit;
        edit.id = query.value(0).toLongLong();
        edit.jobId = query.value(1).toInt();
        edit.frame = query.value(2).toInt();
        edit.action = query.value(3).toString();
        edit.payload = QJsonDocument::fromJson(query.value(4).toString().toUtf8()).object();
        edit.createdAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODateWithMs);
        edits.push_back(edit);
    }
    return edits;
}

QVector<LocalStore::PendingEdit> LocalStore::allPendingEdits() const
{
    QVector<PendingEdit> edits;
    if (!isOpen()) {
        return edits;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT id, job_id, frame, action, payload, created_at FROM pending_edits ORDER BY id"));
    if (!query.exec()) {
        return edits;
    }
    while (query.next()) {
        PendingEdit edit;
        edit.id = query.value(0).toLongLong();
        edit.jobId = query.value(1).toInt();
        edit.frame = query.value(2).toInt();
        edit.action = query.value(3).toString();
        edit.payload = QJsonDocument::fromJson(query.value(4).toString().toUtf8()).object();
        edit.createdAt = QDateTime::fromString(query.value(5).toString(), Qt::ISODateWithMs);
        edits.push_back(edit);
    }
    return edits;
}

bool LocalStore::removePendingEdit(qint64 id)
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM pending_edits WHERE id = ?"));
    query.addBindValue(id);
    return query.exec();
}

bool LocalStore::clearPendingEdits(int jobId)
{
    if (!isOpen()) {
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM pending_edits WHERE job_id = ?"));
    query.addBindValue(jobId);
    return query.exec();
}

int LocalStore::pendingEditCount(int jobId) const
{
    if (!isOpen()) {
        return 0;
    }
    QSqlQuery query(db_);
    if (jobId >= 0) {
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM pending_edits WHERE job_id = ?"));
        query.addBindValue(jobId);
    } else {
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM pending_edits"));
    }
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool LocalStore::pruneSnapshots(int olderThanDays, int keepPerJob)
{
    if (!isOpen()) {
        return false;
    }
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-olderThanDays);
    QSqlQuery jobs(db_);
    if (!jobs.exec(QStringLiteral("SELECT DISTINCT job_id FROM snapshots"))) {
        return false;
    }
    QVector<int> jobIds;
    while (jobs.next()) {
        jobIds.push_back(jobs.value(0).toInt());
    }
    for (int jobId : jobIds) {
        QSqlQuery query(db_);
        query.prepare(QStringLiteral(
            "SELECT frame FROM snapshots WHERE job_id = ? ORDER BY updated_at DESC LIMIT -1 OFFSET ?"));
        query.addBindValue(jobId);
        query.addBindValue(keepPerJob);
        if (!query.exec()) {
            return false;
        }
        QVector<int> frames;
        while (query.next()) {
            frames.push_back(query.value(0).toInt());
        }
        for (int frame : frames) {
            QSqlQuery remove(db_);
            remove.prepare(QStringLiteral(
                "DELETE FROM snapshots WHERE job_id = ? AND frame = ? AND updated_at < ?"));
            remove.addBindValue(jobId);
            remove.addBindValue(frame);
            remove.addBindValue(cutoff.toString(Qt::ISODateWithMs));
            if (!remove.exec()) {
                return false;
            }
        }
    }
    return true;
}

bool LocalStore::prunePendingEdits(int olderThanDays)
{
    if (!isOpen()) {
        return false;
    }
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-olderThanDays);
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM pending_edits WHERE created_at < ?"));
    query.addBindValue(cutoff.toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        error_ = query.lastError().text();
        return false;
    }
    return true;
}

bool LocalStore::transaction()
{
    if (!isOpen()) {
        return false;
    }
    return db_.transaction();
}

} // namespace cvat
