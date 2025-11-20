#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <iostream>

namespace DeepXOCR {

/**
 * Text bounding box with recognition result
 */
struct TextBox {
    // Bounding box coordinates (quad format: 4 points)
    cv::Point2f points[4];
    
    // Recognition result
    std::string text;
    float confidence;
    
    // Classification result (180° rotation)
    bool rotated;
    
    // Constructor
    TextBox() : text(""), confidence(0.0f), rotated(false) {
        for(int i = 0; i < 4; i++) {
            points[i] = cv::Point2f(0, 0);
        }
    }
    
    // Display info
    void Show() const {
        std::cout << "  TextBox: \"" << text << "\" (conf=" << confidence << ")" << std::endl;
        std::cout << "    Points: [" 
                  << points[0] << ", " << points[1] << ", " 
                  << points[2] << ", " << points[3] << "]" << std::endl;
    }
    
    // Get bounding rectangle
    cv::Rect GetRect() const {
        float min_x = points[0].x, max_x = points[0].x;
        float min_y = points[0].y, max_y = points[0].y;
        
        for(int i = 1; i < 4; i++) {
            min_x = std::min(min_x, points[i].x);
            max_x = std::max(max_x, points[i].x);
            min_y = std::min(min_y, points[i].y);
            max_y = std::max(max_y, points[i].y);
        }
        
        return cv::Rect(cv::Point(min_x, min_y), cv::Point(max_x, max_y));
    }
};

/**
 * OCR result for single image
 */
struct OCRResult {
    std::vector<TextBox> textBoxes;
    cv::Mat preprocessedImage;  // Document preprocessed image
    
    // Timing info
    double totalTime;      // Total processing time (ms)
    double detTime;        // Detection time (ms)
    double clsTime;        // Classification time (ms)
    double recTime;        // Recognition time (ms)
    
    OCRResult() : totalTime(0), detTime(0), clsTime(0), recTime(0) {}
    
    void Show() const {
        std::cout << "OCR Result:" << std::endl;
        std::cout << "  Total texts: " << textBoxes.size() << std::endl;
        std::cout << "  Time: det=" << detTime << "ms, cls=" << clsTime 
                  << "ms, rec=" << recTime << "ms, total=" << totalTime << "ms" << std::endl;
        
        for(const auto& box : textBoxes) {
            box.Show();
        }
    }
};

} // namespace DeepXOCR
