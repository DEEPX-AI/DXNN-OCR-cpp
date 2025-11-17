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
#include <gmock/gmock.h>
#include "../../lib/dxrt_service/scheduler_service.h"
#include "../../lib/dxrt_service/service_device.h"
#include "dxrt_test.h"
#include "dxrt/driver.h"

using namespace std;

using ::testing::Expectation;
using ::testing::Return;

#ifdef __linux__

class MockDevice: public dxrt::ServiceDevice
{
public:
    MockDevice(): dxrt::ServiceDevice("Dummy"){}
    MOCK_METHOD(int, InferenceRequest, (dxrt::dxrt_request_acc_t*), (override));
};

TEST(Scheduler, Fifo1)
{
    shared_ptr<MockDevice> mock = make_shared<MockDevice>();
    std::vector<std::shared_ptr<dxrt::ServiceDevice>> devs;
    devs.push_back(mock);
    FIFOSchedulerService scheduler(devs);

    dxrt::dxrt_request_acc_t acc;

    EXPECT_CALL(*mock,InferenceRequest).WillOnce(Return(0));
    
    scheduler.AddScheduler(acc, 0);

}

#endif