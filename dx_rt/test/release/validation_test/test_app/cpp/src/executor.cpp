#include <iostream>
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

#include "../include/executor.h"
#include "../include/utils.h"
#include "../include/generator.h"
#include "../include/concurrent_queue.h"
#include "../include/input_utils.h"
#include "../include/bitmatcher.h"

using std::cout;
using std::endl;

BaseExecutor::BaseExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath) 
    : _ie(ie), _testCase(testCase), _execOption(execOption), _inputBuffer(inputBuffer), _version(version), _inputPath(inputPath)
{
    validateOptions();

    if (_execOption.time > 0)
    {
        _time = _execOption.time;
        _loop = 0;
    }
    else
    {
        _time = 0;
        _loop = _execOption.loop;
    }

    _count = 0; // Check whether this variable is essential or optional

    _shouldBitMatch = _execOption.bitmatch;

    if (_ie.GetCompileType() == "debug")
    {
        _shouldBitMatch = false;
    }

    // Prepare bitmatch mask for v6 models
    if (_version == 6 && _shouldBitMatch)
    {
        _mask = _ie.GetBitmatchMask(0);
    }

    _bm = std::make_unique<BitMatcher>(_inputPath, _version, _testCase.ieOption.ort, _ie.GetOutputSize(), _mask);
    if (_shouldBitMatch)
    {
        _bm->LoadGTBuffer();
    }
}

BaseExecutor::~BaseExecutor() 
{
    // Reset BitMatcher (smart pointer will handle deallocation)
    if (_bm)
    {
        _bm.reset();
    }
    
    // Clear mask vector
    _mask.clear();
}

BitMatchResult BaseExecutor::Execute()
{
    // Skip model run
    if (!_isValid)
    {
        return {false, false, false, 0}; // model run, bitmatch run, bitmatch fail, failCount
    }
    
    doExecute();

    return _bm->GetResult();
}

dxrt::TensorPtrs BaseExecutor::runInference()
{
    if (_execOption.inputStyle == "multi-map") 
    {
        // void* → map<string, void*> transformation
        auto input_names = _ie.GetInputTensorNames();
        auto input_sizes = _ie.GetInputTensorSizes();
        
        std::map<std::string, void*> input_map;
        uint8_t* bufferPtr = static_cast<uint8_t*>(_inputBuffer);
        size_t offset = 0;
        
        for (size_t i = 0; i < input_names.size(); ++i)
        {
            input_map[input_names[i]] = bufferPtr + offset;
            offset += input_sizes[i];
        }
        
        return _ie.RunMultiInput(input_map, nullptr, _outputPtr);
    }
    else if (_execOption.inputStyle == "multi-vec") 
    {
        // void* → vector<void*> transformation
        auto input_sizes = _ie.GetInputTensorSizes();
        std::vector<void*> input_vector;
        uint8_t* bufferPtr = static_cast<uint8_t*>(_inputBuffer);
        size_t offset = 0;
        
        for (size_t size : input_sizes)
        {
            input_vector.push_back(bufferPtr + offset);
            offset += size;
        }
        
        return _ie.RunMultiInput(input_vector, nullptr, _outputPtr);
    }
    else // "auto-split"
    {
        // void* as-is (auto-split by inference engine)
        return _ie.Run(_inputBuffer, nullptr, _outputPtr);
    }
}

int BaseExecutor::runInferenceAsync(void* userArg)
{
    if (_execOption.inputStyle == "multi-map") 
    {
        // void* → map<string, void*> transformation
        auto input_names = _ie.GetInputTensorNames();
        auto input_sizes = _ie.GetInputTensorSizes();
        
        std::map<std::string, void*> input_map;
        uint8_t* bufferPtr = static_cast<uint8_t*>(_inputBuffer);
        size_t offset = 0;
        
        for (size_t i = 0; i < input_names.size(); ++i)
        {
            input_map[input_names[i]] = bufferPtr + offset;
            offset += input_sizes[i];
        }
        
        return _ie.RunAsyncMultiInput(input_map, userArg, _outputPtr);
    }
    else if (_execOption.inputStyle == "multi-vec") 
    {
        // void* → vector<void*> transformation
        auto input_sizes = _ie.GetInputTensorSizes();
        
        std::vector<void*> input_vector;
        uint8_t* bufferPtr = static_cast<uint8_t*>(_inputBuffer);
        size_t offset = 0;
        
        for (size_t size : input_sizes)
        {
            input_vector.push_back(bufferPtr + offset);
            offset += size;
        }
        
        return _ie.RunAsyncMultiInput(input_vector, userArg, _outputPtr);
    }
    else // "auto-split"
    {
        // void* as-is (auto-split by inference engine)
        return _ie.RunAsync(_inputBuffer, userArg, _outputPtr);
    }
}

void BaseExecutor::validateOptions()
{
    _isValid = true;

    if (_execOption.inferenceFunction == "batch")
    {
        if (_execOption.outputBuffer == "internal")
        {
            _isValid = false;
            return;
        }
        else
        {
            if (_ie.IsMultiInputModel())
            {
                if (_execOption.inputStyle != "multi-autosplit")
                {
                    _isValid = false;
                    return;
                }
            }
            else
            {
                if (_execOption.inputStyle != "single")
                {
                    _isValid = false;
                    return;
                }
            }
        }
    }

    if (!_ie.IsMultiInputModel()) // single input model
    {
        if (_execOption.inputStyle.find("multi") != std::string::npos)
        {
            _isValid = false;
            return;
        }
        else
        {
            _inputCount = 1;
        }
    }
    else  // multi input model
    {
        if (_execOption.inputStyle.find("single") != std::string::npos)
        {
            _isValid = false;
            return;
        }
        else
        {
            _inputCount = _ie.GetInputTensorCount();
        }
    }
}

SyncExecutor::SyncExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath) 
    : BaseExecutor(ie, testCase, execOption, inputBuffer, version, inputPath) 
{
    if (_execOption.outputBuffer == "user")
    {
        _outputPtr = new uint8_t[_ie.GetOutputSize()];
    }
    else
    {
        _outputPtr = nullptr;
    }

	_ie.RegisterCallback(nullptr);

}

SyncExecutor::~SyncExecutor() 
{
    // Free output buffer if allocated
    if (_outputPtr != nullptr)
    {
        delete[] static_cast<uint8_t*>(_outputPtr);
        _outputPtr = nullptr;
    }
}

void SyncExecutor::doExecute() 
{
    // Handle Input
    if (_time > 0)
    {
        auto start = std::chrono::high_resolution_clock::now();
        const int TIME_CHECK_INTERVAL = 100;  
        
        for(int iter = 0; ; iter++)
        {
            auto outputs = runInference();

            if (_shouldBitMatch)
            {
                _bm->SetOutput(outputs);
                _bm->BitMatch();
            }
            
            if (iter % TIME_CHECK_INTERVAL == 0)
            {
                auto current = std::chrono::high_resolution_clock::now();
                int duration_sec = std::chrono::duration_cast<std::chrono::seconds>(current - start).count();
                if (duration_sec >= _time)
                {
                    break;
                }
            }
            _count++;
        }
    }
    else
    {
        for(int i = 0; i < _loop; ++i)
        {
            auto outputs = runInference();

            if (_shouldBitMatch)
            {
                _bm->SetOutput(outputs);
                _bm->BitMatch();
            }
            _count++;
        }
    }
}

AsyncCallbackExecutor::AsyncCallbackExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath, ThreadSyncData* syncData)
    : BaseExecutor(ie, testCase, execOption, inputBuffer, version, inputPath), _mySyncData(syncData)
{
    if (_execOption.outputBuffer == "user")
    {
        // MEMORY_SAFETY: Use shared_ptr for automatic reference counting
        // This ensures the buffer remains valid even if executor is destroyed while callbacks are running
        _outputBuffer = std::shared_ptr<uint8_t>(
            new uint8_t[_ie.GetOutputSize()],
            std::default_delete<uint8_t[]>()  // Use array deleter
        );
        _outputPtr = _outputBuffer.get();  // Store raw pointer for inference calls
    }
    else
    {
        _outputPtr = nullptr;
        _outputBuffer = nullptr;
    }

    // If _mySyncData is nullptr, this is a multi-IE scenario, so register a per-thread callback.
    if (_mySyncData == nullptr)
    {
        // MEMORY_SAFETY: Capture shared_ptr by value to extend lifetime
        auto outputBuffer = _outputBuffer;  // Copy shared_ptr (increases ref count)
        
        _ie.RegisterCallback([this, outputBuffer](dxrt::TensorPtrs &outputs, void *userArg) -> int {
            (void)userArg; // Suppress unused parameter warning
            (void)outputBuffer; // Keep buffer alive during callback (suppresses unused variable warning)

            if (_shouldBitMatch)
            {
                _bm->SetOutput(outputs);
                _bm->BitMatch();
            }

            {
                std::lock_guard<std::mutex> lock(_individualCbMutex);
                _individualCallbackCount++;
                if (_individualRunCount == _individualCallbackCount)
                {
                    _individualCbCv.notify_one();
                }
            }
            return 0;
        });
    }
}
AsyncCallbackExecutor::~AsyncCallbackExecutor() 
{
    // MEMORY_SAFETY: shared_ptr will automatically clean up when last reference is gone
    // No manual deletion needed - callback may still hold a reference
    
    // Just clear the raw pointer (doesn't affect shared_ptr refcount)
    _outputPtr = nullptr;
    
    // Reset callback counters for safety
    _individualCallbackCount = 0;
    _individualRunCount = 0;
}
void AsyncCallbackExecutor::doExecute() 
{
    bool isSharedIE = (_mySyncData != nullptr);

    if (_time > 0)
    {
        auto start = std::chrono::high_resolution_clock::now();
        const int TIME_CHECK_INTERVAL = 100; 
        
        for(int iter = 0; ; iter++)
        {
            if (isSharedIE)
            {
                {
                    std::lock_guard<std::mutex> lock(_mySyncData->mutex);
                    _mySyncData->runCount++;
                }
                runInferenceAsync(_mySyncData);
            }
            else
            {
                {
                    std::lock_guard<std::mutex> lock(_individualCbMutex);
                    _individualRunCount++;
                }
                runInferenceAsync(nullptr);
            }

            if (iter % TIME_CHECK_INTERVAL == 0)
            {
                auto current = std::chrono::high_resolution_clock::now();
                int duration_sec = std::chrono::duration_cast<std::chrono::seconds>(current - start).count();
                if (duration_sec >= _time)
                {
                    break;
                }
            }
            _count++;
        }
    }

    // Loop-based execution
    else
    {
        for(int i = 0; i < _loop; ++i)
        {
            if (isSharedIE)
            {
                {
                    std::lock_guard<std::mutex> lock(_mySyncData->mutex);
                    _mySyncData->runCount++;
                }
                runInferenceAsync(_mySyncData);
            }
            else
            {
                {
                    std::lock_guard<std::mutex> lock(_individualCbMutex);
                    _individualRunCount++;
                }
                runInferenceAsync(nullptr);
            }
            _count++;
        }
    }

    if (isSharedIE)
    {
        // Wait for all callbacks for this thread to complete
        std::unique_lock<std::mutex> lock(_mySyncData->mutex);
        auto timeout = std::chrono::seconds(_execOption.callbackDelay);
        if (!_mySyncData->cv.wait_for(lock, timeout, [this](){return _mySyncData->callbackCount == _mySyncData->runCount;})) {
            cout << "Timeout waiting for shared IE callbacks to complete" << endl;
        }
    }
    else // Multi IE mode (individual callback)
    {
        // Wait for all callbacks for this individual IE to complete
        std::unique_lock<std::mutex> lock(_individualCbMutex);
        auto timeout = std::chrono::seconds(_execOption.callbackDelay);
        if (!_individualCbCv.wait_for(lock, timeout, [this](){return _individualCallbackCount == _individualRunCount;})) {
            cout << "Timeout waiting for individual IE callbacks to complete" << endl;
        }
    }
}

AsyncWaitExecutor::AsyncWaitExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath) 
    : BaseExecutor(ie, testCase, execOption, inputBuffer, version, inputPath), _idQueue(1000)
{
    if (_execOption.outputBuffer == "user")
    {
        // MEMORY_SAFETY: Use shared_ptr for automatic reference counting
        _outputBuffer = std::shared_ptr<uint8_t>(
            new uint8_t[_ie.GetOutputSize()],
            std::default_delete<uint8_t[]>()
        );
        _outputPtr = _outputBuffer.get();
    }
    else
    {
        _outputPtr = nullptr;
        _outputBuffer = nullptr;
    }
    _ie.RegisterCallback(nullptr);
}

AsyncWaitExecutor::~AsyncWaitExecutor() 
{
    // Signal consumer thread to stop and wait for it to finish
    _producerDone = true;
    if (_consumerThread.joinable())
    {
        _consumerThread.join();
    }
    
    // MEMORY_SAFETY: shared_ptr will automatically clean up
    _outputPtr = nullptr;
    
    // Clear any remaining items in queue
    while (!_idQueue.empty())
    {
        _idQueue.pop();
    }
}

void AsyncWaitExecutor::consumerWorker()
{
    try {
        while (!_producerDone || !_idQueue.empty())
        {
            if (!_idQueue.empty())
            {
                int id = _idQueue.pop();
                auto output = _ie.Wait(id);
                if (_shouldBitMatch)
                {
                    _bm->SetOutput(output);
                    _bm->BitMatch();
                }
            }
            else
            {
                // Small delay to prevent busy waiting when queue is empty
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    } catch (const std::exception& e) {
        cout << "Consumer thread error: " << e.what() << endl;
    }
}

void AsyncWaitExecutor::doExecute() 
{
    // Reset producer done flag
    _producerDone = false;
    
    // Start consumer thread
    _consumerThread = std::thread(&AsyncWaitExecutor::consumerWorker, this);

    // Producer logic
    if (_time > 0)
    {
        auto start = std::chrono::high_resolution_clock::now();
        const int TIME_CHECK_INTERVAL = 100;
        
        for(int iter = 0; ; iter++)
        {
            _idQueue.push(runInferenceAsync());
            if (iter % TIME_CHECK_INTERVAL == 0)
            {
                auto current = std::chrono::high_resolution_clock::now();
                int duration_sec = std::chrono::duration_cast<std::chrono::seconds>(current - start).count();
                if (duration_sec >= _time)
                {
                    break;
                }
            }
            _count++;
        }
    }
    else
    {
        for(int i = 0; i < _loop; ++i)
        {
            _idQueue.push(runInferenceAsync());
            _count++;
        }
    }
    
    // Signal producer is done
    _producerDone = true;
    
    // Wait for consumer to finish processing all items
    if (_consumerThread.joinable())
    {
        _consumerThread.join();
    }
    
    // Verify all jobs are completed
    if(!_idQueue.empty())
    {
        cout << "Error: Some jobs are not completed" << endl;
        std::exit(-1);
    }
}

// BatchExecutor implementation
BatchExecutor::BatchExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath) 
    : BaseExecutor(ie, testCase, execOption, inputBuffer, version, inputPath)
{
    // Create input buffers vector - all pointing to the same original data
    _inputBuffers.assign(_batchSize, _inputBuffer); 

    // Create output buffers
    if (execOption.outputBuffer == "user")
    {
        _outputBuffers.resize(_batchSize, nullptr);
        for(auto& ptr : _outputBuffers)
        {
            ptr = new uint8_t[ie.GetOutputSize()];
        }
    }
    else
    {
        // Engine will allocate output buffers automatically
        _outputBuffers.clear();
    }

	_ie.RegisterCallback(nullptr);

}

BatchExecutor::~BatchExecutor() 
{
    
    // Free allocated output buffers
    if (!_outputBuffers.empty())
    {
        for(auto& ptr : _outputBuffers)
        {
            if (ptr != nullptr)
            {
                delete[] static_cast<uint8_t*>(ptr);
                ptr = nullptr;
            }
        }
        _outputBuffers.clear();
    }
    
    // Clear input buffers vector (these are just pointers, no need to delete)
    _inputBuffers.clear();
    
}

void BatchExecutor::doExecute() 
{
    if (_time > 0)
    {
        auto start = std::chrono::high_resolution_clock::now();
        const int TIME_CHECK_INTERVAL = 100; 
        
        for(;;)
        {
            auto batch_outputs = _ie.Run(_inputBuffers, _outputBuffers);
            for (auto& output : batch_outputs)
            {
                if (_shouldBitMatch)
                {
                    _bm->SetOutput(output);
                    _bm->BitMatch();
                }

            }
            
            if (_count % TIME_CHECK_INTERVAL == 0)
            {
                auto current = std::chrono::high_resolution_clock::now();
                int duration_sec = std::chrono::duration_cast<std::chrono::seconds>(current - start).count();
                if (duration_sec >= _time)
                {
                    break;
                }
            }
            _count++;
        }
    }
    else
    {
        for(int i = 0; i < _loop; ++i)
        {
            auto batch_outputs = _ie.Run(_inputBuffers, _outputBuffers);
            for (auto& output : batch_outputs)
            {
                if (_shouldBitMatch)
                {
                    _bm->SetOutput(output);
                    _bm->BitMatch();
                }
            }
            _count++;
        }
    }
}

// Function to create test executor based on execution option
std::unique_ptr<BaseExecutor> CreateExecutor(dxrt::InferenceEngine& ie, const TestCase& testCase, const ExecutionOption& execOption, void* inputBuffer, int version, string inputPath, ThreadSyncData* syncData)
{
    if (execOption.inferenceFunction == "sync")
    {
        return std::make_unique<SyncExecutor>(ie, testCase, execOption, inputBuffer, version, inputPath);
    }
    else if (execOption.inferenceFunction == "async")
    {
        if (execOption.asyncMethod == "callback")
        {
            return std::make_unique<AsyncCallbackExecutor>(ie, testCase, execOption, inputBuffer, version, inputPath, syncData);
        }
        else if (execOption.asyncMethod == "wait")
        {
            return std::make_unique<AsyncWaitExecutor>(ie, testCase, execOption, inputBuffer, version, inputPath);
        }
        else
        {
            cout << "Error: Unknown async method: " << execOption.asyncMethod << endl;
            std::exit(-1);
        }
    }
    else if (execOption.inferenceFunction == "batch")
    {
        return std::make_unique<BatchExecutor>(ie, testCase, execOption, inputBuffer, version, inputPath);
    }
    else
    {
        cout << "Error: Unknown inference function: " << execOption.inferenceFunction << endl;
        std::exit(-1);
    }
}
