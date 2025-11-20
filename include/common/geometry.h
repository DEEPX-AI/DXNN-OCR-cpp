#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace ocr {

/**
 * @brief 几何变换工具类
 */
class Geometry {
public:
    /**
     * @brief 计算两点之间的欧氏距离
     */
    static float distance(const cv::Point2f& p1, const cv::Point2f& p2);

    /**
     * @brief 计算多边形的面积
     */
    static float polygonArea(const std::vector<cv::Point>& polygon);

    /**
     * @brief 计算多边形的外接矩形
     */
    static cv::RotatedRect minAreaRect(const std::vector<cv::Point>& points);

    /**
     * @brief 对四个点进行排序：左上、右上、右下、左下
     * @param points 输入点集（4个点）
     * @return 排序后的点
     */
    static std::vector<cv::Point2f> orderPointsClockwise(const std::vector<cv::Point2f>& points);

    /**
     * @brief 获取旋转裁剪图像（四点透视变换）
     * @param image 输入图像
     * @param box 四个顶点（任意顺序）
     * @return 矫正后的图像
     */
    static cv::Mat getRotateCropImage(const cv::Mat& image,
                                      const std::vector<cv::Point2f>& box);

    /**
     * @brief 裁剪并矫正文本区域（保持原始尺寸，不做resize）
     * @param image 输入图像
     * @param box 文本框的四个顶点
     * @return 矫正后的文本图像（尺寸由box计算得出）
     * @note 最终的resize由Recognition的PPOCRResize处理
     */
    static cv::Mat cropTextRegion(const cv::Mat& image,
                                  const std::vector<cv::Point2f>& box);

    /**
     * @brief 计算多边形的置信度（用于过滤低质量检测框）
     * @param polygon 多边形顶点
     * @param bitmap 置信度图
     * @return 平均置信度
     */
    static float getScore(const std::vector<cv::Point>& polygon, const cv::Mat& bitmap);

    /**
     * @brief 扩展多边形（用于检测后处理）
     * @param polygon 原始多边形
     * @param ratio 扩展比例（默认1.5）
     * @return 扩展后的多边形
     */
    static std::vector<cv::Point> expandPolygon(const std::vector<cv::Point>& polygon,
                                                float ratio = 1.5f);

    /**
     * @brief 多边形近似（减少顶点数量）
     * @param polygon 原始多边形
     * @param epsilon 近似精度
     * @return 近似后的多边形
     */
    static std::vector<cv::Point> approximatePolygon(const std::vector<cv::Point>& polygon,
                                                     double epsilon = 2.0);

    /**
     * @brief 计算最小外接矩形的四个顶点
     * @param points 点集
     * @return 四个顶点（顺时针）
     */
    static std::vector<cv::Point2f> getMinBoxPoints(const std::vector<cv::Point>& points);

    /**
     * @brief 检查点是否在多边形内
     */
    static bool isPointInPolygon(const cv::Point2f& point,
                                const std::vector<cv::Point>& polygon);

    /**
     * @brief 计算两个矩形的IoU
     */
    static float calculateIoU(const cv::Rect& rect1, const cv::Rect& rect2);
};

} // namespace ocr
