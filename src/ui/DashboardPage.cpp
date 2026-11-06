#include "cvatnative/ui/DashboardPage.h"

#include "cvatnative/api/JobsApi.h"
#include "cvatnative/api/OrganizationsApi.h"
#include "cvatnative/api/ProjectsApi.h"
#include "cvatnative/api/TasksApi.h"
#include "cvatnative/storage/LocalStore.h"

#include <QDateTime>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

namespace cvat {

namespace {
constexpr int kPageSize = 50;

QTableWidget* makeTable(const QStringList& headers, QWidget* parent)
{
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(false);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setSortingEnabled(false);
    return table;
}

QString dateText(const QDateTime& date)
{
    if (!date.isValid()) {
        return {};
    }
    return date.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QTableWidgetItem* item(const QString& text, const QVariant& id = {})
{
    auto* result = new QTableWidgetItem(text);
    if (id.isValid()) {
        result->setData(Qt::UserRole, id);
    }
    return result;
}
} // namespace

DashboardPage::DashboardPage(ProjectsApi* projects, TasksApi* tasks, JobsApi* jobs,
                             OrganizationsApi* organizations, LocalStore* store,
                             QWidget* parent)
    : QWidget(parent),
      projectsApi_(projects),
      tasksApi_(tasks),
      jobsApi_(jobs),
    organizationsApi_(organizations),
      store_(store)
{
    auto* layout = new QVBoxLayout(this);
    auto* toolbar = new QHBoxLayout;

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QStringLiteral("Search jobs…"));
    searchEdit_->setClearButtonEnabled(true);
    toolbar->addWidget(searchEdit_, 1);

    organizationCombo_ = new QComboBox(this);
    organizationCombo_->addItem(QStringLiteral("All organizations"), QString());
    organizationCombo_->addItem(QStringLiteral("Personal workspace"), QStringLiteral(""));
    organizationCombo_->setMinimumWidth(180);
    organizationCombo_->setToolTip(QStringLiteral("Organization context"));
    toolbar->addWidget(organizationCombo_);

    auto* refreshButton = new QPushButton(QStringLiteral("Refresh"), this);
    toolbar->addWidget(refreshButton);
    layout->addLayout(toolbar);

    tabs_ = new QTabWidget(this);
    projectTable_ = makeTable({QStringLiteral("Name"), QStringLiteral("Status"),
                               QStringLiteral("Dimension"), QStringLiteral("Tasks"),
                               QStringLiteral("Updated")}, tabs_);
    projectTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    projectTable_->setColumnWidth(1, 100);
    projectTable_->setColumnWidth(2, 90);
    projectTable_->setColumnWidth(3, 60);
    projectTable_->setColumnWidth(4, 140);

    taskTable_ = makeTable({QStringLiteral("Name"), QStringLiteral("Project"),
                            QStringLiteral("Status"), QStringLiteral("Frames"),
                            QStringLiteral("Updated")}, tabs_);
    taskTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    taskTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    taskTable_->setColumnWidth(2, 100);
    taskTable_->setColumnWidth(3, 65);
    taskTable_->setColumnWidth(4, 140);

    jobTable_ = makeTable({QStringLiteral("Job"), QStringLiteral("Task"),
                           QStringLiteral("Status"), QStringLiteral("Stage"),
                           QStringLiteral("State"), QStringLiteral("Frames")}, tabs_);
    jobTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    jobTable_->setColumnWidth(0, 80);
    jobTable_->setColumnWidth(2, 100);
    jobTable_->setColumnWidth(3, 90);
    jobTable_->setColumnWidth(4, 90);
    jobTable_->setColumnWidth(5, 100);
    tabs_->addTab(projectTable_, QStringLiteral("Projects"));
    tabs_->addTab(taskTable_, QStringLiteral("Tasks"));
    tabs_->addTab(jobTable_, QStringLiteral("Jobs"));
    tabs_->setCurrentIndex(2);
    layout->addWidget(tabs_, 1);

    auto* footer = new QHBoxLayout;
    prevButton_ = new QPushButton(QStringLiteral("Previous"), this);
    nextButton_ = new QPushButton(QStringLiteral("Next"), this);
    pageLabel_ = new QLabel(this);
    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #888;"));
    footer->addWidget(prevButton_);
    footer->addWidget(nextButton_);
    footer->addWidget(pageLabel_);
    footer->addStretch();
    footer->addWidget(statusLabel_, 1);
    layout->addLayout(footer);

    searchTimer_ = new QTimer(this);
    searchTimer_->setSingleShot(true);
    searchTimer_->setInterval(350);
    connect(searchTimer_, &QTimer::timeout, this, &DashboardPage::refreshCurrent);
    connect(searchEdit_, &QLineEdit::textChanged, this, [this]() { searchTimer_->start(); });
    connect(refreshButton, &QPushButton::clicked, this, &DashboardPage::refresh);
    connect(organizationCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!organizationsApi_ || index < 0) {
            return;
        }
        organizationsApi_->setOrganization(organizationCombo_->itemData(index).toString());
        projectPage_ = taskPage_ = jobPage_ = 1;
        refreshCurrent();
    });
    connect(prevButton_, &QPushButton::clicked, this, [this]() {
        if (currentPage() > 1) {
            setCurrentPage(currentPage() - 1);
            refreshCurrent();
        }
    });
    connect(nextButton_, &QPushButton::clicked, this, [this]() {
        setCurrentPage(currentPage() + 1);
        refreshCurrent();
    });
    connect(tabs_, &QTabWidget::currentChanged, this, &DashboardPage::switchTab);
    connect(projectTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto* cell = projectTable_->item(row, 0);
        if (!cell) {
            return;
        }
        taskFilterProject_ = cell->data(Qt::UserRole).toInt();
        jobFilterTask_ = 0;
        jobFilterProject_ = taskFilterProject_;
        tabs_->setCurrentIndex(1);
        taskPage_ = 1;
        jobPage_ = 1;
        refreshTasks(1);
    });
    connect(taskTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto* cell = taskTable_->item(row, 0);
        if (!cell) {
            return;
        }
        jobFilterTask_ = cell->data(Qt::UserRole).toInt();
        jobFilterProject_ = 0;
        tabs_->setCurrentIndex(2);
        jobPage_ = 1;
        refreshJobs(1);
    });
    connect(jobTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row >= 0 && row < jobs_.size()) {
            emit jobActivated(jobs_[row]);
        }
    });

    refresh();
}

void DashboardPage::refresh()
{
    if (organizationsApi_ && !organizationsLoaded_) {
        organizationsApi_->list([this](Result<QVector<Organization>> result) {
            if (result) {
                QSignalBlocker blocker(organizationCombo_);
                for (const auto& organization : *result) {
                    organizationCombo_->addItem(
                        organization.name.isEmpty() ? organization.slug : organization.name,
                        organization.slug);
                }
                organizationsLoaded_ = true;
            }
            refreshCurrent();
        });
        return;
    }
    refreshCurrent();
}

void DashboardPage::switchTab(int index)
{
    if (index == 0) {
        searchEdit_->setPlaceholderText(QStringLiteral("Search projects…"));
    } else if (index == 1) {
        searchEdit_->setPlaceholderText(QStringLiteral("Search tasks…"));
    } else {
        searchEdit_->setPlaceholderText(QStringLiteral("Search jobs…"));
    }
    refreshCurrent();
}

QTableWidget* DashboardPage::currentTable() const
{
    switch (tabs_->currentIndex()) {
    case 1: return taskTable_;
    case 2: return jobTable_;
    default: return projectTable_;
    }
}

int DashboardPage::currentPage() const
{
    switch (tabs_->currentIndex()) {
    case 1: return taskPage_;
    case 2: return jobPage_;
    default: return projectPage_;
    }
}

void DashboardPage::setCurrentPage(int page)
{
    switch (tabs_->currentIndex()) {
    case 1: taskPage_ = page; break;
    case 2: jobPage_ = page; break;
    default: projectPage_ = page; break;
    }
}

void DashboardPage::refreshCurrent()
{
    if (loading_) {
        return;
    }
    switch (tabs_->currentIndex()) {
    case 1: refreshTasks(taskPage_); break;
    case 2: refreshJobs(jobPage_); break;
    default: refreshProjects(projectPage_); break;
    }
}

void DashboardPage::saveCache(const QString& key, const QJsonArray& array)
{
    if (!store_) {
        return;
    }
    QJsonObject object;
    object.insert(QStringLiteral("results"), array);
    object.insert(QStringLiteral("saved_at"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    store_->saveJson(key, object);
}

QJsonArray DashboardPage::loadCache(const QString& key) const
{
    if (!store_) {
        return {};
    }
    const auto object = store_->loadJson(key);
    return object ? object->value(QStringLiteral("results")).toArray() : QJsonArray();
}

void DashboardPage::showError(const QString& message)
{
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(QStringLiteral("color: #c44;"));
}

void DashboardPage::refreshProjects(int page)
{
    loading_ = true;
    projectPage_ = page;
    const QString search = searchEdit_->text().trimmed();
    projectsApi_->listAll(search, QStringLiteral("-updated_date"),
                       [this, page](Result<QVector<Project>> result) {
        loading_ = false;
        if (!result) {
            showError(result.error().toString());
            if (page == 1) {
                const QJsonArray cached = loadCache(QStringLiteral("dashboard/projects"));
                if (!cached.isEmpty()) {
                    projectTable_->setRowCount(0);
                    projects_.clear();
                    for (const auto& value : cached) {
                        projects_.push_back(Project::fromJson(value.toObject()));
                    }
                    for (const auto& project : projects_) {
                        const int row = projectTable_->rowCount();
                        projectTable_->insertRow(row);
                        projectTable_->setItem(row, 0, item(project.name, project.id));
                        projectTable_->setItem(row, 1, item(project.status));
                        projectTable_->setItem(row, 2, item(project.dimension));
                        projectTable_->setItem(row, 3, item(QString::number(project.taskCount)));
                        projectTable_->setItem(row, 4, item(dateText(project.updatedDate)));
                    }
                    statusLabel_->setText(QStringLiteral("Showing cached projects (offline)"));
                }
            }
            return;
        }
        projects_ = *result;
        projectMeta_.count = projects_.size();
        projectMeta_.next.clear();
        projectMeta_.previous.clear();
        projectTable_->setRowCount(0);
        QJsonArray cache;
        for (const auto& project : projects_) {
            cache.push_back(project.raw);
            const int row = projectTable_->rowCount();
            projectTable_->insertRow(row);
            projectTable_->setItem(row, 0, item(project.name, project.id));
            projectTable_->setItem(row, 1, item(project.status));
            projectTable_->setItem(row, 2, item(project.dimension));
            projectTable_->setItem(row, 3, item(QString::number(project.taskCount)));
            projectTable_->setItem(row, 4, item(dateText(project.updatedDate)));
        }
        if (page == 1) {
            saveCache(QStringLiteral("dashboard/projects"), cache);
        }
        statusLabel_->setStyleSheet(QStringLiteral("color: #888;"));
        statusLabel_->setText(QStringLiteral("%1 projects").arg(projects_.size()));
        updatePager();
    });
}

void DashboardPage::refreshTasks(int page)
{
    loading_ = true;
    taskPage_ = page;
    const QString search = searchEdit_->text().trimmed();
    tasksApi_->list(page, kPageSize, search, taskFilterProject_, QString(),
                    QStringLiteral("-updated_date"),
                    [this, page](Result<Page<Task>> result) {
        loading_ = false;
        if (!result) {
            showError(result.error().toString());
            if (page == 1) {
                const QJsonArray cached = loadCache(QStringLiteral("dashboard/tasks"));
                taskTable_->setRowCount(0);
                tasks_.clear();
                for (const auto& value : cached) {
                    tasks_.push_back(Task::fromJson(value.toObject()));
                }
                for (const auto& task : tasks_) {
                    const int row = taskTable_->rowCount();
                    taskTable_->insertRow(row);
                    taskTable_->setItem(row, 0, item(task.name, task.id));
                    taskTable_->setItem(row, 1, item(task.projectName));
                    taskTable_->setItem(row, 2, item(task.status));
                    taskTable_->setItem(row, 3, item(QString::number(task.size)));
                    taskTable_->setItem(row, 4, item(dateText(task.updatedDate)));
                }
                if (!cached.isEmpty()) {
                    statusLabel_->setText(QStringLiteral("Showing cached tasks (offline)"));
                }
            }
            return;
        }
        tasks_ = result->results;
        taskMeta_ = result->meta;
        taskTable_->setRowCount(0);
        QJsonArray cache;
        for (const auto& task : tasks_) {
            cache.push_back(task.raw);
            const int row = taskTable_->rowCount();
            taskTable_->insertRow(row);
            taskTable_->setItem(row, 0, item(task.name, task.id));
            taskTable_->setItem(row, 1, item(task.projectName));
            taskTable_->setItem(row, 2, item(task.status));
            taskTable_->setItem(row, 3, item(QString::number(task.size)));
            taskTable_->setItem(row, 4, item(dateText(task.updatedDate)));
        }
        if (page == 1) {
            saveCache(QStringLiteral("dashboard/tasks"), cache);
        }
        statusLabel_->setStyleSheet(QStringLiteral("color: #888;"));
        statusLabel_->setText(QStringLiteral("%1 tasks").arg(result->meta.count));
        updatePager();
    });
}

void DashboardPage::refreshJobs(int page)
{
    loading_ = true;
    jobPage_ = page;
    const QString search = searchEdit_->text().trimmed();
    jobsApi_->list(page, kPageSize, search, jobFilterTask_, jobFilterProject_, QString(),
                   QString(), QStringLiteral("-updated_date"),
                   [this, page](Result<Page<Job>> result) {
        loading_ = false;
        if (!result) {
            showError(result.error().toString());
            if (page == 1) {
                const QJsonArray cached = loadCache(QStringLiteral("dashboard/jobs"));
                jobTable_->setRowCount(0);
                jobs_.clear();
                for (const auto& value : cached) {
                    jobs_.push_back(Job::fromJson(value.toObject()));
                }
                for (const auto& job : jobs_) {
                    const int row = jobTable_->rowCount();
                    jobTable_->insertRow(row);
                    jobTable_->setItem(row, 0, item(QString::number(job.id), job.id));
                    jobTable_->setItem(row, 1, item(job.taskName));
                    jobTable_->setItem(row, 2, item(job.status));
                    jobTable_->setItem(row, 3, item(job.stage));
                    jobTable_->setItem(row, 4, item(job.state));
                    jobTable_->setItem(row, 5, item(QStringLiteral("%1–%2")
                                                       .arg(job.startFrame)
                                                       .arg(job.stopFrame)));
                }
                if (!cached.isEmpty()) {
                    statusLabel_->setText(QStringLiteral("Showing cached jobs (offline)"));
                }
            }
            return;
        }
        jobs_ = result->results;
        jobMeta_ = result->meta;
        jobTable_->setRowCount(0);
        QJsonArray cache;
        for (const auto& job : jobs_) {
            cache.push_back(job.raw);
            const int row = jobTable_->rowCount();
            jobTable_->insertRow(row);
            jobTable_->setItem(row, 0, item(QString::number(job.id), job.id));
            jobTable_->setItem(row, 1, item(job.taskName));
            jobTable_->setItem(row, 2, item(job.status));
            jobTable_->setItem(row, 3, item(job.stage));
            jobTable_->setItem(row, 4, item(job.state));
            jobTable_->setItem(row, 5, item(QStringLiteral("%1–%2")
                                               .arg(job.startFrame)
                                               .arg(job.stopFrame)));
        }
        if (page == 1) {
            saveCache(QStringLiteral("dashboard/jobs"), cache);
        }
        statusLabel_->setStyleSheet(QStringLiteral("color: #888;"));
        statusLabel_->setText(QStringLiteral("%1 jobs").arg(result->meta.count));
        updatePager();
    });
}

void DashboardPage::updatePager()
{
    PageMeta meta;
    switch (tabs_->currentIndex()) {
    case 1: meta = taskMeta_; break;
    case 2: meta = jobMeta_; break;
    default: meta = projectMeta_; break;
    }
    const int pages = std::max(1, (meta.count + kPageSize - 1) / kPageSize);
    const int page = currentPage();
    pageLabel_->setText(QStringLiteral("Page %1 of %2").arg(page).arg(pages));
    prevButton_->setEnabled(page > 1);
    nextButton_->setEnabled(page < pages);
}

} // namespace cvat
