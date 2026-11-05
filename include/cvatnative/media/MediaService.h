#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/MediaApi.h"
#include "cvatnative/media/FrameCache.h"

#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace cvat {

// Coordinates HTTP frame fetching, disk persistence and background decoding.
// Requests for the same frame are coalesced, and decoding never runs on the
// GUI thread.
class MediaService : public QObject {
    Q_OBJECT
public:
    using FrameCallback = std::function<void(Result<QImage>)>;

    MediaService(ApiClient* client, FrameCache* cache, QObject* parent = nullptr);

    RequestHandle* requestFrame(int jobId, int frame, const QString& quality,
                                FrameCallback callback);
    RequestHandle* requestTaskFrame(int taskId, int frame, const QString& quality,
                                    FrameCallback callback);
    RequestHandle* requestMeta(int jobId,
                               std::function<void(Result<DataMeta>)> callback);
    RequestHandle* requestTaskMeta(int taskId,
                                   std::function<void(Result<DataMeta>)> callback);

    [[nodiscard]] FrameCache* cache() const { return cache_; }

signals:
    void cacheStatsChanged();

private:
    static FrameCache::Key makeKey(int jobId, int frame, const QString& quality);
    void rememberWaiter(const QString& id, FrameCallback callback);
    void decodeAsync(const QByteArray& bytes, const FrameCache::Key& key);
    void finishWaiters(const QString& id, Result<QImage> result);
    RequestHandle* startJobRequest(int jobId, int frame, const QString& quality,
                                   const QString& keyId, FrameCallback callback);

    ApiClient* client_;
    FrameCache* cache_;
    MediaApi api_;
    QHash<QString, QVector<FrameCallback>> waiters_;
};

} // namespace cvat
