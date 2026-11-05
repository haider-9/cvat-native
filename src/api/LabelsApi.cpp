#include "cvatnative/api/LabelsApi.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace cvat {

LabelsApi::LabelsApi(ApiClient* client) : client_(client) {}

RequestHandle* LabelsApi::list(int projectId, int taskId, const QString& search,
                               std::function<void(Result<QVector<Label>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("500"));
    if (projectId > 0) {
        query.addQueryItem(QStringLiteral("project_id"), QString::number(projectId));
    } else if (taskId > 0) {
        query.addQueryItem(QStringLiteral("task_id"), QString::number(taskId));
    }
    if (!search.isEmpty()) {
        query.addQueryItem(QStringLiteral("search"), search);
    }
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

RequestHandle* LabelsApi::get(int id, std::function<void(Result<Label>)> callback)
{
    return client_->get(QStringLiteral("/api/labels/%1").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(Label::fromJson(result->object()));
    });
}

} // namespace cvat
