#include "fixtures/test_support.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QHash>
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

TEST_F(DownloadManagerTest, DownloadsToDifferentPathsRunConcurrently)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QHash<QString, DownloadFinishedCallback> pendingFinishedCallbacks;
    bool firstSuccess = false;
    bool secondSuccess = false;

    m_networkHooks.setDownloadHandler(
        [&pendingFinishedCallbacks](const QString&,
                                    const QString& savePath,
                                    const QString&,
                                    qint64,
                                    bool,
                                    DownloadProgressCallback,
                                    DownloadFinishedCallback finishedCallback) {
            pendingFinishedCallbacks.insert(savePath, std::move(finishedCallback));
            return true;
        });

    const QString firstPath = tempDir.filePath(QStringLiteral("first.zip"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.zip"));
    ASSERT_TRUE(TestSupport::writeFileBytes(firstPath, QByteArray("PK\x03\x04", 4)));
    ASSERT_TRUE(TestSupport::writeFileBytes(secondPath, QByteArray("PK\x03\x04", 4)));

    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/first.zip"),
        firstPath,
        DLProgressCallback(),
        [&firstSuccess](bool ok, const QString&, const QString&) { firstSuccess = ok; });
    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/second.zip"),
        secondPath,
        DLProgressCallback(),
        [&secondSuccess](bool ok, const QString&, const QString&) { secondSuccess = ok; });

    ASSERT_EQ(pendingFinishedCallbacks.size(), 2);
    EXPECT_TRUE(DownloadManager::getInstance().isDownloading());

    pendingFinishedCallbacks.value(firstPath)(true, QString(), 200);
    EXPECT_TRUE(firstSuccess);
    // The other transfer is still running.
    EXPECT_TRUE(DownloadManager::getInstance().isDownloading());

    pendingFinishedCallbacks.value(secondPath)(true, QString(), 200);
    EXPECT_TRUE(secondSuccess);
    EXPECT_FALSE(DownloadManager::getInstance().isDownloading());
}

TEST_F(DownloadManagerTest, SecondDownloadToTheSamePathIsRejected)
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

    const QString savePath = tempDir.filePath(QStringLiteral("same.zip"));
    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/first.zip"),
        savePath,
        DLProgressCallback(),
        [&firstCompleted, &firstSuccess](bool ok, const QString&, const QString&) {
            firstCompleted = true;
            firstSuccess = ok;
        });

    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/second.zip"),
        savePath,
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

TEST_F(DownloadManagerTest, CancellingOneDownloadLeavesTheOthersRunning)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    m_networkHooks.setDownloadHandler(
        [](const QString&,
           const QString&,
           const QString&,
           qint64,
           bool,
           DownloadProgressCallback,
           DownloadFinishedCallback) {
            return true;  // Never finishes on its own.
        });

    const QString firstPath = tempDir.filePath(QStringLiteral("first.zip"));
    const QString secondPath = tempDir.filePath(QStringLiteral("second.zip"));
    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/first.zip"), firstPath,
        DLProgressCallback(), DLCompletedCallback());
    DownloadManager::getInstance().downloadFile(
        QStringLiteral("https://example.com/second.zip"), secondPath,
        DLProgressCallback(), DLCompletedCallback());

    DownloadManager::getInstance().cancelDownload(firstPath);
    EXPECT_TRUE(DownloadManager::getInstance().isDownloading());

    DownloadManager::getInstance().cancelDownload(secondPath);
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

TEST_F(DownloadManagerTest, CancelDownloadWithoutActiveTransferIsSafe)
{
    // Regression: NetworkManager tracked the in-flight download in an
    // uninitialised raw pointer, so cancelling with nothing in flight read
    // indeterminate memory. Downloads are now keyed by destination path and an
    // unknown path simply matches nothing.
    NetworkManager::getInstance().cancelDownload(QStringLiteral("does/not/exist.zip"));

    DownloadManager::getInstance().cancelDownload();
    EXPECT_FALSE(DownloadManager::getInstance().isDownloading());
}

TEST_F(DownloadManagerTest, DefaultRefererKeepsOnlyTheOrigin)
{
    // Regression: flingtrainer.com started serving trainer payloads only to
    // same-origin referrers, so downloads without this header answered 403.
    EXPECT_EQ(
        NetworkManager::defaultRefererForUrl(
            QStringLiteral("https://flingtrainer.com/downloads/Ekm-bdRFI0A_lCbUwTEb0g,,")),
        QStringLiteral("https://flingtrainer.com/"));

    // A non-default port belongs to the origin and has to survive.
    EXPECT_EQ(
        NetworkManager::defaultRefererForUrl(QStringLiteral("http://localhost:8080/a/b.zip")),
        QStringLiteral("http://localhost:8080/"));
}

TEST_F(DownloadManagerTest, DefaultRefererIsEmptyWithoutSchemeAndHost)
{
    EXPECT_TRUE(NetworkManager::defaultRefererForUrl(QStringLiteral("/relative/path.zip")).isEmpty());
    EXPECT_TRUE(NetworkManager::defaultRefererForUrl(QString()).isEmpty());
}
