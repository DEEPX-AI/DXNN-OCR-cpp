#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <memory>
#include <thread>
#include <chrono>
#include <mutex>

#include "dxrt/dxrt_api.h"
#include "dxrt/extern/cxxopts.hpp"
#include "dxrt/filesys_support.h"
#include "dxrt/profiler.h"

#include "include/generator.h"
#include "include/test_manager.h"
#include "include/executorManager.h"
#include "include/executor.h"
#include "include/input_utils.h"
#include "include/utils.h"

using std::string;
using std::cout;
using std::endl;

int main(int argc, char *argv[])
{
    string base_path;
    string json_file;
    string result_file;
    int verbose;
    int log_level;
    bool random;
    bool only_csv;

    cxxopts::Options options("Inference Engine Validation Test", "Validate DXRT");
    options.add_options()
        ("b,base-path", "Path of base directory for dxnn model files" , cxxopts::value<string>(base_path))
        ("j,json-file", "Path of json file for test files" , cxxopts::value<string>(json_file))
        ("r,result-file", "Path and name of result file" , cxxopts::value<string>(result_file)->default_value(""))
        ("v,verbose", "Verbose level: 0=failed only, 1=show progress, 2=include skipped, 3=all, 4=debug", cxxopts::value<int>(verbose)->default_value("0"))
        ("l,log-level", "Log level: 0=failed only, 1=show progress, 2=include skipped, 3=all, 4=debug", cxxopts::value<int>(log_level)->default_value("0"))
        ("random", "Randomize test case generation")
        ("only-csv", "Only generate data results (csv)")
        ("h, help", "Print usage" );
    
    try
    {
        auto cmd = options.parse(argc, argv);

        if (argc <= 3) // Minimum required arguments: --base-path, --json-file
        {
            cout << "Minimum required arguments: --base-path, --json-file" << endl;
            cout << endl;
            cout << options.help() << endl;
            exit(1);
        }
        
        // Show help if no arguments provided or help flag is used
        if (cmd.count("help"))
        {
            cout << options.help() << endl;
            exit(0);
        }

        
        // Set random flag based on whether --random option was provided
        random = cmd.count("random") > 0;
        
        // Set only_csv flag based on whether --only-csv option was provided
        only_csv = cmd.count("only-csv") > 0;
    }
    catch (const std::exception& e)
    {
        cout << "Error: " << e.what() << endl;
        exit(1);
    }

    if (base_path.empty())
    {
        cout << "Error: base-path is required" << endl;
        exit(1);
    }

    if (json_file.empty())
    {
        cout << "Error: json-path is required" << endl;
        exit(1);
    }

    if (!dxrt::fileExists(base_path))
    {
        cout << "Error: base path does not exist" << endl;
        exit(1);
    }

    if (!dxrt::fileExists(json_file))
    {
        cout << "Error: json file does not exist" << endl;
        exit(1);
    }

    Generator generator(base_path, json_file, random);

    // Load and parse JSON file
    if (!generator.LoadJson())
    {
        cout << "Error: Failed to load or parse JSON file" << endl;
        exit(1);
    }

    // Generate test cases with optimized IE structure
    generator.GenerateTestCases();
    const std::vector<TestCase>& test_cases = generator.GetTestCases();
    
    // Create and run TestManager
    TestManager testManager(test_cases, verbose, log_level, result_file);
    testManager.Run();

    if (!result_file.empty())
    {
        if (only_csv)
        {
            cout << "GEGEGE" << endl;
            testManager.MakeTable();
        }
        else
        {
            testManager.MakeReport();
        }
    }

    return 0;
}
