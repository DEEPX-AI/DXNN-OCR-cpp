#include <iostream>
#include <chrono>
#include <memory>
#include <thread>
#include <algorithm>

#include "../include/executorManager.h"
#include "../include/executor.h"
#include "../include/utils.h"
#include "../include/generator.h"
#include "../include/concurrent_queue.h"
#include "../include/input_utils.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

// ExecutorManager Implementation
ExecutorManager::ExecutorManager(const TestCase& testCase, const ExecutionOption& execOption)
    : _testCase(testCase), _execOption(execOption)
{
    _isGlobalCallbackRegistered = false;
}

ExecutorManager::~ExecutorManager()
{
    // Ensure all threads are joined before cleanup
    if (!_threads.empty())
    {
        for (auto& thread : _threads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        _threads.clear();
    }
    
    // THREAD_SAFETY: Clear shared resources with lock protection
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        // Clear all executors (will call their destructors)
        if (!_executors.empty())
        {
            _executors.clear();
        }
        
        // Clear synchronization data
        if (!_allThreadSyncData.empty())
        {
            _allThreadSyncData.clear();
        }
    }
    
    // Reset shared InferenceEngine
    if (_sharedIE)
    {
        _sharedIE.reset();
    }
    
    // Clear bit match results
    _bitMatchResults.clear();
}

RunResult ExecutorManager::Run()
{
    // Initialize result vector
    _bitMatchResults.clear();
    _bitMatchResults.resize(_testCase.ieOption.threadCount, TC_SKIP);
    
    if (_testCase.ieOption.threadType == "single-ie")
    {
        executeWithSharedIE();
    }
    else if (_testCase.ieOption.threadType == "multi-ie")
    {
        executeWithMultiIE();
    }
    else
    {
        cout << "Error: Unknown thread type: " << _testCase.ieOption.threadType << endl;
        return TC_INVALID;
    }
    
    // Wait for all threads to complete
    for (auto& thread : _threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    
    // Find the maximum (worst) result among all threads
    return GetResult();
}

void ExecutorManager::executeWithSharedIE()
{
    // Create shared InferenceEngine
    createSharedInferenceEngine();

    if (!_sharedIE)
    {
        cout << "Error: Failed to create shared InferenceEngine" << endl;
        return;
    }
    
    // THREAD_SAFETY: Initialize shared resources with lock protection
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        // Initialize thread synchronization data
        _allThreadSyncData.clear(); // Clear existing elements
        _executors.clear(); // Clear existing executors
        _executors.resize(_testCase.ieOption.threadCount); // Pre-allocate executor slots
        
        for (int i = 0; i < _testCase.ieOption.threadCount; ++i)
        {
            auto new_sync_data = std::make_unique<ThreadSyncData>();
            new_sync_data->threadId = i; // Set thread ID
            _allThreadSyncData.push_back(std::move(new_sync_data));
        }
    }

    { 
        std::lock_guard<std::mutex> lock(_globalCallbackRegistrationMutex);
        if (_execOption.inferenceFunction == "async" && !_isGlobalCallbackRegistered && _execOption.asyncMethod == "callback")
        {
            _sharedIE->RegisterCallback([this](dxrt::TensorPtrs &outputs, void *userArg) -> int 
                {
                    if (userArg == nullptr) 
                    {
                        cout << "Error: userArg is null in global callback!" << endl;
                        std::exit(-1);
                    }

                    // Cast userArg back to ThreadSyncData*
                    ThreadSyncData* sync_data = static_cast<ThreadSyncData*>(userArg);
                    int thread_id = sync_data->threadId;
                    
                    // THREAD_SAFETY: Lock when accessing shared _executors vector
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
                        
                        // Now we can access the specific executor for this thread
                        if (thread_id >= 0 && thread_id < static_cast<int>(_executors.size()) && _executors[thread_id])
                        {
                            if (_execOption.bitmatch && _sharedIE->GetCompileType() != "debug")
                            {
                                // Use the executor - for example, for bitmatch processing
                                _executors[thread_id]->SetOutput(outputs);
                                _executors[thread_id]->BitMatch();
                            }
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(sync_data->mutex);
                        sync_data->callbackCount++;
                        if (sync_data->runCount == sync_data->callbackCount)
                        {
                            sync_data->cv.notify_one(); // Notify the specific thread
                        }
                    }
                    return 0;
                }
            );
            _isGlobalCallbackRegistered = true;
        }
    }
    
    // Create threads that share the same IE
    for (int t = 0; t < _testCase.ieOption.threadCount; t++)
    {
        _threads.emplace_back([this, t]() {
            try 
            {
                // Create input buffer for this thread using InputUtils
                InputUtils iu(_execOption, _testCase, *_sharedIE, t);
                iu.CreateInputBuffer();
                std::vector<uint8_t> thread_input_buffer = iu.GetInputBuffer();
                int version = iu.GetVersion();
                string input_path = iu.GetFilePath();

                // THREAD_SAFETY: Create executor and store with lock protection
                // This prevents callback from accessing _executors[t] while it's being assigned
                {
                    std::lock_guard<std::mutex> lock(_mutex);
                    _executors[t] = CreateExecutor(*_sharedIE, _testCase, _execOption, thread_input_buffer.data(), version, input_path, _allThreadSyncData[t].get());
                }
                
                auto result = _executors[t]->Execute();

                gatherResults(result, t);
            }
            catch (const std::exception& e) 
            {
                cout << "Thread " << t << " failed: " << e.what() << endl;
            }
        });
    }
}

void ExecutorManager::executeWithMultiIE()
{
    // Create threads that each have their own IE
    for (int t = 0; t < _testCase.ieOption.threadCount; t++)
    {
        _threads.emplace_back([this, t]() {
            try 
            {
                dxrt::InferenceOption op;
                SetInferenceConfigurationFromIEOption(op, _testCase.ieOption);
                auto ie = std::make_unique<dxrt::InferenceEngine>(_testCase.ieOption.model_path, op);
                
                // Create input buffer for this thread
                InputUtils iu(_execOption, _testCase, *ie, t);
                iu.CreateInputBuffer();
                std::vector<uint8_t> thread_input_buffer = iu.GetInputBuffer();
                int version = iu.GetVersion();
                string input_path = iu.GetFilePath();
                
                auto executor = CreateExecutor(*ie, _testCase, _execOption, thread_input_buffer.data(), version, input_path);
                auto result = executor->Execute();

                gatherResults(result, t);
            } 
            catch (const std::exception& e) 
            {
                cout << "Thread " << t << " failed: " << e.what() << endl;
            }
        });
    }
}

void ExecutorManager::gatherResults(BitMatchResult result, int threadIndex)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (result.modelRun)
    {
        if (result.bitMatchRun)
        {
            if (!result.isFail)
            {
                _bitMatchResults[threadIndex] = BM_PASS;
                return;
            }
            else
            {
                _bitMatchResults[threadIndex] = BM_FAIL;
                return;
            }
        }
        else
        {
            _bitMatchResults[threadIndex] = BM_SKIP;
            return;
        }
    }
    else
    {
        _bitMatchResults[threadIndex] = TC_SKIP;
        return;
    }
}

RunResult ExecutorManager::GetResult()
{
    if (_bitMatchResults.empty())
    {
        return TC_INVALID;  // No results available
    }
    
    // Find maximum value (worst result: TC_FAIL=2 > TC_SKIP=1 > TC_PASS=0)
    int maxResult = *std::max_element(_bitMatchResults.begin(), _bitMatchResults.end());
    return static_cast<RunResult>(maxResult);
}

void ExecutorManager::createSharedInferenceEngine()
{
    try 
    {
        dxrt::InferenceOption op;
        SetInferenceConfigurationFromIEOption(op, _testCase.ieOption);
        _sharedIE = std::make_unique<dxrt::InferenceEngine>(_testCase.ieOption.model_path, op);
    } 
    catch (const std::exception& e) 
    {
        cout << "Error creating shared InferenceEngine: " << e.what() << endl;
        _sharedIE.reset();
    }
}

// Factory function to create ExecutorManager
std::unique_ptr<ExecutorManager> CreateExecutorManager(const TestCase& testCase, 
                                                      const ExecutionOption& execOption)
{
    return std::make_unique<ExecutorManager>(testCase, execOption);
}
