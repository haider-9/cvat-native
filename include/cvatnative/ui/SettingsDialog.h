#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
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
};

} // namespace cvat
