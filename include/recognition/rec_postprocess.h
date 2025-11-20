#pragma once

#include <string>
#include <vector>
#include <memory>
#include <dxrt/dxrt_api.h>

namespace ocr {

/**
 * @brief CTC解码器 - 用于文本识别后处理
 * 
 * 功能:
 * 1. CTC解码 (去重复 + 去blank)
 * 2. 字符索引转文本
 * 3. 置信度计算
 */
class CTCDecoder {
public:
    /**
     * @brief 构造函数
     * @param dict_path 字符字典路径
     * @param use_space_char 是否使用空格字符
     */
    explicit CTCDecoder(const std::string& dict_path, bool use_space_char = true);
    ~CTCDecoder() = default;
    
    /**
     * @brief 解码CTC输出
     * @param preds 模型输出 [time_steps, num_classes]
     * @return pair<文本, 置信度>
     */
    std::pair<std::string, float> decode(const dxrt::TensorPtr& output);
    
    /**
     * @brief 获取字典大小
     */
    size_t getDictSize() const { return character_dict_.size(); }
    
    /**
     * @brief 加载字典文件
     * @return 是否加载成功
     */
    bool loadDictionary(const std::string& dict_path, bool use_space_char);

private:
    /**
     * @brief CTC解码核心算法
     * @param indices 预测的字符索引序列
     * @param probs 对应的概率值
     * @return pair<文本, 置信度>
     */
    std::pair<std::string, float> decodeSequence(
        const std::vector<int>& indices,
        const std::vector<float>& probs);
    
    std::vector<std::string> character_dict_;  // 字符字典
    bool use_space_char_;                      // 是否使用空格
    int blank_index_;                          // blank字符的索引（通常是0）
};

} // namespace ocr
