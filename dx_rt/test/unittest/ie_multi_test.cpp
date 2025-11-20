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

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <atomic>
using std::cout;
using std::endl;
using std::vector;
using std::hex;
using std::dec;

static constexpr int engine_count = 5;

TEST(ie, DISABLED_multi_run)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    std::vector<std::shared_ptr<dxrt::InferenceEngine> > ies;

    // int engine_count = 2;
    std::atomic<int> callBackCnt[engine_count];
    for (int i = 0; i < engine_count; i++)
    {
        ies.push_back(std::make_shared<dxrt::InferenceEngine>(testModelPath));
        callBackCnt[i] = 0;
        ies[i]->RegisterCallback(
        [&callBackCnt, i](const dxrt::TensorPtrs &outputs, void *arg)->int {
            callBackCnt[i]++;
            LOG_VALUE(callBackCnt[i]);
            LOG_VALUE(reinterpret_cast<uint64_t>(arg));
            std::ignore = outputs;
            return 0;
        });
    }
    auto outputsOrg = ies[0]->GetOutputs();
    std::vector<uint8_t> input(ies[0]->GetInputSize(), 0);

    std::vector<uint8_t> ref_outputs_buffer(ies[0]->GetOutputSize(), 0);
    auto ref_outputs = ies[0]->Run(input.data(), reinterpret_cast<void*>(12345));
    int idx = 0;
    for (unsigned int k = 0; k < ref_outputs.size(); k++)
    {
        int64_t output_size = ref_outputs[k]->elem_size();
        for (unsigned int l = 0; l < ref_outputs[k]->shape().size(); l++)
            output_size *= ref_outputs[k]->shape()[l];
        uint8_t* temp_data = static_cast<uint8_t*>(ref_outputs_buffer.data())+idx;
        for (unsigned int l = 0; l < output_size; l++)
        {
            const uint8_t* dataPtr = static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
            ref_outputs_buffer[idx] = *dataPtr;
            idx++;
        }
        ref_outputs[k]->data() = static_cast<void*>(temp_data);
    }

    int numReq = testNum;
    for (int i = 0; i < numReq; i++)
    {
        for ( int j = 0; j < engine_count; j++)
        {
            auto outputs = ies[j]->Run(input.data(), reinterpret_cast<void*>(i) );
            for (const auto &output : outputs)
            {
                cout << *output << endl;
            }
            EXPECT_EQ(outputsOrg.size(), outputs.size());
            EXPECT_EQ(outputsOrg.front().type(), outputs.front()->type());
            EXPECT_EQ(outputsOrg.front().shape().size(), outputs.front()->shape().size());

            for (unsigned int k = 0; k < outputs.size(); k++)
            {
                int64_t output_size = outputs[k]->elem_size();
                for (unsigned int l = 0; l < outputs[k]->shape().size(); l++)
                    output_size *= outputs[k]->shape()[l];

                for (unsigned int l = 0; l < output_size; l++)
                {
                    uint8_t* dataPtr = static_cast<uint8_t*>(outputs[k]->data()) + l;
                    uint8_t* refDataPtr =  static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
                    ASSERT_NE(dataPtr, refDataPtr);
                    ASSERT_EQ(*dataPtr, *refDataPtr);
                }
            }
        }
    }
#ifdef __linux__
    usleep(500);  // wait for callback
#elif _WIN32
    std::this_thread::sleep_for(std::chrono::microseconds(500));
#endif
    EXPECT_EQ(callBackCnt[0], numReq + 1);
    for ( int j = 1; j < engine_count; j++)
    {
        EXPECT_EQ(callBackCnt[j], numReq);
    }
}

TEST(ie, DISABLED_multi_async)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    vector<std::shared_ptr<dxrt::InferenceEngine> > ies;
    // int engine_count = 2;

    vector<vector<int> > ids(engine_count);
    vector<dxrt::TensorPtrs> output_data[engine_count];
    std::atomic<int> callBackCnt[engine_count];
    for (int i = 0; i < engine_count; i++)
    {
        ies.push_back(std::make_shared<dxrt::InferenceEngine>(testModelPath));
        callBackCnt[i] = 0;
    }
    auto outputsOrg = ies[0]->GetOutputs();
    vector<uint8_t> input(ies[0]->GetInputSize(), 0xA5);

    vector<uint8_t> ref_outputs_buffer(ies[0]->GetOutputSize(), 0);
    auto ref_outputs = ies[0]->Run(input.data(), reinterpret_cast<void*>(12345));
    int idx = 0;
    for (unsigned int k = 0; k < ref_outputs.size(); k++)
    {
        int64_t output_size = ref_outputs[k]->elem_size();
        for (unsigned int l = 0; l < ref_outputs[k]->shape().size(); l++)
            output_size *= ref_outputs[k]->shape()[l];
        uint8_t* temp_data = static_cast<uint8_t*>(ref_outputs_buffer.data())+idx;
        for (unsigned int l = 0; l < output_size; l++)
        {
            uint8_t* dataPtr = static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
            ref_outputs_buffer[idx] = *dataPtr;
            idx++;
        }
        ref_outputs[k]->data() = static_cast<void*>(temp_data);
    }


    for (int i = 0; i < engine_count; i++)
    {
        ies[i]->RegisterCallback(
        [&callBackCnt, &ref_outputs, i](dxrt::TensorPtrs &outputs, void *arg)->int {
            callBackCnt[i]++;
            LOG_VALUE(callBackCnt[i]);
            LOG_VALUE(reinterpret_cast<uint64_t>(arg));
            for (auto &output : outputs)
            {
                cout << "Async: "<< *output << endl;
            }
            for (unsigned int k = 0; k < outputs.size(); k++)
            {
                int64_t output_size = outputs[k]->elem_size();
                for (unsigned int l = 0; l < outputs[k]->shape().size(); l++)
                    output_size *= outputs[k]->shape()[l];

                for (unsigned int l = 0; l < output_size; l++)
                {
                    uint8_t* dataPtr = static_cast<uint8_t*>(outputs[k]->data()) + l;
                    uint8_t* refDataPtr =  static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
                    if (dataPtr == refDataPtr)
                    {
                        EXPECT_NE(dataPtr, refDataPtr);
                        return -1;
                    }
                    if (*dataPtr != *refDataPtr)
                    {
                        EXPECT_EQ(*dataPtr, *refDataPtr);
                        return -1;
                    }
                }
            }
            return 0;
        });
    }




    int numReq = testNum;
    for (int i = 0; i < numReq; i++)
    {
        for ( int j = 0; j < engine_count; j++)
        {
            int requestId = ies[j]->RunAsync(input.data(), reinterpret_cast<void*>(j));
            ids[j].push_back(requestId);
        }
    }
    for (int i = 0; i < numReq; i++)
    {
        for ( int j = 0; j < engine_count; j++)
        {
            ies[j]->Wait(ids[j][i]);
        }
    }

    for ( int j = 0; j < engine_count; j++)
    {
        EXPECT_EQ(callBackCnt[j], numReq);
    }
}

TEST(ie, DISABLED_multi_async_run)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    vector<std::shared_ptr<dxrt::InferenceEngine> > ies;
    // int engine_count = 2;

    vector<vector<int> > ids(engine_count);
    vector<dxrt::TensorPtrs> output_data[engine_count];
    std::atomic<int> callBackCnt[engine_count];
    for (int i = 0; i < engine_count; i++)
    {
        ies.push_back(std::make_shared<dxrt::InferenceEngine>(testModelPath));
        callBackCnt[i] = 0;
    }
    auto outputsOrg = ies[0]->GetOutputs();
    vector<uint8_t> input(ies[0]->GetInputSize(), 0x3C);

    vector<uint8_t> ref_outputs_buffer(ies[0]->GetOutputSize(), 0);
    auto ref_outputs = ies[0]->Run(input.data(), reinterpret_cast<void*>(12345));
    int idx = 0;
    for (unsigned int k = 0; k < ref_outputs.size(); k++)
    {
        int64_t output_size = ref_outputs[k]->elem_size();
        for (unsigned int l = 0; l < ref_outputs[k]->shape().size(); l++)
            output_size *= ref_outputs[k]->shape()[l];
        uint8_t* temp_data = static_cast<uint8_t*>(ref_outputs_buffer.data())+idx;
        for (unsigned int l = 0; l < output_size; l++)
        {
            uint8_t* dataPtr = static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
            ref_outputs_buffer[idx] = *dataPtr;
            idx++;
        }
        ref_outputs[k]->data() = static_cast<void*>(temp_data);
    }


    for (int i = 0; i < engine_count; i++)
    {
        ies[i]->RegisterCallback(
        [&callBackCnt, &ref_outputs, i](dxrt::TensorPtrs &outputs, void *arg)->int {
            callBackCnt[i]++;
            for (auto &output : outputs)
            {
                cout << "Async: "<< *output << endl;
            }
            LOG_VALUE(callBackCnt[i]);
            LOG_VALUE(reinterpret_cast<uint64_t>(arg));
            for (unsigned int k = 0; k < outputs.size(); k++)
            {
                int64_t output_size = outputs[k]->elem_size();
                for (unsigned int l = 0; l < outputs[k]->shape().size(); l++)
                    output_size *= outputs[k]->shape()[l];

                for (unsigned int l = 0; l < output_size; l++)
                {
                    uint8_t* dataPtr = static_cast<uint8_t*>(outputs[k]->data()) + l;
                    uint8_t* refDataPtr =  static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
                    if (dataPtr == refDataPtr)
                    {
                        EXPECT_NE(dataPtr, refDataPtr);
                        return -1;
                    }
                    if (*dataPtr != *refDataPtr)
                    {
                        EXPECT_EQ(*dataPtr, *refDataPtr);
                        return -1;
                    }
                }
            }
            return 0;
        });
    }

    int numReq = testNum;
    for (int i = 0; i < numReq; i++)
    {
        for ( int j = 0; j < engine_count; j++)
        {
            int requestId = ies[j]->RunAsync(input.data(), reinterpret_cast<void*>(j));
            ids[j].push_back(requestId);
        }
    }
    for (int i = 0; i < numReq; i++)
    {
        for ( int j = 0; j < engine_count; j++)
        {
            auto outputs = ies[j]->Run(input.data(), reinterpret_cast<void*>(i) );
            for (auto &output : outputs)
            {
                cout << *output << endl;
            }
            EXPECT_EQ(outputsOrg.size(), outputs.size());
            EXPECT_EQ(outputsOrg.front().type(), outputs.front()->type());
            EXPECT_EQ(outputsOrg.front().shape().size(), outputs.front()->shape().size());

            for (unsigned int k = 0; k < outputs.size(); k++)
            {
                int64_t output_size = outputs[k]->elem_size();
                for (unsigned int l = 0; l < outputs[k]->shape().size(); l++)
                    output_size *= outputs[k]->shape()[l];

                for (unsigned int l = 0; l < output_size; l++)
                {
                    uint8_t* dataPtr = static_cast<uint8_t*>(outputs[k]->data()) + l;
                    uint8_t* refDataPtr =  static_cast<uint8_t*>(ref_outputs[k]->data()) + l;
                    ASSERT_NE(dataPtr, refDataPtr);
                    ASSERT_EQ(*dataPtr, *refDataPtr);
                }
            }
        }
        for ( int j = 0; j < engine_count; j++)
        {
            ies[j]->Wait(ids[j][i]);
        }
    }
#ifdef __linux__
    usleep(500);  // wait for callback
#elif _WIN32
    std::this_thread::sleep_for(std::chrono::microseconds(500));
#endif
    EXPECT_EQ(callBackCnt[0], numReq * 2);
    for ( int j = 1; j < engine_count; j++)
    {
        EXPECT_EQ(callBackCnt[j], numReq * 2);
    }
}

TEST(ie, DISABLED_output_ptr_location_bitmatch)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    dxrt::InferenceEngine ie(testModelPath);
    int numReq = testNum;



    vector<uint8_t> input(ie.GetInputSize(), 0x12);
    vector<uint8_t> output(ie.GetOutputSize(), 0);
    EXPECT_GT(ie.GetOutputSize(), 0);
    LOG_VALUE(ie.GetOutputSize());

    auto outputsOrg = ie.GetOutputs(output.data());
    // EXPECT_EQ(outputsOrg.front().data(), output.data());


    auto outputs = ie.Run(input.data(), nullptr, output.data());
    std::cout << outputs.front()->type() << std::endl;
    for (auto it : outputs)
        cout << *it  << "(" << dec << (int64_t)it->data() - (int64_t)output.data() << ")" << endl;
    cout << hex << "0x" << (int64_t)output.data() << endl;
    if ( outputs.front()->type() != dxrt::DataType::BBOX )
    {
        for (int times = 0; times < numReq; times++)
        {
            auto outputs2 = ie.Run(input.data(), nullptr, nullptr);

            EXPECT_NE(outputs2.front()->data(), output.data());

            EXPECT_EQ(outputs2.size(), outputs.size());
            for (size_t i = 0; i < outputs2.size(); i++)
            {
                EXPECT_EQ(outputs2[i]->type(), outputs[i]->type());
                LOG_VALUE(outputs2[i]->shape().size());
                LOG_VALUE(outputs[i]->shape().size());
                EXPECT_EQ(outputs2[i]->shape().size(), outputs[i]->shape().size());
                int64_t output_size = outputs[i]->elem_size();
                for (size_t j = 0; j < outputs[i]->shape().size(); j++)
                {
                    EXPECT_EQ(outputs2[i]->shape()[j], outputs[i]->shape()[j]);
                    output_size *= outputs[i]->shape()[j];
                }
                if (output_size == 0)
                {
                    output_size = 128*1024;
                }
                for (unsigned int j = 0; j < output_size; j++)
                {
                    uint8_t* dataPtr = static_cast<uint8_t*>(outputs[i]->data()) + j;
                    uint8_t* refDataPtr =  static_cast<uint8_t*>(outputs2[i]->data()) + j;
                    ASSERT_NE(dataPtr, refDataPtr);
                    ASSERT_EQ(*dataPtr, *refDataPtr);
                }
            }
        }
    }
}



TEST(ie, DISABLED_output_ptr_location_bitmatch2)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    dxrt::InferenceEngine ie(testModelPath);
    int numReq = testNum;

    vector<uint8_t> input1(ie.GetInputSize(), 0);
    for (size_t i = 0; i < input1.size(); i++)
    {
        input1[i] = static_cast<uint8_t>(i * 101 + 89);
    }
    vector<uint8_t> input2(ie.GetInputSize(), 0);
    for (size_t i = 0; i < input1.size(); i++)
    {
        input1[i] = static_cast<uint8_t>(i * 79 + 113);
    }
    std::vector<uint8_t> output1(ie.GetOutputSize(), 0);
    std::vector<uint8_t> output2(ie.GetOutputSize(), 0);

    EXPECT_GT(ie.GetOutputSize(), 0);
    LOG_VALUE(ie.GetOutputSize());

    auto outputsOrg = ie.GetOutputs(output1.data());
    // EXPECT_EQ(outputsOrg.front().data(), output1.data());


    auto outputs1 = ie.Run(input1.data(), output1.data(), output1.data());
    auto outputs2 = ie.Run(input1.data(), output2.data(), output2.data());
    cout << " outputs1[0]->shape()[0]: " <<  outputs1[0]->shape()[0] << endl;
    cout << " outputs2[0]->shape()[0]: " <<  outputs2[0]->shape()[0] << endl;
    for (int times = 0; times < numReq; times++)
    {
        auto outputs3 = ie.Run((numReq%2)?input1.data():input2.data(), nullptr, nullptr);
        EXPECT_NE(outputs3.front()->data(), outputs1.front()->data());

        EXPECT_EQ(outputs3.size(), outputs1.size());
        for (size_t i = 0; i < outputs1.size(); i++)
        {
            EXPECT_EQ(outputs3[i]->type(), outputs1[i]->type());

            EXPECT_EQ(outputs3[i]->shape().size(), outputs1[i]->shape().size());
            int64_t output_size = outputs3[i]->elem_size();
            for (size_t j = 0; j < outputs3[i]->shape().size(); j++)
            {
                EXPECT_EQ(outputs3[i]->shape()[j], outputs1[i]->shape()[j]);
                output_size *= outputs3[i]->shape()[j];
            }

            cout << " outputs3[i]->shape()[0]: " <<  outputs3[i]->shape()[0] << " output_size: " << output_size << endl;
            if(output_size == 0)
            {
                output_size = 128*1024;
            }
            for (unsigned int j = 0; j < output_size; j++)
            {
                uint8_t* dataPtr = static_cast<uint8_t*>((numReq%2)?outputs1[i]->data():outputs2[i]->data()) + j;
                uint8_t* refDataPtr =  static_cast<uint8_t*>(outputs3[i]->data()) + j;
                ASSERT_NE(dataPtr, refDataPtr);
                ASSERT_EQ(*dataPtr, *refDataPtr);
            }
        }
    }
}


TEST(ie, DISABLED_multi_ie_test)
{
    LOG_VALUE(testModelPath);
    LOG_VALUE(testNum);
    for (int times = 0; times < testNum; times++)
    {
        dxrt::InferenceEngine ie(testModelPath);
        auto outputsOrg = ie.GetOutputs();
        std::vector<uint8_t> input(ie.GetInputSize(), 0);
        std::vector<int> requests;
        int numReq = 5;
        std::atomic<int> callBackCnt;
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
        for (int i = 0; i < numReq; i++)
        {
            // LOG_VALUE_HEX((void*)input.data());
            requests.emplace_back(ie.RunAsync(input.data(), reinterpret_cast<void*>(i) ) );
            // requests.emplace_back( ie.RunAsync(nullptr) );
        }
        // sleep(1);
        // cout << "........................" << endl;
        for (auto &request : requests)
        {
            auto outputs = ie.Wait(request);
            // for(auto &output:outputs)
            // {
            //     cout << output << endl;
            // }

        }
        EXPECT_EQ(callBackCnt, numReq);
    }
}
