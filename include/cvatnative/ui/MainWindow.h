#pragma once

#include "cvatnative/api/Models.h"

#include <QMainWindow>
#include <QString>

#include <functional>
#include <memory>

class QAction;
class QCloseEvent;
class QLabel;
class QStackedWidget;

namespace cvat {

class AnnotationsApi;
class ApiClient;
class AuthApi;
class DashboardPage;
class FrameCache;
class JobWorkspace;
class JobsApi;
class LabelsApi;
class LocalStore;
class MediaApi;
class MediaService;
class ProjectsApi;
class OrganizationsApi;
class RecoveryStore;
class TasksApi;
class UsersApi;

// Owns the application session: API services, local caches, the dashboard and
// at most one open annotation workspace. Reconnecting to a different server
// tears the workspace down first so no request can outlive its credentials.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupServices();
    void setupUi();
    void applySettings();
    void startSession();
    void validateStoredToken();
    void showLogin();
    void logout();

    void openDashboard();
    void openJob(const Job& job);
    void closeWorkspace();

    void showSettings();
    void showAbout();
    void updateSessionUi();

    [[nodiscard]] QString displayUser() const;

    ApiClient* client_ = nullptr;
    std::unique_ptr<AuthApi> auth_;
    std::unique_ptr<UsersApi> usersApi_;
    std::unique_ptr<ProjectsApi> projectsApi_;
    std::unique_ptr<OrganizationsApi> organizationsApi_;
    std::unique_ptr<TasksApi> tasksApi_;
    std::unique_ptr<JobsApi> jobsApi_;
    std::unique_ptr<AnnotationsApi> annotationsApi_;
    std::unique_ptr<LabelsApi> labelsApi_;
    std::unique_ptr<MediaApi> mediaApi_;

    std::unique_ptr<LocalStore> store_;
    std::unique_ptr<RecoveryStore> recovery_;
    std::unique_ptr<FrameCache> cache_;
    MediaService* media_ = nullptr;

    QStackedWidget* stack_ = nullptr;
    DashboardPage* dashboard_ = nullptr;
    JobWorkspace* workspace_ = nullptr;
    QLabel* sessionLabel_ = nullptr;

    QAction* logoutAction_ = nullptr;
    QAction* dashboardAction_ = nullptr;

    QString username_;
    bool authenticated_ = false;
};

} // namespace cvat
