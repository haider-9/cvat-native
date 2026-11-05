#include "cvatnative/media/MediaService.h"

#include "cvatnative/core/Logging.h"

#include <QFutureWatcher>
#include <QImageReader>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace cvat {

MediaService::MediaService(ApiClient* client, FrameCache* cache, QObject* parent)
    : QObject(parent), client_(client), cache_(cache), api_(client)
{
    // Technical drawings can exceed Qt's default 256 MiB decoded-image limit.
    QImageReader::setAllocationLimit(1024);
}

FrameCache::Key MediaService::makeKey(int jobId, int frame, const QString& quality)
{
    FrameCache::Key key;
    key.jobId = jobId;
    key.frame = frame;
    key.quality = quality.isEmpty() ? QStringLiteral("compressed") : quality;
    return key;
}

void MediaService::rememberWaiter(const QString& id, FrameCallback callback)
{
    waiters_[id].push_back(std::move(callback));
}

void MediaService::finishWaiters(const QString& id, Result<QImage> result)
{
    const auto callbacks = waiters_.take(id);
    for (const auto& callback : callbacks) {
        if (callback) {
            callback(result);
        }
    }
}

void MediaService::decodeAsync(const QByteArray& bytes, const FrameCache::Key& key)
{
    const QString id = key.id();
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, key, id]() {
        const QImage image = watcher->result();
        watcher->deleteLater();
        if (image.isNull()) {
            finishWaiters(id, ApiError(ErrorKind::Parse,
                                       QStringLiteral("The server returned an undecodable frame")));
            return;
        }
        cache_->insert(key, image);
        finishWaiters(id, image);
        emit cacheStatsChanged();
    });
    watcher->setFuture(QtConcurrent::run([bytes]() { return QImage::fromData(bytes); }));
}

RequestHandle* MediaService::startJobRequest(int jobId, int frame, const QString& quality,
                                             const QString& keyId, FrameCallback callback)
{
    rememberWaiter(keyId, std::move(callback));
    const FrameCache::Key key = makeKey(jobId, frame, quality);
    return api_.jobFrame(jobId, frame, key.quality,
                         [this, key, keyId](Result<QByteArray> result) {
        if (!result) {
            finishWaiters(keyId, result.error());
            return;
        }
        cache_->writeDisk(key, *result);
        decodeAsync(*result, key);
    });
}

RequestHandle* MediaService::requestFrame(int jobId, int frame, const QString& quality,
                                          FrameCallback callback)
{
    const FrameCache::Key key = makeKey(jobId, frame, quality);
    const QString id = key.id();

    if (const auto cached = cache_->get(key)) {
        QMetaObject::invokeMethod(this, [callback = std::move(callback), image = *cached]() {
            callback(image);
        }, Qt::QueuedConnection);
        return nullptr;
    }

    if (waiters_.contains(id)) {
        rememberWaiter(id, std::move(callback));
        return nullptr;
    }

    if (const auto disk = cache_->readDisk(key)) {
        rememberWaiter(id, std::move(callback));
        decodeAsync(*disk, key);
        return nullptr;
    }

    return startJobRequest(jobId, frame, quality, id, std::move(callback));
}

RequestHandle* MediaService::requestTaskFrame(int taskId, int frame, const QString& quality,
                                              FrameCallback callback)
{
    // Negative IDs keep task previews in a separate cache namespace.
    const int pseudoJob = -taskId;
    const FrameCache::Key key = makeKey(pseudoJob, frame, quality);
    const QString id = key.id();

    if (const auto cached = cache_->get(key)) {
        QMetaObject::invokeMethod(this, [callback = std::move(callback), image = *cached]() {
            callback(image);
        }, Qt::QueuedConnection);
        return nullptr;
    }
    if (waiters_.contains(id)) {
        rememberWaiter(id, std::move(callback));
        return nullptr;
    }
    if (const auto disk = cache_->readDisk(key)) {
        rememberWaiter(id, std::move(callback));
        decodeAsync(*disk, key);
        return nullptr;
    }

    rememberWaiter(id, std::move(callback));
    return api_.taskFrame(taskId, frame, key.quality,
                          [this, key, id](Result<QByteArray> result) {
        if (!result) {
            finishWaiters(id, result.error());
            return;
        }
        cache_->writeDisk(key, *result);
        decodeAsync(*result, key);
    });
}

RequestHandle* MediaService::requestMeta(int jobId,
                                         std::function<void(Result<DataMeta>)> callback)
{
    return api_.jobDataMeta(jobId, std::move(callback));
}

RequestHandle* MediaService::requestTaskMeta(int taskId,
                                             std::function<void(Result<DataMeta>)> callback)
{
    return api_.taskDataMeta(taskId, std::move(callback));
}

} // namespace cvat
