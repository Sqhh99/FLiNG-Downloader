#include "CoverExtractor.h"
#include "FileSystem.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>
#include <QCryptographicHash>
#include <QDebug>
#include <QBuffer>
#include <QImageReader>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include "yolos/tasks/detection.hpp"
#include "Logger.h"

namespace {

// Confidence / IoU thresholds for the single-class game-cover detector.
constexpr float kCoverConfThreshold = 0.25f;
constexpr float kCoverIouThreshold = 0.45f;

// The model file name doubles as the no-cover marker's suffix, so shipping a
// new model retries screenshots the old one gave up on.
constexpr auto kCoverModelName = "game-cover-v2";

// Lazily-initialized, shared YOLO detector. Loading the ONNX model is a
// one-time cost (tens to hundreds of ms); subsequent inferences reuse it.
yolos::det::YOLODetector* coverDetector()
{
    static std::once_flag onceFlag;
    static std::unique_ptr<yolos::det::YOLODetector> detector;

    std::call_once(onceFlag, []() {
        const QString baseDir = QCoreApplication::applicationDirPath();
        const QString modelPath = baseDir + "/models/" + kCoverModelName + ".onnx";
        const QString labelsPath = baseDir + "/models/game-cover.names";

        if (!QFile::exists(modelPath)) {
            LOG_WARN() << "Cover detection model not found:" << modelPath;
            return;
        }

        try {
            detector = std::make_unique<yolos::det::YOLODetector>(
                modelPath.toStdString(),
                labelsPath.toStdString(),
                /*useGPU=*/false);
        } catch (const std::exception& e) {
            LOG_WARN() << "Failed to load cover detection model:" << e.what();
            detector.reset();
        }
    });

    return detector.get();
}

QStringList readNoCoverUrls(const QString& markerPath)
{
    QFile marker(markerPath);
    if (!marker.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(marker.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

} // namespace

CoverExtractor::CoverExtractor(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

CoverExtractor::~CoverExtractor()
{
}

void CoverExtractor::extractCoverToCache(const QString& imageUrl,
                                         const QString& gameId,
                                         std::function<void(CoverResult)> callback)
{
    QNetworkRequest request{QUrl(imageUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    // Without a limit a stalled screenshot host keeps the spinner up forever.
    request.setTransferTimeout(30000);

    QNetworkReply* reply = m_networkManager->get(request);

    // Paths are resolved here on the GUI thread; the worker only gets strings.
    const QString coverPath = cachedCoverPath(gameId);
    const QString markerPath = noCoverMarkerPath(gameId);

    // Bind the callback to this specific reply so overlapping requests never
    // clobber one another (the previous shared-member design crossed results
    // when modifiers were switched quickly).
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, callback, coverPath, markerPath, imageUrl]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            callback(CoverResult::Failed);
            return;
        }

        const QByteArray imageData = reply->readAll();

        // Decode, model inference and PNG encoding are CPU-heavy; run them on
        // a worker thread so the GUI stays responsive.
        auto* watcher = new QFutureWatcher<CoverResult>(this);
        connect(watcher, &QFutureWatcher<CoverResult>::finished, this, [watcher, callback]() {
            const CoverResult result = watcher->result();
            watcher->deleteLater();
            callback(result);
        });
        watcher->setFuture(QtConcurrent::run(&CoverExtractor::extractCoverDataToFile,
                                             imageData, coverPath, markerPath, imageUrl));
    });
}

void CoverExtractor::warmUpModelAsync()
{
    (void)QtConcurrent::run([]() {
        (void)coverDetector();
    });
}

CoverExtractor::CoverResult CoverExtractor::extractCoverDataToFile(const QByteArray& imageData,
                                                                   const QString& coverPath,
                                                                   const QString& markerPath,
                                                                   const QString& imageUrl)
{
    QImage img;
    if (!img.loadFromData(imageData)) {
        return CoverResult::Failed;
    }
    // A missing or broken model says nothing about this screenshot, so it
    // must not be remembered as "no cover".
    if (!coverDetector()) {
        return CoverResult::Failed;
    }
    if (img.format() != QImage::Format_RGB888) {
        img = img.convertToFormat(QImage::Format_RGB888);
    }

    // Wrap the QImage buffer as an RGB cv::Mat (extractCoverByModel clones the
    // crop, so the wrapper only needs to stay valid for the call).
    cv::Mat rgb(img.height(), img.width(), CV_8UC3,
                const_cast<uchar*>(img.bits()),
                static_cast<size_t>(img.bytesPerLine()));

    bool inferenceFailed = false;
    cv::Mat cover = extractCoverByModel(rgb, &inferenceFailed);
    if (inferenceFailed) {
        return CoverResult::Failed;
    }
    if (cover.empty() || cover.channels() != 3) {
        QFile marker(markerPath);
        if (marker.open(QIODevice::Append | QIODevice::Text)) {
            marker.write(imageUrl.toUtf8() + '\n');
        }
        return CoverResult::NoCover;
    }

    const QImage out(cover.data, cover.cols, cover.rows,
                     static_cast<int>(cover.step), QImage::Format_RGB888);

    // QSaveFile writes to a unique temp file and swaps it in, so neither QML
    // nor hasCachedCover() ever sees a half-written PNG, and two extractions
    // for one game cannot interleave their bytes.
    QSaveFile file(coverPath);
    if (!file.open(QIODevice::WriteOnly) || !out.save(&file, "PNG") || !file.commit()) {
        return CoverResult::Failed;
    }
    return CoverResult::Saved;
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

cv::Mat CoverExtractor::extractCoverByModel(const cv::Mat& rgbImage, bool* inferenceFailed)
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
        LOG_WARN() << "Model-based cover extraction failed:" << e.what();
        if (inferenceFailed) {
            *inferenceFailed = true;
        }
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
        LOG_DEBUG() << "Mat to QPixmap conversion failed:" << e.what();
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
        LOG_DEBUG() << "QPixmap to Mat conversion failed:" << e.what();
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

QString CoverExtractor::cachedCoverPath(const QString& gameId)
{
    return QDir(getCacheDirectory()).absoluteFilePath(gameId + ".png");
}

QString CoverExtractor::noCoverMarkerPath(const QString& gameId)
{
    return QDir(getCacheDirectory()).absoluteFilePath(
        gameId + "." + kCoverModelName + ".nocover");
}

bool CoverExtractor::hasCachedCover(const QString& gameId)
{
    if (gameId.isEmpty()) {
        return false;
    }
    const QFileInfo info(cachedCoverPath(gameId));
    return info.isFile() && info.size() > 0;
}

bool CoverExtractor::isKnownWithoutCover(const QString& gameId, const QString& imageUrl)
{
    if (gameId.isEmpty() || imageUrl.isEmpty()) {
        return false;
    }
    // Keyed by screenshot URL: a trainer page that gets a new screenshot is
    // worth another look.
    return readNoCoverUrls(noCoverMarkerPath(gameId)).contains(imageUrl);
}
