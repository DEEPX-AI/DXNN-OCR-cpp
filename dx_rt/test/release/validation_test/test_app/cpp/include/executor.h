#pragma once

#include <memory>
#include <vector>

#include "dxrt/dxrt_api.h"
#include "concurrent_queue.h"
#include "generator.h"
#include "bitmatcher.h"

struct ThreadSyncData {
    int runCount = 0;
    int callbackCount = 0;
    int threadId = -1;  // 추가: thread number
    std::mutex mutex;
    std::condition_variable cv;
};

class BaseExecutor
{
    public:
        BaseExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath);
        ~BaseExecutor();
        
        // Template method - handles common validation logic
        BitMatchResult Execute();

        // Wrapping functions for Shared IE Bitmatch
        void SetOutput(dxrt::TensorPtrs& outputs) { _bm->SetOutput(outputs); };
        void BitMatch() { _bm->BitMatch(); };
        
        // Debug helper to get output buffer pointer
        void* GetOutputPtr() const { return _outputPtr; }

    protected:
        // Pure virtual method for actual execution logic
        virtual void doExecute() = 0;
        
        // Helper methods for style-aware inference
        dxrt::TensorPtrs runInference();
        int runInferenceAsync(void* userArg = nullptr);
        void validateOptions();

        dxrt::InferenceEngine& _ie;
        const TestCase& _testCase;
        const ExecutionOption& _execOption;
        void* _inputBuffer;
        int _time;
        int _loop;
        int _count;
        bool _isValid;
        int _inputCount;
        void* _outputPtr;
        std::shared_ptr<uint8_t> _outputBuffer;  // Shared ownership for async callbacks
        int _version;
        std::string _inputPath;
        vector<uint8_t> _mask = {};
        std::unique_ptr<BitMatcher> _bm;
        bool _shouldBitMatch;
};

class SyncExecutor : public BaseExecutor
{
    public:
        SyncExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath);
        ~SyncExecutor();
        
    protected:
        void doExecute() override;
};

class AsyncCallbackExecutor : public BaseExecutor
{
    public:
        AsyncCallbackExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath, ThreadSyncData* syncData = nullptr);
        ~AsyncCallbackExecutor();
        
    protected:
        void doExecute() override;
    private:
        ThreadSyncData* _mySyncData;

        // For multi-ie mode (when _mySyncData is nullptr)
        int _individualRunCount = 0;
        int _individualCallbackCount = 0;
        bool _individualRunComplete = false;
        std::mutex _individualCbMutex;
        std::condition_variable _individualCbCv;
};

class AsyncWaitExecutor : public BaseExecutor
{
    public:
        AsyncWaitExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath);
        ~AsyncWaitExecutor();
        
    protected:
        void doExecute() override;
    private:
        ConcurrentQueue<int> _idQueue;
        std::atomic<bool> _producerDone{false};
        std::thread _consumerThread;
        
        void consumerWorker();
};


class BatchExecutor : public BaseExecutor
{
    public:
        BatchExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath);
        ~BatchExecutor();
        
    protected:
        void doExecute() override;
    private:
        int _batchSize = 3;
        std::vector<void*> _inputBuffers;
        std::vector<void*> _outputBuffers;
        std::vector<void*> _userArgs;
};


// Factory function to create BaseExecutor instances
std::unique_ptr<BaseExecutor> CreateExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath, ThreadSyncData* syncData = nullptr);