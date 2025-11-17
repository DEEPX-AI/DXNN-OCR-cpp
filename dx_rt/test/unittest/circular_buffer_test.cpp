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
#include "dxrt/circular_buffer.h"
#include "dxrt_test.h"

using namespace std;

TEST(circular_buffer, basic)
{
    int bufSize = 5;
    int numReq = 8;
    dxrt::CircularBuffer<int> buffer(bufSize);
    for(int i=0;i<numReq;i++)
    {
        buffer.Push(i);
    }
    auto result = buffer.ToVector();
    for(auto &x:result)
    {
        cout << x << endl;
    }
    EXPECT_EQ(buffer.size(), bufSize);
    EXPECT_EQ(buffer.count(), buffer.size());
}