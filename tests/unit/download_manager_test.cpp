#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

#include "DownloadManager.h"

class DownloadManagerTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        DownloadManager::getInstance().cancelDownload();
    }

    TestSupport::ScopedNetworkHooks m_networkHooks;
};

TEST_F(DownloadManagerTest, CleanUrlRemovesTrailingCommasAndRejectsUnsupportedSchemes)
{
    DownloadManager& manager = DownloadManager::getInstance();

    EXPECT_EQ(
        manager.cleanUrl(QStringLiteral(" https://example.com/file.zip,, ")),
        QStringLiteral("https://example.com/file.zip"));
    EXPECT_TRUE(manager.cleanUrl(QStringLiteral("ftp://example.com/file.zip")).isEmpty());
}

TEST_F(DownloadManagerTest, DownloadFileRenamesDetectedExecutableFormat)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString originalPath = tempDir.filePath(QStringLiteral("trainer.bin"));
    bool completed = false;
    bool success = false;
    QString message;
    QString actualPath;

    m_networkHooks.setDownloadHandler(
        [](const QString&,
           const QString& savePath,
           const QString&,
           qint64,
           bool,
           DownloadProgressCallback progressCallback,
           DownloadFinishedCallback finishedCallback) {
            if (!TestSupport::writeFileBytes(savePath, QByteArray("MZ\x90\x00", 4))) {
                return false;
            }

            if (progressCallback) {
                progressCallback(4, 4);
            }
            if (finishedCallback) {
                finishedCallback(true, QString(), 200);
            }
            return true;
        });

    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/trainer.bin"),
        originalPath,
        DLProgressCallback(),
        [&completed, &success, &message, &actualPath](bool ok,
                                                      const QString& statusMessage,
                                                      const QString& path) {
            completed = true;
            success = ok;
            message = statusMessage;
            actualPath = path;
        });

    EXPECT_TRUE(completed);
    EXPECT_TRUE(success);
    EXPECT_TRUE(message.contains(QStringLiteral("renamed")));
    EXPECT_TRUE(actualPath.endsWith(QStringLiteral(".exe")));
    EXPECT_TRUE(QFile::exists(actualPath));
    EXPECT_FALSE(QFile::exists(originalPath));
}

TEST_F(DownloadManagerTest, ConcurrentDownloadRequestsAreRejected)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    DownloadFinishedCallback pendingFinishedCallback;
    bool firstCompleted = false;
    bool firstSuccess = false;
    bool secondCompleted = false;
    bool secondSuccess = true;
    QString secondError;

    m_networkHooks.setDownloadHandler(
        [&pendingFinishedCallback](const QString&,
                                   const QString&,
                                   const QString&,
                                   qint64,
                                   bool,
                                   DownloadProgressCallback,
                                   DownloadFinishedCallback finishedCallback) {
            pendingFinishedCallback = std::move(finishedCallback);
            return true;
        });

    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/first.zip"),
        tempDir.filePath(QStringLiteral("first.zip")),
        DLProgressCallback(),
        [&firstCompleted, &firstSuccess](bool ok, const QString&, const QString&) {
            firstCompleted = true;
            firstSuccess = ok;
        });

    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/second.zip"),
        tempDir.filePath(QStringLiteral("second.zip")),
        DLProgressCallback(),
        [&secondCompleted, &secondSuccess, &secondError](bool ok,
                                                         const QString& error,
                                                         const QString&) {
            secondCompleted = true;
            secondSuccess = ok;
            secondError = error;
        });

    ASSERT_TRUE(static_cast<bool>(pendingFinishedCallback));
    EXPECT_TRUE(DownloadManager::getInstance().isDownloading());
    EXPECT_TRUE(secondCompleted);
    EXPECT_FALSE(secondSuccess);
    EXPECT_EQ(secondError, QStringLiteral("Download already in progress"));

    pendingFinishedCallback(true, QString(), 200);

    EXPECT_TRUE(firstCompleted);
    EXPECT_TRUE(firstSuccess);
    EXPECT_FALSE(DownloadManager::getInstance().isDownloading());
}

TEST_F(DownloadManagerTest, DetectFileFormatRecognizesMinimalTarHeader)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString tarPath = tempDir.filePath(QStringLiteral("archive.bin"));
    QByteArray tarBytes(262, '\0');
    tarBytes.replace(257, 5, "ustar");
    ASSERT_TRUE(TestSupport::writeFileBytes(tarPath, tarBytes));

    EXPECT_EQ(DownloadManager::getInstance().detectFileFormat(tarPath), QStringLiteral("tar"));
}

TEST_F(DownloadManagerTest, DetectFileFormatReturnsEmptyWhenFileCannotBeOpened)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const QString missingPath = tempDir.filePath(QStringLiteral("missing.tar"));
    EXPECT_TRUE(DownloadManager::getInstance().detectFileFormat(missingPath).isEmpty());
}
