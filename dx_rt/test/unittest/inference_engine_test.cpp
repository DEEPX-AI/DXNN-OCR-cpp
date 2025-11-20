/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 * 
 * This file uses Google Test (BSD 3-Clause License) - Copyright 2008, Google Inc.
 */

#include "gtest/gtest.h"
#include "dxrt/inference_engine.h"
#include "dxrt/util.h"
#include "dxrt_test.h"

using namespace std;

TEST(ie, basic)
{
    LOG_VALUE(testModelPath);
    dxrt::InferenceEngine ie(testModelPath);
    cout << ie << endl;
}

TEST(ie, run)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    dxrt::InferenceEngine ie(testModelPath);
    auto outputsOrg = ie.GetOutputs();
    vector<uint8_t> input(ie.GetInputSize(), 0);
    int numReq = testNum;
    atomic<int> callBackCnt;
    callBackCnt = 0;
    ie.RegisterCallback(
        [&](dxrt::TensorPtrs &outputs, void *arg)->int {
            callBackCnt++;
            LOG_VALUE(callBackCnt);
            LOG_VALUE(reinterpret_cast<uint64_t>(arg));
            std::ignore = outputs;
            return 0;
        }
    );
    for(int i=0;i<numReq;i++)
    {
        auto outputs = ie.Run( input.data(), reinterpret_cast<void*>(i) );
        for(auto &output:outputs)
        {
            cout << *output << endl;
        }
        EXPECT_EQ(outputsOrg.size(), outputs.size());
        EXPECT_EQ(outputsOrg.front().type(), outputs.front()->type());
        EXPECT_EQ(outputsOrg.front().shape().size(), outputs.front()->shape().size());
        // auto shape = outputs.front()->shape();
        // dxrt::DataDumpTxt(
        //     "output.txt",
        //     (float*)outputs.front()->data(),
        //     shape[0],
        //     shape[1],
        //     shape[2]
        // );
    }    
#ifdef __linux__
    usleep(500); // wait for callback
#elif _WIN32
    this_thread::sleep_for(chrono::microseconds(500));
#endif

    EXPECT_EQ(callBackCnt, numReq);
}

TEST(ie, run_async)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    dxrt::InferenceEngine ie(testModelPath);
    auto outputsOrg = ie.GetOutputs();
    vector<uint8_t> input(ie.GetInputSize(), 0);
    vector<int> requests;
    int numReq = testNum;
    atomic<int> callBackCnt;
    callBackCnt = 0;
    ie.RegisterCallback(
        [&](dxrt::TensorPtrs &outputs, void *arg)->int {            

            std::ignore = arg;
            callBackCnt++;
            EXPECT_EQ(outputsOrg.size(), outputs.size());
            EXPECT_EQ(outputsOrg.front().type(), outputs.front()->type());
            EXPECT_EQ(outputsOrg.front().shape().size(), outputs.front()->shape().size());
            LOG_VALUE(callBackCnt);
            return 0;
        }
    );
    for(int i=0;i<numReq;i++)
    {
        // LOG_VALUE_HEX((void*)input.data());
        requests.emplace_back( ie.RunAsync( input.data(), reinterpret_cast<void*>(i) ) );
        // requests.emplace_back( ie.RunAsync(nullptr) );
    }
    // sleep(1);
    // cout << "........................" << endl;
    for(auto &request:requests)
    {
        auto outputs = ie.Wait(request);
        // for(auto &output:outputs)
        // {
        //     cout << output << endl;
        // }
        
    }
    EXPECT_EQ(callBackCnt, numReq);
}

TEST(ie, tensor)
{
    LOG_VALUE(testModelPath);
    uint64_t inputPhyAddr = 0x90001000;
    uint64_t outputPhyAddr = 0xA0002000;
    dxrt::InferenceEngine ie(testModelPath);
    vector<uint8_t> inputBuf(ie.GetInputSize(), 0);
    vector<uint8_t> outputBuf(ie.GetOutputSize(), 0);
    auto inputs = ie.GetInputs(inputBuf.data(), inputPhyAddr);
    auto outputs = ie.GetOutputs(outputBuf.data(), outputPhyAddr);
    for(auto &tensor:inputs)
    {
        cout << tensor << endl;
    }
    for(auto &tensor:outputs)
    {
        cout << tensor << endl;
    }

}
/*
TEST(ie, validate_device)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    dxrt::InferenceEngine ie(testModelPath);
    auto outputsOrg = ie.GetOutputs();
    vector<uint8_t> input(ie.input_size(), 0);
    int numReq = testNum;
    atomic<int> callBackCnt;
    callBackCnt = 0;
    ie.RegisterCallback(
        [&](dxrt::TensorPtrs &outputs, void *arg)->int {
            std::ignore = outputs;
            callBackCnt++;
            LOG_VALUE(callBackCnt);
            LOG_VALUE(reinterpret_cast<uint64_t>(arg));
            return 0;
        }
    );
    for(int i=0;i<numReq;i++)
    {
        auto outputs = ie.ValidateDevice( input.data(), 0 );
        EXPECT_EQ(outputs.size(), 1);
        EXPECT_EQ(outputs.front()->type(), dxrt::DataType::INT8);
        EXPECT_EQ(outputs.front()->shape().size(), 1);
    }
#ifdef __linux__
    usleep(500); // wait for callback
#elif _WIN32
    this_thread::sleep_for(chrono::microseconds(500));
#endif
    EXPECT_EQ(callBackCnt, numReq);
}*/

TEST(ie, output_ptr_location)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    dxrt::InferenceEngine ie(testModelPath);



    
    vector<uint8_t> input(ie.GetInputSize(), 0);
    vector<uint8_t> output(ie.GetOutputSize(), 0);

    atomic<int> callBackCnt;
    callBackCnt = 0;
    auto outputsOrg = ie.GetOutputs(output.data());
    //EXPECT_EQ(outputsOrg.front().data(), output.data());


    auto outputs = ie.Run(input.data(), nullptr, output.data());

    cout << hex<< (int64_t)output.data() << endl;
    for (auto it: outputs) cout << *it  << "(" << dec << (int64_t)it->data() - (int64_t)output.data() << ")" << endl;

    EXPECT_EQ(outputsOrg.size(), outputs.size());
    EXPECT_EQ(outputsOrg.front().type(), outputs.front()->type());
    EXPECT_EQ(outputsOrg.front().shape().size(), outputs.front()->shape().size());
    EXPECT_EQ(outputsOrg.front().data(), outputs.front()->data());
    for (size_t i = 0; i < outputsOrg.size(); i++)
    {
        EXPECT_EQ(outputsOrg[i].type(), outputs[i]->type());
        /*
        EXPECT_EQ(outputsOrg[i].shape().size(), outputs[i]->shape().size());
        for(size_t j = 0; j < outputs[i]->shape().size(); j++)
        {
            if(outputsOrg[i].shape()[j]==-1)
            {
                EXPECT_EQ(outputs[i]->shape()[j], 0);
            }
            else
            {
                EXPECT_EQ(outputsOrg[i].shape()[j], outputs[i]->shape()[j]);
            }
        }
        */
        //EXPECT_EQ(outputsOrg[i].data(), outputs[i]->data());
        EXPECT_GE(outputs[i]->data(), output.data());
        EXPECT_LT(outputs[i]->data(), output.data() + output.size());

    }
}