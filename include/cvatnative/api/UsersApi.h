#pragma once

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"

namespace cvat {

class UsersApi {
public:
    explicit UsersApi(ApiClient* client);

    RequestHandle* currentUser(std::function<void(Result<UserInfo>)> callback);
    RequestHandle* listUsers(int page, int pageSize, const QString& search,
                             std::function<void(Result<Page<UserInfo>>)> callback);
    RequestHandle* getUser(int id, std::function<void(Result<UserInfo>)> callback);

private:
    ApiClient* client_;
};

} // namespace cvat
