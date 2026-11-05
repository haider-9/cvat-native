#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class TasksApi {
public:
    explicit TasksApi(ApiClient* client);

    RequestHandle* list(int page, int pageSize, const QString& search, int projectId,
                        const QString& status, const QString& sort,
                        std::function<void(Result<Page<Task>>)> callback);
    RequestHandle* get(int id, std::function<void(Result<Task>)> callback);

    RequestHandle* dataMeta(int id, std::function<void(Result<DataMeta>)> callback);
    RequestHandle* preview(int id, std::function<void(Result<QByteArray>)> callback);
    RequestHandle* listLabels(int taskId,
                              std::function<void(Result<QVector<Label>>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
