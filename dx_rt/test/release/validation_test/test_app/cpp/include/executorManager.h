#pragma once

#include <memory>
#include <vector>

#include "dxrt/dxrt_api.h"
#include "concurrent_queue.h"
#include "generator.h"
#include "executor.h"

enum RunResult {
    BM_PASS = 0, // Assume that TC is actually run
    BM_SKIP = 1, // Assume that TC is actually run
    TC_SKIP = 2, 
    BM_FAIL = 3, // Assume that TC is actually run
    TC_INVALID = 4  // It doesn't mean critical errors like seg fault.
                 // It only means the test case run is failed due to invalid option.
};

// ExecutorManager - manages threads and InferenceEngine instances
class ExecutorManager
{
public:
    ExecutorManager(const TestCase& testCase, const ExecutionOption& execOption);
    ~ExecutorManager();
    
    // Execute all threads and wait for completion
    RunResult Run();
    RunResult GetResult();
    
private:
    // Test case and execution options
    const TestCase& _testCase;
    const ExecutionOption& _execOption;
    std::unique_ptr<dxrt::InferenceEngine> _sharedIE; // Only for single-ie mode
    
    // Thread management
    std::vector<std::thread> _threads;
    std::vector<int> _bitMatchResults;

    // Only for single-ie mode
    std::vector<std::unique_ptr<BaseExecutor>> _executors;
    std::vector<std::unique_ptr<ThreadSyncData>> _allThreadSyncData; // Per-thread synchronization data managed by ExecutorManager
    
    // Global callback registration (ensures only one callback is ever registered)
    bool _isGlobalCallbackRegistered;
    std::mutex _globalCallbackRegistrationMutex;
    
    // Mutex for thread-safe operations
    // Protects: _bitMatchResults, _executors, _allThreadSyncData
    std::mutex _mutex;

    // Helper methods
    void gatherResults(BitMatchResult result, int threadIndex); // gather all results from threads, resulsts are accumulated in _bitMatchResults
    void executeWithSharedIE();
    void executeWithMultiIE();
    void createSharedInferenceEngine();
};

std::unique_ptr<ExecutorManager> CreateExecutorManager(const TestCase& testCase, const ExecutionOption& execOption);