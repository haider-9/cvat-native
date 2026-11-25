#include "cvatnative/core/UpdateChecker.h"

#include "cvatnative/core/Logging.h"
#include "cvatnative/core/Settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QVector>

#include <algorithm>

namespace cvat {

namespace {

constexpr QLatin1String kRepository{"haider-9/cvat-native"};

QByteArray userAgent()
{
    return QStringLiteral("cvat-native/%1 (Qt desktop update checker)")
        .arg(QString::fromLatin1(CVATNATIVE_VERSION))
        .toUtf8();
}

QNetworkRequest authenticatedRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setRawHeader(QByteArrayLiteral("User-Agent"), userAgent());
    request.setRawHeader(QByteArrayLiteral("Accept"),
                         QByteArrayLiteral("application/vnd.github+json"));
    // Release downloads redirect to objects.githubusercontent.com. Never follow
    // a redirect that would downgrade HTTPS to HTTP.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString errorText(QNetworkReply* reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status > 0) {
        return QStringLiteral("GitHub returned HTTP %1").arg(status);
    }
    return reply->errorString();
}

} // namespace

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), manager_(new QNetworkAccessManager(this))
{
}

UpdateChecker::~UpdateChecker()
{
    if (downloadReply_) {
        downloadReply_->abort();
    }
    discardDownload();
}

QString UpdateChecker::apiUrl()
{
    return QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(kRepository);
}

QString UpdateChecker::platformToken()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#else
    return QStringLiteral("linux");
#endif
}

int UpdateChecker::compareVersions(const QString& left, const QString& right)
{
    const auto parts = [](QString version) {
        version = version.trimmed();
        if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V'))) {
            version.remove(0, 1);
        }
        const int dash = version.indexOf(QLatin1Char('-'));
        if (dash >= 0) {
            version.truncate(dash);
        }
        const int plus = version.indexOf(QLatin1Char('+'));
        if (plus >= 0) {
            version.truncate(plus);
        }
        QVector<int> numbers;
        const QStringList segments = version.split(QLatin1Char('.'), Qt::KeepEmptyParts);
        for (const QString& segment : segments) {
            bool ok = false;
            numbers.push_back(segment.toInt(&ok));
            if (!ok) {
                numbers.back() = 0;
            }
        }
        while (numbers.size() < 3) {
            numbers.push_back(0);
        }
        return numbers;
    };

    const QVector<int> a = parts(left);
    const QVector<int> b = parts(right);
    const int count = std::max(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        const int av = i < a.size() ? a[i] : 0;
        const int bv = i < b.size() ? b[i] : 0;
        if (av != bv) {
            return av > bv ? 1 : -1;
        }
    }
    return 0;
}

QString UpdateChecker::chooseAssetUrl(const QJsonArray& assets, const QString& platform)
{
    const QString wanted = platform.toLower();
    QString bestUrl;
    int bestScore = -1;
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        const QString lower = name.toLower();
        if (url.isEmpty() || !lower.endsWith(QLatin1String(".zip")) ||
            !lower.contains(wanted)) {
            continue;
        }
        int score = 0;
        if (lower.contains(QLatin1String("x86_64")) || lower.contains(QLatin1String("amd64")) ||
            lower.contains(QLatin1String("win64"))) {
            score += 8;
        }
        if (lower.contains(QLatin1String("arm64")) || lower.contains(QLatin1String("x86_32"))) {
            score -= 8;
        }
        if (lower.contains(QLatin1String("source"))) {
            score -= 16;
        }
        if (score > bestScore) {
            bestScore = score;
            bestUrl = url;
        }
    }
    return bestUrl;
}

QString UpdateChecker::installDirectory()
{
    const QString configured = Settings::instance().updateInstallDirectory();
    if (!configured.isEmpty()) {
        return QDir::cleanPath(configured);
    }
    const QString appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
    return QDir::cleanPath(appDir);
#else
    // A source build runs from build/, a release from <root>/bin/ and a local
    // install from ~/.local/bin/. In both installed cases the files to replace
    // live one level above the executable.
    QDir dir(appDir);
    if (dir.dirName() == QStringLiteral("bin") && dir.cdUp()) {
        return dir.absolutePath();
    }
    return QDir::cleanPath(appDir);
#endif
}

void UpdateChecker::checkForUpdates()
{
    if (checkReply_ || downloadReply_) {
        return;
    }
    checkReply_ = manager_->get(authenticatedRequest(QUrl(apiUrl())));
    connect(checkReply_, &QNetworkReply::finished, this, &UpdateChecker::onCheckFinished);
    qCInfo(logApp) << "checking for updates:" << apiUrl();
}

void UpdateChecker::onCheckFinished()
{
    QNetworkReply* reply = checkReply_;
    checkReply_ = nullptr;
    if (!reply) {
        return;
    }
    const QByteArray body = reply->readAll();
    const QString failure = errorText(reply);
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool networkError = reply->error() != QNetworkReply::NoError;
    reply->deleteLater();
    if (networkError) {
        // A repository with no published releases answers 404; that is simply
        // "nothing to update", not a failed check.
        if (status == 404) {
            emit upToDate(QString::fromLatin1(CVATNATIVE_VERSION));
        } else {
            emit checkFailed(failure);
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit checkFailed(QStringLiteral("Could not read the GitHub release response"));
        return;
    }
    const QJsonObject releaseObject = document.object();
    UpdateRelease release;
    release.version = releaseObject.value(QStringLiteral("tag_name")).toString();
    release.notes = releaseObject.value(QStringLiteral("body")).toString();
    if (release.version.isEmpty()) {
        emit checkFailed(QStringLiteral("The GitHub release has no version tag"));
        return;
    }
    release.assetUrl = chooseAssetUrl(releaseObject.value(QStringLiteral("assets")).toArray(),
                                      platformToken());
    if (release.assetUrl.isEmpty()) {
        emit checkFailed(QStringLiteral("Release %1 has no %2 ZIP attached")
                             .arg(release.version, platformToken()));
        return;
    }

    const QString current = QString::fromLatin1(CVATNATIVE_VERSION);
    if (compareVersions(release.version, current) <= 0 ||
        release.version == Settings::instance().ignoredUpdateVersion()) {
        emit upToDate(release.version);
        return;
    }
    pendingRelease_ = release;
    qCInfo(logApp) << "update available:" << release.version;
    emit updateAvailable(release);
}

void UpdateChecker::downloadUpdate(const UpdateRelease& release)
{
    if (checkReply_ || downloadReply_) {
        return;
    }
    if (release.assetUrl.isEmpty()) {
        emit downloadFailed(QStringLiteral("This release has no downloadable ZIP"));
        return;
    }
    const QUrl url(release.assetUrl);
    if (!url.isValid() || url.scheme() != QLatin1String("https")) {
        emit downloadFailed(QStringLiteral("Unsafe update URL: %1").arg(release.assetUrl));
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString safeName = release.version;
    safeName.replace(QLatin1Char('/'), QLatin1Char('_'));
    downloadPath_ = QStringLiteral("%1/cvat-native-update-%2-%3.zip")
                        .arg(tempDir, safeName,
                             QString::number(QDateTime::currentSecsSinceEpoch()));

    downloadFile_ = new QFile(downloadPath_, this);
    if (!downloadFile_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(QStringLiteral("Cannot write %1").arg(downloadPath_));
        discardDownload();
        return;
    }

    cancelled_ = false;
    pendingRelease_ = release;
    downloadReply_ = manager_->get(authenticatedRequest(url));
    connect(downloadReply_, &QNetworkReply::readyRead, this, &UpdateChecker::onReadyRead);
    connect(downloadReply_, &QNetworkReply::downloadProgress, this,
            &UpdateChecker::onDownloadProgress);
    connect(downloadReply_, &QNetworkReply::finished, this, &UpdateChecker::onDownloadFinished);
    qCInfo(logApp) << "downloading update:" << release.assetName;
}

void UpdateChecker::onReadyRead()
{
    if (downloadReply_ && downloadFile_) {
        const QByteArray chunk = downloadReply_->readAll();
        if (!chunk.isEmpty() && downloadFile_->write(chunk) != chunk.size()) {
            downloadReply_->abort();
        }
    }
}

void UpdateChecker::onDownloadProgress(qint64 received, qint64 total)
{
    emit downloadProgress(received, total);
}

void UpdateChecker::onDownloadFinished()
{
    QNetworkReply* reply = downloadReply_;
    downloadReply_ = nullptr;
    if (!reply) {
        return;
    }
    const QString failure = errorText(reply);
    const bool failed = cancelled_ || reply->error() != QNetworkReply::NoError;
    reply->deleteLater();

    if (failed || !downloadFile_) {
        discardDownload();
        if (cancelled_) {
            emit downloadCancelled();
        } else {
            emit downloadFailed(QStringLiteral("Update download failed: %1").arg(failure));
        }
        return;
    }

    downloadFile_->flush();
    downloadFile_->close();
    delete downloadFile_;
    downloadFile_ = nullptr;
    const QString zipPath = downloadPath_;
    downloadPath_.clear();

    if (!startUpdater(zipPath)) {
        QFile::remove(zipPath);
        emit downloadFailed(QStringLiteral("Could not start the updater helper"));
        return;
    }
    emit downloadFinished(zipPath);
}

void UpdateChecker::cancelDownload()
{
    if (!downloadReply_) {
        return;
    }
    cancelled_ = true;
    downloadReply_->abort();
}

void UpdateChecker::discardDownload()
{
    cancelled_ = false;
    if (downloadFile_) {
        downloadFile_->close();
        delete downloadFile_;
        downloadFile_ = nullptr;
    }
    if (!downloadPath_.isEmpty()) {
        QFile::remove(downloadPath_);
        downloadPath_.clear();
    }
}

bool UpdateChecker::startUpdater(const QString& zipPath)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QString program;
    QStringList args;
#if defined(Q_OS_WIN)
    program = QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
    args << QStringLiteral("-NoProfile") << QStringLiteral("-ExecutionPolicy")
         << QStringLiteral("Bypass") << QStringLiteral("-File")
         << appDir + QStringLiteral("/updater.ps1");
#else
    program = QStandardPaths::findExecutable(QStringLiteral("bash"));
    if (program.isEmpty()) {
        program = QStringLiteral("/bin/sh");
    }
    args << appDir + QStringLiteral("/cvat-native-updater.sh");
#endif
    if (program.isEmpty() || !args.last().startsWith(appDir)) {
        qCWarning(logApp) << "updater helper is missing from" << appDir;
        return false;
    }
    if (!QFile::exists(args.last())) {
        qCWarning(logApp) << "updater helper not found:" << args.last();
        return false;
    }

    const QString installDir = installDirectory();
    args << installDir << zipPath << QString::number(QCoreApplication::applicationPid())
         << QCoreApplication::applicationFilePath();

    if (!QProcess::startDetached(program, args, installDir)) {
        qCWarning(logApp) << "failed to start" << program << args;
        return false;
    }
    qCInfo(logApp) << "handed update to" << args.first() << "in" << installDir;
    return true;
}

} // namespace cvat
