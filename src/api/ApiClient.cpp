#include "cvatnative/api/ApiClient.h"

#include "cvatnative/core/Logging.h"

#include <QJsonObject>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace cvat {

namespace {

QByteArray methodName(HttpMethod method)
{
    switch (method) {
    case HttpMethod::Get: return QByteArrayLiteral("GET");
    case HttpMethod::Post: return QByteArrayLiteral("POST");
    case HttpMethod::Put: return QByteArrayLiteral("PUT");
    case HttpMethod::Patch: return QByteArrayLiteral("PATCH");
    case HttpMethod::Delete: return QByteArrayLiteral("DELETE");
    case HttpMethod::Head: return QByteArrayLiteral("HEAD");
    }
    return QByteArrayLiteral("GET");
}

QString headerValue(const QMap<QString, QString>& headers, const QString& name)
{
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        if (it.key().compare(name, Qt::CaseInsensitive) == 0) {
            return it.value();
        }
    }
    return {};
}

QString extractErrorMessage(const QByteArray& body, int status)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QJsonObject object = document.object();
        for (const auto* key : {"detail", "message", "error", "non_field_errors"}) {
            const auto value = object.value(QLatin1String(key));
            if (value.isString()) {
                return value.toString();
            }
            if (value.isArray() && !value.toArray().isEmpty()) {
                return value.toArray().first().toString();
            }
        }
        if (!object.isEmpty()) {
            return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
        }
    }
    QString text = QString::fromUtf8(body).trimmed();
    if (text.size() > 500) {
        text = text.left(500) + QStringLiteral("…");
    }
    if (text.isEmpty()) {
        text = QStringLiteral("Request failed with HTTP status %1").arg(status);
    }
    return text;
}

ErrorKind kindForStatus(int status)
{
    if (status == 401) return ErrorKind::Unauthorized;
    if (status == 403) return ErrorKind::Forbidden;
    if (status == 404) return ErrorKind::NotFound;
    if (status >= 500) return ErrorKind::Server;
    return ErrorKind::Http;
}

} // namespace

RequestHandle::RequestHandle(ApiClient* client, ApiRequest spec,
                             std::function<void(Result<ApiResponse>)> callback)
    : QObject(client), client_(client), spec_(std::move(spec)), callback_(std::move(callback))
{
    timeoutTimer_ = new QTimer(this);
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout, this, [this]() {
        if (finished_ || !reply_) {
            return;
        }
        timedOut_ = true;
        reply_->abort();
    });
}

RequestHandle::~RequestHandle()
{
    if (reply_) {
        reply_->disconnect(this);
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    delete multipart_;
}

void RequestHandle::start()
{
    if (finished_ || cancelled_) {
        return;
    }
    sendAttempt();
}

void RequestHandle::sendAttempt()
{
    if (finished_ || cancelled_ || !client_) {
        return;
    }

    QUrl url = client_->resolve(spec_.path);
    if (!spec_.query.isEmpty()) {
        url.setQuery(spec_.query);
    }

    QNetworkRequest request(url);
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/vnd.cvat+json"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         QByteArrayLiteral("cvat-native/") + QByteArrayLiteral(CVATNATIVE_VERSION));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (spec_.authenticated && !client_->token().isEmpty()) {
        const QByteArray scheme = client_->authScheme() == AuthScheme::Bearer
            ? QByteArrayLiteral("Bearer ")
            : QByteArrayLiteral("Token ");
        request.setRawHeader(QByteArrayLiteral("Authorization"),
                             scheme + client_->token().toUtf8());
    }
    for (auto it = spec_.headers.constBegin(); it != spec_.headers.constEnd(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
    if (!client_->organization_.isNull()) {
        request.setRawHeader(QByteArrayLiteral("X-Organization"),
                             client_->organization_.toUtf8());
    }
    if (!spec_.body.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, spec_.contentType);
    }

    const QByteArray verb = methodName(spec_.method);
    if (spec_.method == HttpMethod::Get) {
        reply_ = client_->network_->get(request);
    } else if (spec_.method == HttpMethod::Head) {
        reply_ = client_->network_->head(request);
    } else if (multipart_) {
        reply_ = client_->network_->post(request, multipart_);
    } else {
        reply_ = client_->network_->sendCustomRequest(request, verb, spec_.body);
    }

    const int timeout = spec_.timeoutMs > 0 ? spec_.timeoutMs : client_->timeoutMs();
    timeoutTimer_->start(timeout);
    connect(reply_, &QNetworkReply::finished, this, &RequestHandle::onFinished);
}

void RequestHandle::onFinished()
{
    if (finished_ || cancelled_ || !reply_) {
        return;
    }
    timeoutTimer_->stop();

    QNetworkReply* reply = reply_;
    reply_ = nullptr;
    delete multipart_;
    multipart_ = nullptr;

    ApiResponse response = ApiClient::parseResponse(reply);
    const QNetworkReply::NetworkError networkError = reply->error();
    const int httpStatus = response.status;

    reply->deleteLater();

    const bool networkFailure = httpStatus == 0 && networkError != QNetworkReply::NoError;
    const bool retryableStatus = httpStatus == 429 || (httpStatus >= 500 && httpStatus <= 599);
    const bool canRetry = spec_.retry && !timedOut_ && attempt_ < client_->maxRetries() &&
        (networkFailure || retryableStatus);

    if (canRetry) {
        scheduleRetry();
        return;
    }

    if (timedOut_) {
        ApiError error(ErrorKind::Timeout, QStringLiteral("Request timed out"), 0);
        finishWith(Result<ApiResponse>(std::move(error)));
        return;
    }

    if (networkFailure) {
        ApiError error(ErrorKind::Network, reply->errorString(), 0);
        finishWith(Result<ApiResponse>(std::move(error)));
        return;
    }

    if (response.ok()) {
        finishWith(Result<ApiResponse>(std::move(response)));
        return;
    }

    ApiError error = ApiClient::errorFromResponse(response);
    finishWith(Result<ApiResponse>(std::move(error)));
}

void RequestHandle::scheduleRetry()
{
    ++attempt_;
    const int delay = std::min(4000, 250 * (1 << std::min(attempt_, 4)));
    qCWarning(logApi) << "retrying request" << spec_.path << "attempt" << attempt_ << "in" << delay
                      << "ms";
    QTimer::singleShot(delay, this, [this]() {
        if (!finished_ && !cancelled_) {
            sendAttempt();
        }
    });
}

void RequestHandle::finishWith(Result<ApiResponse> result)
{
    if (finished_ || cancelled_) {
        return;
    }
    finished_ = true;
    if (timeoutTimer_) {
        timeoutTimer_->stop();
    }
    if (client_) {
        client_->active_.remove(this);
    }
    auto callback = std::move(callback_);
    if (callback) {
        callback(std::move(result));
    }
    deleteLater();
}

void RequestHandle::cancel()
{
    if (finished_ || cancelled_) {
        return;
    }
    cancelled_ = true;
    if (timeoutTimer_) {
        timeoutTimer_->stop();
    }
    if (reply_) {
        reply_->disconnect(this);
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
    }
    delete multipart_;
    multipart_ = nullptr;
    if (client_) {
        client_->active_.remove(this);
    }
    callback_ = nullptr;
    deleteLater();
}

ApiClient::ApiClient(QObject* parent)
    : QObject(parent), network_(new QNetworkAccessManager(this))
{
    baseUrl_ = QUrl(QStringLiteral("http://localhost:8080"));
}

ApiClient::~ApiClient() = default;

void ApiClient::setBaseUrl(const QUrl& url)
{
    baseUrl_ = url;
    if (baseUrl_.path().endsWith(QLatin1Char('/'))) {
        QString path = baseUrl_.path();
        while (path.endsWith(QLatin1Char('/')) && path.size() > 1) {
            path.chop(1);
        }
        baseUrl_.setPath(path);
    }
}

void ApiClient::setToken(const QString& token) { token_ = token; }
void ApiClient::setAuthScheme(AuthScheme scheme) { authScheme_ = scheme; }
void ApiClient::setOrganization(const QString& slug) { organization_ = slug; }
void ApiClient::setTimeoutMs(int ms) { timeoutMs_ = std::max(1000, ms); }
void ApiClient::setMaxRetries(int retries) { maxRetries_ = std::max(0, retries); }

QUrl ApiClient::resolve(const QString& path) const
{
    if (path.startsWith(QStringLiteral("http://")) || path.startsWith(QStringLiteral("https://"))) {
        return QUrl(path);
    }
    QUrl url = baseUrl_;
    QString basePath = baseUrl_.path();
    if (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    const QString suffix = path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path;
    url.setPath(basePath + suffix);
    return url;
}

RequestHandle* ApiClient::send(const ApiRequest& request,
                               std::function<void(Result<ApiResponse>)> callback)
{
    auto* handle = new RequestHandle(this, request, std::move(callback));
    active_.insert(handle);
    handle->start();
    return handle;
}

RequestHandle* ApiClient::get(const QString& path, const QUrlQuery& query,
                              std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Get;
    request.path = path;
    request.query = query;
    request.retry = true;
    return send(request, std::move(callback));
}

RequestHandle* ApiClient::getJson(const QString& path, const QUrlQuery& query,
                                  std::function<void(Result<ApiResponse>)> callback)
{
    return get(path, query, std::move(callback));
}

RequestHandle* ApiClient::sendJson(HttpMethod method, const QString& path, const QUrlQuery& query,
                                   const QJsonDocument& body,
                                   std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = method;
    request.path = path;
    request.query = query;
    request.body = body.isNull() ? QByteArrayLiteral("{}") : body.toJson(QJsonDocument::Compact);
    request.contentType = QStringLiteral("application/json");
    request.retry = method == HttpMethod::Get || method == HttpMethod::Head ||
        method == HttpMethod::Put || method == HttpMethod::Delete;
    return send(request, std::move(callback));
}

RequestHandle* ApiClient::sendBytes(HttpMethod method, const QString& path, const QUrlQuery& query,
                                    const QByteArray& body, const QString& contentType,
                                    std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = method;
    request.path = path;
    request.query = query;
    request.body = body;
    request.contentType = contentType;
    request.retry = method == HttpMethod::Get || method == HttpMethod::Head;
    return send(request, std::move(callback));
}

RequestHandle* ApiClient::upload(const QString& path, const QUrlQuery& query,
                                 const QByteArray& multipartBody, const QByteArray& boundary,
                                 const QMap<QString, QString>& extraHeaders,
                                 std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Post;
    request.path = path;
    request.query = query;
    request.body = multipartBody;
    request.contentType = QStringLiteral("multipart/form-data; boundary=%1")
                              .arg(QString::fromLatin1(boundary));
    request.headers = extraHeaders;
    request.retry = false;
    request.timeoutMs = 0;
    return send(request, std::move(callback));
}

RequestHandle* ApiClient::uploadMultipart(const QString& path, const QUrlQuery& query,
                                          QHttpMultiPart* multipart,
                                          const QMap<QString, QString>& extraHeaders,
                                          std::function<void(Result<ApiResponse>)> callback)
{
    ApiRequest request;
    request.method = HttpMethod::Post;
    request.path = path;
    request.query = query;
    request.headers = extraHeaders;
    request.contentType.clear();
    request.retry = false;
    auto* handle = new RequestHandle(this, request, std::move(callback));
    handle->multipart_ = multipart;
    active_.insert(handle);
    handle->start();
    return handle;
}

ApiResponse ApiClient::parseResponse(QNetworkReply* reply)
{
    ApiResponse response;
    response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.body = reply->readAll();
    for (const auto& pair : reply->rawHeaderPairs()) {
        response.headers.insert(QString::fromLatin1(pair.first), QString::fromLatin1(pair.second));
    }
    const QString contentType = headerValue(response.headers, QStringLiteral("Content-Type"));
    const bool looksJson = contentType.contains(QStringLiteral("json")) ||
        response.body.startsWith('{') || response.body.startsWith('[');
    if (looksJson) {
        response.json = QJsonDocument::fromJson(response.body);
    }
    return response;
}

ApiError ApiClient::errorFromResponse(const ApiResponse& response)
{
    ApiError error(kindForStatus(response.status), extractErrorMessage(response.body, response.status),
                   response.status);
    error.body = response.body;
    error.details = response.json;
    if (response.status == 401) {
        error.message = QStringLiteral("Authentication required or token expired: ") + error.message;
    }
    return error;
}

} // namespace cvat
