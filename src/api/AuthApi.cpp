#include "cvatnative/api/AuthApi.h"

#include "cvatnative/api/Models.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

namespace cvat {

AuthApi::AuthApi(ApiClient* client) : client_(client) {}

RequestHandle* AuthApi::login(const QString& username, const QString& password,
                              std::function<void(Result<QString>)> callback)
{
    QJsonObject body;
    body.insert(QStringLiteral("username"), username);
    body.insert(QStringLiteral("password"), password);

    ApiRequest request;
    request.method = HttpMethod::Post;
    request.path = QStringLiteral("/api/auth/login");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    request.authenticated = false;
    request.retry = false;
    return client_->send(request, [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        const QString key = result->object().value(QStringLiteral("key")).toString();
        if (key.isEmpty()) {
            callback(ApiError(ErrorKind::Parse, QStringLiteral("Login response did not contain a token")));
            return;
        }
        callback(key);
    });
}

RequestHandle* AuthApi::logout(std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Post;
    request.path = QStringLiteral("/api/auth/logout");
    request.retry = false;
    return client_->send(request, std::move(callback));
}

RequestHandle* AuthApi::listAccessTokens(
    std::function<void(Result<QVector<AccessToken>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("100"));
    return client_->get(QStringLiteral("/api/auth/access_tokens"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        QVector<AccessToken> tokens;
        for (const auto& item : result->object().value(QStringLiteral("results")).toArray()) {
            const QJsonObject object = item.toObject();
            AccessToken token;
            token.id = intValue(object, "id");
            token.name = stringValue(object, "name");
            token.createdDate = stringValue(object, "created_date");
            token.expiryDate = stringValue(object, "expiry_date");
            token.readOnly = boolValue(object, "read_only");
            token.value = stringValue(object, "value");
            tokens.push_back(token);
        }
        callback(tokens);
    });
}

RequestHandle* AuthApi::createAccessToken(const QString& name, bool readOnly,
                                          std::function<void(Result<AccessToken>)> callback)
{
    QJsonObject body;
    body.insert(QStringLiteral("name"), name);
    body.insert(QStringLiteral("read_only"), readOnly);
    return client_->sendJson(HttpMethod::Post, QStringLiteral("/api/auth/access_tokens"), {},
                             QJsonDocument(body),
                             [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        const QJsonObject object = result->object();
        AccessToken token;
        token.id = intValue(object, "id");
        token.name = stringValue(object, "name");
        token.createdDate = stringValue(object, "created_date");
        token.expiryDate = stringValue(object, "expiry_date");
        token.readOnly = boolValue(object, "read_only");
        token.value = stringValue(object, "value");
        callback(token);
    });
}

RequestHandle* AuthApi::revokeAccessToken(int id, std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Delete;
    request.path = QStringLiteral("/api/auth/access_tokens/%1").arg(id);
    return client_->send(request, std::move(callback));
}

} // namespace cvat
