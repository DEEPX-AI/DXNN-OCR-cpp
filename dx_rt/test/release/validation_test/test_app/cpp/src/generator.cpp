#include "../include/generator.h"
#include "../include/utils.h"
#include "dxrt/dxrt_api.h"
#include <iostream>
#include <cstdlib>
#include <map>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "dxrt/device_info_status.h"
#include "dxrt/extern/rapidjson/document.h"
#include "dxrt/extern/rapidjson/istreamwrapper.h"
#include "dxrt/extern/rapidjson/error/en.h"

using std::string;
using std::cout;
using std::endl;

// Helper function to validate device option
bool isValidDeviceOption(const string& deviceOption, int numDevices)
{
    if (deviceOption == "all")
    {
        return true; // "all" is always valid
    }
    
    // Parse comma-separated device IDs
    std::stringstream ss(deviceOption);
    string segment;
    
    while (std::getline(ss, segment, ','))
    {
        // Remove whitespace
        segment.erase(std::remove_if(segment.begin(), segment.end(), ::isspace), segment.end());
        if (segment.empty()) continue;
        
        try
        {
            int device_id = std::stoi(segment);
            if (device_id >= numDevices)
            {
                return false;
            }
        }
        catch (const std::exception& e)
        {
            cout << "Warning: Invalid device ID '" << segment << "' in device option: " << deviceOption << endl;
            return false;
        }
    }
    
    return true;
}

Generator::Generator(const std::string& basePath, const std::string& jsonPath, bool random)
    : _basePath(basePath), _jsonPath(jsonPath), _random(random)
{

}

Generator::~Generator()
{

}

bool Generator::LoadJson()
{
    // Read and parse JSON file
    std::ifstream json_stream(_jsonPath);
    if (!json_stream.is_open()) 
    {
        cout << "Error: Unable to open json file: " << _jsonPath << endl;
        return false;
    }

    rapidjson::Document jsonConfig;
    rapidjson::IStreamWrapper isw(json_stream);
    jsonConfig.ParseStream(isw);
    json_stream.close();
    
    if (jsonConfig.HasParseError())
    {
        cout << "Error: Failed to parse json file - " << rapidjson::GetParseError_En(jsonConfig.GetParseError()) << endl;
        return false;
    }

    // Extract model directory paths (will find .dxnn files in generator)
    _modelPaths.clear();
    
    if (jsonConfig.HasMember("model") && jsonConfig["model"].IsArray())
    {
        const rapidjson::Value& models = jsonConfig["model"];
        for (rapidjson::SizeType i = 0; i < models.Size(); i++)
        {
            if (models[i].IsString())
            {
                string directory_path = _basePath;
                if (!directory_path.empty() && directory_path.back() != '/')
                {
                    directory_path += "/";
                }
                directory_path += models[i].GetString();
                _modelPaths.push_back(directory_path);
            }
        }
    }
    else
    {
        cout << "Error: 'model' field not found or not an array in json file" << endl;
        return false;
    }

    // Store the parsed JSON document for later use in GenerateTestCases
    _jsonDocument.CopyFrom(jsonConfig, _jsonDocument.GetAllocator());
    
    return true;
}

void Generator::PrintTestCases()
{
    // Check if test cases are initialized
    if (_testCases.empty())
    {
        cout << "Test cases not initialized. Please call LoadJson() and GenerateTestCases() first." << endl;
        return;
    }

    cout << "=== Generated Test Cases Summary ===" << endl;
    cout << "Total Test Cases: " << _testCases.size() << endl << endl;

    for (size_t i = 0; i < _testCases.size(); i++)
    {
        const TestCase& testCase = _testCases[i];
        
        cout << "=== Test Case " << (i + 1) << " ===" << endl;
        
        // Print IE Options
        cout << "IE Options:" << endl;
        cout << "  Model: " << testCase.ieOption.model_path << endl;
        cout << "  Dynamic CPU Offloading: " << testCase.ieOption.dynamicCpuOffloading << endl;
        cout << "  Thread Type: " << testCase.ieOption.threadType << endl;
        cout << "  Thread Count: " << testCase.ieOption.threadCount << endl;
        cout << "  ORT: " << (testCase.ieOption.ort ? "true" : "false") << endl;
        cout << "  Bound: " << testCase.ieOption.bound << endl;
        cout << "  Device: " << testCase.ieOption.device << endl;
        
        // Print Execution Options
        cout << endl << "Execution Options (" << testCase.execOptions.size() << " options):" << endl;
        for (size_t j = 0; j < testCase.execOptions.size(); j++)
        {
            const ExecutionOption& execOpt = testCase.execOptions[j];
            cout << "  " << (j + 1) << ". Function: " << execOpt.inferenceFunction
                 << ", Input: " << execOpt.inputStyle
                 << ", Output: " << execOpt.outputBuffer
                 << ", Async: " << execOpt.asyncMethod
                 << ", Loop: " << execOpt.loop
                 << ", Time: " << execOpt.time
                 << ", Bitmatch: " << (execOpt.bitmatch ? "true" : "false") << endl;
        }
        
        cout << "------------------------" << endl << endl;
    }
}

void Generator::CheckForDuplicates() const
{
    if (_testCases.empty())
    {
        cout << "No test cases to check for duplicates." << endl;
        return;
    }
    
    cout << "=== Checking for Duplicate Test Cases ===" << endl;
    
    int duplicateCount = 0;
    std::vector<bool> isDuplicate(_testCases.size(), false);
    
    // Check for duplicate IE options
    std::map<IEOption, std::vector<size_t>> ieOptionMap;
    for (size_t i = 0; i < _testCases.size(); i++)
    {
        ieOptionMap[_testCases[i].ieOption].push_back(i);
    }
    
    // Report IE option duplicates
    for (const auto& pair : ieOptionMap)
    {
        if (pair.second.size() > 1)
        {
            cout << "Duplicate IE Option found in test cases: ";
            for (size_t idx : pair.second)
            {
                cout << (idx + 1) << " ";
                isDuplicate[idx] = true;
            }
            cout << endl;
            cout << "  Model: " << pair.first.model_path << endl;
            cout << "  Dynamic CPU: " << pair.first.dynamicCpuOffloading << endl;
            cout << "  Thread Type: " << pair.first.threadType << endl;
            cout << "  Thread Count: " << pair.first.threadCount << endl;
            cout << "  ORT: " << (pair.first.ort ? "true" : "false") << endl;
            cout << "  Bound: " << pair.first.bound << endl;
            cout << "  Device: " << pair.first.device << endl;
            cout << endl;
            duplicateCount++;
        }
    }
    
    // Check for duplicate execution options within each test case
    for (size_t i = 0; i < _testCases.size(); i++)
    {
        const auto& execOptions = _testCases[i].execOptions;
        std::map<ExecutionOption, std::vector<size_t>> execOptionMap;
        
        for (size_t j = 0; j < execOptions.size(); j++)
        {
            execOptionMap[execOptions[j]].push_back(j);
        }
        
        for (const auto& pair : execOptionMap)
        {
            if (pair.second.size() > 1)
            {
                cout << "Duplicate Execution Options found in test case " << (i + 1) << " at positions: ";
                for (size_t idx : pair.second)
                {
                    cout << (idx + 1) << " ";
                }
                cout << endl;
                cout << "  Function: " << pair.first.inferenceFunction << endl;
                cout << "  Input Style: " << pair.first.inputStyle << endl;
                cout << "  Output Buffer: " << pair.first.outputBuffer << endl;
                cout << "  Async Method: " << pair.first.asyncMethod << endl;
                cout << "  Loop: " << pair.first.loop << endl;
                cout << "  Time: " << pair.first.time << endl;
                cout << "  Bitmatch: " << (pair.first.bitmatch ? "true" : "false") << endl;
                cout << endl;
                duplicateCount++;
            }
        }
    }
    
    if (duplicateCount == 0)
    {
        cout << "No duplicates found in test cases." << endl;
    }
    else
    {
        cout << "Total duplicate groups found: " << duplicateCount << endl;
    }
    
    cout << "=== Duplicate Check Complete ===" << endl;
}

// Generate IE options (model~ieOption combinations)
std::vector<IEOption> Generator::generateIEOptions(const rapidjson::Value& config, const std::vector<string>& modelPaths)
{
    std::vector<IEOption> ie_options;
    
    // Get available device count for validation
    int num_devices = 0;

    try 
    {
        num_devices = dxrt::DeviceStatus::GetDeviceCount();
    }
    catch (const std::exception& e)
    {
        cout << "Warning: Failed to get device count, proceeding without device validation: " << e.what() << endl;
        num_devices = -1; // Disable validation
    }
    
    // Extract options using helper functions
    auto dyn_cpu_options = extractStringArray(config, {"configuration", "dynamic-cpu-offloading"});
    auto ort_options = extractBoolArray(config, {"ieOption", "ort"});
    auto bound_options = extractStringArray(config, {"ieOption", "bound"});
    auto device_options = extractStringArray(config, {"ieOption", "device"});

    // Extract thread style options (array of objects)
    struct ThreadStyle { string type; int count; };
    std::vector<ThreadStyle> thread_styles;
    
    if (config.HasMember("threadStyle") && config["threadStyle"].IsArray())
    {
        const rapidjson::Value& threadStyleArray = config["threadStyle"];
        thread_styles.reserve(threadStyleArray.Size());
        for (rapidjson::SizeType i = 0; i < threadStyleArray.Size(); i++)
        {
            const rapidjson::Value& style = threadStyleArray[i];
            if (style.HasMember("type") && style.HasMember("count") && 
                style["type"].IsString() && style["count"].IsInt())
            {
                thread_styles.emplace_back(ThreadStyle{
                    style["type"].GetString(), 
                    style["count"].GetInt()
                });
            }
        }
    }
    
    // Calculate total combinations and reserve space for better performance
    size_t totalCombinations = modelPaths.size() * dyn_cpu_options.size() * thread_styles.size() * 
                              ort_options.size() * bound_options.size() * device_options.size();
    ie_options.reserve(totalCombinations);
    
    // Generate all combinations for IE options
    for (const auto& model_directory : modelPaths)
    {
        // Find .dxnn file in the directory
        string actual_model_path = findDxnnFileInDirectory(model_directory);
        if (actual_model_path.empty())
        {
            cout << "Error: No .dxnn file found in directory: " << model_directory << endl;
            continue; // Skip this model directory
        }

        if (_random)
        {
            // Run flow will slightly adjusted to maximiaze random option coverage
            for (const auto& dyn_cpu : dyn_cpu_options)
            {
                for (const auto& ort : ort_options)
                {
                    for (const auto& thread_style : thread_styles)
                    {
                        auto bound = GetRandomElement(bound_options);
                        auto device = GetRandomElement(device_options);

                        // Validate device option against available devices
                        if (num_devices > 0 && !isValidDeviceOption(device, num_devices))
                        {
                            continue; // Skip invalid device option
                        }

                        // Pick Random options
                        auto tmp_count = GetRandomInt(thread_style.count, 1);
                        
                        ie_options.emplace_back(IEOption{
                            actual_model_path, dyn_cpu, thread_style.type, tmp_count,
                            ort, bound, device
                        });
                    }
                }
            }
        }
        else
        {
            for (const auto& dyn_cpu : dyn_cpu_options)
            {
                for (const auto& thread_style : thread_styles)
                {
                    for (const auto& ort : ort_options)
                    {
                        for (const auto& bound : bound_options)
                        {
                            for (const auto& device : device_options)
                            {
                                // Validate device option against available devices
                                if (num_devices > 0 && !isValidDeviceOption(device, num_devices))
                                {
                                    continue; // Skip invalid device option
                                }
                                
                                ie_options.emplace_back(IEOption{
                                    actual_model_path, dyn_cpu, thread_style.type, thread_style.count,
                                    ort, bound, device
                                });
                            }
                        }
                    }
                }
            }
        }
    }
    
    return ie_options;
}

// Generate execution options (inferenceFunction, inoutOption combinations from JSON)
std::vector<ExecutionOption> Generator::generateExecutionOptions(const rapidjson::Value& config)
{
    std::vector<ExecutionOption> exec_options;
    
    // Extract options using helper functions
    auto inference_function_options = extractStringArray(config, {"inferenceFunction"});
    auto input_style_options = extractStringArray(config, {"inoutOption", "inputStyle"});
    auto output_buffer_options = extractStringArray(config, {"inoutOption", "outputBuffer"});
    auto async_method_options = extractStringArray(config, {"inoutOption", "asyncMethod"});
    
    // Extract fixed values from inoutOption
    int callback_delay = config["inoutOption"]["callbackDelay"].GetInt();
    int loop = config["inoutOption"]["loop"].GetInt();
    int time = config["inoutOption"]["time"].GetInt();
    bool bitmatch = config["inoutOption"]["bitmatch"].GetBool();
    
    // Reserve space for better performance
    size_t total_combinations = inference_function_options.size() * input_style_options.size() * 
                              output_buffer_options.size() * async_method_options.size();
    exec_options.reserve(total_combinations);
    
    // Generate all combinations for execution options
    for (const auto& inference_func : inference_function_options)
    {
        for (const auto& input_style : input_style_options)
        {
            for (const auto& output_buffer : output_buffer_options)
            {
                for (const auto& async_method : async_method_options)
                {
                    exec_options.emplace_back(ExecutionOption{
                        inference_func, input_style, output_buffer, async_method,
                        callback_delay, loop, time, bitmatch
                    });
                }
            }
        }
    }
    
    return exec_options;
}

// Generate combined test cases (IE options + execution options)
void Generator::GenerateTestCases()
{
    _testCases.clear();
    
    auto ie_options = generateIEOptions(_jsonDocument, _modelPaths);
    auto exec_options = generateExecutionOptions(_jsonDocument);
    
    // Create one test case per unique IE option, with all execution options
    _testCases.reserve(ie_options.size());
    
    for (const auto& ie_option : ie_options)
    {
        TestCase test_case;
        test_case.ieOption = ie_option;
        test_case.execOptions = exec_options; // Store all execution options for this IE
        _testCases.push_back(test_case);
    }
}

// Helper function implementations
std::vector<string> Generator::extractStringArray(const rapidjson::Value& config, const std::vector<string>& keys)
{
    std::vector<string> result;
    const rapidjson::Value* current = &config;
    
    // Navigate through nested keys
    for (const auto& key : keys)
    {
        if (current->HasMember(key.c_str()))
        {
            current = &(*current)[key.c_str()];
        }
        else
        {
            return result; // Return empty if key not found
        }
    }
    
    // Extract array values
    if (current->IsArray())
    {
        for (rapidjson::SizeType i = 0; i < current->Size(); i++)
        {
            const rapidjson::Value& opt = (*current)[i];
            if (opt.IsString())
            {
                result.push_back(opt.GetString());
            }
        }
    }
    
    return result;
}

std::vector<bool> Generator::extractBoolArray(const rapidjson::Value& config, const std::vector<string>& keys)
{
    std::vector<bool> result;
    const rapidjson::Value* current = &config;
    
    // Navigate through nested keys
    for (const auto& key : keys)
    {
        if (current->HasMember(key.c_str()))
        {
            current = &(*current)[key.c_str()];
        }
        else
        {
            return result; // Return empty if key not found
        }
    }
    
    // Extract array values
    if (current->IsArray())
    {
        for (rapidjson::SizeType i = 0; i < current->Size(); i++)
        {
            const rapidjson::Value& opt = (*current)[i];
            if (opt.IsBool())
            {
                result.push_back(opt.GetBool());
            }
        }
    }
    
    return result;
}

std::vector<int> Generator::extractIntArray(const rapidjson::Value& config, const std::vector<string>& keys)
{
    std::vector<int> result;
    const rapidjson::Value* current = &config;
    
    // Navigate through nested keys
    for (const auto& key : keys)
    {
        if (current->HasMember(key.c_str()))
        {
            current = &(*current)[key.c_str()];
        }
        else
        {
            return result; // Return empty if key not found
        }
    }
    
    // Extract array values
    if (current->IsArray())
    {
        for (rapidjson::SizeType i = 0; i < current->Size(); i++)
        {
            const rapidjson::Value& opt = (*current)[i];
            if (opt.IsInt())
            {
                result.push_back(opt.GetInt());
            }
        }
    }
    
    return result;
}

void SetInferenceConfigurationFromIEOption(dxrt::InferenceOption& op, const IEOption& ieOption)
{
    // Set ORT option (now boolean)
    op.useORT = ieOption.ort;

    // Based on run_model.cpp, the bound option mapping:
    // 0: NPU_ALL, 1: NPU_0, 2: NPU_1, 3: NPU_2, 4: NPU_0/1, 5: NPU_1/2, 6: NPU_0/2
    
    if (ieOption.bound == "NPU_ALL")
    {
        op.boundOption = 0;
    }
    else if (ieOption.bound == "NPU_0")
    {
        op.boundOption = 1;
    }
    else if (ieOption.bound == "NPU_1")
    {
        op.boundOption = 2;
    }
    else if (ieOption.bound == "NPU_2")
    {
        op.boundOption = 3;
    }
    else if (ieOption.bound == "NPU_01" || ieOption.bound == "NPU_0/1")
    {
        op.boundOption = 4;  // NPU_0/1
    }
    else if (ieOption.bound == "NPU_12" || ieOption.bound == "NPU_1/2")
    {
        op.boundOption = 5;  // NPU_1/2
    }
    else if (ieOption.bound == "NPU_02" || ieOption.bound == "NPU_0/2")
    {
        op.boundOption = 6;  // NPU_0/2
    }
    else
    {
        cout << "Error: Invalid bound option: " << ieOption.bound << endl;
        std::exit(-1);
    }

    // Set device options
    op.devices.clear();  // Clear any existing devices
    
    if (ieOption.device == "all")
    {
        // Leave devices empty for "all" - engine will use all available devices
    }
    else
    {
        // Parse comma-separated device IDs
        std::stringstream ss(ieOption.device);
        string segment;
        bool has_valid_device = false;
        
        while (std::getline(ss, segment, ','))
        {
            // Remove whitespace
            segment.erase(std::remove_if(segment.begin(), segment.end(), ::isspace), segment.end());
            if (segment.empty()) continue;
            
            // Check if segment contains only digits
            if (segment.find_first_not_of("0123456789") != std::string::npos)
            {
                cout << "Error: Invalid device option: " << ieOption.device << " (contains non-numeric value: '" << segment << "')" << endl;
                std::exit(-1);
            }
            
            try
            {
                int device_id = std::stoi(segment);
                op.devices.push_back(device_id);
                has_valid_device = true;
            }
            catch (const std::exception& e)
            {
                cout << "Error: Invalid device option: " << ieOption.device << " (failed to parse: '" << segment << "')" << endl;
                std::exit(-1);
            }
        }
        
        // If no valid devices were parsed, it's an error
        if (!has_valid_device)
        {
            cout << "Error: Invalid device option: " << ieOption.device << " (no valid device IDs found)" << endl;
            std::exit(-1);
        }
    }
}
