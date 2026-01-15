#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include "common/types.hpp"

namespace ocr {

/**
 * @brief DBNet 后处理器
 * 将网络输出的概率图转换为文本检测框
 */
class DBPostProcessor {
public:
    /**
     * @brief 构造函数
     * @param thresh 二值化阈值 (默认0.3)
     * @param box_thresh 检测框置信度阈值 (默认0.6)
     * @param max_candidates 最大候选框数量 (默认1000)
     * @param unclip_ratio 检测框扩展比例 (默认1.5)
     */
    DBPostProcessor(float thresh = 0.3f,
                   float box_thresh = 0.6f,
                   int max_candidates = 1000,
                   float unclip_ratio = 1.5f);

    /**
     * @brief 处理检测输出（使用成员变量中的默认参数）
     * @param pred 网络输出的概率图 [H, W] (0-1之间的float值)
     * @param src_h 原始图像高度
     * @param src_w 原始图像宽度
     * @param resized_h Resized image height (before padding)
     * @param resized_w Resized image width (before padding)
     * @return 检测到的文本框列表
     */
    std::vector<DeepXOCR::TextBox> process(const cv::Mat& pred, 
                                           int src_h, int src_w,
                                           int resized_h = -1, int resized_w = -1);
    
    /**
     * @brief 处理检测输出（支持 per-call 参数覆盖）
     * @param pred 网络输出的概率图 [H, W] (0-1之间的float值)
     * @param src_h 原始图像高度
     * @param src_w 原始图像宽度
     * @param resized_h Resized image height (before padding)
     * @param resized_w Resized image width (before padding)
     * @param thresh 二值化阈值（覆盖默认值）
     * @param box_thresh 检测框置信度阈值（覆盖默认值）
     * @param unclip_ratio 检测框扩展比例（覆盖默认值）
     * @return 检测到的文本框列表
     */
    std::vector<DeepXOCR::TextBox> process(const cv::Mat& pred, 
                                           int src_h, int src_w,
                                           int resized_h, int resized_w,
                                           float thresh,
                                           float box_thresh,
                                           float unclip_ratio);

private:
    /**
     * @brief 从二值图中提取轮廓
     */
    std::vector<std::vector<cv::Point>> findContours(const cv::Mat& bitmap);

    /**
     * @brief 从轮廓获取最小外接矩形
     */
    std::vector<cv::Point2f> getMinBoxes(const std::vector<cv::Point>& contour,
                                         float& min_side);

    /**
     * @brief 计算检测框的置信度分数
     */
    float boxScoreFast(const cv::Mat& bitmap,
                      const std::vector<cv::Point>& contour);

    /**
     * @brief 扩展检测框（使用成员变量中的默认参数）
     */
    std::vector<cv::Point2f> unclip(const std::vector<cv::Point2f>& box);
    
    /**
     * @brief 扩展检测框（支持 per-call 参数覆盖）
     * @param box 检测框四个顶点
     * @param unclip_ratio 扩展比例（覆盖默认值）
     */
    std::vector<cv::Point2f> unclip(const std::vector<cv::Point2f>& box, float unclip_ratio);

    /**
     * @brief 计算多边形面积
     */
    float polygonArea(const std::vector<cv::Point2f>& box);

    /**
     * @brief 计算多边形的周长
     */
    float polygonLength(const std::vector<cv::Point2f>& box);

private:
    float thresh_;          // 二值化阈值
    float box_thresh_;      // 框置信度阈值
    int max_candidates_;    // 最大候选框数
    float unclip_ratio_;    // 扩展比例
};

} // namespace ocr
