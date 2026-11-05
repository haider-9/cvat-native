#include "cvatnative/api/OrganizationsApi.h"

#include <QUrlQuery>

namespace cvat {

OrganizationsApi::OrganizationsApi(ApiClient* client) : client_(client) {}

RequestHandle* OrganizationsApi::list(
    std::function<void(Result<QVector<Organization>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("500"));
    return client_->get(QStringLiteral("/api/organizations"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        QVector<Organization> organizations;
        for (const auto& value : result->object().value(QStringLiteral("results")).toArray()) {
            organizations.push_back(Organization::fromJson(value.toObject()));
        }
        callback(organizations);
    });
}

void OrganizationsApi::setOrganization(const QString& slug)
{
    client_->setOrganization(slug);
}

} // namespace cvat