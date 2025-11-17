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
#include "dxrt/util.h"
#include "dxrt_test.h"

using namespace std;
using namespace dxrt;

TEST(util, select_elem)
{
    vector<uint32_t> input{1,2,3,4,5};
    vector<int> indices{0, 2, 4};
    auto output = SelectElements(input, indices);
    EXPECT_EQ(output.size(), indices.size());
    for(auto &index : indices)
    {
        EXPECT_EQ( input.at(index) - 1, index);
    }
}
