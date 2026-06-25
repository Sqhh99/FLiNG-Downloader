#pragma once

#include <QString>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <functional>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

/**
 * Game Cover Extractor
 * Uses a YOLO object-detection model (ONNX Runtime) to extract game covers
 * from modifier interface screenshots.
 */
class CoverExtractor : public QObject
{
    Q_OBJECT

public:
    explicit CoverExtractor(QObject* parent = nullptr);
    ~CoverExtractor();

    // Extract game cover from modifier screenshot URL
    void extractCoverFromTrainerImage(const QString& imageUrl, 
                                     std::function<void(const QPixmap&, bool)> callback);

    // Extract game cover from local image file
    static QPixmap extractCoverFromLocalImage(const QString& imagePath);

    // Get cached cover (if exists)
    static QPixmap getCachedCover(const QString& gameId);

    // Save cover to cache
    static bool saveCoverToCache(const QString& gameId, const QPixmap& cover);

    // Get cache directory path
    static QString getCacheDirectory();

private slots:
    void onImageDownloaded();

private:
    // Detect and crop the game cover using the YOLO ONNX model.
    // Input is RGB (as produced by qPixmapToMat); detection runs on a BGR copy
    // internally. Returns the cropped cover in RGB, or an empty Mat on failure.
    static cv::Mat extractCoverByModel(const cv::Mat& rgbImage);

    // Image conversion tools
    static QPixmap matToQPixmap(const cv::Mat& mat);
    static cv::Mat qPixmapToMat(const QPixmap& pixmap);
    
    // Image processing: extract cover region from modifier screenshot
    static QPixmap processTrainerImage(const QPixmap& originalImage);

private:
    QNetworkAccessManager* m_networkManager;
    std::function<void(const QPixmap&, bool)> m_callback;
};
