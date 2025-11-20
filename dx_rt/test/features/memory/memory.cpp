
/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#ifdef __linux__	// all or dummy

#include "dxrt/dxrt_api.h"
#include "dxrt/tensor.h"
#include "dxrt/common.h"
#include <time.h>

#ifdef __linux__
#include <sys/wait.h>
#include <sys/resource.h>

#include <sys/resource.h>
#endif
#include <iostream>
// getrusage not found in windows

static std::shared_ptr<uint8_t> readDumpFile(int32_t &outSize, const std::string& path)
{

    FILE *stream = fopen(path.c_str(), "rb");
    if ( stream != NULL ) {

        fseek(stream, 0L, SEEK_END);
        auto size = ftell(stream);

        // create buffer to read dump file
        std::shared_ptr<uint8_t> dump(new uint8_t[size], std::default_delete<uint8_t[]>());

        fseek(stream, 0L, SEEK_SET);
        outSize = fread(dump.get(), 1, size, stream);
        fclose(stream);

        LOG_DXRT << "[TEST] Read File: " << path << " size: " << size << std::endl;

        return dump;

    } // stream
    else {
        outSize = 0;
        LOG_DXRT_ERR("[TEST] File not found: " << path);
    }

    return nullptr;
}

static bool isArgMaxOutput(dxrt::TensorPtrs& outputs)
{
    LOG_DXRT << "[TEST] Tensor name=" << outputs.front()->name() << std::endl;
    return outputs.front()->name() == "argmax_output";
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

static bool compareOutputs(uint8_t* gtData, int32_t gtSize, uint8_t* rtData, int32_t rtSize)
{

    if ( gtSize != rtSize )
    {
        //return memcmp(rawData, expectOutputResult.get(), dumpSize) == 0 ? true : false;
        LOG_DXRT << "[WARN] Warning: Mismatch gt and rt file size (gt=" << gtSize << ", rt=" << rtSize << ")" << std::endl;
    }

    if ( gtSize > 0 ) 
    {
        return memcmp(rtData, gtData, rtSize) == 0 ? true : false;
    }
    
    return false;
}

long getMemoryUsage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    //std::cout << "Memory usage: " << usage.ru_maxrss << " KB" << std::endl;
    return usage.ru_maxrss;
}

static int bitMatchSync(std::string modelPath, std::string modelFolderPath, int loopCount)
{
    std::string inputPath = modelFolderPath + "gt/npu_0_input_0.bin";
    std::string outputPath = modelFolderPath + "gt/npu_0_output_0.bin";

    int32_t inputGTSize = 0;
    std::shared_ptr<uint8_t> inputPtr = readDumpFile(inputGTSize, inputPath);
    if ( inputPtr == nullptr )
    {
        return -1;
    }

    int32_t outputGTSize = 0;
    std::shared_ptr<uint8_t> outputPtr = nullptr; //readDumpFile(outputGTSize, outputPath);

    std::vector<std::pair<int,long>> vectorMemUsages;

    long memory_start = getMemoryUsage();
    
    vectorMemUsages.push_back(std::pair<int,long>(-1, memory_start));

    for (int i = 0; i < loopCount; i++)
    {

        LOG_DXRT << "[TEST] Test seq=" << i << std::endl;

        // load ai model
        try {

            // load model
            dxrt::InferenceEngine inferenceEngine(modelPath);

            auto outputs = inferenceEngine.Run(inputPtr.get());

            if ( outputGTSize == 0 )
            {

                std::string outputPath = modelFolderPath + "gt/";
                if ( isArgMaxOutput(outputs) ) // check argmax or ppu
                {
                    outputPath += "npu_0_output_0.argmax.bin";
                }
                else 
                {
                    outputPath += "npu_0_output_0.ppu.bin";
                }

                outputPtr = readDumpFile(outputGTSize, outputPath);
                if ( outputPtr == nullptr )
                {
                    return -1;
                }
            }
        
            int outputSize = calcuOutputRawDataSize(outputs);
            bool compareResult = false;
            if ( outputSize > 0 )
            {
                compareResult = compareOutputs(outputPtr.get(), outputGTSize, reinterpret_cast<uint8_t*>(outputs.front()->data()), outputSize);
            }
            
            if ( compareResult )
            {
                LOG_DXRT << "[TEST] output data is matched (Success)" << std::endl;
            }
            else
            {
                LOG_DXRT_ERR("[ERROR] output data is not matched with GT (Failure)");
                return -1;
            }

            if ( loopCount >= 10 && i % (loopCount/10) == 0 )
            {
                vectorMemUsages.push_back(std::pair<int,long>(i, getMemoryUsage()));
            }
            //memory_start_1 = getMemoryUsage();

        }
        catch(std::exception &e) 
        {
            LOG_DXRT_ERR("[ERROR] " << e.what());
            return -1;
        }
        catch(...)
        {
            LOG_DXRT_ERR("[ERROR] Exception");
            return -1;
        }

    } // for i


    long memory_end = getMemoryUsage();

    //LOG_DXRT << "[TEST] memory usage start=" << memory_start << std::endl;
    LOG_DXRT << "[TEST] memory usage start=" << memory_start << " end=" << memory_end 
                << " diff=" << (memory_end - memory_start) << std::endl;

    LOG_DXRT << "[TEST] memory usages" << std::endl;
    for(auto &mu : vectorMemUsages)
    {
        std::cout << mu.first << ", " << mu.second << std::endl;
    }

    return 0;

}



static std::string getFolderPath(std::string& fileFullPath)
{
    size_t pos = fileFullPath.find_last_of("/\\");
    
    // folder path
    std::string folderPath = (pos == std::string::npos) ? "" : fileFullPath.substr(0, pos);
    folderPath += "/";
   
    return folderPath;
}


int main(int argc, char* argv[])
{
    const int DEFAULT_LOOP_COUNT = 100;
    std::string modelFolderPath;
    std::string modelPath;
    int loop_count = DEFAULT_LOOP_COUNT;
    if ( argc > 1 )
    {
        modelPath = argv[1];
        modelFolderPath = getFolderPath(modelPath);

        if ( argc > 2 ) 
        {
            loop_count = std::stoi(argv[2]);
            LOG_DXRT << "[TEST] loop_count=" << loop_count << std::endl;
        }

    }
    else
    {
        LOG_DXRT << "[Usage] dxrt_test_memory [model-file-path] [loop-count]" << std::endl;
        return -1;
    }


    LOG_DXRT << "[TEST] TEST::Memory" << std::endl;
    LOG_DXRT << "[TEST] Model Full Path: " << modelPath << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    bitMatchSync(modelPath, modelFolderPath, loop_count);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    LOG_DXRT << "[TEST] Total Running Time: " << duration.count() << "ms" 
                << " (" << (duration.count() / 1000.0) / 60.0 << "min)" << std::endl;

    return 0;
}

#else
#include <stdio.h>
int main(int argc, char* argv[])
{
	printf("Not implemented in Windows.\n") ;
    return 0;
}


#endif
