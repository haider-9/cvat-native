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

QString timeAgo(const QDateTime& when)
{
    if (!when.isValid()) {
        return {};
    }
    const qint64 seconds = when.secsTo(QDateTime::currentDateTime());
    if (seconds < 60) {
        return QStringLiteral("just now");
    }
    if (seconds < 3600) {
        return QStringLiteral("%1 min ago").arg(seconds / 60);
    }
    if (seconds < 86400) {
        return QStringLiteral("%1 h ago").arg(seconds / 3600);
    }
    if (seconds < 7 * 86400) {
        return QStringLiteral("%1 d ago").arg(seconds / 86400);
    }
    return dateText(when);
}

QTableWidgetItem* item(const QString& text, const QVariant& id = {})
{
    auto* result = new QTableWidgetItem(text);
    result->setToolTip(text);
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

    // Landing page: a local history of everything the user has opened. It is
    // first in the tab bar and is the page shown when the dashboard opens.
    homePage_ = new QWidget(tabs_);
    auto* homeLayout = new QVBoxLayout(homePage_);
    homeLayout->setContentsMargins(12, 12, 12, 12);
    homeLayout->setSpacing(8);
    auto* welcome = new QLabel(QStringLiteral(
        "<h2>Welcome back</h2>"
        "<span style='color:#888;'>Pick up where you left off — your recently opened projects, tasks and jobs are below.</span>"),
        homePage_);
    welcome->setWordWrap(true);
    homeLayout->addWidget(welcome);

    auto* historyHeader = new QHBoxLayout;
    historyHeader->addWidget(new QLabel(QStringLiteral("<b>Recent activity</b>"), homePage_), 1);
    clearHistoryButton_ = new QPushButton(QStringLiteral("Clear history"), homePage_);
    clearHistoryButton_->setToolTip(QStringLiteral("Forget all locally recorded recent activity"));
    historyHeader->addWidget(clearHistoryButton_);
    homeLayout->addLayout(historyHeader);

    historyTable_ = makeTable({QStringLiteral("Type"), QStringLiteral("Name"),
                               QStringLiteral("Context"), QStringLiteral("Last opened")}, homePage_);
    historyTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    historyTable_->horizontalHeader()->setMinimumSectionSize(60);
    historyTable_->setColumnWidth(0, 100);
    historyTable_->setColumnWidth(1, 420);
    historyTable_->setColumnWidth(2, 300);
    historyTable_->setColumnWidth(3, 140);
    homeLayout->addWidget(historyTable_, 1);
    homeLayout->addWidget(new QLabel(QStringLiteral(
        "Double-click a row to jump back to it. Job history also records its task and project."), homePage_));
    tabs_->addTab(homePage_, QStringLiteral("Home"));

    projectTable_ = makeTable({QStringLiteral("Name"), QStringLiteral("Status"),
                               QStringLiteral("Dimension"), QStringLiteral("Tasks"),
                               QStringLiteral("Updated")}, tabs_);
    projectTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    projectTable_->horizontalHeader()->setMinimumSectionSize(60);
    projectTable_->setColumnWidth(0, 500); // Name
    projectTable_->setColumnWidth(1, 110); // Status
    projectTable_->setColumnWidth(2, 100); // Dimension
    projectTable_->setColumnWidth(3, 70);  // Tasks
    projectTable_->setColumnWidth(4, 150); // Updated

    taskTable_ = makeTable({QStringLiteral("Name"), QStringLiteral("Project"),
                            QStringLiteral("Status"), QStringLiteral("Frames"),
                            QStringLiteral("Updated")}, tabs_);
    taskTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    taskTable_->horizontalHeader()->setMinimumSectionSize(60);
    taskTable_->setColumnWidth(0, 500); // Name
    taskTable_->setColumnWidth(1, 240); // Project
    taskTable_->setColumnWidth(2, 110); // Status
    taskTable_->setColumnWidth(3, 75);  // Frames
    taskTable_->setColumnWidth(4, 150); // Updated

    jobTable_ = makeTable({QStringLiteral("Job"), QStringLiteral("Task"),
                           QStringLiteral("Status"), QStringLiteral("Stage"),
                           QStringLiteral("State"), QStringLiteral("Frames")}, tabs_);
    jobTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    jobTable_->horizontalHeader()->setMinimumSectionSize(60);
    jobTable_->setColumnWidth(0, 90);  // Job ID
    jobTable_->setColumnWidth(1, 500); // Task Name
    jobTable_->setColumnWidth(2, 110); // Status
    jobTable_->setColumnWidth(3, 100); // Stage
    jobTable_->setColumnWidth(4, 100); // State
    jobTable_->setColumnWidth(5, 110); // Frames
    tabs_->addTab(projectTable_, QStringLiteral("Projects"));
    tabs_->addTab(taskTable_, QStringLiteral("Tasks"));
    tabs_->addTab(jobTable_, QStringLiteral("Jobs"));
    tabs_->setCurrentIndex(0);
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
    connect(clearHistoryButton_, &QPushButton::clicked, this, [this]() {
        saveHistory({});
        refreshHistory();
        statusLabel_->setText(QStringLiteral("Recent activity cleared"));
    });
    connect(historyTable_, &QTableWidget::cellDoubleClicked, this,
            &DashboardPage::activateHistoryRow);
    connect(projectTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0 || row >= projects_.size()) {
            return;
        }
        recordProject(projects_[row]);
        taskFilterProject_ = projects_[row].id;
        jobFilterTask_ = 0;
        jobFilterProject_ = taskFilterProject_;
        tabs_->setCurrentIndex(2);
        taskPage_ = 1;
        jobPage_ = 1;
        refreshTasks(1);
    });
    connect(taskTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (row < 0 || row >= tasks_.size()) {
            return;
        }
        recordTask(tasks_[row]);
        jobFilterTask_ = tasks_[row].id;
        jobFilterProject_ = 0;
        tabs_->setCurrentIndex(3);
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
    switch (index) {
    case 0: searchEdit_->setPlaceholderText(QStringLiteral("Filter recent activity…")); break;
    case 1: searchEdit_->setPlaceholderText(QStringLiteral("Search projects…")); break;
    case 2: searchEdit_->setPlaceholderText(QStringLiteral("Search tasks…")); break;
    default: searchEdit_->setPlaceholderText(QStringLiteral("Search jobs…")); break;
    }
    refreshCurrent();
}

QTableWidget* DashboardPage::currentTable() const
{
    switch (tabs_->currentIndex()) {
    case 0: return historyTable_;
    case 1: return projectTable_;
    case 2: return taskTable_;
    default: return jobTable_;
    }
}

int DashboardPage::currentPage() const
{
    switch (tabs_->currentIndex()) {
    case 1: return projectPage_;
    case 2: return taskPage_;
    case 3: return jobPage_;
    default: return 1;
    }
}

void DashboardPage::setCurrentPage(int page)
{
    switch (tabs_->currentIndex()) {
    case 1: projectPage_ = page; break;
    case 2: taskPage_ = page; break;
    case 3: jobPage_ = page; break;
    default: break;
    }
}

void DashboardPage::refreshCurrent()
{
    if (loading_) {
        return;
    }
    switch (tabs_->currentIndex()) {
    case 0: refreshHistory(); break;
    case 1: refreshProjects(projectPage_); break;
    case 2: refreshTasks(taskPage_); break;
    case 3: refreshJobs(jobPage_); break;
    default: break;
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
    if (tabs_->currentIndex() == 0) {
        prevButton_->hide();
        nextButton_->hide();
        pageLabel_->setText(QStringLiteral("%1 recent item(s)").arg(history_.size()));
        pageLabel_->show();
        return;
    }
    prevButton_->show();
    nextButton_->show();

    PageMeta meta;
    switch (tabs_->currentIndex()) {
    case 2: meta = taskMeta_; break;
    case 3: meta = jobMeta_; break;
    default: meta = projectMeta_; break;
    }
    const int pages = std::max(1, (meta.count + kPageSize - 1) / kPageSize);
    const int page = currentPage();
    pageLabel_->setText(QStringLiteral("Page %1 of %2").arg(page).arg(pages));
    prevButton_->setEnabled(page > 1);
    nextButton_->setEnabled(page < pages);
}

QVector<DashboardPage::HistoryEntry> DashboardPage::loadHistory() const
{
    QVector<HistoryEntry> result;
    if (!store_) {
        return result;
    }
    const auto object = store_->loadJson(QStringLiteral("history/recent"));
    if (!object) {
        return result;
    }
    for (const auto& value : object->value(QStringLiteral("entries")).toArray()) {
        const QJsonObject source = value.toObject();
        HistoryEntry entry;
        entry.type = source.value(QStringLiteral("type")).toString();
        entry.id = source.value(QStringLiteral("id")).toInt();
        entry.name = source.value(QStringLiteral("name")).toString();
        entry.context = source.value(QStringLiteral("context")).toString();
        entry.viewedAt = QDateTime::fromString(
            source.value(QStringLiteral("viewed_at")).toString(), Qt::ISODateWithMs);
        entry.payload = source.value(QStringLiteral("payload")).toObject();
        if (!entry.type.isEmpty() && entry.id > 0) {
            result.push_back(entry);
        }
    }
    return result;
}

void DashboardPage::saveHistory(const QVector<HistoryEntry>& entries) const
{
    if (!store_) {
        return;
    }
    QJsonArray array;
    for (const auto& entry : entries) {
        QJsonObject source;
        source.insert(QStringLiteral("type"), entry.type);
        source.insert(QStringLiteral("id"), entry.id);
        source.insert(QStringLiteral("name"), entry.name);
        source.insert(QStringLiteral("context"), entry.context);
        source.insert(QStringLiteral("viewed_at"),
                      entry.viewedAt.isValid()
                          ? entry.viewedAt.toString(Qt::ISODateWithMs)
                          : QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
        source.insert(QStringLiteral("payload"), entry.payload);
        array.push_back(source);
    }
    QJsonObject root;
    root.insert(QStringLiteral("entries"), array);
    store_->saveJson(QStringLiteral("history/recent"), root);
}

void DashboardPage::recordHistory(const HistoryEntry& entry)
{
    HistoryEntry fresh = entry;
    fresh.viewedAt = QDateTime::currentDateTime();

    QVector<HistoryEntry> entries = loadHistory();
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].type == fresh.type && entries[i].id == fresh.id) {
            entries.removeAt(i);
            break;
        }
    }
    entries.prepend(fresh);
    constexpr int kMaxHistory = 40;
    while (entries.size() > kMaxHistory) {
        entries.removeAt(entries.size() - 1);
    }
    saveHistory(entries);
    if (tabs_ && tabs_->currentIndex() == 0) {
        refreshHistory();
    }
}

void DashboardPage::recordProject(const Project& project)
{
    if (project.id <= 0) {
        return;
    }
    HistoryEntry entry;
    entry.type = QStringLiteral("project");
    entry.id = project.id;
    entry.name = project.name.isEmpty() ? QStringLiteral("Project %1").arg(project.id) : project.name;
    entry.context = QStringLiteral("Project");
    entry.viewedAt = QDateTime::currentDateTime();
    entry.payload = project.raw;
    recordHistory(entry);
}

void DashboardPage::recordTask(const Task& task)
{
    if (task.id <= 0) {
        return;
    }
    HistoryEntry entry;
    entry.type = QStringLiteral("task");
    entry.id = task.id;
    entry.name = task.name.isEmpty() ? QStringLiteral("Task %1").arg(task.id) : task.name;
    entry.context = task.projectName.isEmpty() ? QStringLiteral("Task") : task.projectName;
    entry.viewedAt = QDateTime::currentDateTime();
    entry.payload = task.raw;
    recordHistory(entry);
}

void DashboardPage::recordJob(const Job& job)
{
    if (job.id <= 0) {
        return;
    }
    HistoryEntry entry;
    entry.type = QStringLiteral("job");
    entry.id = job.id;
    const QString task = job.taskName.isEmpty() ? QStringLiteral("Job") : job.taskName;
    entry.name = QStringLiteral("%1 · #%2").arg(task).arg(job.id);
    entry.context = job.projectName.isEmpty()
        ? QStringLiteral("Job %1").arg(job.id)
        : job.projectName;
    entry.viewedAt = QDateTime::currentDateTime();
    entry.payload = job.raw;
    recordHistory(entry);
}

void DashboardPage::refreshHistory()
{
    history_ = loadHistory();
    const QString filter = searchEdit_->text().trimmed();
    historyTable_->setRowCount(0);
    for (int i = 0; i < history_.size(); ++i) {
        const HistoryEntry& entry = history_[i];
        if (!filter.isEmpty() &&
            !entry.name.contains(filter, Qt::CaseInsensitive) &&
            !entry.context.contains(filter, Qt::CaseInsensitive) &&
            !entry.type.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        const QString kind = entry.type == QStringLiteral("job")   ? QStringLiteral("Job")
                           : entry.type == QStringLiteral("task")  ? QStringLiteral("Task")
                                                                   : QStringLiteral("Project");
        const int row = historyTable_->rowCount();
        historyTable_->insertRow(row);
        historyTable_->setItem(row, 0, item(kind, i));
        historyTable_->setItem(row, 1, item(entry.name));
        historyTable_->setItem(row, 2, item(entry.context));
        historyTable_->setItem(row, 3, item(timeAgo(entry.viewedAt)));
    }
    statusLabel_->setStyleSheet(QStringLiteral("color: #888;"));
    statusLabel_->setText(QStringLiteral("%1 recent item(s)").arg(history_.size()));
    updatePager();
}

void DashboardPage::activateHistoryRow(int row)
{
    const auto* cell = historyTable_->item(row, 0);
    if (!cell) {
        return;
    }
    const int index = cell->data(Qt::UserRole).toInt();
    if (index < 0 || index >= history_.size()) {
        return;
    }
    const HistoryEntry entry = history_[index];

    if (entry.type == QStringLiteral("job")) {
        Job job = Job::fromJson(entry.payload);
        if (job.id <= 0) {
            job.id = entry.id;
        }
        emit jobActivated(job); // MainWindow records the job when it opens it.
        return;
    }

    recordHistory(entry); // bump it to the top
    if (entry.type == QStringLiteral("task")) {
        jobFilterTask_ = entry.id;
        jobFilterProject_ = 0;
        jobPage_ = 1;
        tabs_->setCurrentIndex(3);
        refreshJobs(1);
    } else {
        taskFilterProject_ = entry.id;
        jobFilterProject_ = entry.id;
        jobFilterTask_ = 0;
        taskPage_ = 1;
        jobPage_ = 1;
        tabs_->setCurrentIndex(2);
        refreshTasks(1);
    }
}

} // namespace cvat
