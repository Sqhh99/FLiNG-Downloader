#include "CoverExtractor.h"
#include "FileSystem.h"
#include <QNetworkRequest>
#include <QDir>
#include <QCryptographicHash>
#include <QDebug>
#include <QBuffer>
#include <QImageReader>
#include <QCoreApplication>
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include "yolos/tasks/detection.hpp"

namespace {

// Confidence / IoU thresholds for the single-class game-cover detector.
constexpr float kCoverConfThreshold = 0.25f;
constexpr float kCoverIouThreshold = 0.45f;

// Lazily-initialized, shared YOLO detector. Loading the ONNX model is a
// one-time cost (tens to hundreds of ms); subsequent inferences reuse it.
yolos::det::YOLODetector* coverDetector()
{
    static std::once_flag onceFlag;
    static std::unique_ptr<yolos::det::YOLODetector> detector;

    std::call_once(onceFlag, []() {
        const QString baseDir = QCoreApplication::applicationDirPath();
        const QString modelPath = baseDir + "/models/game-cover-v2.onnx";
        const QString labelsPath = baseDir + "/models/game-cover.names";

        if (!QFile::exists(modelPath)) {
            qWarning() << "Cover detection model not found:" << modelPath;
            return;
        }

        try {
            detector = std::make_unique<yolos::det::YOLODetector>(
                modelPath.toStdString(),
                labelsPath.toStdString(),
                /*useGPU=*/false);
        } catch (const std::exception& e) {
            qWarning() << "Failed to load cover detection model:" << e.what();
            detector.reset();
        }
    });

    return detector.get();
}

} // namespace

CoverExtractor::CoverExtractor(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_callback(nullptr)
{
}

CoverExtractor::~CoverExtractor()
{
}

void CoverExtractor::extractCoverFromTrainerImage(const QString& imageUrl, 
                                                 std::function<void(const QPixmap&, bool)> callback)
{
    m_callback = callback;
    
    QNetworkRequest request(imageUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &CoverExtractor::onImageDownloaded);
}

void CoverExtractor::onImageDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    
    if (!reply || !m_callback) {
        if (reply) reply->deleteLater();
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        m_callback(QPixmap(), false);
        reply->deleteLater();
        return;
    }
    
    QByteArray imageData = reply->readAll();
    reply->deleteLater();
    
    QPixmap originalPixmap;
    if (!originalPixmap.loadFromData(imageData)) {
        m_callback(QPixmap(), false);
        return;
    }
    
    QPixmap coverPixmap = processTrainerImage(originalPixmap);
    
    if (!coverPixmap.isNull()) {
        m_callback(coverPixmap, true);
    } else {
        m_callback(QPixmap(), false);
    }
}

QPixmap CoverExtractor::processTrainerImage(const QPixmap& originalImage)
{
    if (originalImage.isNull()) {
        return QPixmap();
    }

    // qPixmapToMat returns RGB; the detector runs on a BGR copy internally.
    cv::Mat rgb = qPixmapToMat(originalImage);
    if (rgb.empty()) {
        return QPixmap();
    }

    cv::Mat cover = extractCoverByModel(rgb);
    if (cover.empty()) {
        return QPixmap();
    }

    return matToQPixmap(cover);
}

cv::Mat CoverExtractor::extractCoverByModel(const cv::Mat& rgbImage)
{
    try {
        yolos::det::YOLODetector* detector = coverDetector();
        if (!detector || rgbImage.empty()) {
            return cv::Mat();
        }

        // YOLOs-CPP preprocessing expects BGR input (it converts BGR->RGB).
        cv::Mat bgr;
        cv::cvtColor(rgbImage, bgr, cv::COLOR_RGB2BGR);

        // The detector is a shared singleton and detect() mutates its internal
        // (mutable) preprocessing buffer, so serialize the inference call to
        // keep it safe if ever invoked from multiple threads.
        std::vector<yolos::det::Detection> detections;
        {
            static std::mutex inferenceMutex;
            std::lock_guard<std::mutex> lock(inferenceMutex);
            detections = detector->detect(bgr, kCoverConfThreshold, kCoverIouThreshold);
        }
        if (detections.empty()) {
            return cv::Mat();
        }

        // Single-class model: pick the highest-confidence detection.
        const auto best = std::max_element(
            detections.begin(), detections.end(),
            [](const yolos::det::Detection& a, const yolos::det::Detection& b) {
                return a.conf < b.conf;
            });

        // Clamp the box to image bounds before cropping (defensive).
        int x = std::max(0, best->box.x);
        int y = std::max(0, best->box.y);
        int w = std::min(best->box.width, rgbImage.cols - x);
        int h = std::min(best->box.height, rgbImage.rows - y);
        if (w <= 0 || h <= 0) {
            return cv::Mat();
        }

        // Crop from the RGB image so colors stay consistent with matToQPixmap.
        return rgbImage(cv::Rect(x, y, w, h)).clone();

    } catch (const std::exception& e) {
        qWarning() << "Model-based cover extraction failed:" << e.what();
        return cv::Mat();
    }
}

QPixmap CoverExtractor::matToQPixmap(const cv::Mat& mat)
{
    try {
        if (mat.empty()) {
            return QPixmap();
        }
        
        if (mat.channels() == 3) {
            // Data is already RGB — no color conversion needed
            QImage qimg(mat.data, mat.cols, mat.rows, 
                       static_cast<int>(mat.step), QImage::Format_RGB888);
            return QPixmap::fromImage(qimg);
        } else if (mat.channels() == 1) {
            cv::Mat rgbMat;
            cv::cvtColor(mat, rgbMat, cv::COLOR_GRAY2RGB);
            QImage qimg(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                       static_cast<int>(rgbMat.step), QImage::Format_RGB888);
            return QPixmap::fromImage(qimg);
        } else {
            // 4-channel: data is RGBA
            QImage qimg(mat.data, mat.cols, mat.rows, 
                       static_cast<int>(mat.step), QImage::Format_RGBA8888);
            return QPixmap::fromImage(qimg);
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Mat to QPixmap conversion failed:" << e.what();
        return QPixmap();
    }
}

cv::Mat CoverExtractor::qPixmapToMat(const QPixmap& pixmap)
{
    try {
        QImage qimg = pixmap.toImage();
        if (qimg.format() != QImage::Format_RGB888) {
            qimg = qimg.convertToFormat(QImage::Format_RGB888);
        }
        
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC3, 
                   const_cast<uchar*>(qimg.bits()), 
                   static_cast<size_t>(qimg.bytesPerLine()));
        
        // Keep data in RGB format — clone to own memory since qimg is local
        return mat.clone();
        
    } catch (const std::exception& e) {
        qDebug() << "QPixmap to Mat conversion failed:" << e.what();
        return cv::Mat();
    }
}

// Static method interface
QPixmap CoverExtractor::extractCoverFromLocalImage(const QString& imagePath)
{
    QPixmap originalPixmap(imagePath);
    return processTrainerImage(originalPixmap);
}

QString CoverExtractor::getCacheDirectory()
{
    QString cacheDir = FileSystem::getInstance().getCacheDirectory();
    QDir dir(cacheDir);
    if (!dir.exists("covers")) {
        dir.mkpath("covers");
    }
    return dir.absoluteFilePath("covers");
}

QPixmap CoverExtractor::getCachedCover(const QString& gameId)
{
    QString cacheDir = getCacheDirectory();
    QString cachedPath = QDir(cacheDir).absoluteFilePath(gameId + ".png");
    
    if (QFile::exists(cachedPath)) {
        QPixmap cached(cachedPath);
        if (!cached.isNull()) {
            return cached;
        }
    }
    
    return QPixmap();
}

bool CoverExtractor::saveCoverToCache(const QString& gameId, const QPixmap& cover)
{
    if (cover.isNull()) {
        return false;
    }
    
    QString cacheDir = getCacheDirectory();
    QString cachedPath = QDir(cacheDir).absoluteFilePath(gameId + ".png");
    
    return cover.save(cachedPath, "PNG");
}
