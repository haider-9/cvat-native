#pragma once

#include "cvatnative/api/Models.h"

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTimer;

namespace cvat {

class ProjectsApi;
class TasksApi;
class JobsApi;
class OrganizationsApi;
class LocalStore;

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    DashboardPage(ProjectsApi* projects, TasksApi* tasks, JobsApi* jobs,
                  OrganizationsApi* organizations,
                  LocalStore* store, QWidget* parent = nullptr);

    void refresh();

signals:
    void jobActivated(const Job& job);

private:
    void refreshCurrent();
    void refreshProjects(int page);
    void refreshTasks(int page);
    void refreshJobs(int page);
    void updatePager();

    void switchTab(int index);

    [[nodiscard]] QTableWidget* currentTable() const;
    [[nodiscard]] int currentPage() const;
    void setCurrentPage(int page);
    void saveCache(const QString& key, const QJsonArray& array);
    [[nodiscard]] QJsonArray loadCache(const QString& key) const;
    void showError(const QString& message);

    ProjectsApi* projectsApi_;
    TasksApi* tasksApi_;
    JobsApi* jobsApi_;
    OrganizationsApi* organizationsApi_;
    LocalStore* store_;

    QTabWidget* tabs_ = nullptr;
    QTableWidget* projectTable_ = nullptr;
    QTableWidget* taskTable_ = nullptr;
    QTableWidget* jobTable_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* organizationCombo_ = nullptr;
    QPushButton* prevButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QLabel* pageLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QTimer* searchTimer_ = nullptr;

    QVector<Project> projects_;
    QVector<Task> tasks_;
    QVector<Job> jobs_;
    int projectPage_ = 1;
    int taskPage_ = 1;
    int jobPage_ = 1;
    PageMeta projectMeta_;
    PageMeta taskMeta_;
    PageMeta jobMeta_;
    int taskFilterProject_ = 0;
    int jobFilterTask_ = 0;
    int jobFilterProject_ = 0;
    bool loading_ = false;
    bool organizationsLoaded_ = false;
};

} // namespace cvat
