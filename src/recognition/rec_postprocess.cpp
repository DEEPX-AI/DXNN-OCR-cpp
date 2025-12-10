#include "recognition/rec_postprocess.h"
#include "common/logger.hpp"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cstring>

namespace ocr {

CTCDecoder::CTCDecoder(const std::string& dict_path, bool use_space_char)
    : use_space_char_(use_space_char), blank_index_(0) {
    
    if (!loadDictionary(dict_path, use_space_char)) {
        LOG_ERROR("Failed to load dictionary from: {}", dict_path);
    }
}

bool CTCDecoder::loadDictionary(const std::string& dict_path, bool use_space_char) {
    std::ifstream file(dict_path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open dictionary file: {}", dict_path);
        return false;
    }
    
    // 添加 blank 字符作为索引0
    character_dict_.clear();
    character_dict_.push_back("blank");
    
    // 读取字典文件 (UTF-8编码)
    std::string line;
    while (std::getline(file, line)) {
        // 移除换行符
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        
        if (!line.empty()) {
            character_dict_.push_back(line);
        }
    }
    
    // 如果使用空格，添加空格字符
    if (use_space_char) {
        character_dict_.push_back(" ");
    }
    
    file.close();
    
    LOG_INFO("Loaded dictionary with {} characters (including blank)", 
             character_dict_.size());
    LOG_DEBUG("First few chars: blank, {}, {}, {}", 
              character_dict_.size() > 1 ? character_dict_[1] : "N/A",
              character_dict_.size() > 2 ? character_dict_[2] : "N/A",
              character_dict_.size() > 3 ? character_dict_[3] : "N/A");
    
    return true;
}

std::pair<std::string, float> CTCDecoder::decode(const dxrt::TensorPtr& output) {
    if (!output) {
        LOG_ERROR("Output tensor is null");
        return {"", 0.0f};
    }
    
    auto shape = output->shape();
    
    if (shape.size() != 3) {
        LOG_ERROR("Expected 3D output [batch, time_steps, num_classes], got {} dimensions", 
                  shape.size());
        return {"", 0.0f};
    }
    
    int batch_size = shape[0];
    int time_steps = shape[1];
    int num_classes = shape[2];
    
    if (batch_size != 1) {
        LOG_WARN("Batch size is {}, only processing first sample", batch_size);
    }
    
    if (num_classes != static_cast<int>(character_dict_.size())) {
        LOG_ERROR("Dictionary size mismatch: model={}, dict={}", 
                  num_classes, character_dict_.size());
        return {"", 0.0f};
    }
    
    // 获取数据指针
    const float* data = reinterpret_cast<const float*>(output->data());
    
    // Step 1: Argmax - 获取每个时间步的最大概率索引
    std::vector<int> pred_indices;
    std::vector<float> pred_probs;
    
    pred_indices.reserve(time_steps);
    pred_probs.reserve(time_steps);
    
    for (int t = 0; t < time_steps; t++) {
        const float* timestep_data = data + t * num_classes;
        
        // 找到最大值索引和概率
        int max_idx = 0;
        float max_prob = timestep_data[0];
        
        for (int c = 1; c < num_classes; c++) {
            if (timestep_data[c] > max_prob) {
                max_prob = timestep_data[c];
                max_idx = c;
            }
        }
        
        pred_indices.push_back(max_idx);
        pred_probs.push_back(max_prob);
    }
    
    // Step 2-5: CTC解码
    return decodeSequence(pred_indices, pred_probs);
}

std::pair<std::string, float> CTCDecoder::decodeSequence(
    const std::vector<int>& indices,
    const std::vector<float>& probs) {
    
    if (indices.empty()) {
        return {"", 0.0f};
    }
    
    // Step 2: 去重复 (CTC特性 - 连续相同的字符只保留一个)
    std::vector<int> deduped_indices;
    std::vector<float> deduped_probs;
    
    deduped_indices.push_back(indices[0]);
    deduped_probs.push_back(probs[0]);
    
    for (size_t i = 1; i < indices.size(); i++) {
        if (indices[i] != indices[i-1]) {
            deduped_indices.push_back(indices[i]);
            deduped_probs.push_back(probs[i]);
        }
    }
    
    // Step 3: 去除 blank (index=0)
    std::string text;
    std::vector<float> confidences;
    
    for (size_t i = 0; i < deduped_indices.size(); i++) {
        if (deduped_indices[i] != blank_index_) {
            int char_idx = deduped_indices[i];
            
            // 边界检查
            if (char_idx >= 0 && char_idx < static_cast<int>(character_dict_.size())) {
                text += character_dict_[char_idx];
                confidences.push_back(deduped_probs[i]);
            } else {
                LOG_WARN("Character index {} out of bounds (dict size: {})", 
                         char_idx, character_dict_.size());
            }
        }
    }
    
    // Step 4: 计算平均置信度
    float avg_confidence = 0.0f;
    if (!confidences.empty()) {
        avg_confidence = std::accumulate(confidences.begin(), confidences.end(), 0.0f) 
                        / confidences.size();
    }
    
    return {text, avg_confidence};
}

} // namespace ocr
