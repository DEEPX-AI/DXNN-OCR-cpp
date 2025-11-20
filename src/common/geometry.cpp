#include "common/geometry.h"
#include "common/logger.hpp"
#include <algorithm>
#include <cmath>

namespace ocr {

float Geometry::distance(const cv::Point2f& p1, const cv::Point2f& p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

float Geometry::polygonArea(const std::vector<cv::Point>& polygon) {
    return cv::contourArea(polygon);
}

cv::RotatedRect Geometry::minAreaRect(const std::vector<cv::Point>& points) {
    return cv::minAreaRect(points);
}

std::vector<cv::Point2f> Geometry::orderPointsClockwise(const std::vector<cv::Point2f>& points) {
    if (points.size() != 4) {
        return points;
    }

    // 复制点
    std::vector<cv::Point2f> pts = points;
    
    // 计算中心点
    cv::Point2f center(0, 0);
    for (const auto& pt : pts) {
        center.x += pt.x;
        center.y += pt.y;
    }
    center.x /= 4;
    center.y /= 4;

    // 按照与中心点的角度排序
    std::sort(pts.begin(), pts.end(), [&center](const cv::Point2f& a, const cv::Point2f& b) {
        float angle_a = std::atan2(a.y - center.y, a.x - center.x);
        float angle_b = std::atan2(b.y - center.y, b.x - center.x);
        return angle_a < angle_b;
    });

    // 找到左上角的点（x+y最小）
    int top_left_idx = 0;
    float min_sum = pts[0].x + pts[0].y;
    for (int i = 1; i < 4; i++) {
        float sum = pts[i].x + pts[i].y;
        if (sum < min_sum) {
            min_sum = sum;
            top_left_idx = i;
        }
    }

    // 旋转数组，让左上角点在第一个位置
    std::vector<cv::Point2f> ordered;
    for (int i = 0; i < 4; i++) {
        ordered.push_back(pts[(top_left_idx + i) % 4]);
    }

    return ordered;
}

cv::Mat Geometry::getRotateCropImage(const cv::Mat& image,
                                     const std::vector<cv::Point2f>& box) {
    return cropTextRegion(image, box);
}

cv::Mat Geometry::cropTextRegion(const cv::Mat& image,
                                 const std::vector<cv::Point2f>& box) {
    if (box.size() != 4) {
        return cv::Mat();
    }

    // 排序点：左上、右上、右下、左下
    std::vector<cv::Point2f> ordered_pts = orderPointsClockwise(box);

    // 计算宽度和高度
    float width1 = distance(ordered_pts[0], ordered_pts[1]);
    float width2 = distance(ordered_pts[2], ordered_pts[3]);
    float height1 = distance(ordered_pts[0], ordered_pts[3]);
    float height2 = distance(ordered_pts[1], ordered_pts[2]);

    float max_width = std::max(width1, width2);
    float max_height = std::max(height1, height2);

    // DON'T resize here! Keep original dimensions.
    // Let Recognition's Preprocess do the PPOCRResize (pad + resize)
    // Use round() for dimension calculation (balanced approach)
    int crop_width = static_cast<int>(std::round(max_width));
    int crop_height = static_cast<int>(std::round(max_height));

    // 目标点（矩形，保持原始尺寸）
    // IMPORTANT: Match Python's pts_std coordinates exactly
    std::vector<cv::Point2f> dst_pts = {
        cv::Point2f(0, 0),
        cv::Point2f(crop_width, 0),
        cv::Point2f(crop_width, crop_height),
        cv::Point2f(0, crop_height)
    };

    // 透视变换（保持原始尺寸）
    // IMPORTANT: Match Python's warpPerspective parameters:
    // - flags=cv2.INTER_CUBIC (bicubic interpolation)
    // - borderMode=cv2.BORDER_REPLICATE (replicate border pixels)
    cv::Mat M = cv::getPerspectiveTransform(ordered_pts, dst_pts);
    cv::Mat warped;
    cv::warpPerspective(image, warped, M, cv::Size(crop_width, crop_height),
                       cv::INTER_CUBIC, cv::BORDER_REPLICATE);

    // IMPORTANT: For vertical text (height > width * 2), rotate the CROPPED IMAGE 90 degrees counterclockwise
    // This matches Python's rotate_if_vertical logic which rotates the crop AFTER extraction
    if (crop_height > crop_width * 2) {
        cv::rotate(warped, warped, cv::ROTATE_90_COUNTERCLOCKWISE);
    }

    return warped;
}

float Geometry::getScore(const std::vector<cv::Point>& polygon, const cv::Mat& bitmap) {
    if (polygon.empty() || bitmap.empty()) {
        return 0.0f;
    }

    // 创建掩码
    cv::Mat mask = cv::Mat::zeros(bitmap.size(), CV_8UC1);
    std::vector<std::vector<cv::Point>> contours = {polygon};
    cv::fillPoly(mask, contours, cv::Scalar(255));

    // 计算平均值
    cv::Scalar mean_val = cv::mean(bitmap, mask);
    return static_cast<float>(mean_val[0]) / 255.0f;
}

std::vector<cv::Point> Geometry::expandPolygon(const std::vector<cv::Point>& polygon,
                                               float ratio) {
    if (polygon.empty() || ratio <= 1.0f) {
        return polygon;
    }

    // 计算中心点
    cv::Point2f center(0, 0);
    for (const auto& pt : polygon) {
        center.x += pt.x;
        center.y += pt.y;
    }
    center.x /= polygon.size();
    center.y /= polygon.size();

    // 扩展每个点
    std::vector<cv::Point> expanded;
    for (const auto& pt : polygon) {
        cv::Point2f vec(pt.x - center.x, pt.y - center.y);
        cv::Point2f new_pt = center + vec * ratio;
        expanded.push_back(cv::Point(static_cast<int>(new_pt.x), 
                                    static_cast<int>(new_pt.y)));
    }

    return expanded;
}

std::vector<cv::Point> Geometry::approximatePolygon(const std::vector<cv::Point>& polygon,
                                                    double epsilon) {
    std::vector<cv::Point> approx;
    cv::approxPolyDP(polygon, approx, epsilon, true);
    return approx;
}

std::vector<cv::Point2f> Geometry::getMinBoxPoints(const std::vector<cv::Point>& points) {
    cv::RotatedRect rect = cv::minAreaRect(points);
    cv::Point2f vertices[4];
    rect.points(vertices);
    
    std::vector<cv::Point2f> box_points;
    for (int i = 0; i < 4; i++) {
        box_points.push_back(vertices[i]);
    }
    
    return orderPointsClockwise(box_points);
}

bool Geometry::isPointInPolygon(const cv::Point2f& point,
                               const std::vector<cv::Point>& polygon) {
    double result = cv::pointPolygonTest(polygon, point, false);
    return result >= 0;
}

float Geometry::calculateIoU(const cv::Rect& rect1, const cv::Rect& rect2) {
    cv::Rect intersection = rect1 & rect2;
    float inter_area = intersection.area();
    
    float union_area = rect1.area() + rect2.area() - inter_area;
    
    if (union_area == 0) {
        return 0.0f;
    }
    
    return inter_area / union_area;
}

} // namespace ocr
