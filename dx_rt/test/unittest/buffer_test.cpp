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
#include "dxrt/buffer.h"
#include "dxrt_test.h"

using namespace std;

TEST(buffer, basic)
{
    uint32_t totalSize = 128*1024*1024;
    uint32_t requestSize = 5*1024*1024;
    int numReq = 2*totalSize/requestSize;
    void *buf, *prevBuf;
    dxrt::Buffer buffer(totalSize);
    LOG_VALUE_HEX(buffer.Get());
    for(int i=0;i<numReq;i++)
    {        
        buf = buffer.Get(requestSize);
        if(i==0)
            EXPECT_EQ(buf, buffer.Get());
        else
            EXPECT_TRUE(buf == static_cast<void*>(static_cast<uint8_t*>(prevBuf) + requestSize) || buf == buffer.Get());
        prevBuf = buf;
        // cout << i << ": " << buf << endl;
    }
}