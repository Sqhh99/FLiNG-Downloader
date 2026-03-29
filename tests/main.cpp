#include <QGuiApplication>
#include <QStandardPaths>

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));

    QGuiApplication::setOrganizationName("Sqhh99");
    QGuiApplication::setApplicationName("FLiNG Downloader Tests");

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion("0.0.0-test");

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
