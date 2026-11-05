#pragma once

#include "cvatnative/api/ApiClient.h"

namespace cvat {

class AuthApi {
public:
    explicit AuthApi(ApiClient* client);

    // CVAT returns a 40 character REST token on success. The caller is
    // responsible for installing it in ApiClient after validation.
    RequestHandle* login(const QString& username, const QString& password,
                         std::function<void(Result<QString>)> callback);
    RequestHandle* logout(std::function<void(Result<ApiResponse>)> callback);

    struct AccessToken {
        int id = 0;
        QString name;
        QString createdDate;
        QString expiryDate;
        bool readOnly = false;
        QString value; // returned only once, on creation
    };

    RequestHandle* listAccessTokens(std::function<void(Result<QVector<AccessToken>>)> callback);
    RequestHandle* createAccessToken(const QString& name, bool readOnly,
                                     std::function<void(Result<AccessToken>)> callback);
    RequestHandle* revokeAccessToken(int id,
                                     std::function<void(Result<ApiResponse>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
