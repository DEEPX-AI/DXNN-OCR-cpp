#pragma once

#include <string>

#include "generator.h"
#include "dxrt/dxrt_api.h"

class InputUtils 
{
    public:
        // Main interface for creating input buffers
        InputUtils(const ExecutionOption& execOption, const TestCase& testCase, dxrt::InferenceEngine& ie, int threadNumber);
        ~InputUtils();

        //Split Create and Get for debugging
        void CreateInputBuffer();
        std::vector<uint8_t> GetInputBuffer() { return _inputBuffer; }
        int GetVersion() { return _version; }
        std::string GetFilePath() { return _filePath; }

    private:
        const ExecutionOption& _execOption;
        const TestCase& _testCase;
        dxrt::InferenceEngine& _ie;
        std::vector<uint8_t> _inputBuffer;
        string _filePath;
        size_t _inputSize;
        int _version;
        string _fileIndex;

        std::string getInputFilePath();
        void generateDummyInput();
        void readInputFile();
};
