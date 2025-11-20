/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#include "gtest/gtest.h"
#include "dxrt/inference_engine.h"
#include "dxrt_test.h"
#include "dxrt/filesys_support.h"

using namespace std;
using namespace dxrt;

TEST(rmap_info, basic)
{
    LOG_VALUE(testModelPath);
    if(dxrt::fileExists(testModelPath))
    {        
        //auto model = dxrt::LoadModelParam(testModelPath);
        dxrt::ModelDataBase model;
        dxrt::LoadModelParam(model, testModelPath);


        // EXPECT_TRUE(!rmapInfo.version().npu().empty());
        // EXPECT_TRUE(!rmapInfo.version().rmap().empty());
        // EXPECT_TRUE(!rmapInfo.version().rmapinfo().empty());
        // cout << rmapInfo.model() << endl;
    }
}

