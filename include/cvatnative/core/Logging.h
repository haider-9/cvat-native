#pragma once

#include <QLoggingCategory>
#include <QString>

namespace cvat {

Q_DECLARE_LOGGING_CATEGORY(logApi)
Q_DECLARE_LOGGING_CATEGORY(logApp)
Q_DECLARE_LOGGING_CATEGORY(logStore)
Q_DECLARE_LOGGING_CATEGORY(logMedia)

void installLogging();
void setLogLevel(const QString& level);
QString redactUrl(const QString& url);

} // namespace cvat
