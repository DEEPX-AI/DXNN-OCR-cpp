#include <iostream>
#include <fstream>
#include <random>
#include <sys/stat.h>

#include "../include/input_utils.h"
#include "../include/utils.h"

using std::cout;
using std::endl;
using std::string;

InputUtils::InputUtils(const ExecutionOption& execOption, const TestCase& testCase, dxrt::InferenceEngine& ie, int threadNumber) 
: _execOption(execOption), _testCase(testCase), _ie(ie)
{
    _fileIndex = std::to_string(threadNumber % 5);
}

InputUtils::~InputUtils() 
{
    // Clear input buffer (vector will handle memory deallocation automatically)
    if (!_inputBuffer.empty())
    {
        _inputBuffer.clear();
        _inputBuffer.shrink_to_fit(); // Force memory deallocation
    }
    
    // Clear file path
    _filePath.clear();
}

void InputUtils::CreateInputBuffer() 
{
    _filePath = getInputFilePath();

    std::ifstream file(_filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) 
    {
        throw std::runtime_error("Failed to open binary input file: " + _filePath);
    }
    
    // Get file size (ate moves to end)
    auto file_size_pos = file.tellg();
    _inputSize = static_cast<std::size_t>(file_size_pos);
    file.seekg(0, std::ios::beg);

    if (_execOption.bitmatch)
    {
        readInputFile();
    }
    else
    {
        generateDummyInput();
    }
}

string InputUtils::getInputFilePath() 
{
    // Extract directory path from model_path and append "gt/"
    string model_path = _testCase.ieOption.model_path;
    size_t last_slash = model_path.find_last_of("/\\");

    string post_fix = _fileIndex + ".bin";

    string base_input = "input_" + post_fix;
    string npu_input = "npu_0_input_" + post_fix;
    string cpu_input = "cpu_0_input_" + post_fix;
    string encoder_input = "npu_0_encoder_input_" + post_fix;

    string result;
    
    string gt_path;
    if (last_slash != string::npos)
    {
        gt_path = model_path.substr(0, last_slash) + "/gt/";
    }
    else
    {
        cout << "Error: Directory should be in the format of /path/to/model/model.dxnn: " << model_path << endl;
        exit(-1);
    }

    // Test whether it's v6 or v7
    string test_v7 = gt_path + encoder_input;
    std::ifstream file(test_v7);

    if (file.good())
    {
        _version = 7;
    }
    else
    {
        _version = 6;
    }

    if (_ie.GetCompileType() == "debug")
    {
        result = gt_path + npu_input;
        return result;
    }

    if (_testCase.ieOption.ort)
    {
        result = gt_path + base_input;
        return result;
    }
    else
    {
        // Valid for both v6 and v7
        result = gt_path + npu_input;

        if (_version == 7)
        {
            return test_v7;
        }
        else
        {
            return result;
        }
    }
}

void InputUtils::readInputFile() 
{
    std::ifstream file(_filePath, std::ios::binary);
    
    if (!file.is_open()) 
    {
        throw std::runtime_error("Failed to open binary input file: " + _filePath);
    }
    
    // Create vector and read data
    _inputBuffer.resize(_inputSize);
    file.read(reinterpret_cast<char*>(_inputBuffer.data()), _inputSize);

    if (!file) 
    {
        throw std::runtime_error("Failed to read file: " + _filePath);
    }
}


void InputUtils::generateDummyInput()
{
    size_t inputSize = _ie.GetInputSize();
    _inputBuffer.resize(inputSize);
    
    // Fill with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < inputSize; ++i) 
    {
        _inputBuffer[i] = static_cast<uint8_t>(dis(gen));
    }
}