#include "../include/test_manager.h"
#include "../include/executorManager.h"
#include "dxrt/dxrt_api.h"

#include <iostream>
#include <chrono>
#include <unistd.h>    
#include <limits.h>    

using std::cout;
using std::endl;

TestManager::TestManager(const std::vector<TestCase>& testCases, int verbose, int logLevel, const std::string& resultName)
    : _testCases(testCases), _verbose(verbose), _logLevel(logLevel), _resultName(resultName)
{
    // Get executable directory path and set _tempPath to test_config/tmp
    char executable_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);

    if (len != -1) 
    {
        executable_path[len] = '\0';
        std::string exe_path = executable_path;
        
        // Find project root by looking for the main bin directory pattern
        // Expected path: /some/path/dx_rt_fork/bin/validation_test
        std::string pattern = "/bin/validation_test";
        size_t pattern_pos = exe_path.find(pattern);
        if (pattern_pos != std::string::npos) 
        {
            // Extract project root path: everything before "/bin/validation_test"
            std::string project_root = exe_path.substr(0, pattern_pos);
            
            // Set _tempPath to test_config/tmp relative to project root
            _tempPath = project_root + "/test/release/validation_test/test_config/tmp";
        } 
        else 
        {
            // Fallback: use relative path from executable directory
            size_t last_slash = exe_path.find_last_of("/");
            if (last_slash != std::string::npos) 
            {
                std::string exe_dir = exe_path.substr(0, last_slash);
                // Navigate from main bin to test_config/tmp
                _tempPath = exe_dir + "/test/release/validation_test/test_config/tmp";
            } 
            else 
            {
                _tempPath = "./test_config/tmp";
            }
        }
    }
    else 
    {
        _tempPath = "."; // Fallback
    }

    // Count total execution options across all test cases
    for (const auto& test_case : _testCases)
    {
        _totalTests += test_case.execOptions.size();
    }
    _start = std::chrono::steady_clock::now();
    _systemStart = std::chrono::system_clock::now();
}

TestManager::~TestManager()
{
}

void TestManager::Run()
{
    int test_case_num = 0;
    string current_model = "";
    
    cout << "Run total " << _totalTests << " test cases" << endl;
    for (const auto& test_case : _testCases)
    {
        if (current_model != test_case.ieOption.model_path)
        {
            if (current_model != "")
            {
                printTestSummary(current_model);
                flushResults(current_model);
            }
            current_model = test_case.ieOption.model_path;
            cout << endl;
            cout << "Current Model: " << current_model << endl;
        }
        test_case_num++;
        runSingleTestCase(test_case, test_case_num);
    }
    
    // Print summary for the last model
    if (!current_model.empty())
    {
        printTestSummary(current_model);
        flushResults(current_model);
    }
}


void TestManager::runSingleTestCase(const TestCase& testCase, int testCaseNum)
{
    try
    {
        // Configure dynamic CPU offloading
        dxrt::Configuration& config = dxrt::Configuration::GetInstance();
        config.SetEnable(dxrt::Configuration::ITEM::SHOW_PROFILE, false);
        if (testCase.ieOption.dynamicCpuOffloading == "on")
        {
            config.SetEnable(dxrt::Configuration::ITEM::DYNAMIC_CPU_THREAD, true);
        }
        else
        {
            config.SetEnable(dxrt::Configuration::ITEM::DYNAMIC_CPU_THREAD, false);
        }
        
        for (const auto& exec_option : testCase.execOptions)
        {
            runExecutionOption(testCase, exec_option);
        }
    }
    catch (const std::exception& e)
    {
        cout << "====== Test Case " << testCaseNum << " failed ======" << endl;
        cout << "Error: " << e.what() << endl;
    }
    catch (const dxrt::Exception& e)
    {
        cout << "====== Test Case " << testCaseNum << " failed (dxrt) ======" << endl;
        cout << "Error: " << e.what() << " error-code=" << e.code() << endl;
    }
}

void TestManager::runExecutionOption(const TestCase& testCase, const ExecutionOption& execOption)
{
    _currentRun++;
    if (_verbose >= 1)
    {
        cout << "Progress: " << _currentRun << "/" << _totalTests << endl;
    }

    // For debug
    if (_verbose >= 4)
    {
        cout << "--------------------- RUNNING TEST CASE ---------------------" << endl;
        printTestCaseInfo(testCase);
        printExecutionOptionInfo(execOption);
    }

    saveTempResult(testCase.ieOption, execOption);

    if (validate(testCase, execOption))
    {
        auto manager = CreateExecutorManager(testCase, execOption);

        auto duration_start = std::chrono::steady_clock::now();
        auto result = manager->Run();
        auto duration_end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration<double>(duration_end - duration_start);

        switch (result)
        {
            case RunResult::BM_PASS:
            {
                _passedTests++;
                if (_verbose >= 3)
                {
                    cout << endl;
                    cout << "++++++++++++++++++++ SUCCESS TEST CASE ++++++++++++++++++++" << endl;
                    printTestCaseInfo(testCase);
                    printExecutionOptionInfo(execOption);
                }

                if (_logLevel >= 3)
                {
                    _savedTests.push_back({ResultStatus::SUCCESS, ResultType::BITMATCH, testCase.ieOption, execOption, duration});
                }
                break;
            }

            case RunResult::BM_SKIP:
            {
                _bmSkippedTests++;
                if (_verbose >= 2)
                {
                    cout << endl;
                    cout << "==================== BITMATCH SKIPPED TEST CASE ====================" << endl;
                    printTestCaseInfo(testCase);
                    printExecutionOptionInfo(execOption);
                }
                if (_logLevel >= 2)
                {
                    _savedTests.push_back({ResultStatus::SKIP, ResultType::BITMATCH, testCase.ieOption, execOption, duration});
                }
                break;
            }

            case RunResult::TC_SKIP:
            {
                _tcSkippedTests++;
                if (_verbose >= 2)
                {
                    cout << endl;
                    cout << "==================== TEST CASE IGNORED ====================" << endl;
                    printTestCaseInfo(testCase);
                    printExecutionOptionInfo(execOption);
                }
                if (_logLevel >= 2)
                {
                    _savedTests.push_back({ResultStatus::SKIP, ResultType::EXECUTION, testCase.ieOption, execOption, duration});
                }
                break;
            }

            case RunResult::BM_FAIL:
            {
                cout << endl;
                cout << "xxxxxxxxxxxxxxxxxxxx BITMATCH FAILED TEST CASE xxxxxxxxxxxxxxxxxxxx" << endl;

                _bmFailedTests++;
                printTestCaseInfo(testCase);
                printExecutionOptionInfo(execOption);
                _savedTests.push_back({ResultStatus::FAIL, ResultType::BITMATCH, testCase.ieOption, execOption, duration});
                break;
            }

            case RunResult::TC_INVALID:
            {
                cout << endl;
                cout << "xxxxxxxxxxxxxxxxxxxx INVALID TEST CASE xxxxxxxxxxxxxxxxxxxx" << endl;

                _tcInvalidTests++;
                printTestCaseInfo(testCase);
                printExecutionOptionInfo(execOption);
                _savedTests.push_back({ResultStatus::FAIL, ResultType::EXECUTION, testCase.ieOption, execOption, duration});
                break;
            }

            default:
            {
                cout << "Error: Unknown RunResult value" << endl;
                break;
            }
        }
    }
    else
    {
        _tcInvalidTests++;
        return;
    }
}

bool TestManager::validate(const TestCase& testCase, const ExecutionOption& execOption)
{
    if (testCase.ieOption.threadType != "single-ie" && testCase.ieOption.threadType != "multi-ie")
    {
        cout << "Error: Unknown thread type: " << testCase.ieOption.threadType << endl;
        return false;
    }

    if (testCase.ieOption.threadCount < 1)
    {
        cout << "Error: Invalid thread count: " << testCase.ieOption.threadCount << endl;
        return false;
    }

    if (execOption.time <= 0 && execOption.loop <= 0)
    {
        cout << "Error: both time and loop cannot be zero, skipping this execution option" << endl;
        return false;
    }
    
    return true;
}

void TestManager::flushResults()
{
    _totalPassedTests += _passedTests;
    _totalBmFailedTests += _bmFailedTests;
    _totalTcInvalidTests += _tcInvalidTests;
    _totalBmSkippedTests += _bmSkippedTests;
    _totalTcSkippedTests += _tcSkippedTests;

    _passedTests = 0;
    _bmFailedTests = 0;
    _tcInvalidTests = 0;
    _bmSkippedTests = 0;
    _tcSkippedTests = 0;
}

void TestManager::flushResults(const std::string& modelName)
{
    // Save model summary before flushing
    ModelSummary summary;
    summary.modelName = modelName;
    summary.totalTests = _passedTests + _bmFailedTests + _tcInvalidTests + _bmSkippedTests + _tcSkippedTests;
    summary.passedTests = _passedTests;
    summary.bmFailedTests = _bmFailedTests;
    summary.tcInvalidTests = _tcInvalidTests;
    summary.bmSkippedTests = _bmSkippedTests;
    summary.tcSkippedTests = _tcSkippedTests;
    
    // Determine if model passed (failed or invalid > 0 means model failed)
    summary.isModelPassed = (_bmFailedTests == 0 && _tcInvalidTests == 0);
    
    _modelSummaries.push_back(summary);
    
    // Update model-level counters
    _totalModels++;
    if (summary.isModelPassed)
    {
        _passedModels++;
    }
    else
    {
        _failedModels++;
    }

    // Update totals
    _totalPassedTests += _passedTests;
    _totalBmFailedTests += _bmFailedTests;
    _totalTcInvalidTests += _tcInvalidTests;
    _totalBmSkippedTests += _bmSkippedTests;
    _totalTcSkippedTests += _tcSkippedTests;

    // Reset current counters
    _passedTests = 0;
    _bmFailedTests = 0;
    _tcInvalidTests = 0;
    _bmSkippedTests = 0;
    _tcSkippedTests = 0;
}

void TestManager::saveTempResult(const IEOption& ieOption, const ExecutionOption& execOption)
{
    // Create filename based on model name and current timestamp
    std::string model_name = ieOption.model_path;

    // Extract model directory name from path
    // Remove the filename first
    size_t last_slash = model_name.find_last_of("/\\");
    if (last_slash != std::string::npos) 
    {
        model_name = model_name.substr(0, last_slash);
        
        // Now extract the last directory name (model name)
        size_t second_last_slash = model_name.find_last_of("/\\");
        if (second_last_slash != std::string::npos) 
        {
            model_name = model_name.substr(second_last_slash + 1);
        }
    }
    
    std::string file_name = "temp_config.json";
    std::string full_path = _tempPath + "/" + file_name;
    
    std::ofstream json_file(full_path);
    if (!json_file.is_open()) 
    {
        std::cerr << "Error: Could not create temp result file: " << full_path << std::endl;
        return;
    }

    // Write JSON in partial_test.json format
    json_file << "{\n";
    
    // Model
    json_file << "    \"model\": [\"" << model_name << "\"],\n\n";
    
    // Configuration
    json_file << "    \"configuration\": {\n";
    json_file << "      \"dynamic-cpu-offloading\": [\"" << ieOption.dynamicCpuOffloading << "\"]\n";
    json_file << "    },\n\n";
    
    // Thread Style
    json_file << "    \"threadStyle\": [\n";
    json_file << "      {\n";
    json_file << "        \"type\": \"" << ieOption.threadType << "\",\n";
    json_file << "        \"count\": " << ieOption.threadCount << "\n";
    json_file << "      }\n";
    json_file << "    ],\n\n";
    
    // IE Options
    json_file << "    \"ieOption\": {\n";
    json_file << "      \"ort\": [" << (ieOption.ort ? "true" : "false") << "],\n";
    json_file << "      \"bound\": [\"" << ieOption.bound << "\"],\n";
    json_file << "      \"device\": [\"" << ieOption.device << "\"]\n";
    json_file << "    },\n\n";
    
    // Inference Function
    json_file << "    \"inferenceFunction\": [\"" << execOption.inferenceFunction << "\"],\n\n";
    
    // Input/Output Options
    json_file << "    \"inoutOption\": {\n";
    json_file << "      \"inputStyle\": [\"" << execOption.inputStyle << "\"],\n";
    json_file << "      \"outputBuffer\": [\"" << execOption.outputBuffer << "\"],\n";
    
    // Add asyncMethod only if inference function is async
    json_file << "      \"asyncMethod\": [\"" << execOption.asyncMethod << "\"],\n";
    json_file << "      \"callbackDelay\": 100,\n";
    
    json_file << "      \"loop\": " << execOption.loop << ",\n";
    json_file << "      \"time\": " << execOption.time << ",\n";
    json_file << "      \"bitmatch\": " << (execOption.bitmatch ? "true" : "false") << "\n";
    json_file << "    }\n";
    json_file << "}\n";
    
    json_file.close();
}

void TestManager::printTestSummary(string modelName)
{
    int currentModelTests = _passedTests + _bmFailedTests + _tcInvalidTests + _bmSkippedTests + _tcSkippedTests;
    
    cout << endl;
    cout << "================== Model Test Summary ==================" << endl;
    cout << "Model Name: " << modelName << endl;
    cout << "Total Tests: " << currentModelTests << endl;
    cout << "   Test Pass: " << _passedTests << endl;
    cout << "   Test Case Ignored: " << _tcSkippedTests << endl; // To clarify the meaning of "Skipped", we print it as "Test Case Ignored" 
    cout << "   BitMatch Skipped: " << _bmSkippedTests << endl;
    cout << "   BitMatch Failed: " << _bmFailedTests << endl;
    cout << "   Invalid: " << _tcInvalidTests << endl;
}

void TestManager::printTestCaseInfo(const TestCase& testCase)
{
    cout << "  Model: " << testCase.ieOption.model_path << endl;
    cout << "  Dynamic CPU Offloading: " << testCase.ieOption.dynamicCpuOffloading << endl;
    cout << "  Thread Type: " << testCase.ieOption.threadType << endl;
    cout << "  Thread Count: " << testCase.ieOption.threadCount << endl;
    cout << "  ORT: " << (testCase.ieOption.ort ? "true" : "false") << endl;
    cout << "  Bound: " << testCase.ieOption.bound << endl;
    cout << "  Device: " << testCase.ieOption.device << endl;
}

void TestManager::printExecutionOptionInfo(const ExecutionOption& execOption)
{
    cout << "  Inference Function: " << execOption.inferenceFunction << endl;
    if (execOption.inferenceFunction == "async")
    {
        cout << "      Async Method: " << execOption.asyncMethod << endl;
    }
    cout << "  Input Style: " << execOption.inputStyle << endl;
    cout << "  Output Buffer: " << execOption.outputBuffer << endl;
    if (execOption.time > 0)
    {
        cout << "  Time: " << execOption.time << endl;
    }
    else
    {
        cout << "  Loop: " << execOption.loop << endl;
    }
    cout << "  Bitmatch: " << (execOption.bitmatch ? "True" : "False") << endl;
}

void TestManager::MakeReport()
{
    _end = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration<double>(_end - _start).count();

    // Generate JSON report
    std::ofstream report_file(_resultName);
    if (!report_file.is_open()) 
    {
        std::cerr << "Error: Could not open file " << _resultName << " for writing" << std::endl;
        return;
    }

    // Convert _systemStart to ISO 8601 string (KST timezone)
    auto time_t = std::chrono::system_clock::to_time_t(_systemStart);
    
    // Add 9 hours for KST (UTC+9)
    time_t += 9 * 3600;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    std::string start_time = ss.str();

    report_file << "{\n";
    report_file << "  \"test_suite_name\": \"validation_app_test\",\n";
    report_file << "  \"start_time\": \"" << start_time << "\",\n";
    report_file << "  \"run_time_sec\": " << total_duration << ",\n";
    report_file << "  \"summary\": {\n";
    report_file << "    \"total_cases\": " << _totalTests << ",\n";
    report_file << "    \"passed\": " << _totalPassedTests << ",\n";
    report_file << "    \"failed\": " << _totalBmFailedTests << ",\n";
    report_file << "    \"ignored\": " << _totalTcSkippedTests << ",\n";
    report_file << "    \"skipped\": " << _totalBmSkippedTests << ",\n";
    report_file << "    \"invalid\": " << _totalTcInvalidTests << ",\n";
    report_file << "    \"total_models\": " << _totalModels << ",\n";
    report_file << "    \"passed_models\": " << _passedModels << ",\n";
    report_file << "    \"failed_models\": " << _failedModels << "\n";
    report_file << "  },\n";
    
    // Add model summaries
    report_file << "  \"model_summaries\": [\n";
    for (size_t i = 0; i < _modelSummaries.size(); ++i) 
    {
        const auto& summary = _modelSummaries[i];
        report_file << "    {\n";
        report_file << "      \"model_name\": \"" << summary.modelName << "\",\n";
        report_file << "      \"model_status\": \"" << (summary.isModelPassed ? "pass" : "fail") << "\",\n";
        report_file << "      \"total_tests\": " << summary.totalTests << ",\n";
        report_file << "      \"passed\": " << summary.passedTests << ",\n";
        report_file << "      \"bitmatch_failed\": " << summary.bmFailedTests << ",\n";
        report_file << "      \"invalid\": " << summary.tcInvalidTests << ",\n";
        report_file << "      \"bitmatch_skipped\": " << summary.bmSkippedTests << ",\n";
        report_file << "      \"test_case_ignored\": " << summary.tcSkippedTests << "\n";
        
        if (i < _modelSummaries.size() - 1) 
        {
            report_file << "    },\n";
        }
        else 
        {
            report_file << "    }\n";
        }
    }
    report_file << "  ],\n";
    
    report_file << "  \"results\": {\n";

    // Group test results by model path
    std::map<std::string, std::vector<const ResultInform*>> modelResults;
    for (const auto& save : _savedTests) 
    {
        modelResults[save.ieOption.model_path].push_back(&save);
    }

    // Write grouped test results
    size_t modelCount = 0;
    for (const auto& modelResult : modelResults)
    {
        const std::string& modelPath = modelResult.first;
        const std::vector<const ResultInform*>& results = modelResult.second;
        
        report_file << "    \"" << modelPath << "\": [\n";
        
        for (size_t i = 0; i < results.size(); ++i) 
        {
            const auto& save = *results[i];

            report_file << "      {\n";

            // Status and type
            if (save.status == ResultStatus::SUCCESS) 
            {
                report_file << "        \"status\": \"success\",\n";
                if (save.type == ResultType::BITMATCH) 
                {
                    report_file << "        \"type\": \"bitmatch_pass\",\n";
                }
            }
            else if (save.status == ResultStatus::FAIL) 
            {
                report_file << "        \"status\": \"fail\",\n";
                if (save.type == ResultType::BITMATCH) 
                {
                    report_file << "        \"type\": \"bitmatch_fail\",\n";
                }
                else if (save.type == ResultType::EXECUTION) 
                {
                    report_file << "        \"type\": \"execution_fail\",\n";
                }
            }
            else if (save.status == ResultStatus::SKIP) 
            {
                report_file << "        \"status\": \"skip\",\n";
                if (save.type == ResultType::BITMATCH) 
                {
                    report_file << "        \"type\": \"bitmatch_skip\",\n";
                }
                else if (save.type == ResultType::EXECUTION) 
                {
                    report_file << "        \"type\": \"execution_ignored\",\n";
                }
            }

            report_file << "        \"duration_sec\": " << save.duration.count() << ",\n";
            report_file << "        \"dynamic_cpu_offloading\": " << (save.ieOption.dynamicCpuOffloading == "on" ? "true" : "false") << ",\n";
            report_file << "        \"thread_type\": \"" << save.ieOption.threadType << "\",\n";
            report_file << "        \"thread_count\": " << save.ieOption.threadCount << ",\n";
            report_file << "        \"ort\": " << (save.ieOption.ort ? "true" : "false") << ",\n";
            report_file << "        \"core_bound\": \"" << save.ieOption.bound << "\",\n";
            report_file << "        \"device_bound\": \"" << save.ieOption.device << "\",\n";
            report_file << "        \"inference_function\": \"" << save.execOption.inferenceFunction << "\",\n";
            report_file << "        \"input_style\": \"" << save.execOption.inputStyle << "\",\n";
            report_file << "        \"output_buffer\": \"" << save.execOption.outputBuffer << "\",\n";
            report_file << "        \"async_method\": \"" << save.execOption.asyncMethod << "\",\n";
            report_file << "        \"loop\": " << save.execOption.loop << ",\n";
            report_file << "        \"time\": " << save.execOption.time << ",\n";
            report_file << "        \"bitmatch\": " << (save.execOption.bitmatch ? "true" : "false") << "\n";

            if (i < results.size() - 1) 
            {
                report_file << "      },\n";
            }
            else 
            {
                report_file << "      }\n";
            }
        }
        
        modelCount++;
        if (modelCount < modelResults.size()) 
        {
            report_file << "    ],\n";
        }
        else 
        {
            report_file << "    ]\n";
        }
    }

    report_file << "  }\n";
    report_file << "}\n";
    
    report_file.close();
    
    std::cout << "\nTest report saved to: " << _resultName << std::endl;
    std::cout << "Total execution time: " << total_duration << " sec" << std::endl;
}

void TestManager::MakeTable()
{
    // Generate CSV filename from JSON filename
    std::string csv_filename = _resultName;
    size_t json_pos = csv_filename.rfind(".json");
    if (json_pos != std::string::npos) 
    {
        csv_filename.replace(json_pos, 5, ".csv");
    }
    else 
    {
        csv_filename += ".csv";
    }

    std::ofstream csv_file(csv_filename);
    if (!csv_file.is_open()) 
    {
        std::cerr << "Error: Could not open file " << csv_filename << " for writing" << std::endl;
        return;
    }

    // Write CSV header
    csv_file << "model_path,status,type,duration_sec,dynamic_cpu_offloading,thread_type,thread_count,ort,core_bound,device_bound,inference_function,input_style,output_buffer,async_method,loop,time,bitmatch\n";

    // Write data rows
    for (const auto& save : _savedTests) 
    {
        // Model path
        csv_file << "\"" << save.ieOption.model_path << "\",";

        // Status
        if (save.status == ResultStatus::SUCCESS) 
        {
            csv_file << "success,";
        }
        else if (save.status == ResultStatus::FAIL) 
        {
            csv_file << "fail,";
        }
        else if (save.status == ResultStatus::SKIP) 
        {
            csv_file << "skip,";
        }

        // Type
        if (save.status == ResultStatus::SUCCESS && save.type == ResultType::BITMATCH) 
        {
            csv_file << "bitmatch_pass,";
        }
        else if (save.status == ResultStatus::FAIL && save.type == ResultType::BITMATCH) 
        {
            csv_file << "bitmatch_fail,";
        }
        else if (save.status == ResultStatus::FAIL && save.type == ResultType::EXECUTION) 
        {
            csv_file << "execution_fail,";
        }
        else if (save.status == ResultStatus::SKIP && save.type == ResultType::BITMATCH) 
        {
            csv_file << "bitmatch_skip,";
        }
        else if (save.status == ResultStatus::SKIP && save.type == ResultType::EXECUTION) 
        {
            csv_file << "execution_ignored,";
        }
        else 
        {
            csv_file << "unknown,";
        }

        // Duration
        csv_file << save.duration.count() << ",";

        // Dynamic CPU offloading
        csv_file << (save.ieOption.dynamicCpuOffloading == "on" ? "true" : "false") << ",";

        // Thread type and count
        csv_file << save.ieOption.threadType << ",";
        csv_file << save.ieOption.threadCount << ",";

        // ORT
        csv_file << (save.ieOption.ort ? "true" : "false") << ",";

        // Bounds
        csv_file << save.ieOption.bound << ",";
        csv_file << save.ieOption.device << ",";

        // Inference function
        csv_file << save.execOption.inferenceFunction << ",";

        // Input/Output options
        csv_file << save.execOption.inputStyle << ",";
        csv_file << save.execOption.outputBuffer << ",";

        // Async method
        csv_file << save.execOption.asyncMethod << ",";

        // Loop and time
        csv_file << save.execOption.loop << ",";
        csv_file << save.execOption.time << ",";

        // Bitmatch
        csv_file << (save.execOption.bitmatch ? "true" : "false") << "\n";
    }

    csv_file.close();
    
    std::cout << "Test results table saved to: " << csv_filename << std::endl;
}