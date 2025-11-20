
#ifdef __linux__	// all or dummy

#include "dxrt/dxrt_api.h"
#include "dxrt/tensor.h"
#include "dxrt/common.h"
#include "concurrent_queue.h"
#include <time.h>
#include <future>

#ifdef __linux__
#include <sys/wait.h>
#include <sys/resource.h>
#endif
bool USE_ORT_OPT = false;

struct ThreadData
{
    dxrt::InferenceEngine *inferenceEnginePtr;
    std::vector<int> passCountArray;
    int testCount;
    int loopCount;
    std::string modelFolderPath;
    std::mutex mutexLock;
};

// reuse binary loaded data
// key: file path, value: size and buffer pointer
static std::map<std::string, std::pair<std::vector<uint8_t>, int32_t>> gBinaryDataMap;
static long gMemoryUsageStart = 0L;
static long gMemoryUsageEnd = 0L;

static long getMemoryUsage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    //std::cout << "Memory usage: " << usage.ru_maxrss << " KB" << std::endl;

    return usage.ru_maxrss;
}

// find binary data 
static std::vector<uint8_t> findBinaryData(int32_t &size, std::string path)
{
    auto it = gBinaryDataMap.find(path);
    if ( it != gBinaryDataMap.end() )  // found binary data
    {
        LOG_DXRT << "[TEST] Found Binary Ptr path=" << path << std::endl;
        size = it->second.second;
        return it->second.first;
    }

    return {};
}

// add binary data
static void addBinaryData(std::string path, std::vector<uint8_t> binPtr, int32_t size)
{
    auto it = gBinaryDataMap.find(path);
    if ( it == gBinaryDataMap.end() )
    {
        gBinaryDataMap.emplace(path, std::pair<std::vector<uint8_t>, int32_t>(binPtr, size));
        LOG_DXRT << "[TEST] Add Binary Ptr path=" << path << std::endl;
    }
}

static std::vector<uint8_t> readDumpFile(int32_t &outSize, const std::string& path)
{
    std::vector<uint8_t> binPtr = findBinaryData(outSize, path);

    // no binary data, load binary data
    if ( binPtr.empty() )
    {

        FILE *stream = fopen(path.c_str(), "rb");
        if ( stream != NULL ) {

            fseek(stream, 0L, SEEK_END);
            auto size = ftell(stream);

            // create buffer to read dump file
            std::vector<uint8_t> dump(size); 

            fseek(stream, 0L, SEEK_SET);
            outSize = fread(dump.data(), 1, size, stream);
            fclose(stream);

            LOG_DXRT << "[TEST] Read File: " << path << " size: " << size << std::endl;

            // to reuse the buffer
            addBinaryData(path, dump, size);

            return dump;

        } // stream
        else {
            outSize = 0;
            LOG_DXRT_ERR("[TEST] File not found: " << path);
        }
    }
    else // found binary data, return the pointer
    {
        return binPtr;
    }

    return {};
}

// compare result & expect-result
static bool compareOutputs(const std::string& expectOutputDumpFilePath, uint8_t* rawData, int32_t size)
{
    int32_t dumpSize = 0;
    std::vector<uint8_t> expectOutputResult = readDumpFile(dumpSize, expectOutputDumpFilePath);

    if ( dumpSize != size )
    {
        //return memcmp(rawData, expectOutputResult.get(), dumpSize) == 0 ? true : false;
        LOG_DXRT << "[WARN] Warning: Mismatch gt and rt file size (gt=" << dumpSize << ", rt=" << size << ")" << std::endl;
    }

    if ( size >= dumpSize ) 
    {
        return std::memcmp(rawData, expectOutputResult.data(), dumpSize) == 0 ? true : false;
    }
    
    return false;
}

static int calcuOutputRawDataSize(dxrt::TensorPtrs& outputs)
{
    
    int totalSize = 0;
    for(auto &tensor:outputs)
    {
        auto shape_count = dxrt::vectorProduct(tensor->shape());
        auto elem_size = tensor->elem_size();
        int size = shape_count * elem_size;
        totalSize += size;
        LOG_DXRT << "[TEST] Tensor shape_count=" << shape_count << " elem_size=" << elem_size 
            << " size=" << size << std::endl;
    }

    LOG_DXRT << "[TEST] Tensors total-size=" << totalSize << std::endl;

    return totalSize;

}

static bool isArgMaxOutput(dxrt::TensorPtrs& outputs)
{
    LOG_DXRT << "[TEST] Tensor name=" << outputs.front()->name() << std::endl;
    return outputs.front()->name() == "argmax_output";
}

static std::string getFolderPath(std::string& fileFullPath)
{
    size_t pos = fileFullPath.find_last_of("/\\");
    
    // folder path
    std::string folderPath = (pos == std::string::npos) ? "" : fileFullPath.substr(0, pos);
    folderPath += "/";
   
    return folderPath;
}

static int bitMatchSyncThread(int threadIndex, ThreadData& threadData)
{
    int Success_count = 0;
    //double total_latency_time = 0.0;

    auto output_size = threadData.inferenceEnginePtr->GetOutputSize();
    //uint8_t *output_buffer = new uint8_t[output_size];
    std::vector<uint8_t> output_buffer(output_size, 0);

    for(int t = 0; t < threadData.testCount; ++t)
    {
        
        for(int i = 0; i < threadData.loopCount; ++i) 
        {
            LOG_DXRT << "[TEST][" << t << "][" << i << "] Begin inference: pid=" << getpid() << " thread-index=" << threadIndex << std::endl;

            std::string inputPath = threadData.modelFolderPath + "gt/";
            char inputFilename[255];
            if(USE_ORT_OPT)
            {
                sprintf(inputFilename, "input_%d.bin", i);
            }
            else
            {
                sprintf(inputFilename, "npu_0_input_%d.bin", i);
            }
            inputPath += inputFilename; //"npu_0_input_0.bin";
            LOG_DXRT << "[TEST] Input Tensor Path: " << inputPath << std::endl;
            int32_t dumpSize = 0;
            std::vector<uint8_t> inputPtr = readDumpFile(dumpSize, inputPath);
            if ( !inputPtr.empty() ) 
            {
                //std::lock_guard<std::mutex> lock(threadData.mutexLock);
                try {
                    
                    // input and inference
                    #if 0 // memory leak test
                    auto start = std::chrono::high_resolution_clock::now();
                    //auto outputs = threadData.inferenceEnginePtr->Run(inputPtr.get());
                    usleep(10000);
                    auto end = std::chrono::high_resolution_clock::now();

                    bool compareResult = true;

                    #else
                    auto start = std::chrono::high_resolution_clock::now();
                    auto outputs = threadData.inferenceEnginePtr->Run(inputPtr.data(), nullptr, output_buffer.data());
                    auto end = std::chrono::high_resolution_clock::now();

                    if(outputs.size() == 0){
                        LOG_DXRT_ERR("[ERROR] Output size is 0");
                        continue;
                    }
                    

                    std::string outputPath = threadData.modelFolderPath + "gt/";
                    char outputFilename[255];
                    if ( isArgMaxOutput(outputs) ) // check argmax or ppu
                    {
                        sprintf(outputFilename, "npu_0_output_%d.argmax.bin", i);
                    }
                    else {
                        sprintf(outputFilename, "npu_0_output_%d.ppu.bin", i);
                    }

                    outputPath += outputFilename; //"npu_0_output_0.ppu.bin";
                    LOG_DXRT << "[TEST] Output Tensor Path: " << outputPath << std::endl;
                    
                    //auto resultIndex = postProcessing(outputs);
                    int outputSize = calcuOutputRawDataSize(outputs);
                    bool compareResult = false;
                    compareResult = compareOutputs(outputPath, reinterpret_cast<uint8_t*>(outputs.front()->data()), outputSize);
                    #endif

                    LOG_DXRT << "[TEST] Result=" << (compareResult ? "Success" : "Failure") << ", pid=" << getpid() << ", thread-index=" << threadIndex << std::endl;

                    std::chrono::duration<double, std::milli> duration = end - start;
                    auto resultTime = duration.count();
                    LOG_DXRT << "[TEST] Latency Time: " << resultTime << "ms" << std::endl;

                    if ( compareResult ) {
                        Success_count++;
                        //total_latency_time += resultTime;
                    }

                } 
                catch(const dxrt::Exception& e)
                {
                    LOG_DXRT_ERR(e.what() << " error-code=" << e.code());
                    return -1;
                }
                catch(const std::exception& e)
                {
                    LOG_DXRT_ERR(e.what());
                    return -1;
                }
                catch(...)
                {
                    LOG_DXRT_ERR("Exception...");
                    return -1;
                }

                LOG_DXRT << "[TEST][" << t << "][" << i << "] End inference: pid=" << getpid() << " thread-index=" << threadIndex << std::endl;
                LOG_DXRT << std::endl;
            }
            else 
            {
                LOG_DXRT_ERR("[ERROR][" << t << "][" << i << "] Input size is 0");
            }

        } // for i

        // check memory leak, no thread only
        if ( t == 50 && threadIndex == -1 ) gMemoryUsageStart = getMemoryUsage();

    } // for t

    if ( threadIndex >= 0 )
        threadData.passCountArray.at(threadIndex) = Success_count;

    // check memory leak, no thread only
    if ( threadIndex == -1 ) gMemoryUsageEnd = getMemoryUsage();

    return Success_count;
}

static int bitMatchSync(std::string modelPath, std::string modelFolderPath, int testCount, int threadCount, int boundOption)
{

    const int LOOP_COUNT = 5; // for bit-match data
    int Success_count = 0;
    double total_latency_time = 0.0;
    
    LOG_DXRT << "[TEST] Started Process (Sync) pid=" << getpid() << " thread-count=" << threadCount << std::endl;

    // load ai model
    try {

        // bound option
        dxrt::InferenceOption op;
        op.boundOption = boundOption;

        // load model
        dxrt::InferenceEngine inferenceEngine(modelPath, op);

        ThreadData threadData;
        threadData.inferenceEnginePtr = &inferenceEngine;
        threadData.loopCount = LOOP_COUNT;
        threadData.testCount = testCount;
        threadData.modelFolderPath = modelFolderPath;

        if ( threadCount > 0 )
        {
            threadData.passCountArray.assign(threadCount, 0);

            std::vector<std::thread> threads;

            for(int i = 0; i < threadCount; ++i)
            {
                //threads.push_back(std::thread(bitMatchSyncThread, i, testCount, LOOP_COUNT, std::ref(modelFolderPath), std::ref(inferenceEngine)));
                threads.push_back(std::thread(bitMatchSyncThread, i, std::ref(threadData)));
            }

            for(auto& t : threads)
            {
                t.join();
            }

            if ( threadCount > 0 )
            {
                int total_Success_count = 0;
                for(auto& c : threadData.passCountArray)
                {
                    total_Success_count += c;
                }

                Success_count = total_Success_count / threadCount;
            }
        }
        else
        {
            Success_count = bitMatchSyncThread(-1, threadData);
        }

    }
    catch(dxrt::Exception &e) 
    {
        LOG_DXRT_ERR("[ERROR] " << e.what());
        return -1;
    }
    catch(std::exception &e) 
    {
        LOG_DXRT_ERR("[ERROR] " << e.what());
        return -1;
    }
    catch(...)
    {
        LOG_DXRT_ERR("Exception...");
        return -1;
    }

    

    int total_test_count = testCount * LOOP_COUNT;
    LOG_DXRT << "[TEST] Total Result=" << (Success_count == total_test_count ? "Success" : "Failure") 
                << "(" << Success_count << "/" << total_test_count << "), pid=" << getpid() << std::endl;
    if ( Success_count > 0 )
    {
        LOG_DXRT << "[TEST] Total Average Latency Time=" << total_latency_time / static_cast<double>(Success_count) << "ms" << std::endl;
    }

    return Success_count == total_test_count ? 0 : -1;

}

// User Data for RunAsync
struct BitmatchAsyncData
{
    std::chrono::_V2::system_clock::time_point startTime;
    int bitmatchIndex;
    int sequenceNumber; 
    int jobId;
    int testIndex;
    int threadIndex;
    std::vector<uint8_t> inputBuffer;
    ThreadData *threadDataPtr;

    BitmatchAsyncData(size_t bufferSize)
    {
        // create input buffer 
        inputBuffer = std::vector<uint8_t>(bufferSize);

        Reset();
    }

    void Reset()
    {
        bitmatchIndex = -1;
        sequenceNumber = -1;
        jobId = -1;
        testIndex = -1;
        threadIndex = -1;
    }
};

// usrArg buffer pool
class BitmatchAsyncDataPool
{
    //const int POOL_SIZE = 100;
    int _poolSize = 0;
    size_t _headIndex = 0;
    std::mutex _mutexLock;
    std::vector<std::shared_ptr<BitmatchAsyncData>>  _dataPool;
public:
    BitmatchAsyncDataPool(int poolSize, size_t bufferSize)
    {
        _headIndex = 0;
        _poolSize = poolSize;
        for(int i = 0; i < _poolSize; ++i)
        {
            _dataPool.emplace_back(std::make_shared<BitmatchAsyncData>(bufferSize));
        }
       
    }

    // reuse buffer pointer
    BitmatchAsyncData* GetPointer()
    {
        std::lock_guard<std::mutex> guard(_mutexLock);

        size_t curIndex = _headIndex;
        _headIndex++;
        if ( _headIndex == _dataPool.size() ) _headIndex = 0;

        return _dataPool.at(curIndex).get();
    }
};

struct BitmatchAsyncResult
{
    std::atomic<int> successCount;
    std::atomic<int> testCount;
    int totalTestCount;
    double totalLatencyTime;
    std::string modelFolderPath;
    std::mutex mutex;
    std::shared_ptr<ConcurrentQueue<int>> cqueue;
    std::shared_ptr<BitmatchAsyncDataPool> dataPool;
    std::vector<int> failIndices;
};

static BitmatchAsyncResult gBitmatchAsyncResult;
static std::mutex gAsyncCBMutex;

static int bitMatchAsyncCB(dxrt::TensorPtrs &outputs, void *userArg)
{
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::lock_guard<std::mutex> guard(gAsyncCBMutex);

    //std::lock_guard<std::mutex> guard(gBitmatchAsyncResult.mutex);

    auto end = std::chrono::high_resolution_clock::now();

    BitmatchAsyncData *userData = reinterpret_cast<BitmatchAsyncData*>(userArg);

    LOG_DXRT << "[TEST][CB] bitMatchAsyncCB" << " thread-index=" << userData->threadIndex << std::endl;

    if(outputs.size() == 0){
        LOG_DXRT_ERR("[ERROR][CB] Output size is 0");
        return -1;
    }

    
    LOG_DXRT << "[TEST][CB][" << userData->testIndex << "][" << userData->bitmatchIndex 
        << "] RunAsyncCB (Start) seq=" << userData->sequenceNumber 
        << " pid=" << getpid() << " thread-index=" << userData->threadIndex
        << std::endl;


    std::string outputPath = gBitmatchAsyncResult.modelFolderPath + "gt/";
    char outputFilename[255];
    if ( isArgMaxOutput(outputs) ) // check argmax or ppu
    {
        sprintf(outputFilename, "npu_0_output_%d.argmax.bin", userData->bitmatchIndex);
    }
    else {
        sprintf(outputFilename, "npu_0_output_%d.ppu.bin", userData->bitmatchIndex);
    }
    
    outputPath += outputFilename; //"npu_0_output_0.ppu.bin";
    LOG_DXRT << "[TEST][CB] Output Tensor Path: " << outputPath << " thread-index=" << userData->threadIndex << std::endl;
    
    int outputSize = calcuOutputRawDataSize(outputs);
    bool compareResult = false;
    compareResult = compareOutputs(outputPath, reinterpret_cast<uint8_t*>(outputs.front()->data()), outputSize);
    LOG_DXRT << "[TEST][CB] Result=" << (compareResult ? "Success" : "Failure") << ", pid=" << getpid() << ", thread-index=" << userData->threadIndex << std::endl;

    
    std::chrono::duration<double, std::milli> duration = end - userData->startTime;
    auto resultTime = duration.count();
    LOG_DXRT << "[TEST][CB] Latency Time: " << resultTime << "ms" << " thread-index=" << userData->threadIndex << std::endl;

    if ( compareResult ) {
        gBitmatchAsyncResult.successCount++;
        gBitmatchAsyncResult.totalLatencyTime += resultTime;
    }
    else {
        gBitmatchAsyncResult.failIndices.push_back(userData->testIndex);
    }

    gBitmatchAsyncResult.testCount ++;

    // check input data 
    
    //if ( userData->inputPtr != nullptr)
    if ( !userData->inputBuffer.empty() )
    {
        LOG_DXRT << "[TEST][CB] "
            << "Input data seq=" << userData->sequenceNumber << " thread-index=" << userData->threadIndex << std::endl;
    }
    else 
    {
        LOG_DXRT_ERR("[TEST][CB] " 
            << "Input data seq=" << userData->sequenceNumber << ", ptr is null" << " thread-index=" << userData->threadIndex);
    }

    LOG_DXRT << "[TEST][CB][" << userData->testIndex << "][" << userData->bitmatchIndex << "]"
        << " RunAsyncCB (End) seq=" << userData->sequenceNumber 
        << " pid=" << getpid() << " thread-index=" << userData->threadIndex
        << std::endl;
    

    if ( gBitmatchAsyncResult.testCount.load() == (gBitmatchAsyncResult.totalTestCount))
    {
        int success_count = gBitmatchAsyncResult.successCount.load();
        int total_test_count = gBitmatchAsyncResult.totalTestCount;
        int total_latency_time = gBitmatchAsyncResult.totalLatencyTime;
        LOG_DXRT << "[TEST][CB] " 
                    << "Total Result=" << (success_count == total_test_count ? "Success" : "Failure") 
                    << "(" << success_count << "/" << total_test_count << "), pid=" << getpid() << std::endl;

        if ( success_count > 0 )
        {
            LOG_DXRT << "[TEST][CB] "
                << "Total Average Latency Time=" << total_latency_time / static_cast<double>(success_count) << "ms" 
                << std::endl;
        }

        // set result
        //gBitmatchAsyncResult.prom.set_value(Success_count == total_test_count ? 0 : -1);
        gBitmatchAsyncResult.cqueue->push(success_count == total_test_count ? 0 : -1);
    }

    userData->Reset();

    return 0;
}

static void bitMatchAsyncThread(int threadIndex, ThreadData& threadData)
{
    std::lock_guard<std::mutex> lock(threadData.mutexLock);    

    LOG_DXRT << "[TEST] thread-index=" << threadIndex << " test-count=" << threadData.testCount 
            << " loop-count=" << threadData.loopCount << std::endl;

    
    for(int t = 0; t < threadData.testCount; ++t)
    {

        for(int i = 0; i < threadData.loopCount; ++i) 
        {
            LOG_DXRT << "[TEST][" << t << "][" << i << "] Begin inference: pid=" << getpid() << " thread-index=" << threadIndex << std::endl;

            std::string inputPath = threadData.modelFolderPath + "gt/";
            char inputFilename[255];
            if(USE_ORT_OPT)
            {
                sprintf(inputFilename, "input_%d.bin", i);
            }
            else
            {
                sprintf(inputFilename, "npu_0_input_%d.bin", i);
            }
            inputPath += inputFilename; //"npu_0_input_0.bin";
            LOG_DXRT << "[TEST] Input Tensor Path: " << inputPath << std::endl;
            int32_t dumpSize = 0;
            std::vector<uint8_t> inputPtr = readDumpFile(dumpSize, inputPath);

            if ( !inputPtr.empty() ) 
            {
                // userData from data pool and it will be reset at Callback
                BitmatchAsyncData* userData = gBitmatchAsyncResult.dataPool->GetPointer();
                
                // input and inference
                userData->bitmatchIndex = i; // for matching with gt
                userData->testIndex = t;
                userData->threadIndex = threadIndex;
                userData->sequenceNumber = t * threadData.loopCount + i;
                userData->startTime = std::chrono::high_resolution_clock::now();
                if ( userData->inputBuffer.size() == static_cast<size_t>(dumpSize) ) {
                    memcpy(userData->inputBuffer.data(), inputPtr.data(), dumpSize);
                }
                else {
                    LOG_DXRT << "[TEST] Mismatch input buffer size and dump size (input=" << userData->inputBuffer.size() 
                        << ", dump=" << dumpSize << ")" << std::endl;
                    std::exit(-1);
                }
                LOG_DXRT << "[TEST] RunAsync seq=" << userData->sequenceNumber << std::endl;
                
                try {
                    
                    auto id = threadData.inferenceEnginePtr->RunAsync(userData->inputBuffer.data(), reinterpret_cast<void*>(userData));
                    userData->jobId = id;

                } 
                catch(dxrt::Exception& e)
                {
                    LOG_DXRT_ERR(e.what());
                    return;
                }
                catch(std::exception& e)
                {
                    LOG_DXRT_ERR(e.what());
                    return;
                }

                LOG_DXRT << "[TEST][" << t << "][" << i << "] End RunAsync: pid=" << getpid() << std::endl;
                LOG_DXRT << std::endl;
            }
            else 
            {
                LOG_DXRT_ERR("[ERROR][" << t << "][" << i << "] Input size is 0");
            }

        } // for i

        // no thread only
        if ( t == 50 && threadIndex == -1 ) gMemoryUsageStart = getMemoryUsage();

    } // for t

    // check memory leak, no thread only
    if ( threadIndex == -1 ) gMemoryUsageEnd = getMemoryUsage();
    
}

static int bitMatchAsync(std::string modelPath, std::string modelFolderPath, int testCount, int threadCount, int boundOption)
{

    const int LOOP_COUNT = 5; // for bit-match data
    int total_test_count = testCount * LOOP_COUNT;
    if ( threadCount > 0 )
    {
        total_test_count *= threadCount;
    }
    
    LOG_DXRT << "[TEST] Started Process (Async) pid=" << getpid() << std::endl;

    gBitmatchAsyncResult.modelFolderPath = modelFolderPath;
    gBitmatchAsyncResult.successCount.store(0);
    gBitmatchAsyncResult.testCount.store(0);
    gBitmatchAsyncResult.totalLatencyTime = 0.0;
    gBitmatchAsyncResult.totalTestCount = total_test_count;
    gBitmatchAsyncResult.cqueue = std::make_shared<ConcurrentQueue<int>>(1);

    int result = -1;

    // load ai model
    try {

        // bound option
        dxrt::InferenceOption op;
        op.boundOption = boundOption;

        // load model
        dxrt::InferenceEngine inferenceEngine(modelPath, op);
        inferenceEngine.RegisterCallback(bitMatchAsyncCB);

        int poolSize = threadCount > 0 ? threadCount * 100 : 100;

        // data pool of input buffer & user data 
        gBitmatchAsyncResult.dataPool = std::make_shared<BitmatchAsyncDataPool>(poolSize, inferenceEngine.GetInputSize());

        ThreadData threadData;
        threadData.inferenceEnginePtr = &inferenceEngine;
        threadData.loopCount = LOOP_COUNT;
        threadData.testCount = testCount;
        threadData.modelFolderPath = modelFolderPath;

        if ( threadCount > 0 )
        {
            threadData.passCountArray.assign(threadCount, 0);

            std::vector<std::thread> threads;

            for(int i = 0; i < threadCount; ++i)
            {
                threads.push_back(std::thread(bitMatchAsyncThread, i, std::ref(threadData)));
            }

            for(auto& t : threads)
            {
                t.join();
            }
        }
        else
        {
            bitMatchAsyncThread(-1, threadData);
        }
       

        // wait for finishing the callback job
        LOG_DXRT << "[TEST] Wait for the result" << std::endl;
        result = gBitmatchAsyncResult.cqueue->pop();

        

    }
    catch(dxrt::Exception& e)
    {
        LOG_DXRT_ERR(e.what());
        return -1;
    }
    catch(std::exception &e)
    {
        LOG_DXRT_ERR(e.what());
        return -1;
    }
    catch(...)
    {
        LOG_DXRT_ERR("Exception...");
        return -1;
    }
    
    return result;

}

// classification function and performance testing
int main(int argc, char* argv[])
{
    const int DEFAULT_PROCESS_COUNT = 1;
    const int DEFAULT_TEST_COUNT = 5;
    const bool DEFAULT_TEST_SYNC = true;
    const int DEFAULT_THREAD_COUNT = 0;
    const int DEFAULT_BOUND_OPTION = 0;

    std::string modelFolderPath;
    std::string modelPath;
    int process_count = DEFAULT_PROCESS_COUNT;
    int test_count = DEFAULT_TEST_COUNT;
    bool test_sync = DEFAULT_TEST_SYNC;
    int thread_count = DEFAULT_THREAD_COUNT;
    int bound_option = DEFAULT_BOUND_OPTION;
    if ( argc > 1 )
    {
        //modelFolderPath = argv[1];
        //modelPath = modelFolderPath + argv[2];
        modelPath = argv[1];
        modelFolderPath = getFolderPath(modelPath);

        if ( argc > 2 )
        {
            process_count = std::stoi(argv[2]);
            LOG_DXRT << "[TEST] process-count=" << process_count << std::endl;
        }

        if ( argc > 3 )
        {
            test_count = std::stoi(argv[3]);
            LOG_DXRT << "[TEST] test-count=" << test_count << std::endl;
        }

        if ( argc > 4 )
        {
            if (strcmp(argv[4], "true") == 0 ) test_sync = true;
            else if (strcmp(argv[4], "false") == 0 ) test_sync = false;
            else if (strcmp(argv[4], "sync") == 0 ) test_sync = true;
            else if (strcmp(argv[4], "async") == 0 ) test_sync = false;
            LOG_DXRT << "[TEST] test-sync=" << (test_sync ? "sync" : "async") << std::endl;
        }

        if ( argc > 5 )
        {
            thread_count = std::stoi(argv[5]);
            LOG_DXRT << "[TEST] thread-count=" << thread_count << std::endl;
        }

        if ( argc > 6 ) 
        {
            // USE_ORT-option: 0:False, 1:True
            if (strcmp(argv[6], "off") == 0 ) {
                USE_ORT_OPT = false;
            } else if (strcmp(argv[6], "on") == 0 ) {
                USE_ORT_OPT = true;
            } else {
                throw std::invalid_argument("USE_ORT_OPT must be on or off");
            }
            LOG_DXRT << "[TEST] USE_ORT_OPT=" << USE_ORT_OPT << std::endl;
        }

        if ( argc > 7 )
        {
            // bound-option: 0-all, 1=npu0, 2=npu1, 3=npu2
            bound_option = std::stoi(argv[7]);
            LOG_DXRT << "[TEST] bound-option=" << bound_option << std::endl;
        }
        

    }
    else
    {
        LOG_DXRT << "[Usage] dxrt_test_bit_match [model-file-path] [process-count] [loop-count] [sync or async] [thread-count] [bound-option] [useORT-option]" << std::endl;
        LOG_DXRT << "        bound-option: 0-all, 1=npu0, 2=npu1, 3=npu2" << std::endl;
        return -1;
    }


    LOG_DXRT << "[TEST] TEST::Bitmatch" << std::endl;
    LOG_DXRT << "[TEST] Model Full Path: " << modelPath << std::endl;
    LOG_DXRT << "[TEST] Model Folder Full Path: " << modelFolderPath << std::endl;

    // clock_t start = clock(); // for performance test
    auto start = std::chrono::high_resolution_clock::now();
    // int ret = bitMatch(modelPath, modelFolderPath, TEST_COUNT);

    

    int success_count = 0;
    for (int i = 0; i < process_count; i++)
    {
        pid_t pid = 0;
        if ( process_count > 1 ) pid = fork(); // if 1 process, no fork()

        if ( pid == 0 )  // child process
        {
            LOG_DXRT << "[TEST] Child process: pid=" << getpid() << " index=" << i << std::endl;
            int ret = -1;

            auto bitmatch_start = std::chrono::high_resolution_clock::now();
            if (test_sync)
            {
                ret = bitMatchSync(modelPath, modelFolderPath, test_count, thread_count, bound_option);
            }
            else
            {
                ret = bitMatchAsync(modelPath, modelFolderPath, test_count, thread_count, bound_option);
            }
            auto bitmatch_end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = bitmatch_end - bitmatch_start;
            
            if ( process_count > 1 ) {
                LOG_DXRT << "[TEST] Terminated Child process: pid=" << getpid() << " index=" << i 
                    << " duration=" << duration.count() << "ms" << std::endl;
                std::exit(ret);
            }
            else {
                LOG_DXRT << "[TEST] Finished bitmatch function: pid=" << getpid() << " index=" << i 
                    << " duration=" << duration.count() << "ms" << std::endl;
                if ( ret == 0 ) success_count = 1;
            }
        }
        else if ( pid < 0 )
        {
            LOG_DXRT_ERR("[ERROR] Fork Failureed");
            return -1;
        }
    }

    // no child process if process_count == 1
    
    if ( process_count > 1 ) 
    {
        for (int i = 0; i < process_count; i++)
        {
            int status;
            pid_t pid = wait(&status);
            if ( status == 0 ) success_count++;

            LOG_DXRT << "[TEST] pid=" << pid << " status=" << status << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    LOG_DXRT << "[TEST] Terminated Main process: pid=" << getpid() << " durtaion=" << duration.count() << "ms" << std::endl;
    LOG_DXRT << "[TEST] Model path=" << modelPath << std::endl;
    LOG_DXRT << "[TEST] process-count=" << process_count << ", test-count=" << test_count << ", " 
                << (test_sync ? "Sync" : "Async") << ", thread-count=" << thread_count << std::endl;
    LOG_DXRT << "[TEST] Total Running Time: " << duration.count() << "ms" 
                << " (" << (duration.count() / 1000.0) / 60.0 << "min)" << std::endl;
    LOG_DXRT << "[TEST] Process count: " << process_count << std::endl;
    LOG_DXRT << "[TEST] Process Total Result=" << (process_count == success_count ? "Success" : "Failure")
                << "(" << success_count << "/" << process_count << ")" << std::endl;

    LOG_DXRT << "[TEST] Bitmatch fail count=" << gBitmatchAsyncResult.failIndices.size() << std::endl;
    for(auto index : gBitmatchAsyncResult.failIndices)
    {
        LOG_DXRT << index << " ";
    }
    LOG_DXRT << std::endl;

    //long memory_usage_end = getMemoryUsage();
    //LOG_DXRT << "[TEST] Memory Diff: start=" << gMemoryUsageStart << " end=" << gMemoryUsageEnd 
    //        << " diff=" << (gMemoryUsageEnd - gMemoryUsageStart) << std::endl;

    return process_count == success_count ? 0 : -1;
}

#else
#include <stdio.h>
int main(int argc, char* argv[])
{
	printf("Not implemented in Windows.\n") ;
	return 0;
}
#endif
