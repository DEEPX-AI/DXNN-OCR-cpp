#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <map>

#include "generator.h"
#include "executorManager.h"

enum ResultStatus
{
    FAIL,
    SUCCESS,
    SKIP
};

enum ResultType
{
    NONE,
    BITMATCH,
    EXECUTION,
};

struct ResultInform
{
    ResultStatus status;
    ResultType type;
    IEOption ieOption;
    ExecutionOption execOption;
    std::chrono::duration<double> duration;
};

struct ModelSummary
{
    std::string modelName;
    int totalTests;
    int passedTests;
    int bmFailedTests;
    int tcInvalidTests;
    int bmSkippedTests;
    int tcSkippedTests;
    bool isModelPassed;  // true if bmFailedTests == 0 && tcInvalidTests == 0
};

class TestManager 
{
    public:
        TestManager(const std::vector<TestCase>& testCases, int verbose, int logLevel, const std::string& resultName);
        ~TestManager();
        
        void MakeReport();
        void MakeTable();
        void Run();

    private:
        const std::vector<TestCase>& _testCases;

        int _verbose; // 0: failed only, 1: show progress, 2: include skipped, 3: all, 4: debug
        int _logLevel; // 0: failed only, 1: show progress, 2: include skipped, 3: all, 4: debug
        std::string _resultName;

        int _totalTests = 0;
        int _passedTests = 0;
        int _bmFailedTests = 0;
        int _tcInvalidTests = 0;
        int _bmSkippedTests = 0;
        int _tcSkippedTests = 0; // Ignored Test Cases

        int _totalPassedTests = 0;
        int _totalBmFailedTests = 0;
        int _totalTcInvalidTests = 0;
        int _totalBmSkippedTests = 0;
        int _totalTcSkippedTests = 0;

        // Model-level counters
        int _totalModels = 0;
        int _passedModels = 0;
        int _failedModels = 0;

        std::string _tempPath = "";


        int _currentRun = 0;

        std::chrono::steady_clock::time_point _start;
        std::chrono::steady_clock::time_point _end;
        std::chrono::system_clock::time_point _systemStart; // For ISO 8601 timestamp

        std::vector<ResultInform> _savedTests;
        std::vector<ModelSummary> _modelSummaries;
        
        void runSingleTestCase(const TestCase& testCase, int testCaseNum);
        void runExecutionOption(const TestCase& testCase, const ExecutionOption& execOption);

        bool validate(const TestCase& testCase, const ExecutionOption& execOption);
        void flushResults();
        void flushResults(const std::string& modelName);

        void saveTempResult(const IEOption& ieOption, const ExecutionOption& execOption);
        void printTestSummary(std::string modelName);
        void printTestCaseInfo(const TestCase& testCase);
        void printExecutionOptionInfo(const ExecutionOption& execOption);
};