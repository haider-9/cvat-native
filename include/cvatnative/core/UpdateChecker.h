#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace cvat {

// One GitHub release, reduced to the fields the updater needs.
struct UpdateRelease {
    QString version;   // tag_name, for example "v0.2.0"
    QString notes;     // release body
    QString assetName; // platform ZIP attached to the release
    QString assetUrl;  // browser_download_url of that ZIP
};

// Talks to the GitHub Releases API, downloads a platform ZIP and hands it to a
// detached updater script. All network work happens on the Qt event loop; the
// script does the file swap after this process has exited.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    static QString apiUrl();
    static QString platformToken(); // "windows" or "linux"

    // Returns 1 when left is newer, -1 when right is newer, 0 when equal.
    // Leading "v" and pre-release suffixes are ignored, so "v0.2.0" == "0.2".
    static int compareVersions(const QString& left, const QString& right);

    // Picks the best-matching platform ZIP from a GitHub "assets" array.
    // Returns an empty string when the release has no usable asset.
    static QString chooseAssetUrl(const QJsonArray& assets, const QString& platform);

    // Where the release ZIP should be extracted. A user setting wins;
    // otherwise the parent of bin/ on Linux, or the exe folder on Windows.
    static QString installDirectory();

    void checkForUpdates();
    void downloadUpdate(const UpdateRelease& release);
    void cancelDownload();

    [[nodiscard]] bool isChecking() const { return checkReply_ != nullptr; }
    [[nodiscard]] bool isDownloading() const { return downloadReply_ != nullptr; }

signals:
    void updateAvailable(const UpdateRelease& release);
    void upToDate(const QString& version);
    void checkFailed(const QString& message);
    void downloadProgress(qint64 received, qint64 total);
    void downloadFinished(const QString& path);
    void downloadFailed(const QString& message);
    void downloadCancelled();

private slots:
    void onCheckFinished();
    void onDownloadFinished();
    void onReadyRead();
    void onDownloadProgress(qint64 received, qint64 total);

private:
    bool startUpdater(const QString& zipPath);
    void discardDownload();

    QNetworkAccessManager* manager_ = nullptr;
    QNetworkReply* checkReply_ = nullptr;
    QNetworkReply* downloadReply_ = nullptr;
    QFile* downloadFile_ = nullptr;
    UpdateRelease pendingRelease_;
    QString downloadPath_;
    bool cancelled_ = false;
};

} // namespace cvat
