#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include "types.hpp"

namespace ocr {

// Import types from DeepXOCR namespace
using DeepXOCR::TextBox;
using DeepXOCR::OCRResult;

/**
 * @brief 可视化工具类，用于绘制OCR各阶段的结果
 */
class Visualizer {
public:
    /**
     * @brief 在图像上绘制文本检测框
     * @param image 输入图像
     * @param boxes 检测框列表
     * @param color 绘制颜色 (默认绿色)
     * @param thickness 线条粗细 (默认2)
     * @return 绘制后的图像
     */
    static cv::Mat drawTextBoxes(const cv::Mat& image,
                                  const std::vector<TextBox>& boxes,
                                  const cv::Scalar& color = cv::Scalar(0, 255, 0),
                                  int thickness = 2);

    /**
     * @brief 绘制OCR结果（检测框+识别文本）
     * @param image 输入图像
     * @param boxes 文本框列表
     * @param draw_text 是否绘制识别文本
     * @param draw_confidence 是否绘制置信度
     * @return 可视化结果图像
     */
    static cv::Mat drawOCRResults(const cv::Mat& image,
                                  const std::vector<TextBox>& boxes,
                                  bool draw_text = true,
                                  bool draw_confidence = true);

    /**
     * @brief 绘制OCR结果（左右拼接版本：左边检测框，右边文字）
     * @param image 输入图像
     * @param boxes 文本框列表
     * @param font_path TrueType字体路径（用于中文显示）
     * @return 拼接后的可视化结果（宽度为原图2倍）
     */
    static cv::Mat drawOCRResultsSideBySide(const cv::Mat& image,
                                           const std::vector<TextBox>& boxes,
                                           const std::string& font_path = "");

    /**
     * @brief 在图像上绘制单个多边形
     * @param image 输入图像
     * @param points 多边形顶点
     * @param color 绘制颜色
     * @param thickness 线条粗细
     * @param fill 是否填充 (默认false)
     * @return 绘制后的图像
     */
    static cv::Mat drawPolygon(const cv::Mat& image,
                               const std::vector<cv::Point>& points,
                               const cv::Scalar& color,
                               int thickness = 2,
                               bool fill = false);

    /**
     * @brief 在图像上绘制文本
     * @param image 输入图像
     * @param text 要绘制的文本
     * @param position 文本位置
     * @param font_scale 字体大小 (默认0.6)
     * @param color 文本颜色 (默认红色)
     * @param thickness 线条粗细 (默认2)
     * @param background 是否添加背景 (默认true)
     * @return 绘制后的图像
     */
    static cv::Mat drawText(const cv::Mat& image,
                           const std::string& text,
                           const cv::Point& position,
                           double font_scale = 0.6,
                           const cv::Scalar& color = cv::Scalar(0, 0, 255),
                           int thickness = 2,
                           bool background = true);

    /**
     * @brief 创建拼接图，用于展示pipeline各阶段结果
     * @param images 图像列表
     * @param labels 每张图的标签
     * @param cols 每行显示的图像数量 (默认2)
     * @return 拼接后的图像
     */
    static cv::Mat createMosaic(const std::vector<cv::Mat>& images,
                               const std::vector<std::string>& labels = {},
                               int cols = 2);

    /**
     * @brief 保存可视化结果到文件
     * @param image 要保存的图像
     * @param output_path 输出路径
     * @param quality JPEG质量 (0-100，默认95)
     * @return 是否保存成功
     */
    static bool save(const cv::Mat& image,
                    const std::string& output_path,
                    int quality = 95);

    /**
     * @brief 显示图像窗口（用于调试）
     * @param image 要显示的图像
     * @param window_name 窗口名称
     * @param wait_key 等待按键时间(ms)，0表示无限等待
     */
    static void show(const cv::Mat& image,
                    const std::string& window_name = "OCR Visualization",
                    int wait_key = 0);

    /**
     * @brief 调整图像大小以适应显示
     * @param image 输入图像
     * @param max_width 最大宽度
     * @param max_height 最大高度
     * @return 调整后的图像
     */
    static cv::Mat resizeForDisplay(const cv::Mat& image,
                                    int max_width = 1920,
                                    int max_height = 1080);

private:
    // 辅助函数：生成随机颜色
    static cv::Scalar getRandomColor();
    
    // 辅助函数：计算文本边界框大小
    static cv::Size getTextSize(const std::string& text, double font_scale, int thickness);

    /**
     * @brief 使用FreeType绘制UTF-8文本到图像
     * @param img 目标图像
     * @param text UTF-8编码的文本
     * @param org 文本起始位置
     * @param font_path 字体文件路径
     * @param font_size 字体大小
     * @param color 文本颜色
     */
    static void putTextUTF8(cv::Mat& img, const std::string& text, cv::Point org,
                           const std::string& font_path, int font_size,
                           const cv::Scalar& color);
};

} // namespace ocr
