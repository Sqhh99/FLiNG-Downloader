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

struct CoverCandidate {
    int x, y, w, h;
    double area;
    double quality;
    int contourIndex;
    
    CoverCandidate(int x, int y, int w, int h, double area, double quality, int idx)
        : x(x), y(y), w(w), h(h), area(area), quality(quality), contourIndex(idx) {}
};

/**
 * Game Cover Extractor
 * Uses shape analysis-based methods to intelligently extract game covers from modifier interface screenshots
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
    // Core shape analysis methods
    static cv::Mat extractCoverByShapeAnalysis(const cv::Mat& image);
    static cv::Mat extractFallbackByPosition(const cv::Mat& roi);
    static cv::Mat removeCoverBorders(const cv::Mat& coverImage);
    
    // Helper analysis methods
    static std::vector<CoverCandidate> findCoverCandidates(const cv::Mat& roi);
    static std::vector<CoverCandidate> findCoverByColorSegmentation(const cv::Mat& roi);
    static int detectCoverRightBoundary(const cv::Mat& roi);
    static cv::Mat applyRobustEdgeDetection(const cv::Mat& gray);
    static double calculateRegionQuality(const cv::Mat& region);
    
    // Image conversion tools
    static QPixmap matToQPixmap(const cv::Mat& mat);
    static cv::Mat qPixmapToMat(const QPixmap& pixmap);
    
    // Image processing: extract cover region from modifier screenshot
    static QPixmap processTrainerImage(const QPixmap& originalImage);

private:
    QNetworkAccessManager* m_networkManager;
    std::function<void(const QPixmap&, bool)> m_callback;
};