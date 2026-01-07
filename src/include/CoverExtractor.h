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
 * 游戏封面提取器
 * 使用基于形状分析的方法从修改器界面截图中智能提取游戏封面
 */
class CoverExtractor : public QObject
{
    Q_OBJECT

public:
    explicit CoverExtractor(QObject* parent = nullptr);
    ~CoverExtractor();

    // 从修改器截图URL提取游戏封面
    void extractCoverFromTrainerImage(const QString& imageUrl, 
                                     std::function<void(const QPixmap&, bool)> callback);

    // 从本地图片文件提取游戏封面
    static QPixmap extractCoverFromLocalImage(const QString& imagePath);

    // 获取缓存的封面（如果存在）
    static QPixmap getCachedCover(const QString& gameId);

    // 保存封面到缓存
    static bool saveCoverToCache(const QString& gameId, const QPixmap& cover);

    // 获取缓存目录路径
    static QString getCacheDirectory();

private slots:
    void onImageDownloaded();

private:
    // 核心形状分析方法
    static cv::Mat extractCoverByShapeAnalysis(const cv::Mat& image);
    static cv::Mat extractFallbackByPosition(const cv::Mat& roi);
    static cv::Mat removeCoverBorders(const cv::Mat& coverImage);
    
    // 辅助分析方法
    static std::vector<CoverCandidate> findCoverCandidates(const cv::Mat& roi);
    static std::vector<CoverCandidate> findCoverByColorSegmentation(const cv::Mat& roi);
    static int detectCoverRightBoundary(const cv::Mat& roi);
    static cv::Mat applyRobustEdgeDetection(const cv::Mat& gray);
    static double calculateRegionQuality(const cv::Mat& region);
    
    // 图像转换工具
    static QPixmap matToQPixmap(const cv::Mat& mat);
    static cv::Mat qPixmapToMat(const QPixmap& pixmap);
    
    // 图像处理：从修改器截图中提取封面区域
    static QPixmap processTrainerImage(const QPixmap& originalImage);

private:
    QNetworkAccessManager* m_networkManager;
    std::function<void(const QPixmap&, bool)> m_callback;
};