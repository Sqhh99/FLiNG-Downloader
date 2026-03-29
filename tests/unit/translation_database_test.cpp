#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "AppUpdateManager.h"
#include "TranslationDatabase.h"

namespace {
QString deriveOlderVersion(const QString& bundledVersion)
{
    const QString normalized = AppUpdateManager::normalizeVersion(bundledVersion);
    if (normalized.isEmpty()) {
        return QString();
    }

    const QRegularExpression semverPattern(
        QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)(?:-[0-9A-Za-z.-]+)?(?:\\+[0-9A-Za-z.-]+)?$"));
    const QRegularExpressionMatch match = semverPattern.match(normalized);
    if (match.hasMatch()) {
        int major = match.captured(1).toInt();
        int minor = match.captured(2).toInt();
        int patch = match.captured(3).toInt();
        QString candidate;

        if (patch > 0) {
            candidate = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch - 1);
        } else if (minor > 0) {
            candidate = QStringLiteral("%1.%2.0").arg(major).arg(minor - 1);
        } else if (major > 0) {
            candidate = QStringLiteral("%1.0.0").arg(major - 1);
        } else {
            candidate = QStringLiteral("0.0.0-dev.0");
        }

        if (AppUpdateManager::compareVersions(candidate, normalized) < 0) {
            return candidate;
        }
    }

    const QStringList fallbackCandidates = {
        QStringLiteral("0.0.0-dev.0"),
        QStringLiteral("0.0.0-dev.1"),
        QStringLiteral("0.0.0"),
        QStringLiteral("0.0.1")
    };
    for (const QString& candidate : fallbackCandidates) {
        if (AppUpdateManager::compareVersions(candidate, normalized) < 0) {
            return candidate;
        }
    }

    return QString();
}
}

class TranslationDatabaseTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
    }

    void TearDown() override
    {
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath());
        QFile::remove(TranslationDatabase::getInstance().overrideDatabasePath() + QStringLiteral(".tmp"));
    }
};

TEST_F(TranslationDatabaseTest, PrefersBundledCopyWhenOverrideIsOlder)
{
    TranslationDatabase& database = TranslationDatabase::getInstance();
    const QString bundledPath = database.bundledDatabasePath();
    const QString overridePath = database.overrideDatabasePath();
    const QString bundledVersion = database.readMetadata(bundledPath).releaseTag;

    ASSERT_FALSE(bundledPath.isEmpty());
    ASSERT_TRUE(database.isValidDatabaseFile(bundledPath));

    const QString olderOverrideVersion = deriveOlderVersion(bundledVersion);
    ASSERT_FALSE(olderOverrideVersion.isEmpty())
        << "Could not derive an older override version from bundled release tag: "
        << bundledVersion.toStdString();
    ASSERT_TRUE(TestSupport::createTranslationDatabase(overridePath, olderOverrideVersion));

    EXPECT_EQ(QDir::cleanPath(database.databasePath()), QDir::cleanPath(bundledPath));
}

TEST_F(TranslationDatabaseTest, PrefersOverrideWhenItIsNewer)
{
    TranslationDatabase& database = TranslationDatabase::getInstance();
    const QString overridePath = database.overrideDatabasePath();

    ASSERT_TRUE(TestSupport::createTranslationDatabase(overridePath, QStringLiteral("999.0.0")));

    EXPECT_EQ(QDir::cleanPath(database.databasePath()), QDir::cleanPath(overridePath));
    EXPECT_EQ(database.currentReleaseTag(), QStringLiteral("999.0.0"));
}

TEST_F(TranslationDatabaseTest, RejectsDatabaseMissingRequiredGamesColumns)
{
    TranslationDatabase& database = TranslationDatabase::getInstance();
    const QString overridePath = database.overrideDatabasePath();
    QString errorMessage;

    ASSERT_TRUE(TestSupport::createTranslationDatabaseMissingNormalizedEnglish(
        overridePath,
        QStringLiteral("999.0.0")));

    EXPECT_FALSE(database.isValidDatabaseFile(overridePath, &errorMessage));
    EXPECT_TRUE(errorMessage.contains(QStringLiteral("games.normalized_english")));
}

TEST_F(TranslationDatabaseTest, RejectsUnsupportedSchemaVersion)
{
    TranslationDatabase& database = TranslationDatabase::getInstance();
    const QString overridePath = database.overrideDatabasePath();
    QString errorMessage;

    ASSERT_TRUE(TestSupport::createTranslationDatabase(
        overridePath,
        QStringLiteral("999.0.0"),
        QStringLiteral("2")));

    EXPECT_FALSE(database.isValidDatabaseFile(overridePath, &errorMessage));
    EXPECT_TRUE(errorMessage.contains(QStringLiteral("Unsupported schema_version")));
}
