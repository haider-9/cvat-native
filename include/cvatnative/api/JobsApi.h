#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class JobsApi {
public:
    explicit JobsApi(ApiClient* client);

    RequestHandle* list(int page, int pageSize, const QString& search, int taskId,
                        int projectId, const QString& state, const QString& stage,
                        const QString& sort,
                        std::function<void(Result<Page<Job>>)> callback);
    RequestHandle* get(int id, std::function<void(Result<Job>)> callback);

    RequestHandle* dataMeta(int id, std::function<void(Result<DataMeta>)> callback);
    RequestHandle* preview(int id, std::function<void(Result<QByteArray>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
