#include "cvatnative/ui/MainWindow.h"

#include "cvatnative/api/AnnotationsApi.h"
#include "cvatnative/api/ApiClient.h"
#include "cvatnative/api/AuthApi.h"
#include "cvatnative/api/JobsApi.h"
#include "cvatnative/api/LabelsApi.h"
#include "cvatnative/api/MediaApi.h"
#include "cvatnative/api/ProjectsApi.h"
#include "cvatnative/api/OrganizationsApi.h"
#include "cvatnative/api/TasksApi.h"
#include "cvatnative/api/UsersApi.h"
#include "cvatnative/core/Credentials.h"
#include "cvatnative/core/Logging.h"
#include "cvatnative/core/Settings.h"
#include "cvatnative/media/FrameCache.h"
#include "cvatnative/media/MediaService.h"
#include "cvatnative/storage/LocalStore.h"
#include "cvatnative/storage/RecoveryStore.h"
#include "cvatnative/ui/DashboardPage.h"
#include "cvatnative/ui/JobWorkspace.h"
#include "cvatnative/ui/LoginDialog.h"
#include "cvatnative/ui/SettingsDialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>

#include <algorithm>

namespace cvat {

namespace {
constexpr int kMiB = 1024 * 1024;

QString friendlyError(const ApiError& error)
{
    switch (error.kind) {
    case ErrorKind::Unauthorized:
        return QStringLiteral("The session is no longer valid. Sign in again.");
    case ErrorKind::Forbidden:
        return QStringLiteral("This account is not allowed to perform that action.");
    case ErrorKind::Network:
        return QStringLiteral("Cannot reach the CVAT server. Showing locally cached data.");
    case ErrorKind::Timeout:
        return QStringLiteral("The CVAT server did not answer in time. Showing cached data.");
    default:
        return error.toString();
    }
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("CVAT Native"));
    resize(1440, 900);
    setupServices();
    setupUi();
    applySettings();

    QTimer::singleShot(0, this, &MainWindow::startSession);
}

MainWindow::~MainWindow()
{
    // JobWorkspace writes a recovery snapshot from its destructor and must never
    // see a closed LocalStore. Remove every workspace (including ones queued
    // with deleteLater()) before the database is closed.
    workspace_ = nullptr;
    for (JobWorkspace* open : findChildren<JobWorkspace*>()) {
        delete open;
    }
    delete media_;
    media_ = nullptr;
    if (store_) {
        store_->close();
    }
}

void MainWindow::setupServices()
{
    auto& settings = Settings::instance();

    client_ = new ApiClient(this);
    auth_ = std::make_unique<AuthApi>(client_);
    usersApi_ = std::make_unique<UsersApi>(client_);
    projectsApi_ = std::make_unique<ProjectsApi>(client_);
    organizationsApi_ = std::make_unique<OrganizationsApi>(client_);
    tasksApi_ = std::make_unique<TasksApi>(client_);
    jobsApi_ = std::make_unique<JobsApi>(client_);
    annotationsApi_ = std::make_unique<AnnotationsApi>(client_);
    labelsApi_ = std::make_unique<LabelsApi>(client_);
    mediaApi_ = std::make_unique<MediaApi>(client_);

    store_ = std::make_unique<LocalStore>(QStringLiteral("cvat-native-main"));
    QString storeError;
    if (!store_->open(settings.databasePath(), &storeError)) {
        qCWarning(logApp) << "local cache unavailable:" << storeError;
    } else {
        store_->pruneSnapshots(30, 3);
    }
    recovery_ = std::make_unique<RecoveryStore>(store_.get());

    const QString frameCacheRoot = settings.cacheDirectory() + QStringLiteral("/frames");
    QDir().mkpath(frameCacheRoot);
    cache_ = std::make_unique<FrameCache>(frameCacheRoot);
    cache_->pruneDisk();

    media_ = new MediaService(client_, cache_.get(), this);
}

void MainWindow::setupUi()
{
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    auto* connectAction = new QAction(QStringLiteral("&Connect…"), this);
    connectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    connect(connectAction, &QAction::triggered, this, &MainWindow::showLogin);
    fileMenu->addAction(connectAction);

    logoutAction_ = new QAction(QStringLiteral("Log &out"), this);
    connect(logoutAction_, &QAction::triggered, this, &MainWindow::logout);
    fileMenu->addAction(logoutAction_);

    auto* settingsAction = new QAction(QStringLiteral("&Settings…"), this);
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    fileMenu->addAction(settingsAction);

    fileMenu->addSeparator();
    auto* quitAction = new QAction(QStringLiteral("&Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    dashboardAction_ = new QAction(QStringLiteral("&Dashboard"), this);
    dashboardAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
    dashboardAction_->setEnabled(false);
    connect(dashboardAction_, &QAction::triggered, this, [this]() {
        if (workspace_) {
            workspace_->requestBack();
        } else {
            openDashboard();
        }
    });
    viewMenu->addAction(dashboardAction_);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    auto* aboutAction = new QAction(QStringLiteral("&About CVAT Native"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAction);

    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);

    dashboard_ = new DashboardPage(projectsApi_.get(), tasksApi_.get(), jobsApi_.get(),
                                   organizationsApi_.get(), store_.get(), this);
    connect(dashboard_, &DashboardPage::jobActivated, this, &MainWindow::openJob);
    stack_->addWidget(dashboard_);

    sessionLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(sessionLabel_);

    updateSessionUi();
}

void MainWindow::applySettings()
{
    auto& settings = Settings::instance();
    client_->setBaseUrl(QUrl(settings.serverUrl()));
    client_->setTimeoutMs(settings.requestTimeoutMs());
    client_->setMaxRetries(settings.maxRetries());
    client_->setAuthScheme(settings.authScheme() == QStringLiteral("bearer")
                               ? AuthScheme::Bearer
                               : AuthScheme::Token);

    cache_->setMemoryLimit(static_cast<qint64>(settings.memoryCacheMiB()) * kMiB);
    cache_->setDiskLimit(static_cast<qint64>(settings.diskCacheMiB()) * kMiB);
    cache_->pruneDisk();

    if (workspace_) {
        workspace_->applySettings();
    }
}

void MainWindow::startSession()
{
    const auto& settings = Settings::instance();
    const auto token = CredentialStore::instance().load(settings.serverUrl(), settings.username());
    if (!token || token->isEmpty()) {
        showLogin();
        return;
    }

    client_->setToken(*token);
    validateStoredToken();
}

void MainWindow::validateStoredToken()
{
    statusBar()->showMessage(QStringLiteral("Connecting to %1…").arg(client_->baseUrl().toString()));
    usersApi_->currentUser([this](Result<UserInfo> result) {
        if (result) {
            authenticated_ = true;
            username_ = result->displayName().isEmpty() ? result->username
                                                        : result->displayName();
            statusBar()->showMessage(
                QStringLiteral("Connected as %1").arg(displayUser()), 5000);
            updateSessionUi();
            dashboard_->refresh();
            return;
        }

        authenticated_ = false;
        updateSessionUi();
        const ApiError& error = result.error();
        if (error.kind == ErrorKind::Unauthorized || error.kind == ErrorKind::Forbidden) {
            client_->setToken(QString());
            statusBar()->showMessage(QStringLiteral("The saved token was rejected"), 8000);
            showLogin();
            return;
        }

        statusBar()->showMessage(friendlyError(error), 15000);
        dashboard_->refresh();
    });
}

void MainWindow::showLogin()
{
    closeWorkspace();
    authenticated_ = false;
    updateSessionUi();

    if (!client_->baseUrl().isValid() || client_->baseUrl().host().isEmpty()) {
        client_->setBaseUrl(QUrl(Settings::instance().serverUrl()));
    }

    LoginDialog dialog(client_, this);
    if (dialog.exec() != QDialog::Accepted) {
        statusBar()->showMessage(
            QStringLiteral("Not connected. Dashboard will show cached data."), 10000);
        openDashboard();
        return;
    }

    auto& settings = Settings::instance();
    const QString server = client_->baseUrl().toString();
    const QString username = dialog.username();

    client_->setToken(dialog.token());
    client_->setAuthScheme(dialog.usesBearer() ? AuthScheme::Bearer : AuthScheme::Token);

    settings.setServerUrl(server);
    settings.setUsername(username);
    settings.setAuthScheme(dialog.usesBearer() ? QStringLiteral("bearer")
                                               : QStringLiteral("token"));
    settings.sync();

    if (dialog.remember()) {
        const auto stored =
            CredentialStore::instance().store(server, username, dialog.token());
        if (!stored.ok) {
            statusBar()->showMessage(
                QStringLiteral("Connected, but the token was not saved: ") + stored.message,
                12000);
        } else if (!stored.secure) {
            statusBar()->showMessage(stored.message, 12000);
        }
    } else {
        CredentialStore::instance().erase(server, username);
    }

    authenticated_ = true;
    username_ = username;
    updateSessionUi();
    statusBar()->showMessage(QStringLiteral("Connected as %1").arg(displayUser()), 5000);
    openDashboard();
    dashboard_->refresh();
}

void MainWindow::logout()
{
    closeWorkspace();
    auto& settings = Settings::instance();
    if (settings.authScheme() == QStringLiteral("token") && !client_->token().isEmpty()) {
        auth_->logout([](Result<ApiResponse>) {});
    }
    CredentialStore::instance().erase(settings.serverUrl(), settings.username());
    client_->setToken(QString());
    authenticated_ = false;
    username_.clear();
    updateSessionUi();
    openDashboard();
    statusBar()->showMessage(QStringLiteral("Signed out"), 5000);
}

void MainWindow::openDashboard()
{
    if (workspace_) {
        closeWorkspace();
    }
    stack_->setCurrentWidget(dashboard_);
    dashboardAction_->setEnabled(false);
    setWindowTitle(QStringLiteral("CVAT Native"));
}

void MainWindow::openJob(const Job& job)
{
    closeWorkspace();

    workspace_ = new JobWorkspace(job, annotationsApi_.get(), jobsApi_.get(),
                                  labelsApi_.get(), media_, recovery_.get(), store_.get(), this);
    connect(workspace_, &JobWorkspace::backRequested, this, [this]() {
        openDashboard();
        dashboard_->refresh();
    });
    stack_->addWidget(workspace_);
    stack_->setCurrentWidget(workspace_);
    dashboardAction_->setEnabled(true);

    const QString task = job.taskName.isEmpty()
        ? QStringLiteral("Job %1").arg(job.id)
        : job.taskName;
    setWindowTitle(QStringLiteral("%1 · CVAT Native").arg(task));
}

void MainWindow::closeWorkspace()
{
    if (!workspace_) {
        return;
    }
    JobWorkspace* closing = workspace_;
    workspace_ = nullptr;
    stack_->removeWidget(closing);
    closing->hide();
    closing->deleteLater();
    dashboardAction_->setEnabled(false);
    setWindowTitle(QStringLiteral("CVAT Native"));
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(client_, this);
    connect(&dialog, &SettingsDialog::settingsSaved, this, [this]() {
        applySettings();
        if (dashboard_) {
            dashboard_->refresh();
        }
    });
    if (dialog.exec() == QDialog::Accepted) {
        // settingsSaved already applied the changes; this is a no-op safety net
        // for dialogs that accept without emitting.
        applySettings();
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, QStringLiteral("About CVAT Native"),
        QStringLiteral("<b>CVAT Native</b> %1<br>"
                       "A lightweight Qt6 annotation client for a CVAT server.<br><br>"
                       "Annotations, labels, jobs and media are read through the CVAT "
                       "REST API. No browser engine is involved.")
            .arg(QString::fromLatin1(CVATNATIVE_VERSION)));
}

void MainWindow::updateSessionUi()
{
    if (sessionLabel_) {
        if (authenticated_) {
            sessionLabel_->setText(QStringLiteral("● %1").arg(displayUser()));
            sessionLabel_->setStyleSheet(QStringLiteral("color: #2a7;"));
        } else {
            sessionLabel_->setText(QStringLiteral("○ not signed in"));
            sessionLabel_->setStyleSheet(QStringLiteral("color: #888;"));
        }
    }
    if (logoutAction_) {
        logoutAction_->setEnabled(authenticated_);
    }
}

QString MainWindow::displayUser() const
{
    if (!username_.isEmpty()) {
        return username_;
    }
    const QString configured = Settings::instance().username();
    return configured.isEmpty() ? QStringLiteral("unknown user") : configured;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (JobWorkspace* open : findChildren<JobWorkspace*>()) {
        if (!open->isDirty()) {
            continue;
        }
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Unsaved changes"),
            QStringLiteral("“%1” has unsaved annotation changes.\n\n"
                           "A local recovery snapshot will be kept. Quit without "
                           "saving to the server?")
                .arg(windowTitle()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

} // namespace cvat
