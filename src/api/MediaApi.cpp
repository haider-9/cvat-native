#include "cvatnative/api/MediaApi.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QUrlQuery>

namespace cvat {

MediaApi::MediaApi(ApiClient* client) : client_(client) {}

QUrlQuery MediaApi::frameQuery(int number, const QString& quality)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("frame"));
    query.addQueryItem(QStringLiteral("number"), QString::number(number));
    query.addQueryItem(QStringLiteral("quality"),
                       quality.isEmpty() ? QStringLiteral("compressed") : quality);
    return query;
}

RequestHandle* MediaApi::fetchData(const QString& path, int number, const QString& type,
                                   const QString& quality,
                                   std::function<void(Result<QByteArray>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"), type);
    query.addQueryItem(QStringLiteral("number"), QString::number(number));
    query.addQueryItem(QStringLiteral("quality"),
                       quality.isEmpty() ? QStringLiteral("compressed") : quality);
    return client_->get(path, query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* MediaApi::jobFrame(int jobId, int frame, const QString& quality,
                                  std::function<void(Result<QByteArray>)> callback)
{
    return client_->get(QStringLiteral("/api/jobs/%1/data").arg(jobId),
                        frameQuery(frame, quality),
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* MediaApi::taskFrame(int taskId, int frame, const QString& quality,
                                   std::function<void(Result<QByteArray>)> callback)
{
    return client_->get(QStringLiteral("/api/tasks/%1/data").arg(taskId),
                        frameQuery(frame, quality),
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* MediaApi::jobChunk(int jobId, int chunk, const QString& quality,
                                  std::function<void(Result<QByteArray>)> callback)
{
    return fetchData(QStringLiteral("/api/jobs/%1/data").arg(jobId), chunk,
                     QStringLiteral("chunk"), quality, std::move(callback));
}

RequestHandle* MediaApi::taskChunk(int taskId, int chunk, const QString& quality,
                                   std::function<void(Result<QByteArray>)> callback)
{
    return fetchData(QStringLiteral("/api/tasks/%1/data").arg(taskId), chunk,
                     QStringLiteral("chunk"), quality, std::move(callback));
}

RequestHandle* MediaApi::jobDataMeta(int jobId, std::function<void(Result<DataMeta>)> callback)
{
    return client_->get(QStringLiteral("/api/jobs/%1/data/meta").arg(jobId), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(DataMeta::fromJson(result->object()));
    });
}

RequestHandle* MediaApi::taskDataMeta(int taskId, std::function<void(Result<DataMeta>)> callback)
{
    return client_->get(QStringLiteral("/api/tasks/%1/data/meta").arg(taskId), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(DataMeta::fromJson(result->object()));
    });
}

RequestHandle* MediaApi::downloadDataset(int jobId, const QString& format,
                                         std::function<void(Result<QByteArray>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("download"));
    return client_->get(QStringLiteral("/api/jobs/%1/dataset").arg(jobId), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* MediaApi::downloadTaskDataset(int taskId, const QString& format,
                                             std::function<void(Result<QByteArray>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("download"));
    return client_->get(QStringLiteral("/api/tasks/%1/dataset").arg(taskId), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* MediaApi::startJobDatasetExport(int jobId, const QString& format, bool saveImages,
                                               std::function<void(Result<QString>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("save_images"), saveImages ? QStringLiteral("true")
                                                                  : QStringLiteral("false"));
    return client_->send({HttpMethod::Post,
                          QStringLiteral("/api/jobs/%1/dataset/export").arg(jobId), query,
                          {}, QStringLiteral("application/json"), {}, true, false},
                         [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        const QString requestId = result->object().value(QStringLiteral("rq_id")).toString();
        if (requestId.isEmpty()) {
            callback(ApiError(ErrorKind::Parse,
                              QStringLiteral("CVAT did not return an export request id")));
            return;
        }
        callback(requestId);
    });
}

RequestHandle* MediaApi::downloadResultUrl(const QString& resultUrl,
                                           std::function<void(Result<QByteArray>)> callback)
{
    if (resultUrl.isEmpty()) {
        callback(ApiError(ErrorKind::Parse, QStringLiteral("CVAT returned an empty result URL")));
        return nullptr;
    }
    return client_->get(resultUrl, {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* MediaApi::uploadTaskData(int taskId, const QStringList& files, int imageQuality,
                                        std::function<void(Result<ApiResponse>)> callback)
{
    if (files.isEmpty()) {
        callback(ApiError(ErrorKind::Configuration,
                          QStringLiteral("No files were selected for the task")));
        return nullptr;
    }

    auto* multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    for (qsizetype i = 0; i < files.size(); ++i) {
        auto* file = new QFile(files[i]);
        if (!file->open(QIODevice::ReadOnly)) {
            delete file;
            delete multipart;
            callback(ApiError(ErrorKind::Configuration,
                              QStringLiteral("Cannot open: ") + files[i]));
            return nullptr;
        }
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"client_files[%1]\"; filename=\"%2\"")
                           .arg(i)
                           .arg(QFileInfo(files[i]).fileName()));
        part.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/octet-stream"));
        part.setBodyDevice(file);
        file->setParent(multipart);
        multipart->append(part);
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("image_quality"), QString::number(imageQuality));
    QMap<QString, QString> headers;
    headers.insert(QStringLiteral("Upload-Start"), QStringLiteral("true"));
    headers.insert(QStringLiteral("Upload-Finish"), QStringLiteral("true"));
    return client_->uploadMultipart(QStringLiteral("/api/tasks/%1/data").arg(taskId), query,
                                    multipart, headers, std::move(callback));
}

} // namespace cvat
