#include "cvatnative/core/UpdateChecker.h"

#include <QJsonArray>
#include <QJsonObject>

#include <gtest/gtest.h>

using namespace cvat;

namespace {

QJsonArray releaseAssets()
{
    QJsonArray assets;
    assets.push_back(QJsonObject{{QStringLiteral("name"), QStringLiteral("Source code (zip)")},
                                 {QStringLiteral("browser_download_url"),
                                  QStringLiteral("https://example.invalid/source.zip")}});
    assets.push_back(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("cvat-native-v0.2.0-windows-x86_64.zip")},
        {QStringLiteral("browser_download_url"),
         QStringLiteral("https://example.invalid/cvat-native-windows.zip")}});
    assets.push_back(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("cvat-native-v0.2.0-linux-x86_64.zip")},
        {QStringLiteral("browser_download_url"),
         QStringLiteral("https://example.invalid/cvat-native-linux.zip")}});
    return assets;
}

} // namespace

TEST(Update, CompareVersionsHandlesTagsAndSuffixes)
{
    EXPECT_EQ(UpdateChecker::compareVersions(QStringLiteral("v0.2.0"),
                                             QStringLiteral("0.2.0")), 0);
    EXPECT_EQ(UpdateChecker::compareVersions(QStringLiteral("0.2"),
                                             QStringLiteral("v0.1.9")), 1);
    EXPECT_EQ(UpdateChecker::compareVersions(QStringLiteral("v1.10.0"),
                                             QStringLiteral("v1.9.9")), 1);
    EXPECT_EQ(UpdateChecker::compareVersions(QStringLiteral("v2.0.0-beta.1"),
                                             QStringLiteral("2.0.0")), 0);
    EXPECT_EQ(UpdateChecker::compareVersions(QStringLiteral("v1.0.0"),
                                             QStringLiteral("v1.0.0.1")), -1);
}

TEST(Update, ChoosesPlatformZipFromReleaseAssets)
{
    const QJsonArray assets = releaseAssets();
    EXPECT_EQ(UpdateChecker::chooseAssetUrl(assets, QStringLiteral("linux")),
              QStringLiteral("https://example.invalid/cvat-native-linux.zip"));
    EXPECT_EQ(UpdateChecker::chooseAssetUrl(assets, QStringLiteral("windows")),
              QStringLiteral("https://example.invalid/cvat-native-windows.zip"));
    EXPECT_TRUE(UpdateChecker::chooseAssetUrl(assets, QStringLiteral("macos")).isEmpty());
}

TEST(Update, IgnoresAssetsWithoutDownloadUrl)
{
    QJsonArray assets;
    assets.push_back(QJsonObject{{QStringLiteral("name"),
                                  QStringLiteral("cvat-native-linux-x86_64.zip")}});
    EXPECT_TRUE(UpdateChecker::chooseAssetUrl(assets, QStringLiteral("linux")).isEmpty());
}

TEST(Update, UsesGitHubLatestReleaseEndpoint)
{
    EXPECT_EQ(UpdateChecker::apiUrl(),
              QStringLiteral("https://api.github.com/repos/haider-9/cvat-native/releases/latest"));
}
