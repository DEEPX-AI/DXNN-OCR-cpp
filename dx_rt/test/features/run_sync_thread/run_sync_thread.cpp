/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#include "dxrt/dxrt_api.h"
#include "dxrt/tensor.h"
#include "dxrt/common.h"
#include <time.h>
#include <iostream>
using std::cout;
using std::endl;


static int32_t gOutputGTSize = 0;
static int gPassCount = 0;
static std::shared_ptr<uint8_t> gOutputPtr = nullptr;
static std::string gModelFolderPath = "";

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

        LOG_DXRT << "[TEST] Read File: " << path << " size: " << size << endl;

        return dump;
    }  // stream
    else
    {
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
        LOG_DXRT << "[WARN] Warning: Mismatch gt and rt file size (gt=" << gtSize << ", rt=" << rtSize << ")" << std::endl;
    }

    if ( rtSize > 0 && gtSize >= rtSize ) 
    {
        return memcmp(rtData, gtData, rtSize) == 0 ? true : false;
    }
    
    return false;
}

static std::string getFolderPath(std::string& fileFullPath)
{
    size_t pos = fileFullPath.find_last_of("/\\");
    
    // folder path
    std::string folderPath = (pos == std::string::npos) ? "" : fileFullPath.substr(0, pos);
    folderPath += "/";
   
    return folderPath;
}

static int postProcessing(dxrt::TensorPtrs& outputs)
{
    // load gt output data once
    if ( gOutputGTSize == 0 )
    {

        std::string outputPath = gModelFolderPath + "gt/";
        if ( isArgMaxOutput(outputs) ) // check argmax or ppu
        {
            outputPath += "npu_0_output_0.argmax.bin";
        }
        else 
        {
            outputPath += "npu_0_output_0.ppu.bin";
        }

        gOutputPtr = readDumpFile(gOutputGTSize, outputPath);
        if ( gOutputPtr == nullptr )
        {
            return -1;
        }
    }

    // compare output data
    int gOutputSize = calcuOutputRawDataSize(outputs);
    bool compareResult = false;
    if ( gOutputSize > 0 )
    {
        compareResult = compareOutputs(gOutputPtr.get(), gOutputGTSize, reinterpret_cast<uint8_t*>(outputs.front()->data()), gOutputSize);
    }
    
    if ( compareResult )
    {
        LOG_DXRT << "[TEST] output data is matched with GT (Success)" << std::endl;
        gPassCount++;
        return 0;
    }
    else
    {
        LOG_DXRT_ERR("[ERROR] output data is not matched with GT (Failure)");
    }

    return -1;
}

// thread function
static int inferenceThreadFunc(std::string modelPath, std::shared_ptr<uint8_t>& inputPtr, int boundOption, int loopCount)
{
    // select bound option 1~3 per device
    dxrt::InferenceOption op;
    op.boundOption = boundOption;

    // create inference engine and load model data
    dxrt::InferenceEngine ie(modelPath, op);

    for (int i = 0; i < loopCount; i++)
    {

        // run(inference) asynchronously
        // The input data must remain valid until the callback is invoked
        auto outputs = ie.Run(inputPtr.get());

        // post procesing
        postProcessing(outputs);

    } // for i

    return 0;
}

static int bitMatchAsync(std::string modelPath, std::string modelFolderPath, int loopCount)
{
    std::string inputPath = modelFolderPath + "gt/npu_0_input_0.bin";
    std::string outputPath = modelFolderPath + "gt/npu_0_output_0.bin";

    // load input data
    int32_t inputGTSize = 0;
    std::shared_ptr<uint8_t> inputPtr = readDumpFile(inputGTSize, inputPath);
    if ( inputPtr == nullptr )
    {
        return -1;
    }


    auto t1 = std::thread(inferenceThreadFunc, std::ref(modelPath), std::ref(inputPtr), 1, loopCount);
    auto t2 = std::thread(inferenceThreadFunc, std::ref(modelPath), std::ref(inputPtr), 2, loopCount);
    //auto t3 = std::thread(inferenceThreadFunc, std::ref(modelPath), std::ref(inputPtr), 3, loopCount);
    
    // Wait until all threads have completed
    t1.join();
    t2.join();
    //t3.join();


    return 0;

}

int main(int argc, char* argv[])
{
    const int DEFAULT_LOOP_COUNT = 1;
    
    std::string modelPath;
    int loop_count = DEFAULT_LOOP_COUNT;
    if ( argc > 1 )
    {
        modelPath = argv[1];
        gModelFolderPath = getFolderPath(modelPath);

        if ( argc > 2 ) 
        {
            loop_count = std::stoi(argv[2]);
            LOG_DXRT << "[TEST] loop_count=" << loop_count << std::endl;
        }

    }
    else
    {
        LOG_DXRT << "[Usage] dxrt_test_runasync [dxnn-file-path] [loop-count]" << std::endl;
        return -1;
    }


    LOG_DXRT << "[TEST] TEST::RunAsync" << std::endl;
    LOG_DXRT << "[TEST] Model Full Path: " << modelPath << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // inference bitmatch asynchronously
    bitMatchAsync(modelPath, gModelFolderPath, loop_count);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    LOG_DXRT << "[TEST] Total Running Time: " << duration.count() << "ms" 
                << " (" << (duration.count() / 1000.0) / 60.0 << "min)" << std::endl;
    if ( gPassCount == loop_count * 2 )
    {
        LOG_DXRT << "[TEST] Total (" << gPassCount << "/" << loop_count * 2 << "): Success" << std::endl;
        return 0;
    }
    else 
    {
        LOG_DXRT << "[TEST] Total (" << gPassCount << "/" << loop_count * 2 << "): Failure" << std::endl;
    }

    return -1;
}