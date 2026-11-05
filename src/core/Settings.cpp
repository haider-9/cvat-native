#include "cvatnative/core/Settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace cvat {

class Settings::Private {
public:
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("cvat-native"), QStringLiteral("cvat-native")};
};

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

Settings::Settings() : d(new Private) {}

QString Settings::serverUrl() const
{
    return d->settings.value(QStringLiteral("server/url"), QStringLiteral("http://localhost:8080"))
        .toString();
}

void Settings::setServerUrl(const QString& url) { d->settings.setValue(QStringLiteral("server/url"), url); }

QString Settings::username() const
{
    return d->settings.value(QStringLiteral("server/username")).toString();
}

void Settings::setUsername(const QString& username)
{
    d->settings.setValue(QStringLiteral("server/username"), username);
}

QString Settings::authScheme() const
{
    return d->settings.value(QStringLiteral("server/authScheme"), QStringLiteral("token")).toString();
}

void Settings::setAuthScheme(const QString& scheme)
{
    d->settings.setValue(QStringLiteral("server/authScheme"), scheme);
}

int Settings::requestTimeoutMs() const
{
    return d->settings.value(QStringLiteral("network/timeoutMs"), 30000).toInt();
}

void Settings::setRequestTimeoutMs(int ms) { d->settings.setValue(QStringLiteral("network/timeoutMs"), ms); }

int Settings::maxRetries() const
{
    return d->settings.value(QStringLiteral("network/maxRetries"), 2).toInt();
}

void Settings::setMaxRetries(int retries) { d->settings.setValue(QStringLiteral("network/maxRetries"), retries); }

int Settings::memoryCacheMiB() const
{
    return d->settings.value(QStringLiteral("cache/memoryMiB"), 256).toInt();
}

void Settings::setMemoryCacheMiB(int mib) { d->settings.setValue(QStringLiteral("cache/memoryMiB"), mib); }

int Settings::diskCacheMiB() const
{
    return d->settings.value(QStringLiteral("cache/diskMiB"), 1024).toInt();
}

void Settings::setDiskCacheMiB(int mib) { d->settings.setValue(QStringLiteral("cache/diskMiB"), mib); }

int Settings::autosaveSeconds() const
{
    return d->settings.value(QStringLiteral("editing/autosaveSeconds"), 15).toInt();
}

void Settings::setAutosaveSeconds(int seconds)
{
    d->settings.setValue(QStringLiteral("editing/autosaveSeconds"), seconds);
}

bool Settings::remoteAutosave() const
{
    return d->settings.value(QStringLiteral("editing/remoteAutosave"), false).toBool();
}

void Settings::setRemoteAutosave(bool enabled)
{
    d->settings.setValue(QStringLiteral("editing/remoteAutosave"), enabled);
}

bool Settings::showDeletedFrames() const
{
    return d->settings.value(QStringLiteral("view/showDeletedFrames"), false).toBool();
}

void Settings::setShowDeletedFrames(bool enabled)
{
    d->settings.setValue(QStringLiteral("view/showDeletedFrames"), enabled);
}

QString Settings::cacheDirectory() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString dir = root.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.cache/cvat-native")
        : root;
    QDir().mkpath(dir);
    return dir;
}

QString Settings::dataDirectory() const
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
        root = QDir::homePath() + QStringLiteral("/.local/share/cvat-native");
    }
    QDir().mkpath(root);
    return root;
}

QString Settings::databasePath() const
{
    return dataDirectory() + QStringLiteral("/cache.sqlite3");
}

void Settings::sync() { d->settings.sync(); }

} // namespace cvat
