#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class ProjectsApi {
public:
    explicit ProjectsApi(ApiClient* client);

    RequestHandle* list(int page, int pageSize, const QString& search,
                        const QString& sort,
                        std::function<void(Result<Page<Project>>)> callback);
    RequestHandle* listAll(const QString& search, const QString& sort,
                           std::function<void(Result<QVector<Project>>)> callback);
    RequestHandle* get(int id, std::function<void(Result<Project>)> callback);

    // Project labels are exposed through the project representation. These
    // helpers use the same /api/labels endpoint as the CVAT UI.
    RequestHandle* listLabels(int projectId,
                              std::function<void(Result<QVector<Label>>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
