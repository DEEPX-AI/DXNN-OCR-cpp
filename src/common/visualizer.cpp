#include "common/visualizer.h"
#include "common/logger.hpp"
#include <random>
#include <fstream>
#include <opencv2/freetype.hpp>

namespace ocr {

// FreeType字体渲染器（全局实例）
static cv::Ptr<cv::freetype::FreeType2> g_ft2;

cv::Mat Visualizer::drawTextBoxes(const cv::Mat& image,
                                  const std::vector<TextBox>& boxes,
                                  const cv::Scalar& color,
                                  int thickness) {
    cv::Mat vis = image.clone();

    for (const auto& box : boxes) {
        // 绘制多边形
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Point> int_points;
        for (int i = 0; i < 4; i++) {
            int_points.push_back(cv::Point(static_cast<int>(box.points[i].x), 
                                          static_cast<int>(box.points[i].y)));
        }
        contours.push_back(int_points);

        cv::polylines(vis, contours, true, color, thickness);

        // 绘制置信度（如果有）
        if (box.confidence > 0) {
            std::string conf_text = cv::format("%.2f", box.confidence);
            cv::Point text_pos(static_cast<int>(box.points[0].x), 
                             static_cast<int>(box.points[0].y) - 5);
            cv::putText(vis, conf_text, text_pos, cv::FONT_HERSHEY_SIMPLEX,
                       0.5, cv::Scalar(255, 0, 0), 1);
        }
    }

    return vis;
}

cv::Mat Visualizer::drawOCRResults(const cv::Mat& image,
                                   const std::vector<TextBox>& boxes,
                                   bool draw_text,
                                   bool draw_confidence) {
    cv::Mat vis = image.clone();

    for (size_t i = 0; i < boxes.size(); i++) {
        const auto& box = boxes[i];

        // 生成随机颜色
        cv::Scalar color = getRandomColor();

        // 绘制多边形
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Point> int_points;
        for (int j = 0; j < 4; j++) {
            int_points.push_back(cv::Point(static_cast<int>(box.points[j].x), 
                                          static_cast<int>(box.points[j].y)));
        }
        contours.push_back(int_points);

        cv::polylines(vis, contours, true, color, 2);

        // 绘制文本和置信度
        if (draw_text && !box.text.empty()) {
            std::string label = box.text;
            if (draw_confidence && box.confidence > 0) {
                label += cv::format(" (%.2f)", box.confidence);
            }

            cv::Point text_pos(static_cast<int>(box.points[0].x), 
                             static_cast<int>(box.points[0].y) - 5);
            
            // 绘制文本背景
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                                 0.6, 2, &baseline);
            cv::rectangle(vis, 
                         text_pos + cv::Point(0, baseline),
                         text_pos + cv::Point(text_size.width, -text_size.height),
                         cv::Scalar(0, 0, 0), cv::FILLED);

            // 绘制文本
            cv::putText(vis, label, text_pos, cv::FONT_HERSHEY_SIMPLEX,
                       0.6, cv::Scalar(255, 255, 255), 2);
        }
    }

    return vis;
}

cv::Mat Visualizer::drawPolygon(const cv::Mat& image,
                               const std::vector<cv::Point>& points,
                               const cv::Scalar& color,
                               int thickness,
                               bool fill) {
    cv::Mat vis = image.clone();

    if (points.empty()) {
        return vis;
    }

    std::vector<std::vector<cv::Point>> contours = {points};

    if (fill) {
        cv::fillPoly(vis, contours, color);
    } else {
        cv::polylines(vis, contours, true, color, thickness);
    }

    return vis;
}

cv::Mat Visualizer::drawText(const cv::Mat& image,
                            const std::string& text,
                            const cv::Point& position,
                            double font_scale,
                            const cv::Scalar& color,
                            int thickness,
                            bool background) {
    cv::Mat vis = image.clone();

    if (background) {
        int baseline = 0;
        cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                                             font_scale, thickness, &baseline);
        cv::rectangle(vis, 
                     position + cv::Point(0, baseline),
                     position + cv::Point(text_size.width, -text_size.height),
                     cv::Scalar(0, 0, 0), cv::FILLED);
    }

    cv::putText(vis, text, position, cv::FONT_HERSHEY_SIMPLEX,
               font_scale, color, thickness);

    return vis;
}

cv::Mat Visualizer::createMosaic(const std::vector<cv::Mat>& images,
                                const std::vector<std::string>& labels,
                                int cols) {
    if (images.empty()) {
        return cv::Mat();
    }

    int rows = (images.size() + cols - 1) / cols;

    // 找到最大尺寸
    int max_height = 0;
    int max_width = 0;
    for (const auto& img : images) {
        max_height = std::max(max_height, img.rows);
        max_width = std::max(max_width, img.cols);
    }

    // 创建画布
    int canvas_height = rows * (max_height + 40); // 40 for label
    int canvas_width = cols * max_width;
    cv::Mat canvas = cv::Mat::zeros(canvas_height, canvas_width, CV_8UC3);
    canvas.setTo(cv::Scalar(255, 255, 255));

    // 放置图像
    for (size_t i = 0; i < images.size(); i++) {
        int row = i / cols;
        int col = i % cols;

        int x = col * max_width;
        int y = row * (max_height + 40);

        // 确保图像有3个通道
        cv::Mat img_to_place = images[i];
        if (img_to_place.channels() == 1) {
            cv::cvtColor(img_to_place, img_to_place, cv::COLOR_GRAY2BGR);
        }

        // 放置图像
        cv::Rect roi(x, y + 30, img_to_place.cols, img_to_place.rows);
        if (roi.x + roi.width <= canvas.cols && roi.y + roi.height <= canvas.rows) {
            img_to_place.copyTo(canvas(roi));
        }

        // 添加标签
        if (i < labels.size() && !labels[i].empty()) {
            cv::putText(canvas, labels[i], cv::Point(x + 5, y + 20),
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
        }
    }

    return canvas;
}

bool Visualizer::save(const cv::Mat& image,
                     const std::string& output_path,
                     int quality) {
    if (image.empty()) {
        LOG_ERROR("Cannot save empty image to %s", output_path.c_str());
        return false;
    }

    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(quality);

    bool success = cv::imwrite(output_path, image, params);
    if (!success) {
        LOG_ERROR("Failed to save image to %s", output_path.c_str());
    }

    return success;
}

void Visualizer::show(const cv::Mat& image,
                     const std::string& window_name,
                     int wait_key) {
    if (image.empty()) {
        LOG_ERROR("Cannot show empty image");
        return;
    }

    cv::Mat display_img = resizeForDisplay(image);
    cv::imshow(window_name, display_img);
    cv::waitKey(wait_key);
}

cv::Mat Visualizer::resizeForDisplay(const cv::Mat& image,
                                     int max_width,
                                     int max_height) {
    if (image.cols <= max_width && image.rows <= max_height) {
        return image;
    }

    float ratio = std::min(
        static_cast<float>(max_width) / image.cols,
        static_cast<float>(max_height) / image.rows
    );

    int new_width = static_cast<int>(image.cols * ratio);
    int new_height = static_cast<int>(image.rows * ratio);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_width, new_height));

    return resized;
}

cv::Scalar Visualizer::getRandomColor() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);

    return cv::Scalar(dis(gen), dis(gen), dis(gen));
}

cv::Size Visualizer::getTextSize(const std::string& text, 
                                double font_scale, 
                                int thickness) {
    int baseline = 0;
    return cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 
                          font_scale, thickness, &baseline);
}

void Visualizer::putTextUTF8(cv::Mat& img, const std::string& text, cv::Point org,
                            const std::string& font_path, int font_size,
                            const cv::Scalar& color) {
    // 初始化FreeType（只初始化一次）
    if (!g_ft2 || g_ft2.empty()) {
        g_ft2 = cv::freetype::createFreeType2();
        if (!font_path.empty()) {
            g_ft2->loadFontData(font_path, 0);
        }
    }
    
    // 如果提供了新的字体路径，重新加载
    static std::string current_font_path;
    if (!font_path.empty() && font_path != current_font_path) {
        g_ft2->loadFontData(font_path, 0);
        current_font_path = font_path;
    }
    
    // 绘制UTF-8文本
    if (g_ft2 && !g_ft2.empty()) {
        g_ft2->putText(img, text, org, font_size, color, -1, cv::LINE_AA, true);
    } else {
        // 回退到普通ASCII绘制
        cv::putText(img, text, org, cv::FONT_HERSHEY_SIMPLEX, 
                   font_size / 20.0, color, 2);
    }
}

cv::Mat Visualizer::drawOCRResultsSideBySide(const cv::Mat& image,
                                            const std::vector<TextBox>& boxes,
                                            const std::string& font_path) {
    // 字体路径：优先使用用户指定，否则使用绝对路径
    std::string font;
    
    if (!font_path.empty()) {
        std::ifstream file(font_path);
        if (file.good()) {
            font = font_path;
        }
    }
    
    if (font.empty()) {
        // 使用绝对路径（从 PROJECT_ROOT_DIR 宏）
        std::string projectRoot = PROJECT_ROOT_DIR;
        font = projectRoot + "/engine/fonts/NotoSansCJK-Regular.ttc";
        
        std::ifstream file(font);
        if (!file.good()) {
            LOG_ERROR("Font file not found: %s", font.c_str());
            LOG_ERROR("Please ensure the font file exists in: engine/fonts/");
            throw std::runtime_error("Font file not found");
        }
    }
    
    int h = image.rows;
    int w = image.cols;
    
    // 左图：原图 + 半透明检测框
    cv::Mat img_left = image.clone();
    
    // 右图：白色画布 + 文字
    cv::Mat img_right = cv::Mat(h, w, CV_8UC3, cv::Scalar(255, 255, 255));
    
    // 设置随机种子确保颜色一致
    std::srand(0);
    
    for (const auto& box : boxes) {
        // 生成随机颜色
        cv::Scalar color = getRandomColor();
        
        // 左图：绘制检测框
        std::vector<cv::Point> pts;
        for (int i = 0; i < 4; i++) {
            pts.push_back(cv::Point(static_cast<int>(box.points[i].x), 
                                   static_cast<int>(box.points[i].y)));
        }
        
        // 绘制填充多边形（半透明效果）
        cv::Mat overlay = img_left.clone();
        std::vector<std::vector<cv::Point>> contours = {pts};
        cv::fillPoly(overlay, contours, color);
        cv::addWeighted(overlay, 0.3, img_left, 0.7, 0, img_left);
        
        // 绘制边框
        cv::polylines(img_left, contours, true, color, 2);
        
        // 右图：只绘制文字（不绘制框）
        if (!box.text.empty()) {
            // 计算文字区域的中心位置和大小
            cv::Rect bbox = cv::boundingRect(pts);
            int box_width = bbox.width;
            int box_height = bbox.height;
            
            // 判断是否为垂直文本框 (Python logic: box_height > 2 * box_width and box_height > 30)
            bool is_vertical = (box_height > 2 * box_width) && (box_height > 30);
            
            if (is_vertical) {
                // 竖向绘制文本（逐字符从上到下）
                int font_size = std::max(12, std::min(box_width / 2, 32));
                int char_spacing = 2;  // Python的line_spacing=2
                int padding = std::max(box_width / 20, 2);
                
                int start_x = bbox.x + (box_width - font_size) / 2;
                int current_y = bbox.y + padding;
                
                // 逐字符绘制 - 和Python的 for char in text 一样
                for (size_t i = 0; i < box.text.length(); ) {
                    // 获取一个UTF-8字符
                    unsigned char c = box.text[i];
                    int char_len = (c & 0x80) ? ((c & 0x20) ? ((c & 0x10) ? 4 : 3) : 2) : 1;
                    std::string single_char = box.text.substr(i, char_len);
                    
                    // 绘制字符
                    putTextUTF8(img_right, single_char, cv::Point(start_x, current_y), 
                               font, font_size, cv::Scalar(0, 0, 0));
                    
                    // 获取字符高度（类似Python的font.getbbox(char)）
                    int char_height;
                    if (g_ft2 && !g_ft2.empty()) {
                        int baseline = 0;
                        cv::Size size = g_ft2->getTextSize(single_char, font_size, -1, &baseline);
                        char_height = size.height;
                    } else {
                        char_height = font_size;  // 回退方案
                    }
                    
                    current_y += char_height + char_spacing;  // Python: y += char_height + line_spacing
                    i += char_len;
                }
            } else {
                // 横向绘制文本（原有逻辑）
                int font_size = std::max(12, std::min(box_height / 3, 32));
                cv::Point text_pos(bbox.x + 5, bbox.y + bbox.height / 2 + font_size / 3);
                putTextUTF8(img_right, box.text, text_pos, font, font_size, 
                           cv::Scalar(0, 0, 0));
            }
        }
    }
    
    // 拼接左右两图
    cv::Mat result(h, w * 2, CV_8UC3);
    img_left.copyTo(result(cv::Rect(0, 0, w, h)));
    img_right.copyTo(result(cv::Rect(w, 0, w, h)));
    
    return result;
}

} // namespace ocr
