#pragma once

#include "cvatnative/core/Result.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include <functional>
#include <memory>

class QHttpMultiPart;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace cvat {

enum class HttpMethod { Get, Post, Put, Patch, Delete, Head };

enum class AuthScheme { None, Token, Bearer };

struct ApiRequest {
    HttpMethod method = HttpMethod::Get;
    QString path;
    QUrlQuery query;
    QByteArray body;
    QString contentType = QStringLiteral("application/json");
    QMap<QString, QString> headers;
    bool authenticated = true;
    bool retry = true;
    int timeoutMs = 0; // 0 means use the client default
};

struct ApiResponse {
    int status = 0;
    QByteArray body;
    QJsonDocument json;
    QMap<QString, QString> headers;

    [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
    [[nodiscard]] QJsonObject object() const { return json.object(); }
    [[nodiscard]] QJsonArray array() const { return json.array(); }
};

class ApiClient;

// A cancellable request. The handle owns no user-visible data; it is parented
// to the ApiClient and deletes itself after completion.
class RequestHandle : public QObject {
    Q_OBJECT
public:
    ~RequestHandle() override;

    void cancel();
    [[nodiscard]] bool isFinished() const { return finished_; }
    [[nodiscard]] bool isCancelled() const { return cancelled_; }

private:
    friend class ApiClient;
    RequestHandle(ApiClient* client, ApiRequest spec,
                  std::function<void(Result<ApiResponse>)> callback);

    void start();
    void sendAttempt();
    void onFinished();
    void finishWith(Result<ApiResponse> result);
    void scheduleRetry();

    ApiClient* client_ = nullptr;
    ApiRequest spec_;
    std::function<void(Result<ApiResponse>)> callback_;
    QNetworkReply* reply_ = nullptr;
    QHttpMultiPart* multipart_ = nullptr;
    QTimer* timeoutTimer_ = nullptr;
    int attempt_ = 0;
    bool finished_ = false;
    bool cancelled_ = false;
    bool timedOut_ = false;
};

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(QObject* parent = nullptr);
    ~ApiClient() override;

    void setBaseUrl(const QUrl& url);
    [[nodiscard]] QUrl baseUrl() const { return baseUrl_; }

    void setToken(const QString& token);
    [[nodiscard]] QString token() const { return token_; }

    void setAuthScheme(AuthScheme scheme);
    [[nodiscard]] AuthScheme authScheme() const { return authScheme_; }

    void setOrganization(const QString& slug);
    [[nodiscard]] QString organization() const { return organization_; }

    void setTimeoutMs(int ms);
    [[nodiscard]] int timeoutMs() const { return timeoutMs_; }

    void setMaxRetries(int retries);
    [[nodiscard]] int maxRetries() const { return maxRetries_; }

    [[nodiscard]] QUrl resolve(const QString& path) const;

    RequestHandle* send(const ApiRequest& request,
                        std::function<void(Result<ApiResponse>)> callback);

    RequestHandle* get(const QString& path, const QUrlQuery& query,
                       std::function<void(Result<ApiResponse>)> callback);
    RequestHandle* getJson(const QString& path, const QUrlQuery& query,
                           std::function<void(Result<ApiResponse>)> callback);
    RequestHandle* sendJson(HttpMethod method, const QString& path, const QUrlQuery& query,
                            const QJsonDocument& body,
                            std::function<void(Result<ApiResponse>)> callback);
    RequestHandle* sendBytes(HttpMethod method, const QString& path, const QUrlQuery& query,
                             const QByteArray& body, const QString& contentType,
                             std::function<void(Result<ApiResponse>)> callback);

    // Multipart uploads are assembled by callers. This helper sets the correct
    // Content-Type including the boundary and disables automatic retries.
    RequestHandle* upload(const QString& path, const QUrlQuery& query,
                          const QByteArray& multipartBody, const QByteArray& boundary,
                          const QMap<QString, QString>& extraHeaders,
                          std::function<void(Result<ApiResponse>)> callback);

    // Streams a multipart body from file devices. Ownership of multipart is
    // transferred to the request handle. Uploads are never retried implicitly.
    RequestHandle* uploadMultipart(const QString& path, const QUrlQuery& query,
                                   QHttpMultiPart* multipart,
                                   const QMap<QString, QString>& extraHeaders,
                                   std::function<void(Result<ApiResponse>)> callback);

    static ApiResponse parseResponse(QNetworkReply* reply);
    static ApiError errorFromResponse(const ApiResponse& response);

private:
    friend class RequestHandle;
    QNetworkAccessManager* network_ = nullptr;
    QUrl baseUrl_;
    QString token_;
    AuthScheme authScheme_ = AuthScheme::Token;
    QString organization_;
    int timeoutMs_ = 30000;
    int maxRetries_ = 2;
    QSet<RequestHandle*> active_;
};

} // namespace cvat
