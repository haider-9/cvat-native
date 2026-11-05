#pragma once

#include <QString>

namespace cvat {

class Settings {
public:
    static Settings& instance();

    QString serverUrl() const;
    void setServerUrl(const QString& url);

    QString username() const;
    void setUsername(const QString& username);

    QString authScheme() const; // "token" (login key) or "bearer" (personal access token)
    void setAuthScheme(const QString& scheme);

    int requestTimeoutMs() const;
    void setRequestTimeoutMs(int ms);

    int maxRetries() const;
    void setMaxRetries(int retries);

    int memoryCacheMiB() const;
    void setMemoryCacheMiB(int mib);

    int diskCacheMiB() const;
    void setDiskCacheMiB(int mib);

    int autosaveSeconds() const;
    void setAutosaveSeconds(int seconds);

    bool remoteAutosave() const;
    void setRemoteAutosave(bool enabled);

    bool showDeletedFrames() const;
    void setShowDeletedFrames(bool enabled);

    QString cacheDirectory() const;
    QString dataDirectory() const;
    QString databasePath() const;

    void sync();

private:
    Settings();
    class Private;
    Private* d;
};

} // namespace cvat
