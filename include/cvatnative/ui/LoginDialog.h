#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTabWidget;

namespace cvat {

class ApiClient;
class RequestHandle;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(ApiClient* client, QWidget* parent = nullptr);
    ~LoginDialog() override;

    [[nodiscard]] QString serverUrl() const;
    [[nodiscard]] QString username() const;
    [[nodiscard]] QString token() const { return token_; }
    [[nodiscard]] bool usesBearer() const { return bearer_; }
    [[nodiscard]] bool remember() const;

private:
    void submit();
    void submitPassword();
    void submitToken();
    void setBusy(bool busy);
    void fail(const QString& message);

    ApiClient* client_;
    QLineEdit* serverEdit_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QLineEdit* usernameEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QLineEdit* tokenEdit_ = nullptr;
    QCheckBox* rememberBox_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    RequestHandle* pending_ = nullptr;
    QString token_;
    bool bearer_ = false;
};

} // namespace cvat
