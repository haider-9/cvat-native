#include "cvatnative/ui/SettingsDialog.h"

#include "cvatnative/api/ApiClient.h"
#include "cvatnative/core/Credentials.h"
#include "cvatnative/core/Settings.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QApplication>
#include <QFont>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace cvat {

SettingsDialog::SettingsDialog(ApiClient* client, QWidget* parent)
    : QDialog(parent), client_(client)
{
    setWindowTitle(QStringLiteral("Settings"));
    resize(520, 580);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    serverEdit_ = new QLineEdit(this);
    serverEdit_->setPlaceholderText(QStringLiteral("https://cvat.example.com"));
    form->addRow(QStringLiteral("CVAT server"), serverEdit_);

    usernameEdit_ = new QLineEdit(this);
    form->addRow(QStringLiteral("Username"), usernameEdit_);

    tokenEdit_ = new QLineEdit(this);
    tokenEdit_->setEchoMode(QLineEdit::Password);
    tokenEdit_->setPlaceholderText(QStringLiteral("Leave empty to keep the stored token"));
    form->addRow(QStringLiteral("API token"), tokenEdit_);

    schemeCombo_ = new QComboBox(this);
    schemeCombo_->addItem(QStringLiteral("Token (login key)"), QStringLiteral("token"));
    schemeCombo_->addItem(QStringLiteral("Bearer (personal access token)"), QStringLiteral("bearer"));
    form->addRow(QStringLiteral("Authorization"), schemeCombo_);

    timeoutSpin_ = new QSpinBox(this);
    timeoutSpin_->setRange(1000, 600000);
    timeoutSpin_->setSuffix(QStringLiteral(" ms"));
    timeoutSpin_->setSingleStep(1000);
    form->addRow(QStringLiteral("Request timeout"), timeoutSpin_);

    retriesSpin_ = new QSpinBox(this);
    retriesSpin_->setRange(0, 10);
    form->addRow(QStringLiteral("Network retries"), retriesSpin_);

    memorySpin_ = new QSpinBox(this);
    memorySpin_->setRange(32, 8192);
    memorySpin_->setSuffix(QStringLiteral(" MiB"));
    form->addRow(QStringLiteral("Frame memory cache"), memorySpin_);

    diskSpin_ = new QSpinBox(this);
    diskSpin_->setRange(128, 102400);
    diskSpin_->setSuffix(QStringLiteral(" MiB"));
    form->addRow(QStringLiteral("Frame disk cache"), diskSpin_);

    autosaveSpin_ = new QSpinBox(this);
    autosaveSpin_->setRange(5, 3600);
    autosaveSpin_->setSuffix(QStringLiteral(" s"));
    form->addRow(QStringLiteral("Local autosave interval"), autosaveSpin_);

    remoteAutosaveBox_ = new QCheckBox(
        QStringLiteral("Also save changes to the server automatically"), this);
    form->addRow(QString(), remoteAutosaveBox_);

    showDeletedBox_ = new QCheckBox(QStringLiteral("Show deleted frames"), this);
    form->addRow(QString(), showDeletedBox_);

    auto addAutoDelaySpin = [this]() {
        auto* spin = new QSpinBox(this);
        spin->setRange(0, 10000);
        spin->setSingleStep(50);
        spin->setSuffix(QStringLiteral(" ms"));
        spin->setToolTip(QStringLiteral(
            "Wait this long after a shape is created before sending the shortcut"));
        return spin;
    };

    autoDrawBox_ = new QCheckBox(
        QStringLiteral("Auto draw: send N after a shape is created"), this);
    autoDrawBox_->setToolTip(QStringLiteral(
        "Repeats draw mode automatically so you can draw several boxes in a row"));
    form->addRow(QStringLiteral("Auto draw"), autoDrawBox_);
    autoDrawDelaySpin_ = addAutoDelaySpin();
    form->addRow(QStringLiteral("Auto draw delay"), autoDrawDelaySpin_);
    connect(autoDrawBox_, &QCheckBox::toggled, autoDrawDelaySpin_, &QSpinBox::setEnabled);

    autoPasteBox_ = new QCheckBox(
        QStringLiteral("Auto paste: send V after a shape is created"), this);
    autoPasteBox_->setToolTip(QStringLiteral(
        "Repeats label placement automatically so you can paste several copies in a row"));
    form->addRow(QStringLiteral("Auto paste"), autoPasteBox_);
    autoPasteDelaySpin_ = addAutoDelaySpin();
    form->addRow(QStringLiteral("Auto paste delay"), autoPasteDelaySpin_);
    connect(autoPasteBox_, &QCheckBox::toggled, autoPasteDelaySpin_, &QSpinBox::setEnabled);

    themeCombo_ = new QComboBox(this);
    themeCombo_->addItem(QStringLiteral("System"), QStringLiteral("system"));
    themeCombo_->addItem(QStringLiteral("Light"), QStringLiteral("light"));
    themeCombo_->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    form->addRow(QStringLiteral("Theme"), themeCombo_);

    guideButton_ = new QPushButton(this);
    guideButton_->setToolTip(QStringLiteral("Colour of the crosshair guides shown while drawing"));
    connect(guideButton_, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(QColor(guideHex_), this,
                                                     QStringLiteral("Guide colour"));
        if (picked.isValid()) {
            guideHex_ = picked.name();
            updateGuideButton();
        }
    });
    form->addRow(QStringLiteral("Guide colour"), guideButton_);

    fontCombo_ = new QFontComboBox(this);
    form->addRow(QStringLiteral("Interface font"), fontCombo_);

    fontSizeSpin_ = new QSpinBox(this);
    fontSizeSpin_->setRange(0, 24);
    fontSizeSpin_->setSpecialValueText(QStringLiteral("System default"));
    fontSizeSpin_->setSuffix(QStringLiteral(" pt"));
    form->addRow(QStringLiteral("Interface font size"), fontSizeSpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() { save(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    load();
}

void SettingsDialog::load()
{
    auto& settings = Settings::instance();
    const QUrl initialUrl = ApiClient::normalizeServerUrl(settings.serverUrl());
    const QString initialServer = initialUrl.isValid()
        ? initialUrl.toString(QUrl::StripTrailingSlash)
        : settings.serverUrl();
    serverEdit_->setText(initialServer);
    usernameEdit_->setText(settings.username());
    schemeCombo_->setCurrentIndex(settings.authScheme() == QStringLiteral("bearer") ? 1 : 0);
    timeoutSpin_->setValue(settings.requestTimeoutMs());
    retriesSpin_->setValue(settings.maxRetries());
    memorySpin_->setValue(settings.memoryCacheMiB());
    diskSpin_->setValue(settings.diskCacheMiB());
    autosaveSpin_->setValue(settings.autosaveSeconds());
    remoteAutosaveBox_->setChecked(settings.remoteAutosave());
    showDeletedBox_->setChecked(settings.showDeletedFrames());
    autoDrawBox_->setChecked(settings.autoDrawAfterCreate());
    autoDrawDelaySpin_->setValue(settings.autoDrawDelayMs());
    autoPasteBox_->setChecked(settings.autoPasteAfterCreate());
    autoPasteDelaySpin_->setValue(settings.autoPasteDelayMs());

    themeCombo_->setCurrentIndex(std::max(0, themeCombo_->findData(settings.themeMode())));
    guideHex_ = settings.guideColor();
    updateGuideButton();
    fontCombo_->setCurrentFont(QFont(settings.fontFamily().isEmpty()
                                         ? QApplication::font().family()
                                         : settings.fontFamily()));
    fontSizeSpin_->setValue(settings.fontPointSize());
}

void SettingsDialog::updateGuideButton()
{
    const QColor colour(guideHex_);
    guideButton_->setText(guideHex_);
    guideButton_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                    .arg(guideHex_, colour.lightness() > 128
                                                        ? QStringLiteral("#000")
                                                        : QStringLiteral("#fff")));
}

void SettingsDialog::save()
{
    const QUrl normalizedUrl = ApiClient::normalizeServerUrl(serverEdit_->text().trimmed());
    if (!normalizedUrl.isValid() || normalizedUrl.host().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid server"),
                             QStringLiteral("Enter a valid http(s) URL (e.g. https://app.cvat.ai)."));
        return;
    }

    QUrl finalUrl = normalizedUrl;
    if (finalUrl.host() == QStringLiteral("127.0.0.1")) {
        finalUrl.setHost(QStringLiteral("localhost"));
    }
    const QString host = finalUrl.host().toLower();
    const bool loopback = host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1");
    if (finalUrl.scheme() == QLatin1String("http") && !loopback) {
        const auto answer = QMessageBox::warning(
            this, QStringLiteral("Unencrypted connection"),
            QStringLiteral("This server uses plain HTTP, so your API token and annotations "
                           "would be sent unencrypted. Use https:// unless the network is trusted.\n\n"
                           "Save anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    const QString serverString = finalUrl.toString(QUrl::StripTrailingSlash);
    serverEdit_->setText(serverString);

    auto& settings = Settings::instance();
    settings.setServerUrl(serverString);
    settings.setUsername(usernameEdit_->text().trimmed());
    const QString scheme = schemeCombo_->currentData().toString();
    settings.setAuthScheme(scheme);
    settings.setRequestTimeoutMs(timeoutSpin_->value());
    settings.setMaxRetries(retriesSpin_->value());
    settings.setMemoryCacheMiB(memorySpin_->value());
    settings.setDiskCacheMiB(diskSpin_->value());
    settings.setAutosaveSeconds(autosaveSpin_->value());
    settings.setRemoteAutosave(remoteAutosaveBox_->isChecked());
    settings.setShowDeletedFrames(showDeletedBox_->isChecked());
    settings.setAutoDrawAfterCreate(autoDrawBox_->isChecked());
    settings.setAutoDrawDelayMs(autoDrawDelaySpin_->value());
    settings.setAutoPasteAfterCreate(autoPasteBox_->isChecked());
    settings.setAutoPasteDelayMs(autoPasteDelaySpin_->value());
    settings.setThemeMode(themeCombo_->currentData().toString());
    settings.setGuideColor(guideHex_);
    settings.setFontFamily(fontCombo_->currentFont().family());
    settings.setFontPointSize(fontSizeSpin_->value());
    settings.sync();

    if (client_) {
        client_->setBaseUrl(finalUrl);
        client_->setTimeoutMs(timeoutSpin_->value());
        client_->setMaxRetries(retriesSpin_->value());
        client_->setAuthScheme(scheme == QStringLiteral("bearer") ? AuthScheme::Bearer
                                                                 : AuthScheme::Token);
    }

    if (!tokenEdit_->text().isEmpty()) {
        const auto result = CredentialStore::instance().store(
            settings.serverUrl(), settings.username(), tokenEdit_->text());
        if (!result.ok) {
            QMessageBox::warning(this, QStringLiteral("Token"),
                                 QStringLiteral("The token could not be saved: ") + result.message);
        } else if (client_) {
            client_->setToken(tokenEdit_->text());
        }
    }

    emit settingsSaved();
    accept();
}

} // namespace cvat
