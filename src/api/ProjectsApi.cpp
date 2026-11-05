#include "cvatnative/api/ProjectsApi.h"

#include <QJsonArray>
#include <QUrlQuery>

#include <memory>

namespace cvat {

ProjectsApi::ProjectsApi(ApiClient* client) : client_(client) {}

RequestHandle* ProjectsApi::list(int page, int pageSize, const QString& search,
                                 const QString& sort,
                                 std::function<void(Result<Page<Project>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(QStringLiteral("page_size"), QString::number(pageSize));
    if (!search.isEmpty()) {
        query.addQueryItem(QStringLiteral("search"), search);
    }
    if (!sort.isEmpty()) {
        query.addQueryItem(QStringLiteral("sort"), sort);
    }
    return client_->get(QStringLiteral("/api/projects"), query,
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        Page<Project> page;
        page.meta = pageMetaFromJson(result->object());
        for (const auto& item : result->object().value(QStringLiteral("results")).toArray()) {
            page.results.push_back(Project::fromJson(item.toObject()));
        }
        callback(page);
    });
}

RequestHandle* ProjectsApi::listAll(
    const QString& search, const QString& sort,
    std::function<void(Result<QVector<Project>>)> callback)
{
    auto projects = std::make_shared<QVector<Project>>();
    auto request = std::make_shared<RequestHandle*>(nullptr);
    auto fetch = std::make_shared<std::function<void(int)>>();
    *fetch = [this, search, sort, callback = std::move(callback), projects, request, fetch](int page) {
        *request = list(page, 100, search, sort, [this, search, sort, callback, projects, request, fetch,
                                                   page](Result<Page<Project>> result) {
            if (!result) {
                callback(result.error());
                return;
            }
            projects->append(result->results);
            if (result->meta.hasNext() && !result->results.isEmpty()) {
                (*fetch)(page + 1);
                return;
            }
            callback(*projects);
        });
    };
    (*fetch)(1);
    return *request;
}

RequestHandle* ProjectsApi::get(int id, std::function<void(Result<Project>)> callback)
{
    return client_->get(QStringLiteral("/api/projects/%1").arg(id), {},
                        [callback = std::move(callback)](Result<ApiResponse> result) {
        if (!result) {
            callback(result.error());
            return;
        }
        callback(Project::fromJson(result->object()));
    });
}

RequestHandle* ProjectsApi::listLabels(int projectId,
                                       std::function<void(Result<QVector<Label>>)> callback)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("project_id"), QString::number(projectId));
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
