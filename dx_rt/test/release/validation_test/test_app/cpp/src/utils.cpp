#include "../include/utils.h"
#include <dirent.h>
#include <iostream>
#include <sys/stat.h>
#include <cstring>

using std::string;
using std::vector;
using std::cout;
using std::endl;

// Find .dxnn file in the given directory
string findDxnnFileInDirectory(const string& directory_path)
{
    DIR* dir = opendir(directory_path.c_str());
    if (dir == nullptr)
    {
        cout << "Error: Cannot open directory: " << directory_path << endl;
        return "";
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        string filename = entry->d_name;
        
        // Check if it's a regular file and has .dxnn extension
        if (filename.length() > 5 && filename.substr(filename.length() - 5) == ".dxnn")
        {
            string full_path = directory_path + "/" + filename;
            
            // Verify it's a regular file
            struct stat file_stat;
            if (stat(full_path.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode))
            {
                closedir(dir);
                return full_path;
            }
        }
    }
    
    closedir(dir);
    cout << "Warning: No .dxnn file found in directory: " << directory_path << endl;
    return "";
}


void PrintIE(dxrt::InferenceEngine& ie)
{
    cout << "\n=== InferenceEngine Information ===" << endl;
    
    // Basic model information
    cout << "Model Name: " << ie.GetModelName() << endl;
    cout << "Model Version: " << ie.GetModelVersion() << endl;
    cout << "Compile Type: " << ie.GetCompileType() << endl;
    
    // Model characteristics
    cout << "Is PPU Model: " << (ie.IsPPU() ? "Yes" : "No") << endl;
    cout << "Is ORT Configured: " << (ie.IsOrtConfigured() ? "Yes" : "No") << endl;
    cout << "Is Multi-Input Model: " << (ie.IsMultiInputModel() ? "Yes" : "No") << endl;
    
    // Input information
    cout << "\n--- Input Information ---" << endl;
    cout << "Input Tensor Count: " << ie.GetInputTensorCount() << endl;
    cout << "Total Input Size: " << ie.GetInputSize() << " bytes" << endl;
    
    auto inputNames = ie.GetInputTensorNames();
    auto inputSizes = ie.GetInputTensorSizes();
    cout << "Input Tensors:" << endl;
    for (size_t i = 0; i < inputNames.size(); ++i)
    {
        cout << "  [" << i << "] " << inputNames[i];
        if (i < inputSizes.size())
        {
            cout << " (" << inputSizes[i] << " bytes)";
        }
        cout << endl;
    }
    
    // Output information
    cout << "\n--- Output Information ---" << endl;
    cout << "Total Output Size: " << ie.GetOutputSize() << " bytes" << endl;
    cout << "Number of Tail Tasks: " << ie.GetNumTailTasks() << endl;
    
    auto outputNames = ie.GetOutputTensorNames();
    auto outputSizes = ie.GetOutputTensorSizes();
    cout << "Output Tensors:" << endl;
    for (size_t i = 0; i < outputNames.size(); ++i)
    {
        cout << "  [" << i << "] " << outputNames[i];
        if (i < outputSizes.size())
        {
            cout << " (" << outputSizes[i] << " bytes)";
        }
        cout << endl;
    }
    
    // Task information
    cout << "\n--- Task Information ---" << endl;
    auto taskOrder = ie.GetTaskOrder();
    cout << "Task Count: " << taskOrder.size() << endl;
    cout << "Task Order:" << endl;
    for (size_t i = 0; i < taskOrder.size(); ++i)
    {
        cout << "  [" << i << "] " << taskOrder[i] << endl;
    }
    
    // Input tensor to task mapping
    if (ie.IsMultiInputModel())
    {
        cout << "\n--- Input Tensor to Task Mapping ---" << endl;
        auto mapping = ie.GetInputTensorToTaskMapping();
        for (const auto& pair : mapping)
        {
            cout << "  " << pair.first << " -> " << pair.second << endl;
        }
    }
    
    cout << "===================================" << endl;
}

vector<uint8_t> CreateDummyInput(dxrt::InferenceEngine& ie)
{
    size_t inputSize = ie.GetInputSize();
    vector<uint8_t> dummyInput(inputSize);
    for (size_t i = 0; i < inputSize; ++i)
    {
        dummyInput[i] = static_cast<uint8_t>(i % 256);
    }
    return dummyInput;
}

void SleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
} 

int GetRandomInt(int max, int min)
{
    static std::random_device rd;  // Seed for random number generator
    static std::mt19937 gen(rd()); // Mersenne Twister RNG
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

std::string GetRandomElement(const std::vector<std::string>& options)
{
    if (options.empty()) return "";
    int index = GetRandomInt(options.size() - 1, 0);
    return options[index];
}

void saveBinary(void* ptr, size_t size, const std::string& filename)
{
    // Create dump directory if it doesn't exist
    struct stat info;
    if (stat("dump", &info) != 0 || !(info.st_mode & S_IFDIR)) 
    {
        if (mkdir("dump", 0777) != 0) 
        {
            std::cerr << "Error creating dump directory" << std::endl;
            return;
        }
    }

    // Ensure filename has .bin extension
    std::string full_filename = filename;
    if (full_filename.find(".bin") == std::string::npos) {
        full_filename += ".bin";
    }
    
    // Construct full file path
    std::string file_path = "dump/" + full_filename;

    // Open file for binary writing
    std::ofstream output_file(file_path, std::ios::binary);
    if (!output_file.is_open()) 
    {
        std::cerr << "Error opening file for dump: " << file_path << std::endl;
        return;
    }

    // Write data
    output_file.write(static_cast<char*>(ptr), size);
    output_file.close();
    
    std::cout << "Binary data saved to " << file_path << " (" << size << " bytes)" << std::endl;
}