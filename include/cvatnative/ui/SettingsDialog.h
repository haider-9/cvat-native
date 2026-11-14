#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace cvat {

class ApiClient;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(ApiClient* client, QWidget* parent = nullptr);

signals:
    void settingsSaved();

private:
    void load();
    void save();
    void updateGuideButton();

    ApiClient* client_;
    QLineEdit* serverEdit_ = nullptr;
    QLineEdit* usernameEdit_ = nullptr;
    QLineEdit* tokenEdit_ = nullptr;
    QComboBox* schemeCombo_ = nullptr;
    QSpinBox* timeoutSpin_ = nullptr;
    QSpinBox* retriesSpin_ = nullptr;
    QSpinBox* memorySpin_ = nullptr;
    QSpinBox* diskSpin_ = nullptr;
    QSpinBox* autosaveSpin_ = nullptr;
    QCheckBox* remoteAutosaveBox_ = nullptr;
    QCheckBox* showDeletedBox_ = nullptr;
    QComboBox* themeCombo_ = nullptr;
    QPushButton* guideButton_ = nullptr;
    QString guideHex_;
    QFontComboBox* fontCombo_ = nullptr;
    QSpinBox* fontSizeSpin_ = nullptr;
};

} // namespace cvat
