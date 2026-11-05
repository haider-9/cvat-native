#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class ServerApi {
public:
    explicit ServerApi(ApiClient* client);

    RequestHandle* about(std::function<void(Result<ServerInfo>)> callback);
    RequestHandle* formats(std::function<void(Result<Formats>)> callback);
    RequestHandle* request(const QString& requestId,
                           std::function<void(Result<RequestState>)> callback);
    RequestHandle* cancelRequest(const QString& requestId,
                                 std::function<void(Result<ApiResponse>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
