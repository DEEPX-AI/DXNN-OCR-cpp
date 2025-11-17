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
#include "dxrt_test.h"

using namespace std;
using namespace dxrt;

TEST(tensor, basic)
{
    dxrt::Tensor tensor("test", {1, 2, 3, 4}, dxrt::DataType::UINT16, reinterpret_cast<void*>(0xffaabb));
    cout << tensor << endl;
}

TEST(tensor, copy)
{
    dxrt::Tensor tensor("test", {1, 2, 3, 4}, dxrt::DataType::UINT16, reinterpret_cast<void*>(0xffaabb));
    auto newTensor = Tensor(tensor, reinterpret_cast<void*>(0xcafe));
    // newTensor.name() = "test-new";
    cout << tensor << endl;
    cout << newTensor << endl;
    EXPECT_EQ(newTensor.data(), reinterpret_cast<void*>(0xcafe));
    // EXPECT_EQ(newTensor.name(), "test-new");
    EXPECT_EQ(tensor.name(), "test");
}