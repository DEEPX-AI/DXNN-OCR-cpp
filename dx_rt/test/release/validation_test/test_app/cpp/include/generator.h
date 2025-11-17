#pragma once
#include "dxrt/extern/rapidjson/document.h"
#include <string>
#include <vector>
#include <map>
#include "dxrt/dxrt_api.h"

using std::string;
using std::vector;

// Options for IE creation (model~ieOption from JSON)
struct IEOption
{
    string model_path;
    string dynamicCpuOffloading;  // "on" or "off"
    string threadType;            // "single-ie" or "multi-ie"
    int threadCount;              // thread count
    bool ort;                     // true or false 
    string bound;                 // "NPU_ALL", "NPU_0", "NPU_12"
    string device;                // "all", "1", "0,1"
    
    // Comparison operators for duplicate detection and use as map key
    bool operator==(const IEOption& other) const
    {
        return model_path == other.model_path &&
               dynamicCpuOffloading == other.dynamicCpuOffloading &&
               threadType == other.threadType &&
               threadCount == other.threadCount &&
               ort == other.ort &&
               bound == other.bound &&
               device == other.device;
    }
    
    bool operator<(const IEOption& other) const
    {
        if (model_path != other.model_path) return model_path < other.model_path;
        if (dynamicCpuOffloading != other.dynamicCpuOffloading) return dynamicCpuOffloading < other.dynamicCpuOffloading;
        if (threadType != other.threadType) return threadType < other.threadType;
        if (threadCount != other.threadCount) return threadCount < other.threadCount;
        if (ort != other.ort) return ort < other.ort;
        if (bound != other.bound) return bound < other.bound;
        return device < other.device;
    }
};

// Options for execution (inferenceFunction, inoutOption from JSON)
struct ExecutionOption
{
    string inferenceFunction;     // "sync", "async", "batch"
    string inputStyle;            // "single" or "multi"
    string outputBuffer;          // "user" or "internal"
    string asyncMethod;           // "callback" or "wait"
    int callbackDelay;            // delay value
    int loop;                     // loop count
    int time;                     // time value
    bool bitmatch;                // bitmatch enabled
    
    // Comparison operator for duplicate detection
    bool operator==(const ExecutionOption& other) const
    {
        return inferenceFunction == other.inferenceFunction &&
               inputStyle == other.inputStyle &&
               outputBuffer == other.outputBuffer &&
               asyncMethod == other.asyncMethod &&
               callbackDelay == other.callbackDelay &&
               loop == other.loop &&
               time == other.time &&
               bitmatch == other.bitmatch;
    }
    
    bool operator<(const ExecutionOption& other) const
    {
        if (inferenceFunction != other.inferenceFunction) return inferenceFunction < other.inferenceFunction;
        if (inputStyle != other.inputStyle) return inputStyle < other.inputStyle;
        if (outputBuffer != other.outputBuffer) return outputBuffer < other.outputBuffer;
        if (asyncMethod != other.asyncMethod) return asyncMethod < other.asyncMethod;
        if (callbackDelay != other.callbackDelay) return callbackDelay < other.callbackDelay;
        if (loop != other.loop) return loop < other.loop;
        if (time != other.time) return time < other.time;
        return bitmatch < other.bitmatch;
    }
};

// Test case that combines one IE option with multiple execution options
struct TestCase
{
    IEOption ieOption;
    std::vector<ExecutionOption> execOptions;
};

// Generator class for creating test options with IE reuse optimization
class Generator
{
public:
    Generator(const std::string& basePath, const std::string& jsonPath, bool random);
    ~Generator();

    bool LoadJson();
    void GenerateTestCases();
    const std::vector<TestCase>& GetTestCases() const { return _testCases; };
    void PrintTestCases();
    void CheckForDuplicates() const;
    
private:
    // Internal methods to generate IE options and execution options separately  
    std::vector<IEOption> generateIEOptions(const rapidjson::Value& config, const std::vector<string>& model_paths);
    std::vector<ExecutionOption> generateExecutionOptions(const rapidjson::Value& config);
    
    // Helper methods for options extraction
    std::vector<string> extractStringArray(const rapidjson::Value& config, const std::vector<string>& keys);
    std::vector<bool> extractBoolArray(const rapidjson::Value& config, const std::vector<string>& keys);
    std::vector<int> extractIntArray(const rapidjson::Value& config, const std::vector<string>& keys);

    std::vector<TestCase> _testCases;
    std::vector<string> _modelPaths;
    std::string _basePath;
    std::string _jsonPath;
    rapidjson::Document _jsonDocument;
    bool _random; // Whether to randomize options or use all combinations
};


void SetInferenceConfigurationFromIEOption(dxrt::InferenceOption& op, const IEOption& ieOption);