/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#include "gtest/gtest.h"

using namespace std;

static void temp_func(shared_ptr<int> shared)
{
    cout << "Inside : shared_ptr use_count = " << shared.use_count() << endl;
}

TEST(shared_ptr, count)
{
    shared_ptr<int> shared = make_shared<int>(2732);
    cout << "Before : shared_ptr use_count = " << shared.use_count() << endl;
    temp_func(shared);
    cout << "After : shared_ptr use_count = " << shared.use_count() << endl;
}