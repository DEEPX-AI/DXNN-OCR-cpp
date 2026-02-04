/**
 * @file test_cli_args.cpp
 * @brief 命令行参数解析异常处理测试
 * 
 * 测试 parseIntArg 函数的各种异常场景，确保命令行参数解析的健壮性
 */

#include <gtest/gtest.h>
#include <climits>
#include <cstring>
#include <string>
#include <sstream>

// ==================== parseIntArg 函数副本（用于测试）====================
// 注意: 此函数与 server_main.cpp 中的实现保持同步

/**
 * @brief 安全解析整数参数
 * @param arg 输入字符串
 * @param value 输出的整数值
 * @param name 参数名称（用于错误提示）
 * @param min_val 最小允许值
 * @param max_val 最大允许值
 * @return 解析成功返回 true，失败返回 false
 */
static bool parseIntArg(const char* arg, int& value, const char* name,
                        int min_val = INT_MIN, int max_val = INT_MAX) {
    // 检查空指针或空字符串
    if (arg == nullptr || arg[0] == '\0') {
        std::cerr << "Error: " << name << " value can't be empty" << std::endl;
        return false;
    }

    try {
        size_t pos = 0;
        int parsed = std::stoi(arg, &pos);

        // 检查是否完全转换：防止 "123abc" 字母数字混合输入情况
        if (pos != strlen(arg)) {
            std::cerr << "Error: " << name << " value: '" << arg 
                      << "' (contains non-digit characters)" << std::endl;
            return false;
        }

        // 范围检查
        if (parsed < min_val || parsed > max_val) {
            std::cerr << "Error: " << name << " must be in range [" 
                      << min_val << ", " << max_val << "], got: " << parsed << std::endl;
            return false;
        }

        value = parsed;
        return true;
    } catch (const std::invalid_argument&) {
        std::cerr << "Error: Invalid " << name << " value: '" << arg
                  << "' (not a valid integer)" << std::endl;
        return false;
    } catch (const std::out_of_range&) {
        std::cerr << "Error: " << name << " value out of range: '" << arg << "'" << std::endl;
        return false;
    }
}

// ==================== 测试类定义 ====================

/**
 * @brief CLI 参数解析测试夹具
 */
class CLIArgsTest : public ::testing::Test {
protected:
    int value = 0;  // 用于存储解析结果
    
    void SetUp() override {
        value = 0;  // 每次测试前重置
    }
};

// ==================== 异常场景：空输入测试 ====================

/**
 * @brief 测试空指针输入
 */
TEST_F(CLIArgsTest, NullPointerInput) {
    EXPECT_FALSE(parseIntArg(nullptr, value, "port", 1, 65535));
    EXPECT_EQ(value, 0);  // 值不应该改变
}

/**
 * @brief 测试空字符串输入
 */
TEST_F(CLIArgsTest, EmptyStringInput) {
    EXPECT_FALSE(parseIntArg("", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

// ==================== 异常场景：非数字输入测试 ====================

/**
 * @brief 测试纯字母输入
 */
TEST_F(CLIArgsTest, PureAlphabeticInput) {
    EXPECT_FALSE(parseIntArg("abc", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试单词输入
 */
TEST_F(CLIArgsTest, WordInput) {
    EXPECT_FALSE(parseIntArg("port", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试特殊字符输入
 */
TEST_F(CLIArgsTest, SpecialCharacterInput) {
    EXPECT_FALSE(parseIntArg("@#$%", value, "threads", 1, 256));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试只有空格的输入
 */
TEST_F(CLIArgsTest, WhitespaceOnlyInput) {
    EXPECT_FALSE(parseIntArg("   ", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

// ==================== 异常场景：混合输入测试 ====================

/**
 * @brief 测试数字后跟字母 (trailing characters)
 */
TEST_F(CLIArgsTest, NumberFollowedByLetters) {
    EXPECT_FALSE(parseIntArg("8080abc", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试数字后跟空格
 */
TEST_F(CLIArgsTest, NumberFollowedBySpaces) {
    EXPECT_FALSE(parseIntArg("8080 ", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试数字前有空格
 */
TEST_F(CLIArgsTest, NumberPrecededBySpaces) {
    EXPECT_TRUE(parseIntArg(" 8080", value, "port", 1, 65535));
    EXPECT_EQ(value, 8080);
}

/**
 * @brief 测试字母后跟数字
 */
TEST_F(CLIArgsTest, LettersFollowedByNumbers) {
    EXPECT_FALSE(parseIntArg("abc123", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试数字中间夹杂字母
 */
TEST_F(CLIArgsTest, NumbersWithEmbeddedLetters) {
    EXPECT_FALSE(parseIntArg("80a80", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

// ==================== 异常场景：数值范围测试 ====================

/**
 * @brief 测试超出 int 范围的大正数
 */
TEST_F(CLIArgsTest, LargePositiveNumber) {
    // 比 INT_MAX (2147483647) 更大的数
    EXPECT_FALSE(parseIntArg("99999999999999", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试超出 int 范围的大负数
 */
TEST_F(CLIArgsTest, LargeNegativeNumber) {
    // 比 INT_MIN 更小的数
    EXPECT_FALSE(parseIntArg("-99999999999999", value, "threads", 1, 256));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试端口号超出最大范围
 */
TEST_F(CLIArgsTest, PortExceedsMaxRange) {
    EXPECT_FALSE(parseIntArg("65536", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试端口号超出最大范围 (更大的值)
 */
TEST_F(CLIArgsTest, PortExceedsMaxRangeLarge) {
    EXPECT_FALSE(parseIntArg("99999", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试端口号低于最小范围
 */
TEST_F(CLIArgsTest, PortBelowMinRange) {
    EXPECT_FALSE(parseIntArg("0", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试负数端口号
 */
TEST_F(CLIArgsTest, NegativePortNumber) {
    EXPECT_FALSE(parseIntArg("-1", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试负数大端口号
 */
TEST_F(CLIArgsTest, LargeNegativePortNumber) {
    EXPECT_FALSE(parseIntArg("-8080", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试线程数超出最大范围
 */
TEST_F(CLIArgsTest, ThreadsExceedsMaxRange) {
    EXPECT_FALSE(parseIntArg("257", value, "threads", 1, 256));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试线程数低于最小范围
 */
TEST_F(CLIArgsTest, ThreadsBelowMinRange) {
    EXPECT_FALSE(parseIntArg("0", value, "threads", 1, 256));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试线程数为负数
 */
TEST_F(CLIArgsTest, NegativeThreadsNumber) {
    EXPECT_FALSE(parseIntArg("-4", value, "threads", 1, 256));
    EXPECT_EQ(value, 0);
}

// ==================== 异常场景：特殊格式输入测试 ====================

/**
 * @brief 测试十六进制格式输入
 */
TEST_F(CLIArgsTest, HexadecimalInput) {
    // "0x1F90" 不是有效的十进制整数
    EXPECT_FALSE(parseIntArg("0x1F90", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试八进制格式输入
 */
TEST_F(CLIArgsTest, OctalInput) {
    // std::stoi 会将以 0 开头的数字解释为八进制，但后续字符检查会失败
    // "0777" 解析为 777（十进制），但如果检测到非数字会失败
    // 实际上 "0777" 会被解析为 777，这是有效的
    // 让我们测试一个真正的八进制格式问题
    EXPECT_FALSE(parseIntArg("0o777", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试二进制格式输入
 */
TEST_F(CLIArgsTest, BinaryInput) {
    EXPECT_FALSE(parseIntArg("0b1010", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试科学计数法输入
 */
TEST_F(CLIArgsTest, ScientificNotationInput) {
    // "1e4" 会被解释为 1，然后 "e4" 剩余
    EXPECT_FALSE(parseIntArg("1e4", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试浮点数输入
 */
TEST_F(CLIArgsTest, FloatingPointInput) {
    // "8080.5" 会被解释为 8080，然后 ".5" 剩余
    EXPECT_FALSE(parseIntArg("8080.5", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试整数后跟小数点
 */
TEST_F(CLIArgsTest, IntegerWithTrailingDecimalPoint) {
    EXPECT_FALSE(parseIntArg("8080.", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试纯小数点输入
 */
TEST_F(CLIArgsTest, DecimalPointOnlyInput) {
    EXPECT_FALSE(parseIntArg(".", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试只有正号
 */
TEST_F(CLIArgsTest, PlusSignOnlyInput) {
    EXPECT_FALSE(parseIntArg("+", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试只有负号
 */
TEST_F(CLIArgsTest, MinusSignOnlyInput) {
    EXPECT_FALSE(parseIntArg("-", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试正号后跟字母
 */
TEST_F(CLIArgsTest, PlusSignFollowedByLetters) {
    EXPECT_FALSE(parseIntArg("+abc", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

// ==================== 边界值测试（应该通过）====================

/**
 * @brief 测试端口最小有效值
 */
TEST_F(CLIArgsTest, PortMinValidValue) {
    EXPECT_TRUE(parseIntArg("1", value, "port", 1, 65535));
    EXPECT_EQ(value, 1);
}

/**
 * @brief 测试端口最大有效值
 */
TEST_F(CLIArgsTest, PortMaxValidValue) {
    EXPECT_TRUE(parseIntArg("65535", value, "port", 1, 65535));
    EXPECT_EQ(value, 65535);
}

/**
 * @brief 测试常见端口值
 */
TEST_F(CLIArgsTest, CommonPortValue) {
    EXPECT_TRUE(parseIntArg("8080", value, "port", 1, 65535));
    EXPECT_EQ(value, 8080);
}

/**
 * @brief 测试线程数最小有效值
 */
TEST_F(CLIArgsTest, ThreadsMinValidValue) {
    EXPECT_TRUE(parseIntArg("1", value, "threads", 1, 256));
    EXPECT_EQ(value, 1);
}

/**
 * @brief 测试线程数最大有效值
 */
TEST_F(CLIArgsTest, ThreadsMaxValidValue) {
    EXPECT_TRUE(parseIntArg("256", value, "threads", 1, 256));
    EXPECT_EQ(value, 256);
}

/**
 * @brief 测试常见线程数值
 */
TEST_F(CLIArgsTest, CommonThreadsValue) {
    EXPECT_TRUE(parseIntArg("4", value, "threads", 1, 256));
    EXPECT_EQ(value, 4);
}

/**
 * @brief 测试带正号的有效数字
 */
TEST_F(CLIArgsTest, PositiveSignedValidNumber) {
    EXPECT_TRUE(parseIntArg("+8080", value, "port", 1, 65535));
    EXPECT_EQ(value, 8080);
}

/**
 * @brief 测试以零开头的有效数字
 */
TEST_F(CLIArgsTest, LeadingZeroValidNumber) {
    // "08080" 应该被解析为 8080
    EXPECT_TRUE(parseIntArg("08080", value, "port", 1, 65535));
    EXPECT_EQ(value, 8080);
}

// ==================== 边界条件组合测试 ====================

/**
 * @brief 测试刚好超出最大边界
 */
TEST_F(CLIArgsTest, JustOverMaxBoundary) {
    // 65535 是最大有效端口，65536 应该失败
    EXPECT_TRUE(parseIntArg("65535", value, "port", 1, 65535));
    EXPECT_EQ(value, 65535);
    
    value = 0;
    EXPECT_FALSE(parseIntArg("65536", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试刚好低于最小边界
 */
TEST_F(CLIArgsTest, JustUnderMinBoundary) {
    // 1 是最小有效端口，0 应该失败
    EXPECT_TRUE(parseIntArg("1", value, "port", 1, 65535));
    EXPECT_EQ(value, 1);
    
    value = 0;
    EXPECT_FALSE(parseIntArg("0", value, "port", 1, 65535));
    EXPECT_EQ(value, 0);
}

// ==================== 参数名称测试 ====================

/**
 * @brief 确保不同参数名称不影响解析逻辑
 */
TEST_F(CLIArgsTest, DifferentParameterNames) {
    int port_value = 0;
    int threads_value = 0;
    
    EXPECT_TRUE(parseIntArg("8080", port_value, "port", 1, 65535));
    EXPECT_TRUE(parseIntArg("4", threads_value, "threads", 1, 256));
    
    EXPECT_EQ(port_value, 8080);
    EXPECT_EQ(threads_value, 4);
}

// ==================== 极端测试用例 ====================

/**
 * @brief 测试 INT_MAX 边界
 */
TEST_F(CLIArgsTest, IntMaxBoundary) {
    // 使用默认范围测试 INT_MAX
    std::string int_max_str = std::to_string(INT_MAX);
    EXPECT_TRUE(parseIntArg(int_max_str.c_str(), value, "test"));
    EXPECT_EQ(value, INT_MAX);
}

/**
 * @brief 测试 INT_MIN 边界
 */
TEST_F(CLIArgsTest, IntMinBoundary) {
    // 使用默认范围测试 INT_MIN
    std::string int_min_str = std::to_string(INT_MIN);
    EXPECT_TRUE(parseIntArg(int_min_str.c_str(), value, "test"));
    EXPECT_EQ(value, INT_MIN);
}

/**
 * @brief 测试超过 INT_MAX 一点点
 */
TEST_F(CLIArgsTest, JustOverIntMax) {
    // INT_MAX + 1 应该抛出 out_of_range
    std::string overflow_str = std::to_string((long long)INT_MAX + 1);
    EXPECT_FALSE(parseIntArg(overflow_str.c_str(), value, "test"));
    EXPECT_EQ(value, 0);
}

/**
 * @brief 测试超过 INT_MIN 一点点
 */
TEST_F(CLIArgsTest, JustUnderIntMin) {
    // INT_MIN - 1 应该抛出 out_of_range
    std::string underflow_str = std::to_string((long long)INT_MIN - 1);
    EXPECT_FALSE(parseIntArg(underflow_str.c_str(), value, "test"));
    EXPECT_EQ(value, 0);
}
