#include "cvatnative/core/Logging.h"
#include "cvatnative/core/Settings.h"
#include "cvatnative/ui/Appearance.h"
#include "cvatnative/ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QIcon>
#include <QUrl>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("cvat-native"));
    QApplication::setOrganizationDomain(QStringLiteral("cvat-native.local"));
    QApplication::setApplicationName(QStringLiteral("cvat-native"));
    QApplication::setApplicationDisplayName(QStringLiteral("CVAT Native"));
    QApplication::setApplicationVersion(QString::fromLatin1(CVATNATIVE_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("cvat-native"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Native Qt6 annotation client for a CVAT backend."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption debugOption(QStringLiteral("debug"),
                                   QStringLiteral("Enable verbose application logging."));
    QCommandLineOption serverOption(QStringLiteral("server"),
                                    QStringLiteral("Override the stored CVAT server URL."),
                                    QStringLiteral("url"));
    parser.addOption(debugOption);
    parser.addOption(serverOption);
    parser.process(app);

    cvat::installLogging();
    cvat::setLogLevel(parser.isSet(debugOption) ? QStringLiteral("debug")
                                                : QStringLiteral("info"));

    if (parser.isSet(serverOption)) {
        const QUrl url(parser.value(serverOption));
        if (url.isValid() && !url.host().isEmpty()) {
            cvat::Settings::instance().setServerUrl(url.toString());
            cvat::Settings::instance().sync();
        } else {
            qWarning("Ignoring invalid --server value");
        }
    }

    cvat::applyAppearance();
    cvat::MainWindow window;
    window.show();
    return app.exec();
}
