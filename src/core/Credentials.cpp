#include "cvatnative/core/Credentials.h"

#include "cvatnative/core/Logging.h"
#include "cvatnative/core/Settings.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace cvat {

namespace {
constexpr auto kService = "cvat-native";

bool runSecretTool(const QStringList& arguments, const QByteArray& input,
                   QByteArray* output, QString* error)
{
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("secret-tool"));
    if (executable.isEmpty()) {
        if (error) {
            *error = QStringLiteral("secret-tool is not installed");
        }
        return false;
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(3000)) {
        if (error) {
            *error = process.errorString();
        }
        return false;
    }
    if (!input.isEmpty()) {
        process.write(input);
        process.closeWriteChannel();
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        if (error) {
            *error = QStringLiteral("secret-tool timed out");
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        }
        return false;
    }
    if (output) {
        *output = process.readAllStandardOutput();
    }
    return true;
}
} // namespace

CredentialStore& CredentialStore::instance()
{
    static CredentialStore store;
    return store;
}

CredentialStore::CredentialStore() = default;

QString CredentialStore::fallbackPath() const
{
    return Settings::instance().dataDirectory() + QStringLiteral("/credentials.ini");
}

QString CredentialStore::lookupKey(const QString& server, const QString& username) const
{
    return server + QLatin1Char('\n') + username;
}

bool CredentialStore::hasSecureBackend() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("secret-tool")).isEmpty();
}

CredentialStore::Result CredentialStore::store(const QString& server, const QString& username,
                                               const QString& token)
{
    Result result;
    if (token.isEmpty()) {
        result.message = QStringLiteral("Token is empty");
        return result;
    }

    QByteArray ignored;
    QString error;
    if (runSecretTool({QStringLiteral("store"),
                       QStringLiteral("--label=CVAT Native %1").arg(server),
                       QStringLiteral("service"), QString::fromLatin1(kService),
                       QStringLiteral("server"), server,
                       QStringLiteral("username"), username},
                      token.toUtf8(), &ignored, &error)) {
        result.ok = true;
        result.secure = true;
        result.message = QStringLiteral("Token saved in the system keyring");
        return result;
    }

    qCWarning(logApp) << "Secret Service unavailable; falling back to a 0600 credential file:"
                      << error;

    const QString path = fallbackPath();
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(lookupKey(server, username), token);
    settings.sync();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    if (settings.status() != QSettings::NoError) {
        result.message = QStringLiteral("Could not write the credential file");
        return result;
    }
    result.ok = true;
    result.secure = false;
    result.message = QStringLiteral("Token saved in a 0600 fallback file; install libsecret for keyring storage");
    return result;
}

std::optional<QString> CredentialStore::load(const QString& server, const QString& username) const
{
    QByteArray output;
    QString error;
    if (runSecretTool({QStringLiteral("lookup"),
                       QStringLiteral("service"), QString::fromLatin1(kService),
                       QStringLiteral("server"), server,
                       QStringLiteral("username"), username},
                      {}, &output, &error)) {
        const QString token = QString::fromUtf8(output).trimmed();
        if (!token.isEmpty()) {
            return token;
        }
    }

    QSettings settings(fallbackPath(), QSettings::IniFormat);
    const QString token = settings.value(lookupKey(server, username)).toString();
    if (!token.isEmpty()) {
        return token;
    }
    return std::nullopt;
}

bool CredentialStore::erase(const QString& server, const QString& username)
{
    QString error;
    runSecretTool({QStringLiteral("clear"),
                   QStringLiteral("service"), QString::fromLatin1(kService),
                   QStringLiteral("server"), server,
                   QStringLiteral("username"), username},
                  {}, nullptr, &error);

    QSettings settings(fallbackPath(), QSettings::IniFormat);
    settings.remove(lookupKey(server, username));
    settings.sync();
    return true;
}

} // namespace cvat
