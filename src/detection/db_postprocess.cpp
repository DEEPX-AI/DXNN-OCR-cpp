#include "detection/db_postprocess.h"
#include "common/geometry.h"
#include "common/logger.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <clipper2/clipper.h>

namespace ocr {

DBPostProcessor::DBPostProcessor(float thresh,
                                 float box_thresh,
                                 int max_candidates,
                                 float unclip_ratio)
    : thresh_(thresh),
      box_thresh_(box_thresh),
      max_candidates_(max_candidates),
      unclip_ratio_(unclip_ratio) {
}

std::vector<DeepXOCR::TextBox> DBPostProcessor::process(const cv::Mat& pred, 
                                                         int src_h, int src_w,
                                                         int resized_h, int resized_w) {
    std::vector<DeepXOCR::TextBox> text_boxes;

    // If resized dimensions not provided, assume no padding
    if (resized_h <= 0) resized_h = src_h;
    if (resized_w <= 0) resized_w = src_w;

    // 二值化
    cv::Mat bitmap;
    cv::threshold(pred, bitmap, thresh_, 255, cv::THRESH_BINARY);
    bitmap.convertTo(bitmap, CV_8UC1);

    LOG_DEBUG("Binary threshold: %.2f, bitmap size: %dx%d, non-zero: %d", 
              thresh_, bitmap.cols, bitmap.rows, cv::countNonZero(bitmap));

    // 查找轮廓
    auto contours = findContours(bitmap);
    LOG_DEBUG("Found %zu contours", contours.size());

    // 3. 处理每个轮廓
    int num_contours = std::min(static_cast<int>(contours.size()), max_candidates_);
    
    for (int i = 0; i < num_contours; i++) {
        const auto& contour = contours[i];

        // 计算置信度分数
        float score = boxScoreFast(pred, contour);
        if (score < box_thresh_) {
            continue;
        }

        // 获取最小外接矩形
        float min_side;
        auto box = getMinBoxes(contour, min_side);
        
        if (min_side < 3) {  // 过滤太小的框
            continue;
        }

        // 扩展检测框
        auto unclipped_box = unclip(box);

        // Clipper2 may return a polygon with many points (e.g., 56 points for rounded corners)
        // Convert to minimum bounding rectangle (4 points)
        std::vector<cv::Point2f> final_box;
        if (unclipped_box.size() > 4) {
            // Convert Point2f to Point for minAreaRect
            std::vector<cv::Point> unclipped_contour;
            for (const auto& pt : unclipped_box) {
                unclipped_contour.push_back(cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)));
            }
            
            // Get minimum bounding rectangle
            cv::RotatedRect rect = cv::minAreaRect(unclipped_contour);
            cv::Point2f vertices[4];
            rect.points(vertices);
            
            for (int k = 0; k < 4; k++) {
                final_box.push_back(vertices[k]);
            }
            
            // Sort clockwise
            final_box = Geometry::orderPointsClockwise(final_box);
        } else {
            final_box = unclipped_box;
        }

        // **Coordinate mapping from model output space to original image space**
        // PPOCR preprocessing: Pad first to square, then resize
        // - Original image: src_h × src_w (e.g., 1800×1349)
        // - Padded to square: resized_h × resized_w (e.g., 1800×1800, added 451px on right)
        // - Resized to model input: pred.rows × pred.cols (e.g., 960×960)
        //
        // Mapping: model_output (960×960) → padded_space (1800×1800)
        // scale = padded_size / model_output_size
        // Coordinates in padded space ARE in original image space!
        // (because padding only adds black borders, doesn't change original content)
        
        float scale_x = static_cast<float>(resized_w) / pred.cols;
        float scale_y = static_cast<float>(resized_h) / pred.rows;
        
        // Debug first box
        static bool debug_first = true;
        if (debug_first && final_box.size() >= 4) {
            LOG_INFO("PPOCR mapping: pred %dx%d -> padded %dx%d, scale %.4f x %.4f",
                     pred.cols, pred.rows, resized_w, resized_h, scale_x, scale_y);
            LOG_INFO("  first point in pred: (%.1f, %.1f) -> padded/orig: (%.1f, %.1f)", 
                     final_box[0].x, final_box[0].y,
                     final_box[0].x * scale_x, final_box[0].y * scale_y);
            debug_first = false;
        }

        DeepXOCR::TextBox text_box;
        size_t num_points = std::min(static_cast<size_t>(4), final_box.size());
        for (size_t j = 0; j < num_points; j++) {
            // Map from model output to padded space (which is original image space + padding)
            float x = final_box[j].x * scale_x;
            float y = final_box[j].y * scale_y;
            
            // Clip to original image bounds
            text_box.points[j].x = std::clamp(x, 0.0f, static_cast<float>(src_w));
            text_box.points[j].y = std::clamp(y, 0.0f, static_cast<float>(src_h));
        }
        text_box.confidence = score;

        text_boxes.push_back(text_box);
    }

    return text_boxes;
}

std::vector<std::vector<cv::Point>> DBPostProcessor::findContours(const cv::Mat& bitmap) {
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    
    cv::findContours(bitmap, contours, hierarchy, 
                    cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    return contours;
}

std::vector<cv::Point2f> DBPostProcessor::getMinBoxes(const std::vector<cv::Point>& contour,
                                                      float& min_side) {
    cv::RotatedRect rect = cv::minAreaRect(contour);
    
    cv::Point2f vertices[4];
    rect.points(vertices);

    // 找到最小边长
    float width = rect.size.width;
    float height = rect.size.height;
    min_side = std::min(width, height);

    // 按顺序排列点（左上、右上、右下、左下）
    std::vector<cv::Point2f> box;
    for (int i = 0; i < 4; i++) {
        box.push_back(vertices[i]);
    }

    // 使用 Geometry 工具排序
    return Geometry::orderPointsClockwise(box);
}

float DBPostProcessor::boxScoreFast(const cv::Mat& bitmap,
                                    const std::vector<cv::Point>& contour) {
    // 获取轮廓的外接矩形
    cv::Rect rect = cv::boundingRect(contour);

    // 边界检查
    int xmin = std::max(0, rect.x);
    int ymin = std::max(0, rect.y);
    int xmax = std::min(bitmap.cols, rect.x + rect.width);
    int ymax = std::min(bitmap.rows, rect.y + rect.height);

    if (xmax <= xmin || ymax <= ymin) {
        return 0.0f;
    }

    // 创建掩码
    cv::Mat mask = cv::Mat::zeros(ymax - ymin, xmax - xmin, CV_8UC1);
    
    // 转换轮廓坐标到局部坐标
    std::vector<cv::Point> local_contour;
    for (const auto& pt : contour) {
        local_contour.push_back(cv::Point(pt.x - xmin, pt.y - ymin));
    }

    // 填充掩码
    std::vector<std::vector<cv::Point>> contours = {local_contour};
    cv::fillPoly(mask, contours, cv::Scalar(1));

    // 计算平均分数
    cv::Mat roi = bitmap(cv::Range(ymin, ymax), cv::Range(xmin, xmax));
    return cv::mean(roi, mask)[0];
}

std::vector<cv::Point2f> DBPostProcessor::unclip(const std::vector<cv::Point2f>& box) {
    // 计算原始面积和周长
    float area = polygonArea(box);
    float length = polygonLength(box);
    
    if (length == 0) {
        return box;
    }

    // 计算扩展距离（与Python pyclipper保持一致）
    float distance = area * unclip_ratio_ / length;

    // 使用Clipper2进行多边形偏移
    // 转换cv::Point2f到Clipper2的Path格式
    Clipper2Lib::PathD path;
    for (const auto& pt : box) {
        path.push_back(Clipper2Lib::PointD(pt.x, pt.y));
    }
    
    // 执行偏移操作（相当于Python的pyclipper.Execute）
    Clipper2Lib::PathsD solution = Clipper2Lib::InflatePaths(
        {path}, 
        distance, 
        Clipper2Lib::JoinType::Round,  // JT_ROUND
        Clipper2Lib::EndType::Polygon   // ET_CLOSEDPOLYGON
    );
    
    // Debug logging
    static int debug_count = 0;
    if (debug_count < 3) {
        LOG_INFO("Unclip: area=%.2f, length=%.2f, distance=%.2f, solution paths=%zu", 
                 area, length, distance, solution.size());
        if (!solution.empty()) {
            LOG_INFO("  First solution has %zu points", solution[0].size());
        }
        debug_count++;
    }
    
    // 转换回cv::Point2f
    std::vector<cv::Point2f> unclipped_box;
    if (!solution.empty() && !solution[0].empty()) {
        for (const auto& pt : solution[0]) {
            unclipped_box.push_back(cv::Point2f(static_cast<float>(pt.x), 
                                                 static_cast<float>(pt.y)));
        }
    } else {
        // 如果偏移失败，返回原始框
        LOG_WARN("Clipper2 unclip failed, using original box");
        unclipped_box = box;
    }
    
    return unclipped_box;
}float DBPostProcessor::polygonArea(const std::vector<cv::Point2f>& box) {
    if (box.size() < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    int n = box.size();
    
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += box[i].x * box[j].y;
        area -= box[j].x * box[i].y;
    }

    return std::abs(area) / 2.0f;
}

float DBPostProcessor::polygonLength(const std::vector<cv::Point2f>& box) {
    float length = 0.0f;
    int n = box.size();
    
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float dx = box[j].x - box[i].x;
        float dy = box[j].y - box[i].y;
        length += std::sqrt(dx * dx + dy * dy);
    }

    return length;
}

} // namespace ocr
