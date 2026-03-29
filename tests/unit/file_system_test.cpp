#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>

#include "FileSystem.h"

namespace {
QString bundledTestPath(const QString& relativePath)
{
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(relativePath);
}
}

class FileSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        cleanupPath(m_directoryPath);
        cleanupPath(m_filePath);
    }

    void TearDown() override
    {
        cleanupPath(m_directoryPath);
        cleanupPath(m_filePath);
    }

    void cleanupPath(const QString& path)
    {
        if (path.isEmpty()) {
            return;
        }

        QFileInfo info(path);
        if (info.isDir()) {
            QDir(path).removeRecursively();
        } else if (info.exists()) {
            QFile::remove(path);
        }
    }

    const QString m_relativeDirectory = QStringLiteral("resources/test_bundled_resource_dir");
    const QString m_relativeFile = QStringLiteral("resources/test_bundled_resource_file.txt");
    const QString m_directoryPath = bundledTestPath(m_relativeDirectory);
    const QString m_filePath = bundledTestPath(m_relativeFile);
};

TEST_F(FileSystemTest, GetBundledResourcePathIgnoresDirectories)
{
    ASSERT_TRUE(QDir().mkpath(m_directoryPath));

    EXPECT_TRUE(FileSystem::getInstance().getBundledResourcePath(m_relativeDirectory).isEmpty());
}

TEST_F(FileSystemTest, GetBundledResourcePathReturnsReadableFile)
{
    ASSERT_TRUE(QDir().mkpath(QFileInfo(m_filePath).absolutePath()));

    QFile file(m_filePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_GT(file.write("ok"), 0);
    file.close();

    EXPECT_EQ(
        QDir::cleanPath(FileSystem::getInstance().getBundledResourcePath(m_relativeFile)),
        QDir::cleanPath(m_filePath));
}
