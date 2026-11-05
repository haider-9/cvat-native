#include "cvatnative/core/Logging.h"

#include "cvatnative/core/Result.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace cvat {

Q_LOGGING_CATEGORY(logApi, "cvat.api")
Q_LOGGING_CATEGORY(logApp, "cvat.app")
Q_LOGGING_CATEGORY(logStore, "cvat.store")
Q_LOGGING_CATEGORY(logMedia, "cvat.media")

namespace {

QString sanitize(QString message)
{
    // Never allow a token or password to reach the log. The patterns are
    // intentionally broad: false positives are cheaper than leaked credentials.
    static const QRegularExpression authorization(
        QStringLiteral("(Authorization\\s*:\\s*)(Token|Bearer|Basic)\\s+\\S+"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tokenJson(
        QStringLiteral("\"(?:key|value|token|password)\"\\s*:\\s*\"[^\"]*\""),
        QRegularExpression::CaseInsensitiveOption);
    message.replace(authorization, QStringLiteral("\\1\\2 [redacted]"));
    message.replace(tokenJson, QStringLiteral("\"[redacted]\":\"[redacted]\""));
    return message;
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const char* level = "INFO";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString line = QStringLiteral("%1 %2 [%3] %4\n")
                             .arg(timestamp, QString::fromLatin1(level),
                                  QString::fromLatin1(context.category ? context.category : "default"),
                                  sanitize(message));
    QTextStream(stderr) << line;
    QTextStream(stderr).flush();
}

} // namespace

QString ApiError::toString() const
{
    QString text = message;
    if (status > 0) {
        text = QStringLiteral("HTTP %1: %2").arg(status).arg(text);
    }
    return text;
}

void installLogging()
{
    qInstallMessageHandler(messageHandler);
}

void setLogLevel(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    QLoggingCategory::setFilterRules(QStringLiteral("cvat.*.debug=%1\ncvat.*.info=true")
                                         .arg(normalized == QStringLiteral("debug") ? QStringLiteral("true")
                                                                                     : QStringLiteral("false")));
}

QString redactUrl(const QString& url)
{
    // Query strings can contain filenames but never credentials in CVAT. Strip
    // them anyway to keep logs stable and small.
    return url.section(u'?', 0, 0);
}

} // namespace cvat
