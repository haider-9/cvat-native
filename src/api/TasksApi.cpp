#include "cvatnative/api/TasksApi.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace cvat {

TasksApi::TasksApi(ApiClient* client) : client_(client) {}

RequestHandle* TasksApi::list(int page, int pageSize, const QString& search, int projectId,
                              const QString& status, const QString& sort,
                              std::function<void(Result<Page<Task>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(QStringLiteral("page_size"), QString::number(pageSize));
    if (!search.isEmpty()) {
        query.addQueryItem(QStringLiteral("search"), search);
    }
    if (projectId > 0) {
        query.addQueryItem(QStringLiteral("project_id"), QString::number(projectId));
    }
    if (!status.isEmpty()) {
        query.addQueryItem(QStringLiteral("status"), status);
    }
    if (!sort.isEmpty()) {
        query.addQueryItem(QStringLiteral("sort"), sort);
    }
    return client_->get(QStringLiteral("/api/tasks"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        Page<Task> page;
        page.meta = pageMetaFromJson(result->object());
        for (const auto& item : result->object().value(QStringLiteral("results")).toArray()) {
            page.results.push_back(Task::fromJson(item.toObject()));
        }
        callback(page);
    });
}

RequestHandle* TasksApi::get(int id, std::function<void(Result<Task>)> callback)
{
    return client_->get(QStringLiteral("/api/tasks/%1").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(Task::fromJson(result->object()));
    });
}

RequestHandle* TasksApi::dataMeta(int id, std::function<void(Result<DataMeta>)> callback)
{
    return client_->get(QStringLiteral("/api/tasks/%1/data/meta").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(DataMeta::fromJson(result->object()));
    });
}

RequestHandle* TasksApi::preview(int id, std::function<void(Result<QByteArray>)> callback)
{
    return client_->get(QStringLiteral("/api/tasks/%1/preview").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(result->body);
    });
}

RequestHandle* TasksApi::listLabels(int taskId,
                                    std::function<void(Result<QVector<Label>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("task_id"), QString::number(taskId));
    query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("500"));
    return client_->get(QStringLiteral("/api/labels"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        QVector<Label> labels;
        for (const auto& item : result->object().value(QStringLiteral("results")).toArray()) {
            labels.push_back(Label::fromJson(item.toObject()));
        }
        callback(labels);
    });
}

} // namespace cvat
