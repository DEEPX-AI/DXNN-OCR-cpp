#pragma once

#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <thread>
#include <map>

#include "dxrt/dxrt_api.h"
#include "generator.h"

struct BitMatchResult 
{
    bool modelRun;
    bool bitMatchRun;
    bool isFail;
    int failCount; // Currently unused
};

class BitMatcher 
{
    public:
        BitMatcher(std::string inputPath, int version, bool ort, size_t outputSize, std::vector<uint8_t>& mask);
        ~BitMatcher();

        void BitMatch();

        void SetOutput(dxrt::TensorPtrs& outputs);
        void LoadGTBuffer();

        BitMatchResult GetResult() { return {true, _isRun, _isFail, _failCount}; }

    private:
        int bitMatch(void* pOutput, uint64_t size , uint64_t offset);
        std::string getGTFilePath();
        void saveDump(void* pOutput, void* gtPtr);

        const int BYTE_BIT_COUNT = 8;

        // MEMORY_SAFETY: Store a copy of outputs to keep shared_ptr alive
        dxrt::TensorPtrs _outputs;  // Actual storage (no pointer needed!)
        
        // THREAD_SAFETY: Protect _outputs from concurrent access
        std::mutex _outputsMutex;
        
        std::string _inputPath;
        std::string _gtPath;
        int _version;
        bool _ort;
        size_t _outputSize;
        const vector<uint8_t>& _mask;
        int _numOutput;
        vector<uint8_t> _gt;
        int _failCount = 0;
        bool _isRun = false;
        bool _isFail = false;
        bool _isOutputSet = false;
        bool _isGTLoaded = false;
        bool _verbose = false;
};