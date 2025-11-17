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
#include "dxrt/tensor.h"
#include "dxrt/request.h"
#include "dxrt_test.h"

using namespace std;
using namespace dxrt;
/*
TEST(request, basic)
{    
    {
        vector<RequestPtr> requests;
        for(int i=1; i<5; i++)
        {
            vector<Tensor> inputs, outputs;
            inputs.emplace_back(
                dxrt::Tensor("input0", {1,2,3,4}, dxrt::DataType::UINT16, reinterpret_cast<void*>(0x1000*i+0xffaabb) )
            );
            outputs.emplace_back(
                dxrt::Tensor("output0", {1,5,23,100}, dxrt::DataType::UINT8, reinterpret_cast<void*>(0x1000*i+0x1) )
            );
            outputs.emplace_back(
                dxrt::Tensor("output1", {1,15,42,75}, dxrt::DataType::UINT8, reinterpret_cast<void*>(0x1000*i+0x2) )
            );
            outputs.emplace_back(
                dxrt::Tensor("output2", {3,5,2}, dxrt::DataType::UINT8, reinterpret_cast<void*>(0x1000*i+0x3) )
            );
            requests.emplace_back(
                Request::Create(nullptr, inputs, outputs, nullptr )
            );            
            auto &req = requests.back();
            // cout << *req << endl;
            EXPECT_EQ(req.use_count(), 1);
            EXPECT_EQ(req->id(), i);
            EXPECT_TRUE(!req->inputs().empty());
            EXPECT_TRUE(!req->outputs().empty());
        }        
    }
    {
        dxrt::Request::ShowAll();
    }
}*/

