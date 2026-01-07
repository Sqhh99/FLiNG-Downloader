#include "CoverExtractor.h"
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QDebug>
#include <QBuffer>
#include <QImageReader>
#include <algorithm>
#include <vector>

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
    
    cv::Mat image = qPixmapToMat(originalImage);
    if (image.empty()) {
        return QPixmap();
    }
    
    cv::Mat cover = extractCoverByShapeAnalysis(image);
    
    if (cover.empty()) {
        return QPixmap();
    }
    
    return matToQPixmap(cover);
}

cv::Mat CoverExtractor::extractCoverByShapeAnalysis(const cv::Mat& image)
{
    try {
        int height = image.rows;
        int width = image.cols;
        
        // Smart scaling
        cv::Mat processImage = image;
        float scale = 1.0f;
        const int MAX_PROCESS_WIDTH = 600;
        if (width > MAX_PROCESS_WIDTH) {
            scale = static_cast<float>(MAX_PROCESS_WIDTH) / width;
            cv::resize(image, processImage, cv::Size(), scale, scale, cv::INTER_AREA);
        }
        
        // Cover is usually in the left 30-35% of the image
        int roiWidth = static_cast<int>(processImage.cols * 0.38);
        int roiHeight = static_cast<int>(processImage.rows * 0.9);
        cv::Rect roiRect(0, 0, roiWidth, roiHeight);
        cv::Mat roi = processImage(roiRect);
        
        // Detect cover right boundary
        int coverRightBound = detectCoverRightBoundary(roi);
        if (coverRightBound > 0 && coverRightBound < roiWidth * 0.95) {
            // Shrink ROI to detected boundary
            roiWidth = coverRightBound + 5;  // Leave some margin
            roiRect = cv::Rect(0, 0, roiWidth, roiHeight);
            roi = processImage(roiRect);
        }
        
        // Find cover candidate regions
        std::vector<CoverCandidate> candidates = findCoverCandidates(roi);
        
        // Filter out candidates that are clearly not covers (too wide)
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [roiWidth](const CoverCandidate& c) {
                    // Cover width should not exceed 90% of ROI width
                    return c.w > roiWidth * 0.92;
                }),
            candidates.end()
        );
        
        // If main method fails, try color segmentation
        if (candidates.empty()) {
            candidates = findCoverByColorSegmentation(roi);
            
            // Apply same filter
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [roiWidth](const CoverCandidate& c) {
                        return c.w > roiWidth * 0.92;
                    }),
                candidates.end()
            );
        }
        
        if (!candidates.empty()) {
            // Select best candidate, preferring portrait orientation and not too wide
            auto best_it = std::max_element(candidates.begin(), candidates.end(),
                [](const CoverCandidate& a, const CoverCandidate& b) {
                    double aspectA = static_cast<double>(a.w) / a.h;
                    double aspectB = static_cast<double>(b.w) / b.h;
                    
                    // Bonus for portrait covers (aspect ratio 0.6-0.85 is ideal)
                    double aspectBonusA = (aspectA >= 0.55 && aspectA <= 0.9) ? 1.5 : 1.0;
                    double aspectBonusB = (aspectB >= 0.55 && aspectB <= 0.9) ? 1.5 : 1.0;
                    
                    double scoreA = a.area * a.quality * aspectBonusA;
                    double scoreB = b.area * b.quality * aspectBonusB;
                    return scoreA < scoreB;
                });
            
            const CoverCandidate& best = *best_it;
            
            // Extract cover
            cv::Mat cover;
            if (scale < 1.0f) {
                // Map back to original image
                int origRoiWidth = static_cast<int>(width * 0.38);
                if (coverRightBound > 0) {
                    origRoiWidth = static_cast<int>((coverRightBound + 5) / scale);
                }
                int origRoiHeight = static_cast<int>(height * 0.9);
                
                cv::Rect coverRect(
                    static_cast<int>(best.x / scale),
                    static_cast<int>(best.y / scale),
                    static_cast<int>(best.w / scale),
                    static_cast<int>(best.h / scale)
                );
                
                coverRect.x = std::max(0, std::min(coverRect.x, origRoiWidth - 1));
                coverRect.y = std::max(0, std::min(coverRect.y, origRoiHeight - 1));
                coverRect.width = std::min(coverRect.width, origRoiWidth - coverRect.x);
                coverRect.height = std::min(coverRect.height, origRoiHeight - coverRect.y);
                
                cv::Rect origRoiRect(0, 0, 
                    std::min(origRoiWidth, width), 
                    std::min(origRoiHeight, height));
                cv::Mat origRoi = image(origRoiRect);
                cover = origRoi(coverRect).clone();
            } else {
                cv::Rect coverRect(best.x, best.y, best.w, best.h);
                cover = roi(coverRect).clone();
            }
            
            // Border optimization
            if (cover.cols > 80 && cover.rows > 100) {
                cv::Mat optimizedCover = removeCoverBorders(cover);
                return optimizedCover.empty() ? cover : optimizedCover;
            }
            
            return cover;
        } else {
            return extractFallbackByPosition(roi);
        }
        
    } catch (const std::exception& e) {
        return cv::Mat();
    }
}

// Detect cover right boundary (find the dividing line between cover and options list)
int CoverExtractor::detectCoverRightBoundary(const cv::Mat& roi)
{
    try {
        int height = roi.rows;
        int width = roi.cols;
        
        // Convert to grayscale
        cv::Mat gray;
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        
        // Method 1: Detect vertical edges (cover right side usually has clear vertical boundary)
        cv::Mat sobelX;
        cv::Sobel(gray, sobelX, CV_16S, 1, 0, 3);
        cv::Mat absSobelX;
        cv::convertScaleAbs(sobelX, absSobelX);
        
        // Calculate vertical edge strength for each column
        std::vector<double> colEdgeStrength(width, 0);
        for (int x = 0; x < width; ++x) {
            cv::Scalar sum = cv::sum(absSobelX.col(x));
            colEdgeStrength[x] = sum[0] / height;
        }
        
        // Look for strong vertical edges in the 20%-80% range
        int searchStart = static_cast<int>(width * 0.2);
        int searchEnd = static_cast<int>(width * 0.85);
        
        double maxStrength = 0;
        int maxPos = -1;
        
        for (int x = searchStart; x < searchEnd; ++x) {
            // Look for local maximum (edge)
            if (colEdgeStrength[x] > maxStrength && colEdgeStrength[x] > 30) {
                // Check if this position is a continuous vertical line
                // by checking the edge strength consistency across rows
                bool isVerticalLine = true;
                double avgStrength = colEdgeStrength[x];
                
                // Simple check: if this column's edge strength is significantly higher than average
                double avgAllCols = 0;
                for (int i = searchStart; i < searchEnd; ++i) {
                    avgAllCols += colEdgeStrength[i];
                }
                avgAllCols /= (searchEnd - searchStart);
                
                if (colEdgeStrength[x] > avgAllCols * 1.5) {
                    maxStrength = colEdgeStrength[x];
                    maxPos = x;
                }
            }
        }
        
        // Method 2: Detect color changes (cover area is colorful, options area is monotone)
        if (maxPos < 0) {
            std::vector<double> colColorVariance(width, 0);
            
            for (int x = 0; x < width; ++x) {
                cv::Mat col = roi.col(x);
                cv::Scalar mean, stddev;
                cv::meanStdDev(col, mean, stddev);
                // Color variance = sum of standard deviations across channels
                colColorVariance[x] = stddev[0] + stddev[1] + stddev[2];
            }
            
            // Find position where color variance suddenly drops (entering options area from cover)
            for (int x = searchStart; x < searchEnd - 10; ++x) {
                double leftVariance = 0, rightVariance = 0;
                for (int i = 0; i < 10; ++i) {
                    leftVariance += colColorVariance[x - 5 + i];
                    rightVariance += colColorVariance[x + 5 + i];
                }
                leftVariance /= 10;
                rightVariance /= 10;
                
                // If left side is colorful, right side is monotone
                if (leftVariance > rightVariance * 1.8 && leftVariance > 20) {
                    maxPos = x;
                    break;
                }
            }
        }
        
        return maxPos;
        
    } catch (const std::exception& e) {
        return -1;
    }
}

std::vector<CoverCandidate> CoverExtractor::findCoverCandidates(const cv::Mat& roi)
{
    std::vector<CoverCandidate> candidates;
    
    try {
        int height = roi.rows;
        int width = roi.cols;
        
        // Convert to grayscale
        cv::Mat gray;
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        
        // Apply robust edge detection
        cv::Mat edges = applyRobustEdgeDetection(gray);
        
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        // Dynamically set minimum area threshold based on image size
        double minArea = std::max(3000.0, (width * height) * 0.01);
        
        for (size_t i = 0; i < contours.size(); ++i) {
            double area = cv::contourArea(contours[i]);
            
            if (area > minArea) {
                cv::Rect boundRect = cv::boundingRect(contours[i]);
                double aspectRatio = static_cast<double>(boundRect.width) / boundRect.height;
                
                // Relaxed aspect ratio constraints for various cover types
                bool validAspectRatio = (aspectRatio > 0.45 && aspectRatio < 1.3);
                bool validSize = (boundRect.width > 60 && boundRect.height > 80);
                bool validPosition = (boundRect.x >= 0 && boundRect.y >= 0);
                
                if (validAspectRatio && validSize && validPosition) {
                    cv::Mat coverRegion = roi(boundRect);
                    double quality = calculateRegionQuality(coverRegion);
                    
                    double positionBonus = 1.0 + (1.0 - static_cast<double>(boundRect.x) / width) * 0.3;
                    quality *= positionBonus;
                    
                    candidates.emplace_back(boundRect.x, boundRect.y, 
                                          boundRect.width, boundRect.height,
                                          area, quality, static_cast<int>(i));
                    
                    if (area > minArea * 5 && quality > 1.5) {
                        break;
                    }
                }
            }
        }
        
        // Fallback strategy: relax conditions if no candidates found
        if (candidates.empty() && !contours.empty()) {
            for (size_t i = 0; i < contours.size(); ++i) {
                double area = cv::contourArea(contours[i]);
                
                if (area > 1500) {
                    cv::Rect boundRect = cv::boundingRect(contours[i]);
                    
                    if (boundRect.width > 40 && boundRect.height > 50) {
                        cv::Mat coverRegion = roi(boundRect);
                        double quality = calculateRegionQuality(coverRegion);
                        
                        candidates.emplace_back(boundRect.x, boundRect.y,
                                              boundRect.width, boundRect.height,
                                              area, quality, static_cast<int>(i));
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        // Silently handle exceptions
    }
    
    return candidates;
}

// Use color segmentation method to find cover candidate regions
std::vector<CoverCandidate> CoverExtractor::findCoverByColorSegmentation(const cv::Mat& roi)
{
    std::vector<CoverCandidate> candidates;
    
    try {
        int height = roi.rows;
        int width = roi.cols;
        
        // Convert to HSV color space
        cv::Mat hsv;
        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
        
        // Analyze background color (usually dark or solid color)
        // Modifier interface background is usually dark gray or black
        cv::Mat mask;
        
        // Method 1: Detect non-background regions (non-dark areas)
        cv::Mat grayRoi;
        cv::cvtColor(roi, grayRoi, cv::COLOR_BGR2GRAY);
        
        // Use adaptive threshold for segmentation
        cv::Mat binary;
        cv::adaptiveThreshold(grayRoi, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, 
                             cv::THRESH_BINARY, 15, -5);
        
        // Morphological operation: closing to fill holes
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
        cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);
        
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        double minArea = std::max(2000.0, (width * height) * 0.008);
        
        for (size_t i = 0; i < contours.size(); ++i) {
            double area = cv::contourArea(contours[i]);
            
            if (area > minArea) {
                cv::Rect boundRect = cv::boundingRect(contours[i]);
                double aspectRatio = static_cast<double>(boundRect.width) / boundRect.height;
                
                // Check if it matches cover characteristics
                if (aspectRatio > 0.4 && aspectRatio < 1.4 &&
                    boundRect.width > 50 && boundRect.height > 70) {
                    
                    cv::Mat coverRegion = roi(boundRect);
                    double quality = calculateRegionQuality(coverRegion);
                    
                    // Color richness check
                    cv::Scalar meanColor = cv::mean(coverRegion);
                    double colorVariance = std::abs(meanColor[0] - meanColor[1]) + 
                                          std::abs(meanColor[1] - meanColor[2]);
                    
                    // Covers usually have rich colors
                    if (colorVariance > 5 || quality > 0.5) {
                        candidates.emplace_back(boundRect.x, boundRect.y,
                                              boundRect.width, boundRect.height,
                                              area, quality, static_cast<int>(i));
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Color segmentation error:" << e.what();
    }
    
    return candidates;
}

cv::Mat CoverExtractor::applyRobustEdgeDetection(const cv::Mat& gray)
{
    cv::Mat edges;
    
    try {
        // Gaussian blur to reduce noise
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);
        
        // Optimization: use single Canny edge detection instead of three (3x speed improvement)
        cv::Canny(blurred, edges, 50, 120);
        
        // Simplified morphological operation (speed improvement)
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, kernel);
        
    } catch (const std::exception& e) {
        qDebug() << "Edge detection error:" << e.what();
        return cv::Mat();
    }
    
    return edges;
}

double CoverExtractor::calculateRegionQuality(const cv::Mat& region)
{
    try {
        if (region.empty()) {
            return 0.0;
        }
        
        int height = region.rows;
        int width = region.cols;
        
        // 1. Size score - covers usually have a certain size
        double sizeScore = std::min(2.0, (width * height) / 40000.0);
        
        // 2. Aspect ratio score - game covers are usually portrait (0.6-0.85)
        double aspectRatio = static_cast<double>(width) / height;
        double aspectScore = 0.5;
        if (aspectRatio >= 0.55 && aspectRatio <= 0.9) {
            aspectScore = 1.5;  // Ideal portrait cover
        } else if (aspectRatio >= 0.45 && aspectRatio <= 1.1) {
            aspectScore = 1.0;  // Acceptable ratio
        }
        
        // 3. Texture complexity score - covers usually have rich details
        cv::Mat gray;
        cv::cvtColor(region, gray, cv::COLOR_BGR2GRAY);
        
        cv::Scalar meanScalar, stdScalar;
        cv::meanStdDev(gray, meanScalar, stdScalar);
        double textureScore = std::min(2.0, stdScalar[0] / 25.0);
        
        // 4. Brightness score - covers are usually not too dark or too bright
        double meanBrightness = meanScalar[0];
        double brightnessScore = 0.3;
        if (meanBrightness > 40 && meanBrightness < 200) {
            brightnessScore = 1.0;
        } else if (meanBrightness > 25 && meanBrightness < 230) {
            brightnessScore = 0.7;
        }
        
        // 5. Color richness score (fast calculation)
        cv::Scalar meanColor = cv::mean(region);
        double colorVariance = std::abs(meanColor[0] - meanColor[1]) + 
                              std::abs(meanColor[1] - meanColor[2]) +
                              std::abs(meanColor[0] - meanColor[2]);
        double colorScore = std::min(1.5, colorVariance / 30.0);
        
        // Combined score
        double qualityScore = (
            sizeScore * 0.3 +      // Size weight
            aspectScore * 0.25 +   // Aspect ratio weight
            textureScore * 0.25 +  // Texture weight
            brightnessScore * 0.1 + // Brightness weight
            colorScore * 0.1       // Color weight
        );
        
        return std::max(0.1, std::min(qualityScore, 10.0));
        
    } catch (const std::exception& e) {
        qDebug() << "Quality calculation error:" << e.what();
        return 0.5;
    }
}

cv::Mat CoverExtractor::extractFallbackByPosition(const cv::Mat& roi)
{
    try {
        int height = roi.rows;
        int width = roi.cols;
        
        // More fallback positions, covering different modifier interface layouts
        std::vector<cv::Rect> positions = {
            // Standard top-left position
            cv::Rect(static_cast<int>(width * 0.02), static_cast<int>(height * 0.08), 
                    static_cast<int>(width * 0.4), static_cast<int>(height * 0.65)),
            // Slightly right position
            cv::Rect(static_cast<int>(width * 0.05), static_cast<int>(height * 0.1), 
                    static_cast<int>(width * 0.35), static_cast<int>(height * 0.55)),
            // Closer to edge
            cv::Rect(static_cast<int>(width * 0.01), static_cast<int>(height * 0.05), 
                    static_cast<int>(width * 0.45), static_cast<int>(height * 0.7)),
            // Center-left
            cv::Rect(static_cast<int>(width * 0.08), static_cast<int>(height * 0.12), 
                    static_cast<int>(width * 0.32), static_cast<int>(height * 0.5)),
            // Larger range
            cv::Rect(static_cast<int>(width * 0.0), static_cast<int>(height * 0.02), 
                    static_cast<int>(width * 0.5), static_cast<int>(height * 0.75))
        };
        
        cv::Mat bestCover;
        double bestScore = 0;
        
        for (size_t i = 0; i < positions.size(); ++i) {
            cv::Rect pos = positions[i];
            
            // Ensure not exceeding boundaries
            pos.x = std::max(0, std::min(pos.x, width - 1));
            pos.y = std::max(0, std::min(pos.y, height - 1));
            pos.width = std::min(pos.width, width - pos.x);
            pos.height = std::min(pos.height, height - pos.y);
            
            if (pos.width > 40 && pos.height > 60) {
                cv::Mat cover = roi(pos);
                double score = calculateRegionQuality(cover);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestCover = cover.clone();
                }
            }
        }
        
        return bestScore > 0.5 ? bestCover : cv::Mat();
        
    } catch (const std::exception& e) {
        qDebug() << "Fallback position extraction failed:" << e.what();
        return cv::Mat();
    }
}

cv::Mat CoverExtractor::removeCoverBorders(const cv::Mat& coverImage)
{
    if (coverImage.empty()) {
        return cv::Mat();
    }
    
    try {
        // Convert to grayscale
        cv::Mat gray;
        cv::cvtColor(coverImage, gray, cv::COLOR_BGR2GRAY);
        
        // Edge detection
        cv::Mat edges;
        cv::Canny(gray, edges, 30, 100);
        
        // Morphological operation
        cv::Mat kernel = cv::Mat::ones(3, 3, CV_8U);
        cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, kernel);
        
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (!contours.empty()) {
            // Find largest contour
            auto largestContour = std::max_element(contours.begin(), contours.end(),
                [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                    return cv::contourArea(a) < cv::contourArea(b);
                });
            
            cv::Rect boundRect = cv::boundingRect(*largestContour);
            
            // Check if crop area is reasonable
            int originalArea = coverImage.rows * coverImage.cols;
            int cropArea = boundRect.width * boundRect.height;
            
            if (cropArea > originalArea * 0.5 &&
                boundRect.width > coverImage.cols * 0.6 &&
                boundRect.height > coverImage.rows * 0.6) {
                
                // Add small margin
                int margin = 3;
                boundRect.x = std::max(0, boundRect.x - margin);
                boundRect.y = std::max(0, boundRect.y - margin);
                boundRect.width = std::min(coverImage.cols - boundRect.x, boundRect.width + 2 * margin);
                boundRect.height = std::min(coverImage.rows - boundRect.y, boundRect.height + 2 * margin);
                
                return coverImage(boundRect).clone();
            }
        }
        
        return coverImage;
        
    } catch (const std::exception& e) {
        qDebug() << "Border removal failed:" << e.what();
        return coverImage;
    }
}

QPixmap CoverExtractor::matToQPixmap(const cv::Mat& mat)
{
    try {
        if (mat.empty()) {
            return QPixmap();
        }
        
        cv::Mat rgbMat;
        if (mat.channels() == 3) {
            cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
        } else if (mat.channels() == 4) {
            cv::cvtColor(mat, rgbMat, cv::COLOR_BGRA2RGBA);
        } else {
            cv::cvtColor(mat, rgbMat, cv::COLOR_GRAY2RGB);
        }
        
        QImage qimg(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                   static_cast<int>(rgbMat.step), QImage::Format_RGB888);
        return QPixmap::fromImage(qimg);
        
    } catch (const std::exception& e) {
        qDebug() << "Mat to QPixmap conversion failed:" << e.what();
        return QPixmap();
    }
}

cv::Mat CoverExtractor::qPixmapToMat(const QPixmap& pixmap)
{
    try {
        QImage qimg = pixmap.toImage();
        qimg = qimg.convertToFormat(QImage::Format_RGB888);
        
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC3, 
                   const_cast<uchar*>(qimg.bits()), 
                   static_cast<size_t>(qimg.bytesPerLine()));
        
        cv::Mat result;
        cv::cvtColor(mat, result, cv::COLOR_RGB2BGR);
        return result.clone();
        
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
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
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