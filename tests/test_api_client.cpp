#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/Models.h"
#include "cvatnative/model/Annotation.h"

#include <QEventLoop>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <optional>

#include <gtest/gtest.h>

using namespace cvat;

namespace {
struct MockReply {
    int status = 200;
    QByteArray body = "{}";
    QByteArray contentType = "application/json";
};

class MockServer : public QTcpServer {
public:
    MockServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    buffer_[socket] += socket->readAll();
                    const QByteArray& data = buffer_[socket];
                    const int headerEnd = data.indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }
                    const QByteArray header = data.left(headerEnd);
                    for (const auto& line : header.split('\n')) {
                        if (line.toLower().startsWith("x-test-path:")) {
                            lastPath_ = QString::fromUtf8(line.mid(13)).trimmed();
                        }
                    }
                    const auto lines = header.split('\n');
                    if (!lines.isEmpty()) {
                        const auto parts = lines.first().trimmed().split(' ');
                        if (parts.size() >= 2) {
                            lastPath_ = QString::fromUtf8(parts[1]);
                        }
                    }
                    const MockReply reply = replyFor(lastPath_);
                    socket->write("HTTP/1.1 " + QByteArray::number(reply.status) + " OK\r\n");
                    socket->write("Content-Type: " + reply.contentType + "\r\n");
                    socket->write("Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n");
                    socket->write("Connection: close\r\n\r\n");
                    socket->write(reply.body);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    quint16 start()
    {
        listen(QHostAddress::LocalHost, 0);
        return serverPort();
    }

    MockReply replyFor(const QString& path)
    {
        if (path.startsWith(QStringLiteral("/api/missing"))) {
            return MockReply{404, R"({"detail":"Not found"})"};
        }
        if (path.startsWith(QStringLiteral("/api/busy"))) {
            return MockReply{503, R"({"detail":"Busy"})"};
        }
        if (path.startsWith(QStringLiteral("/api/frame"))) {
            return MockReply{200, QByteArray("frame-bytes"), "application/octet-stream"};
        }
        return MockReply{200, R"({"count":1,"next":null,"previous":null,"results":[]})"};
    }

private:
    QHash<QTcpSocket*, QByteArray> buffer_;
    QString lastPath_;
};

template <typename T>
T waitFor(std::function<void(std::function<void(T)>)> start, bool* ok = nullptr)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool completed = false;
    std::optional<T> value;
    start([&](T result) {
        value = std::move(result);
        completed = true;
        loop.quit();
    });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();
    if (ok) {
        *ok = completed;
    }
    return value ? std::move(*value) : T(ApiError(ErrorKind::Timeout,
                                                  QStringLiteral("test timed out")));
}
} // namespace

TEST(ApiClient, ResolveKeepsBasePath)
{
    ApiClient client;
    client.setBaseUrl(QUrl(QStringLiteral("https://example.test/cvat/")));
    EXPECT_EQ(client.resolve(QStringLiteral("/api/projects")).toString(),
              QStringLiteral("https://example.test/cvat/api/projects"));
    EXPECT_EQ(client.resolve(QStringLiteral("api/projects")).toString(),
              QStringLiteral("https://example.test/cvat/api/projects"));
}

TEST(ApiClient, GetParsesJsonAndAuthorization)
{
    MockServer server;
    const quint16 port = server.start();
    ASSERT_GT(port, 0);

    ApiClient client;
    client.setBaseUrl(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port)));
    client.setToken(QStringLiteral("secret-token"));
    client.setAuthScheme(AuthScheme::Token);

    bool completed = false;
    auto result = waitFor<Result<ApiResponse>>(
        [&](std::function<void(Result<ApiResponse>)> callback) {
            client.get(QStringLiteral("/api/projects"), {}, std::move(callback));
        },
        &completed);

    ASSERT_TRUE(completed);
    ASSERT_TRUE(result.hasValue()) << result.error().toString().toStdString();
    EXPECT_EQ(result->status, 200);
    EXPECT_EQ(result->object().value(QStringLiteral("count")).toInt(), 1);
}

TEST(ApiClient, HttpErrorBecomesUnauthorized)
{
    MockServer server;
    const quint16 port = server.start();
    ASSERT_GT(port, 0);
    ApiClient client;
    client.setBaseUrl(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port)));

    auto result = waitFor<Result<ApiResponse>>([&](auto callback) {
        client.get(QStringLiteral("/api/missing"), {}, std::move(callback));
    });

    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().kind, ErrorKind::NotFound);
    EXPECT_EQ(result.error().status, 404);
}

TEST(ApiClient, ErrorFromResponseMarksAuth)
{
    ApiResponse response;
    response.status = 401;
    response.body = R"({"detail":"Invalid token"})";
    const ApiError error = ApiClient::errorFromResponse(response);
    EXPECT_EQ(error.kind, ErrorKind::Unauthorized);
    EXPECT_TRUE(error.message.contains(QStringLiteral("Invalid token")));
}
