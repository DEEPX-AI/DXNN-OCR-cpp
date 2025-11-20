#include "preprocessing/image_ops.h"
#include <algorithm>
#include <cmath>

namespace ocr {

cv::Mat ImageOps::resize(const cv::Mat& image,
                        const cv::Size& target_size,
                        bool keep_ratio) {
    if (!keep_ratio) {
        cv::Mat resized;
        cv::resize(image, resized, target_size, 0, 0, cv::INTER_LINEAR);
        return resized;
    }

    // 保持宽高比
    float ratio = std::min(
        static_cast<float>(target_size.width) / image.cols,
        static_cast<float>(target_size.height) / image.rows
    );

    int new_width = static_cast<int>(image.cols * ratio);
    int new_height = static_cast<int>(image.rows * ratio);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_width, new_height), 0, 0, cv::INTER_LINEAR);

    return resized;
}

cv::Mat ImageOps::resizeByMaxLen(const cv::Mat& image, int max_side_len) {
    int h = image.rows;
    int w = image.cols;
    
    float ratio = 1.0f;
    if (std::max(h, w) > max_side_len) {
        if (h > w) {
            ratio = static_cast<float>(max_side_len) / h;
        } else {
            ratio = static_cast<float>(max_side_len) / w;
        }
    }

    int resize_h = static_cast<int>(h * ratio);
    int resize_w = static_cast<int>(w * ratio);

    // 确保是32的倍数（对于某些模型）
    resize_h = (resize_h / 32) * 32;
    resize_w = (resize_w / 32) * 32;

    resize_h = std::max(32, resize_h);
    resize_w = std::max(32, resize_w);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(resize_w, resize_h), 0, 0, cv::INTER_LINEAR);

    return resized;
}

cv::Mat ImageOps::normalize(const cv::Mat& image,
                           const std::vector<float>& mean,
                           const std::vector<float>& scale,
                           int norm_type) {
    cv::Mat normalized;
    image.convertTo(normalized, CV_32FC3, 1.0 / 255.0);

    if (norm_type == 0) {
        // 仅除以255
        return normalized;
    }

    // (x/255 - mean) / scale
    std::vector<cv::Mat> channels(3);
    cv::split(normalized, channels);

    for (int i = 0; i < 3; i++) {
        channels[i] = (channels[i] - mean[i]) / scale[i];
    }

    cv::merge(channels, normalized);
    return normalized;
}

cv::Mat ImageOps::padding(const cv::Mat& image,
                         const cv::Size& target_size,
                         const cv::Scalar& pad_value) {
    int top = 0;
    int bottom = target_size.height - image.rows;
    int left = 0;
    int right = target_size.width - image.cols;

    cv::Mat padded;
    cv::copyMakeBorder(image, padded, top, bottom, left, right, 
                      cv::BORDER_CONSTANT, pad_value);

    return padded;
}

std::vector<float> ImageOps::hwc2chw(const cv::Mat& image) {
    int h = image.rows;
    int w = image.cols;
    int c = image.channels();

    std::vector<float> chw_data(c * h * w);

    if (image.type() == CV_32FC3 || image.type() == CV_32FC1) {
        // Float类型直接复制
        for (int ch = 0; ch < c; ch++) {
            for (int row = 0; row < h; row++) {
                for (int col = 0; col < w; col++) {
                    int chw_idx = ch * h * w + row * w + col;
                    chw_data[chw_idx] = image.at<cv::Vec3f>(row, col)[ch];
                }
            }
        }
    } else {
        // 其他类型需要转换
        cv::Mat float_img;
        image.convertTo(float_img, CV_32FC3);
        return hwc2chw(float_img);
    }

    return chw_data;
}

cv::Mat ImageOps::chw2hwc(const std::vector<float>& data,
                         int height,
                         int width,
                         int channels) {
    cv::Mat hwc_image(height, width, CV_32FC3);

    for (int ch = 0; ch < channels; ch++) {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                int chw_idx = ch * height * width + row * width + col;
                hwc_image.at<cv::Vec3f>(row, col)[ch] = data[chw_idx];
            }
        }
    }

    return hwc_image;
}

cv::Mat ImageOps::warpPerspective(const cv::Mat& image,
                                  const std::vector<cv::Point2f>& src_points,
                                  int dst_width,
                                  int dst_height) {
    if (src_points.size() != 4) {
        return cv::Mat();
    }

    std::vector<cv::Point2f> dst_points = {
        cv::Point2f(0, 0),
        cv::Point2f(dst_width - 1, 0),
        cv::Point2f(dst_width - 1, dst_height - 1),
        cv::Point2f(0, dst_height - 1)
    };

    cv::Mat M = cv::getPerspectiveTransform(src_points, dst_points);
    cv::Mat warped;
    cv::warpPerspective(image, warped, M, cv::Size(dst_width, dst_height));

    return warped;
}

cv::Mat ImageOps::getRotateCropImage(const cv::Mat& image,
                                     const std::vector<cv::Point2f>& box) {
    return resizeForRecognition(warpPerspective(image, box, 100, 48), 48);
}

float ImageOps::distance(const cv::Point2f& p1, const cv::Point2f& p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<cv::Point2f> ImageOps::orderPoints(const std::vector<cv::Point2f>& points) {
    if (points.size() != 4) {
        return points;
    }

    std::vector<cv::Point2f> pts = points;

    // 按照 x + y 排序，找到左上和右下
    std::sort(pts.begin(), pts.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
        return (a.x + a.y) < (b.x + b.y);
    });

    cv::Point2f top_left = pts[0];
    cv::Point2f bottom_right = pts[3];

    // 剩下两个点按照 y - x 排序，找到右上和左下
    std::vector<cv::Point2f> remaining = {pts[1], pts[2]};
    std::sort(remaining.begin(), remaining.end(), [](const cv::Point2f& a, const cv::Point2f& b) {
        return (a.y - a.x) < (b.y - b.x);
    });

    cv::Point2f top_right = remaining[1];
    cv::Point2f bottom_left = remaining[0];

    return {top_left, top_right, bottom_right, bottom_left};
}

cv::Mat ImageOps::resizeForRecognition(const cv::Mat& image,
                                       int target_height,
                                       int max_width) {
    int h = image.rows;
    int w = image.cols;

    if (h == 0) {
        return image;
    }

    float ratio = static_cast<float>(target_height) / h;
    int resize_w = static_cast<int>(w * ratio);

    if (max_width > 0 && resize_w > max_width) {
        resize_w = max_width;
    }

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(resize_w, target_height), 0, 0, cv::INTER_LINEAR);

    return resized;
}

} // namespace ocr
