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
#include "dxrt/datatype.h"
#include "dxrt_test.h"

using namespace std;
using namespace dxrt;

TEST(datatype, basic)
{
    for(int i = DataType::NONE_TYPE; i<DataType::MAX_TYPE; i++)
    {
        DataType type = static_cast<DataType>(i);
        ostringstream os;
        os << type;
        cout << os.str() << endl;
        EXPECT_EQ(DataTypeToString(type), os.str());
    }
}