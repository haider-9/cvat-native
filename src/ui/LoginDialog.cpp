#include "cvatnative/ui/LoginDialog.h"

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/AuthApi.h"
#include "cvatnative/api/UsersApi.h"
#include "cvatnative/core/Settings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace cvat {

LoginDialog::LoginDialog(ApiClient* client, QWidget* parent)
    : QDialog(parent), client_(client)
{
    setWindowTitle(QStringLiteral("Connect to CVAT"));
    resize(480, 340);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    const QUrl initialUrl = ApiClient::normalizeServerUrl(Settings::instance().serverUrl());
    const QString initialServer = initialUrl.isValid()
        ? initialUrl.toString(QUrl::StripTrailingSlash)
        : Settings::instance().serverUrl();
    serverEdit_ = new QLineEdit(initialServer, this);
    serverEdit_->setPlaceholderText(QStringLiteral("https://cvat.example.com"));
    form->addRow(QStringLiteral("Server"), serverEdit_);

    tabs_ = new QTabWidget(this);

    auto* passwordTab = new QWidget(tabs_);
    auto* passwordForm = new QFormLayout(passwordTab);
    usernameEdit_ = new QLineEdit(Settings::instance().username(), passwordTab);
    passwordEdit_ = new QLineEdit(passwordTab);
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordForm->addRow(QStringLiteral("Username"), usernameEdit_);
    passwordForm->addRow(QStringLiteral("Password"), passwordEdit_);
    tabs_->addTab(passwordTab, QStringLiteral("Password"));

    auto* tokenTab = new QWidget(tabs_);
    auto* tokenForm = new QFormLayout(tokenTab);
    tokenEdit_ = new QLineEdit(tokenTab);
    tokenEdit_->setEchoMode(QLineEdit::Password);
    tokenEdit_->setPlaceholderText(QStringLiteral("Personal access token"));
    tokenForm->addRow(QStringLiteral("Token"), tokenEdit_);
    auto* hint = new QLabel(
        QStringLiteral("Use a personal access token from CVAT → Account settings. "
                       "It is sent as an Authorization: Bearer header."),
        tokenTab);
    hint->setWordWrap(true);
    tokenForm->addRow(hint);
    tabs_->addTab(tokenTab, QStringLiteral("Access token"));
    layout->addWidget(tabs_);

    rememberBox_ = new QCheckBox(
        QStringLiteral("Remember the token in the system keyring"), this);
    rememberBox_->setChecked(true);
    layout->addWidget(rememberBox_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(QStringLiteral("color: #c44;"));
    layout->addWidget(statusLabel_);

    auto* buttons = new QDialogButtonBox(this);
    loginButton_ = buttons->addButton(QStringLiteral("Connect"), QDialogButtonBox::AcceptRole);
    cancelButton_ = buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(loginButton_, &QPushButton::clicked, this, &LoginDialog::submit);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginDialog::submit);
    connect(tokenEdit_, &QLineEdit::returnPressed, this, &LoginDialog::submit);
}

LoginDialog::~LoginDialog()
{
    if (pending_) {
        pending_->cancel();
        pending_ = nullptr;
    }
}

QString LoginDialog::serverUrl() const
{
    return serverEdit_->text().trimmed();
}

QString LoginDialog::username() const
{
    return usernameEdit_->text().trimmed();
}

bool LoginDialog::remember() const
{
    return rememberBox_->isChecked();
}

void LoginDialog::setBusy(bool busy)
{
    loginButton_->setEnabled(!busy);
    cancelButton_->setEnabled(!busy);
    serverEdit_->setEnabled(!busy);
    tabs_->setEnabled(!busy);
    rememberBox_->setEnabled(!busy);
    if (busy) {
        statusLabel_->setStyleSheet(QStringLiteral("color: #666;"));
        statusLabel_->setText(QStringLiteral("Connecting…"));
    }
}

void LoginDialog::fail(const QString& message)
{
    pending_ = nullptr;
    setBusy(false);
    statusLabel_->setStyleSheet(QStringLiteral("color: #c44;"));
    statusLabel_->setText(message);
}

void LoginDialog::submit()
{
    const QUrl normalizedUrl = ApiClient::normalizeServerUrl(serverUrl());
    if (!normalizedUrl.isValid() || normalizedUrl.host().isEmpty()) {
        fail(QStringLiteral("Enter a valid http(s) server URL (e.g. https://app.cvat.ai)."));
        return;
    }
    QUrl finalUrl = normalizedUrl;
    // The bundled CVAT Traefik route matches localhost, not its loopback IP.
    if (finalUrl.host() == QStringLiteral("127.0.0.1")) {
        finalUrl.setHost(QStringLiteral("localhost"));
    }
    serverEdit_->setText(finalUrl.toString(QUrl::StripTrailingSlash));
    client_->setBaseUrl(finalUrl);
    if (tabs_->currentIndex() == 0) {
        submitPassword();
    } else {
        submitToken();
    }
}

void LoginDialog::submitPassword()
{
    if (usernameEdit_->text().trimmed().isEmpty() || passwordEdit_->text().isEmpty()) {
        fail(QStringLiteral("Enter a username and password."));
        return;
    }
    setBusy(true);
    AuthApi auth(client_);
    QPointer<LoginDialog> guard(this);
    pending_ = auth.login(usernameEdit_->text().trimmed(), passwordEdit_->text(),
                          [guard](Result<QString> result) {
        if (!guard) {
            return;
        }
        if (!result) {
            guard->fail(result.error().toString());
            return;
        }
        guard->token_ = *result;
        guard->bearer_ = false;
        guard->pending_ = nullptr;
        guard->accept();
    });
}

void LoginDialog::submitToken()
{
    const QString token = tokenEdit_->text().trimmed();
    if (token.isEmpty()) {
        fail(QStringLiteral("Enter a personal access token."));
        return;
    }
    client_->setToken(token);
    client_->setAuthScheme(AuthScheme::Bearer);
    setBusy(true);

    UsersApi users(client_);
    QPointer<LoginDialog> guard(this);
    pending_ = users.currentUser([guard, token](Result<UserInfo> result) {
        if (!guard) {
            return;
        }
        if (!result) {
            guard->client_->setToken(QString());
            guard->fail(result.error().toString());
            return;
        }
        guard->token_ = token;
        guard->bearer_ = true;
        if (guard->usernameEdit_->text().trimmed().isEmpty()) {
            guard->usernameEdit_->setText(result->username);
        }
        guard->pending_ = nullptr;
        guard->accept();
    });
}

} // namespace cvat
