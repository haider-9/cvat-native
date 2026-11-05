#include "cvatnative/api/UsersApi.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace cvat {

UsersApi::UsersApi(ApiClient* client) : client_(client) {}

RequestHandle* UsersApi::currentUser(std::function<void(Result<UserInfo>)> callback)
{
    return client_->get(QStringLiteral("/api/users/self"), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(UserInfo::fromJson(result->object()));
    });
}

RequestHandle* UsersApi::listUsers(int page, int pageSize, const QString& search,
                                   std::function<void(Result<Page<UserInfo>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(QStringLiteral("page_size"), QString::number(pageSize));
    if (!search.isEmpty()) {
        query.addQueryItem(QStringLiteral("search"), search);
    }
    return client_->get(QStringLiteral("/api/users"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        Page<UserInfo> page;
        page.meta = pageMetaFromJson(result->object());
        for (const auto& item : result->object().value(QStringLiteral("results")).toArray()) {
            page.results.push_back(UserInfo::fromJson(item.toObject()));
        }
        callback(page);
    });
}

RequestHandle* UsersApi::getUser(int id, std::function<void(Result<UserInfo>)> callback)
{
    return client_->get(QStringLiteral("/api/users/%1").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(UserInfo::fromJson(result->object()));
    });
}

} // namespace cvat
