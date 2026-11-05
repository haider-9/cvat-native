#include "cvatnative/api/JobsApi.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace cvat {

JobsApi::JobsApi(ApiClient* client) : client_(client) {}

RequestHandle* JobsApi::list(int page, int pageSize, const QString& search, int taskId,
                             int projectId, const QString& state, const QString& stage,
                             const QString& sort,
                             std::function<void(Result<Page<Job>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(QStringLiteral("page_size"), QString::number(pageSize));
    if (!search.isEmpty()) {
        query.addQueryItem(QStringLiteral("search"), search);
    }
    if (taskId > 0) {
        query.addQueryItem(QStringLiteral("task_id"), QString::number(taskId));
    }
    if (projectId > 0) {
        query.addQueryItem(QStringLiteral("project_id"), QString::number(projectId));
    }
    if (!state.isEmpty()) {
        query.addQueryItem(QStringLiteral("state"), state);
    }
    if (!stage.isEmpty()) {
        query.addQueryItem(QStringLiteral("stage"), stage);
    }
    if (!sort.isEmpty()) {
        query.addQueryItem(QStringLiteral("sort"), sort);
    }
    return client_->get(QStringLiteral("/api/jobs"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        Page<Job> page;
        page.meta = pageMetaFromJson(result->object());
        for (const auto& item : result->object().value(QStringLiteral("results")).toArray()) {
            page.results.push_back(Job::fromJson(item.toObject()));
        }
        callback(page);
    });
}

RequestHandle* JobsApi::get(int id, std::function<void(Result<Job>)> callback)
{
    return client_->get(QStringLiteral("/api/jobs/%1").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(Job::fromJson(result->object()));
    });
}

RequestHandle* JobsApi::dataMeta(int id, std::function<void(Result<DataMeta>)> callback)
{
    return client_->get(QStringLiteral("/api/jobs/%1/data/meta").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(DataMeta::fromJson(result->object()));
    });
}

RequestHandle* JobsApi::preview(int id, std::function<void(Result<QByteArray>)> callback)
{
    return client_->get(QStringLiteral("/api/jobs/%1/preview").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

} // namespace cvat
