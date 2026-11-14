#include "cvatnative/core/Credentials.h"

#include "cvatnative/core/Logging.h"
#include "cvatnative/core/Settings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#include <string>
#endif

namespace cvat {

namespace {
#ifdef Q_OS_WIN
QString windowsCredentialTarget(const QString& server, const QString& username)
{
    const QByteArray key = server.toUtf8() + '\n' + username.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex();
    return QStringLiteral("cvat-native:%1").arg(QString::fromLatin1(digest));
}

bool storeWindowsCredential(const QString& server, const QString& username,
                            const QString& token, QString* error)
{
    const QByteArray tokenBytes = token.toUtf8();
    if (tokenBytes.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        if (error) {
            *error = QStringLiteral("Token exceeds the Windows Credential Manager size limit");
        }
        return false;
    }

    const std::wstring target = windowsCredentialTarget(server, username).toStdWString();
    const std::wstring usernameWide = username.toStdWString();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.UserName = const_cast<LPWSTR>(usernameWide.c_str());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.CredentialBlobSize = static_cast<DWORD>(tokenBytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(
        const_cast<char*>(tokenBytes.constData()));

    if (!CredWriteW(&credential, 0)) {
        if (error) {
            *error = QStringLiteral("Windows Credential Manager error %1").arg(GetLastError());
        }
        return false;
    }
    return true;
}
#else
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
#endif
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
#ifdef Q_OS_WIN
    return true;
#else
    return !QStandardPaths::findExecutable(QStringLiteral("secret-tool")).isEmpty();
#endif
}

CredentialStore::Result CredentialStore::store(const QString& server, const QString& username,
                                               const QString& token)
{
    Result result;
    if (token.isEmpty()) {
        result.message = QStringLiteral("Token is empty");
        return result;
    }

    QString error;
#ifdef Q_OS_WIN
    if (storeWindowsCredential(server, username, token, &error)) {
        result.ok = true;
        result.secure = true;
        result.message = QStringLiteral("Token saved in Windows Credential Manager");
        return result;
    }
#else
    QByteArray ignored;
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
#endif

#ifdef Q_OS_WIN
    qCWarning(logApp) << "Windows Credential Manager unavailable; falling back to a credential file:"
                      << error;
#else
    qCWarning(logApp) << "Secret Service unavailable; falling back to a 0600 credential file:"
                      << error;
#endif

    const QString path = fallbackPath();
    // Make the directory owner-only before QSettings writes the token. QSettings
    // replaces the file on save, so the file's own permissions can be briefly
    // looser; with an owner-only directory no other user can reach it.
    const QString dir = QFileInfo(path).absolutePath();
    QDir().mkpath(dir);
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
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
#ifdef Q_OS_WIN
    result.message = QStringLiteral("Token saved in an unencrypted fallback file; Windows Credential Manager was unavailable");
#else
    result.message = QStringLiteral("Token saved in a 0600 fallback file; install libsecret for keyring storage");
#endif
    return result;
}

std::optional<QString> CredentialStore::load(const QString& server, const QString& username) const
{
#ifdef Q_OS_WIN
    const std::wstring target = windowsCredentialTarget(server, username).toStdWString();
    PCREDENTIALW credential = nullptr;
    if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        const QByteArray tokenBytes(reinterpret_cast<const char*>(credential->CredentialBlob),
                                    static_cast<qsizetype>(credential->CredentialBlobSize));
        CredFree(credential);
        const QString token = QString::fromUtf8(tokenBytes);
        if (!token.isEmpty()) {
            return token;
        }
    }
#else
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
#endif

    QSettings settings(fallbackPath(), QSettings::IniFormat);
    const QString token = settings.value(lookupKey(server, username)).toString();
    if (!token.isEmpty()) {
        return token;
    }
    return std::nullopt;
}

bool CredentialStore::erase(const QString& server, const QString& username)
{
#ifdef Q_OS_WIN
    const std::wstring target = windowsCredentialTarget(server, username).toStdWString();
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
#else
    QString error;
    runSecretTool({QStringLiteral("clear"),
                   QStringLiteral("service"), QString::fromLatin1(kService),
                   QStringLiteral("server"), server,
                   QStringLiteral("username"), username},
                  {}, nullptr, &error);
#endif

    QSettings settings(fallbackPath(), QSettings::IniFormat);
    settings.remove(lookupKey(server, username));
    settings.sync();
    return true;
}

} // namespace cvat
