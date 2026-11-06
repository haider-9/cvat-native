#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QString>

#include <optional>

namespace cvat {

// Bounded two-level frame cache:
//  - decoded QImages in memory with LRU eviction by byte budget
//  - raw compressed bytes on disk with age/size based pruning
// Keys include the job, frame and quality so data from different jobs can
// never collide and an "original" download does not get replaced by a
// thumbnail.
class FrameCache {
public:
    struct Key {
        int jobId = 0;
        int frame = 0;
        QString quality = QStringLiteral("compressed");

        [[nodiscard]] QString id() const;
        bool operator==(const Key&) const = default;
    };

    explicit FrameCache(const QString& diskRoot = {});

    void setMemoryLimit(qint64 bytes);
    [[nodiscard]] qint64 memoryLimit() const;
    void setDiskLimit(qint64 bytes);
    [[nodiscard]] qint64 diskLimit() const;

    [[nodiscard]] std::optional<QImage> get(const Key& key) const;
    [[nodiscard]] bool contains(const Key& key) const;
    void insert(const Key& key, const QImage& image);
    void remove(const Key& key);
    void removeJob(int jobId);
    void clear();

    [[nodiscard]] qint64 memoryUsage() const;
    [[nodiscard]] int count() const;

    [[nodiscard]] bool writeDisk(const Key& key, const QByteArray& bytes);
    [[nodiscard]] std::optional<QByteArray> readDisk(const Key& key) const;
    void removeDisk(const Key& key);
    void removeJobDisk(int jobId);
    void pruneDisk();

    [[nodiscard]] QString diskRoot() const { return diskRoot_; }

private:
    struct Entry {
        QImage image;
        qint64 bytes = 0;
    };

    struct DiskEntry {
        qint64 bytes = 0;
        quint64 lastAccess = 0;
    };

    [[nodiscard]] QString diskPath(const Key& key) const;
    void touch(const QString& key);
    void evictLocked();

    mutable QMutex mutex_;
    QHash<QString, Entry> images_;
    QList<QString> order_; // most recently used first
    mutable QHash<QString, DiskEntry> diskEntries_;
    mutable quint64 diskAccessCounter_ = 0;
    qint64 memoryUsage_ = 0;
    mutable qint64 diskBytes_ = 0;
    qint64 memoryLimit_ = 256LL * 1024 * 1024;
    qint64 diskLimit_ = 1024LL * 1024 * 1024;
    QString diskRoot_;
};

} // namespace cvat
