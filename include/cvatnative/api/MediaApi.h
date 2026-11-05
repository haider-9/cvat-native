#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

// Frame and chunk transport. CVAT returns individual frames as image bytes and
// image-set chunks as ZIP archives. MediaService owns decoding and caching;
// this class only performs requests.
class MediaApi {
public:
    explicit MediaApi(ApiClient* client);

    RequestHandle* jobFrame(int jobId, int frame, const QString& quality,
                            std::function<void(Result<QByteArray>)> callback);
    RequestHandle* taskFrame(int taskId, int frame, const QString& quality,
                             std::function<void(Result<QByteArray>)> callback);
    RequestHandle* jobChunk(int jobId, int chunk, const QString& quality,
                            std::function<void(Result<QByteArray>)> callback);
    RequestHandle* taskChunk(int taskId, int chunk, const QString& quality,
                             std::function<void(Result<QByteArray>)> callback);

    RequestHandle* jobDataMeta(int jobId, std::function<void(Result<DataMeta>)> callback);
    RequestHandle* taskDataMeta(int taskId, std::function<void(Result<DataMeta>)> callback);

    RequestHandle* downloadDataset(int jobId, const QString& format,
                                   std::function<void(Result<QByteArray>)> callback);
    RequestHandle* downloadTaskDataset(int taskId, const QString& format,
                                       std::function<void(Result<QByteArray>)> callback);
    RequestHandle* startJobDatasetExport(int jobId, const QString& format, bool saveImages,
                                         std::function<void(Result<QString>)> callback);
    RequestHandle* downloadResultUrl(const QString& resultUrl,
                                     std::function<void(Result<QByteArray>)> callback);

    // One-shot data upload for a newly created task. CVAT accepts
    // Upload-Start and Upload-Finish in the same multipart request.
    RequestHandle* uploadTaskData(int taskId, const QStringList& files, int imageQuality,
                                  std::function<void(Result<ApiResponse>)> callback);

private:
    RequestHandle* fetchData(const QString& path, int number, const QString& type,
                             const QString& quality,
                             std::function<void(Result<QByteArray>)> callback);
    static QUrlQuery frameQuery(int number, const QString& quality);

    ApiClient* client_;
};

} // namespace cvat
