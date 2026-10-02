#pragma once

#include <QString>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QImage>
#include <QByteArray>
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

    enum class CoverResult {
        Saved,    // cover written to cachedCoverPath(gameId)
        NoCover,  // the model found no cover; remembered for this screenshot
        Failed    // network, decode, model-load or disk error; worth retrying later
    };

    // Download the trainer screenshot, crop the cover and write it to the
    // cover cache. Decoding, inference and PNG encoding all run on a worker
    // thread; callback runs on this object's thread.
    void extractCoverToCache(const QString& imageUrl,
                             const QString& gameId,
                             std::function<void(CoverResult)> callback);

    // Load the detection model on a worker thread so the first cover does not
    // pay for it. Safe to call more than once.
    static void warmUpModelAsync();

    // Extract game cover from local image file
    static QPixmap extractCoverFromLocalImage(const QString& imagePath);

    // Cheap existence check for a cached cover; does not decode the image.
    static bool hasCachedCover(const QString& gameId);
    static QString cachedCoverPath(const QString& gameId);

    // Whether the model already found no cover in this screenshot.
    static bool isKnownWithoutCover(const QString& gameId, const QString& imageUrl);

    // Get cache directory path
    static QString getCacheDirectory();

private:
    // Worker-thread half of extractCoverToCache(): decode, detect, crop and
    // save. Touches only QImage/cv::Mat and files, never QPixmap.
    static CoverResult extractCoverDataToFile(const QByteArray& imageData,
                                              const QString& coverPath,
                                              const QString& markerPath,
                                              const QString& imageUrl);
    static QString noCoverMarkerPath(const QString& gameId);

    // Detect and crop the game cover using the YOLO ONNX model.
    // Input is RGB (as produced by qPixmapToMat); detection runs on a BGR copy
    // internally. Returns the cropped cover in RGB, or an empty Mat when there
    // is none; inferenceFailed (optional) tells an error apart from "none".
    static cv::Mat extractCoverByModel(const cv::Mat& rgbImage, bool* inferenceFailed = nullptr);

    // Image conversion tools
    static QPixmap matToQPixmap(const cv::Mat& mat);
    static cv::Mat qPixmapToMat(const QPixmap& pixmap);
    
    // Image processing: extract cover region from modifier screenshot
    static QPixmap processTrainerImage(const QPixmap& originalImage);

private:
    QNetworkAccessManager* m_networkManager;
};
