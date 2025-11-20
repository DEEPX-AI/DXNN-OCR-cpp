#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace ocr {

/**
 * @brief 图像预处理操作集合
 */
class ImageOps {
public:
    /**
     * @brief Resize图像到指定大小
     * @param image 输入图像
     * @param target_size 目标大小 (width, height)
     * @param keep_ratio 是否保持宽高比 (默认false)
     * @return 调整后的图像
     */
    static cv::Mat resize(const cv::Mat& image,
                         const cv::Size& target_size,
                         bool keep_ratio = false);

    /**
     * @brief Resize图像，限制最大边长
     * @param image 输入图像
     * @param max_side_len 最大边长
     * @return 调整后的图像
     */
    static cv::Mat resizeByMaxLen(const cv::Mat& image, int max_side_len);

    /**
     * @brief 归一化图像 (减均值，除标准差)
     * @param image 输入图像 (BGR, 0-255)
     * @param mean 均值 [B, G, R]
     * @param scale 缩放因子 (通常是标准差的倒数)
     * @param norm_type 归一化类型: 0=x/255, 1=(x/255-mean)/scale
     * @return 归一化后的float图像
     */
    static cv::Mat normalize(const cv::Mat& image,
                            const std::vector<float>& mean = {0.485f, 0.456f, 0.406f},
                            const std::vector<float>& scale = {0.229f, 0.224f, 0.225f},
                            int norm_type = 1);

    /**
     * @brief Padding图像到指定大小
     * @param image 输入图像
     * @param target_size 目标大小
     * @param pad_value 填充值 (默认0)
     * @return 填充后的图像
     */
    static cv::Mat padding(const cv::Mat& image,
                          const cv::Size& target_size,
                          const cv::Scalar& pad_value = cv::Scalar(0, 0, 0));

    /**
     * @brief HWC转CHW (OpenCV Mat to DXRT input format)
     * @param image 输入图像 [H, W, C]
     * @return 转换后的数据 [C, H, W]
     */
    static std::vector<float> hwc2chw(const cv::Mat& image);

    /**
     * @brief CHW转HWC
     * @param data CHW格式数据
     * @param height 图像高度
     * @param width 图像宽度
     * @param channels 通道数
     * @return OpenCV Mat [H, W, C]
     */
    static cv::Mat chw2hwc(const std::vector<float>& data,
                          int height,
                          int width,
                          int channels = 3);

    /**
     * @brief 四点透视变换（用于文本矫正）
     * @param image 输入图像
     * @param src_points 源图像四个角点
     * @param dst_width 目标宽度
     * @param dst_height 目标高度
     * @return 变换后的图像
     */
    static cv::Mat warpPerspective(const cv::Mat& image,
                                   const std::vector<cv::Point2f>& src_points,
                                   int dst_width,
                                   int dst_height);

    /**
     * @brief 旋转裁剪（用于提取倾斜文本）
     * @param image 输入图像
     * @param box 文本框的四个顶点
     * @return 矫正后的文本区域
     */
    static cv::Mat getRotateCropImage(const cv::Mat& image,
                                      const std::vector<cv::Point2f>& box);

    /**
     * @brief 计算两点之间的距离
     */
    static float distance(const cv::Point2f& p1, const cv::Point2f& p2);

    /**
     * @brief 对点进行排序（左上、右上、右下、左下）
     * @param points 输入点集（至少4个点）
     * @return 排序后的4个点
     */
    static std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point2f>& points);

    /**
     * @brief Resize图像用于识别 (固定高度，宽度按比例)
     * @param image 输入图像
     * @param target_height 目标高度 (默认48)
     * @param max_width 最大宽度限制 (默认-1表示不限制)
     * @return 调整后的图像
     */
    static cv::Mat resizeForRecognition(const cv::Mat& image,
                                        int target_height = 48,
                                        int max_width = -1);
};

} // namespace ocr
