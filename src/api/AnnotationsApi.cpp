#include "cvatnative/api/AnnotationsApi.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QUrlQuery>

namespace cvat {

AnnotationsApi::AnnotationsApi(ApiClient* client) : client_(client) {}

QString AnnotationsApi::actionName(PatchAction action)
{
    switch (action) {
    case PatchAction::Create: return QStringLiteral("create");
    case PatchAction::Update: return QStringLiteral("update");
    case PatchAction::Delete: return QStringLiteral("delete");
    }
    return QStringLiteral("update");
}

RequestHandle* AnnotationsApi::get(int jobId, std::function<void(Result<LabeledData>)> callback)
{
    return client_->get(QStringLiteral("/api/jobs/%1/annotations").arg(jobId), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(LabeledData::fromJson(result->object()));
    });
}

RequestHandle* AnnotationsApi::patch(int jobId, PatchAction action, const LabeledData& data,
                                     std::function<void(Result<ApiResponse>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("action"), actionName(action));
    return client_->sendJson(HttpMethod::Patch,
                             QStringLiteral("/api/jobs/%1/annotations").arg(jobId), query,
                             QJsonDocument(data.toJson()), std::move(callback));
}

RequestHandle* AnnotationsApi::put(int jobId, const LabeledData& data,
                                   std::function<void(Result<ApiResponse>)> callback)
{
    return client_->sendJson(HttpMethod::Put,
                             QStringLiteral("/api/jobs/%1/annotations").arg(jobId), {},
                             QJsonDocument(data.toJson()), std::move(callback));
}

RequestHandle* AnnotationsApi::clear(int jobId, std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Delete;
    request.path = QStringLiteral("/api/jobs/%1/annotations").arg(jobId);
    request.retry = false;
    return client_->send(request, std::move(callback));
}

RequestHandle* AnnotationsApi::exportAnnotations(
    int jobId, const QString& format,
    std::function<void(Result<QByteArray>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("action"), QStringLiteral("download"));
    return client_->get(QStringLiteral("/api/jobs/%1/annotations").arg(jobId), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* AnnotationsApi::importAnnotations(
    int jobId, const QString& format, const QString& filePath,
    std::function<void(Result<ApiResponse>)> callback)
{
    auto* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        callback(ApiError(ErrorKind::Configuration,
                          QStringLiteral("Cannot open annotation file: ") + filePath));
        return nullptr;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);

    auto* multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"annotation_file\"; filename=\"%1\"")
                       .arg(QFileInfo(filePath).fileName()));
    part.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    part.setBodyDevice(file);
    file->setParent(multipart);
    multipart->append(part);

    QMap<QString, QString> headers;
    headers.insert(QStringLiteral("Upload-Start"), QStringLiteral("true"));
    headers.insert(QStringLiteral("Upload-Finish"), QStringLiteral("true"));
    return client_->uploadMultipart(QStringLiteral("/api/jobs/%1/annotations").arg(jobId),
                                    query, multipart, headers, std::move(callback));
}

} // namespace cvat
