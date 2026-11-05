#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class LabelsApi {
public:
    explicit LabelsApi(ApiClient* client);

    // Exactly one of projectId/taskId must be set.
    RequestHandle* list(int projectId, int taskId, const QString& search,
                        std::function<void(Result<QVector<Label>>)> callback);
    RequestHandle* get(int id, std::function<void(Result<Label>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
