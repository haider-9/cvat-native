#include "cvatnative/media/FrameCache.h"

#include "cvatnative/core/Logging.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>

#include <algorithm>

namespace cvat {

QString FrameCache::Key::id() const
{
    return QStringLiteral("job-%1/frame-%2/%3")
        .arg(jobId)
        .arg(frame)
        .arg(quality);
}

FrameCache::FrameCache(const QString& diskRoot) : diskRoot_(diskRoot)
{
    if (!diskRoot_.isEmpty()) {
        QDir().mkpath(diskRoot_);
        QVector<QPair<QString, QDateTime>> files;
        QDirIterator it(diskRoot_, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            files.push_back({path, QFileInfo(path).lastModified()});
        }
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
        for (const auto& file : files) {
            const qint64 bytes = QFileInfo(file.first).size();
            diskEntries_.insert(file.first, DiskEntry{bytes, ++diskAccessCounter_});
            diskBytes_ += bytes;
        }
    }
}

void FrameCache::setMemoryLimit(qint64 bytes)
{
    QMutexLocker locker(&mutex_);
    memoryLimit_ = std::max<qint64>(0, bytes);
    evictLocked();
}

qint64 FrameCache::memoryLimit() const
{
    QMutexLocker locker(&mutex_);
    return memoryLimit_;
}

void FrameCache::setDiskLimit(qint64 bytes)
{
    QMutexLocker locker(&mutex_);
    diskLimit_ = std::max<qint64>(0, bytes);
}

qint64 FrameCache::diskLimit() const
{
    QMutexLocker locker(&mutex_);
    return diskLimit_;
}

void FrameCache::touch(const QString& key)
{
    order_.removeAll(key);
    order_.prepend(key);
}

void FrameCache::evictLocked()
{
    while (memoryUsage_ > memoryLimit_ && !order_.isEmpty()) {
        const QString oldest = order_.takeLast();
        auto it = images_.find(oldest);
        if (it != images_.end()) {
            memoryUsage_ -= it->bytes;
            images_.erase(it);
        }
    }
}

std::optional<QImage> FrameCache::get(const Key& key) const
{
    QMutexLocker locker(&mutex_);
    const QString id = key.id();
    auto it = images_.constFind(id);
    if (it == images_.constEnd()) {
        return std::nullopt;
    }
    auto* self = const_cast<FrameCache*>(this);
    self->touch(id);
    return it->image;
}

bool FrameCache::contains(const Key& key) const
{
    QMutexLocker locker(&mutex_);
    return images_.contains(key.id());
}

void FrameCache::insert(const Key& key, const QImage& image)
{
    if (image.isNull()) {
        return;
    }
    QMutexLocker locker(&mutex_);
    const QString id = key.id();
    const qint64 bytes = static_cast<qint64>(image.sizeInBytes());
    if (bytes > memoryLimit_) {
        return;
    }
    auto it = images_.find(id);
    if (it != images_.end()) {
        memoryUsage_ -= it->bytes;
        it->image = image;
        it->bytes = bytes;
    } else {
        images_.insert(id, Entry{image, bytes});
    }
    memoryUsage_ += bytes;
    touch(id);
    evictLocked();
}

void FrameCache::remove(const Key& key)
{
    QMutexLocker locker(&mutex_);
    const QString id = key.id();
    order_.removeAll(id);
    auto it = images_.find(id);
    if (it != images_.end()) {
        memoryUsage_ -= it->bytes;
        images_.erase(it);
    }
}

void FrameCache::removeJob(int jobId)
{
    {
        QMutexLocker locker(&mutex_);
        const QString prefix = QStringLiteral("job-%1/").arg(jobId);
        for (auto it = images_.begin(); it != images_.end();) {
            if (it.key().startsWith(prefix)) {
                memoryUsage_ -= it->bytes;
                order_.removeAll(it.key());
                it = images_.erase(it);
            } else {
                ++it;
            }
        }
    }
    removeJobDisk(jobId);
}

void FrameCache::clear()
{
    QMutexLocker locker(&mutex_);
    images_.clear();
    order_.clear();
    memoryUsage_ = 0;
}

qint64 FrameCache::memoryUsage() const
{
    QMutexLocker locker(&mutex_);
    return memoryUsage_;
}

int FrameCache::count() const
{
    QMutexLocker locker(&mutex_);
    return images_.size();
}

QString FrameCache::diskPath(const Key& key) const
{
    if (diskRoot_.isEmpty()) {
        return {};
    }
    return QStringLiteral("%1/job-%2/%3/%4.bin")
        .arg(diskRoot_)
        .arg(key.jobId)
        .arg(key.quality)
        .arg(key.frame);
}

bool FrameCache::writeDisk(const Key& key, const QByteArray& bytes)
{
    const QString path = diskPath(key);
    if (path.isEmpty() || bytes.isEmpty()) {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        return false;
    }
    bool needsPrune = false;
    {
        QMutexLocker locker(&mutex_);
        const auto it = diskEntries_.constFind(path);
        if (it != diskEntries_.constEnd()) {
            diskBytes_ -= it->bytes;
        }
        diskEntries_.insert(path, DiskEntry{bytes.size(), ++diskAccessCounter_});
        diskBytes_ += bytes.size();
        needsPrune = diskBytes_ > diskLimit_;
    }
    if (needsPrune) {
        pruneDisk();
    }
    return true;
}

std::optional<QByteArray> FrameCache::readDisk(const Key& key) const
{
    const QString path = diskPath(key);
    if (path.isEmpty()) {
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    {
        QMutexLocker locker(&mutex_);
        auto it = diskEntries_.find(path);
        if (it != diskEntries_.end()) {
            it->lastAccess = ++diskAccessCounter_;
        }
    }
    return bytes;
}

void FrameCache::removeDisk(const Key& key)
{
    const QString path = diskPath(key);
    if (!path.isEmpty()) {
        const bool removed = QFile::remove(path);
        QMutexLocker locker(&mutex_);
        auto it = diskEntries_.find(path);
        if (it != diskEntries_.end() && (removed || !QFileInfo::exists(path))) {
            diskBytes_ -= it->bytes;
            diskEntries_.erase(it);
        }
    }
}

void FrameCache::removeJobDisk(int jobId)
{
    if (diskRoot_.isEmpty()) {
        return;
    }
    QDir directory(QStringLiteral("%1/job-%2").arg(diskRoot_).arg(jobId));
    if (directory.exists()) {
        directory.removeRecursively();
    }
    const QString prefix = QStringLiteral("%1/job-%2/").arg(diskRoot_).arg(jobId);
    QMutexLocker locker(&mutex_);
    for (auto it = diskEntries_.begin(); it != diskEntries_.end();) {
        if (it.key().startsWith(prefix)) {
            diskBytes_ -= it->bytes;
            it = diskEntries_.erase(it);
        } else {
            ++it;
        }
    }
}

void FrameCache::pruneDisk()
{
    if (diskRoot_.isEmpty()) {
        return;
    }
    QMutexLocker locker(&mutex_);
    if (diskBytes_ <= diskLimit_) {
        return;
    }
    QVector<QPair<QString, DiskEntry>> files;
    files.reserve(diskEntries_.size());
    for (auto it = diskEntries_.cbegin(); it != diskEntries_.cend(); ++it) {
        files.push_back({it.key(), it.value()});
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.second.lastAccess < b.second.lastAccess;
    });
    for (const auto& entry : files) {
        if (diskBytes_ <= diskLimit_) {
            break;
        }
        if (QFile::remove(entry.first) || !QFileInfo::exists(entry.first)) {
            diskBytes_ -= entry.second.bytes;
            diskEntries_.remove(entry.first);
        }
    }
    qCInfo(logMedia) << "pruned frame cache to" << diskBytes_ << "bytes";
}

} // namespace cvat
