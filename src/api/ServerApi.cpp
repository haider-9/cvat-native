#include "cvatnative/api/ServerApi.h"

namespace cvat {

ServerApi::ServerApi(ApiClient* client) : client_(client) {}

RequestHandle* ServerApi::about(std::function<void(Result<ServerInfo>)> callback)
{
    return client_->get(QStringLiteral("/api/server/about"), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(ServerInfo::fromJson(result->object()));
    });
}

RequestHandle* ServerApi::formats(std::function<void(Result<Formats>)> callback)
{
    return client_->get(QStringLiteral("/api/server/annotation/formats"), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(Formats::fromJson(result->object()));
    });
}

RequestHandle* ServerApi::request(const QString& requestId,
                                  std::function<void(Result<RequestState>)> callback)
{
    return client_->get(QStringLiteral("/api/requests/%1").arg(requestId), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(RequestState::fromJson(result->object()));
    });
}

RequestHandle* ServerApi::cancelRequest(const QString& requestId,
                                        std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Post;
    request.path = QStringLiteral("/api/requests/%1/cancel").arg(requestId);
    request.retry = false;
    return client_->send(request, std::move(callback));
}

} // namespace cvat
