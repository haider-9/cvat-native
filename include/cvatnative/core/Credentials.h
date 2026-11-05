#pragma once

#include <QString>

#include <optional>

namespace cvat {

// Stores the CVAT API token outside the Qt settings file.
// Preferred backend: libsecret (secret-tool), which works with GNOME Keyring
// and KWallet's Secret Service bridge on Arch/niri. If no Secret Service is
// available, a 0600 file is used and the caller is told it is a fallback.
class CredentialStore {
public:
    struct Result {
        bool ok = false;
        bool secure = false;
        QString message;
    };

    static CredentialStore& instance();

    Result store(const QString& server, const QString& username, const QString& token);
    std::optional<QString> load(const QString& server, const QString& username) const;
    bool erase(const QString& server, const QString& username);

    [[nodiscard]] bool hasSecureBackend() const;

private:
    CredentialStore();
    QString fallbackPath() const;
    QString lookupKey(const QString& server, const QString& username) const;
};

} // namespace cvat
