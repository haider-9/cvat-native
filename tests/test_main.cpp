#include <QString>
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    // Keep settings-writing tests away from the developer's real configuration.
    static QTemporaryDir settingsDir;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    QCoreApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Core, VersionMacroIsAvailable)
{
    EXPECT_FALSE(QStringLiteral(CVATNATIVE_VERSION).isEmpty());
}
