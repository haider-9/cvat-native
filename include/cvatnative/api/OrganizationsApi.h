#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class OrganizationsApi {
public:
    explicit OrganizationsApi(ApiClient* client);

    RequestHandle* list(std::function<void(Result<QVector<Organization>>)> callback);
    void setOrganization(const QString& slug);

private:
    ApiClient* client_;
};

} // namespace cvat