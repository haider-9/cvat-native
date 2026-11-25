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

    QString themeMode() const; // "system", "light" or "dark"
    void setThemeMode(const QString& mode);

    QString guideColor() const; // "#rrggbb"
    void setGuideColor(const QString& color);

    QString fontFamily() const; // empty = application default
    void setFontFamily(const QString& family);

    int fontPointSize() const; // 0 = application default
    void setFontPointSize(int size);

    bool showMiniMap() const;
    void setShowMiniMap(bool enabled);

    bool multiLabelCopy() const; // area-select + copy/paste several labels at once
    void setMultiLabelCopy(bool enabled);

    bool checkUpdatesOnStartup() const;
    void setCheckUpdatesOnStartup(bool enabled);

    QString ignoredUpdateVersion() const;
    void setIgnoredUpdateVersion(const QString& version);

    // Empty = derive it from the running executable (bin/ parent on Linux,
    // application directory on Windows).
    QString updateInstallDirectory() const;
    void setUpdateInstallDirectory(const QString& directory);

    // Digit 0..9 -> label id for the Ctrl/Alt number shortcuts. -1 = unbound.
    int hotkeyLabel(int digit) const;
    void setHotkeyLabel(int digit, int labelId);

    void sync();

private:
    Settings();
    class Private;
    Private* d;
};

} // namespace cvat
