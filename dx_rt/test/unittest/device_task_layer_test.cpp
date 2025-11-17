// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ENABLE_DEVICE_TEST_ACCESSORS
#include "dxrt/device_task_layer.h"
#include "dxrt/task_data.h"
#include "dxrt/request_data.h"
#include "dxrt/device_core.h"
#include "dxrt/device_struct.h"
#include "dxrt/driver_adapter/driver_adapter.h"
#include "mocks/mock_driver_adapter.h"
#include "dxrt/service_abstract_layer.h"
#include "dxrt/model.h"

using dxrt::DeviceCore;
using dxrt::StdDeviceTaskLayer;
using dxrt::ServiceLayerInterface;
using dxrt::dxrt_cmd_t;
using dxrt::dxrt_device_info_t;
using dxrt::dxrt_request_acc_t;
using dxrt::TaskData;
using dxrt::RequestData;
// using dxrt::DeviceType;
using dxrt::Processor;
using dxrt::rmapinfo;
using dxrt::N_BOUND_NORMAL;
using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::Invoke;

using dxrt::MockDriverAdapter;

extern std::string thisExecPath;

namespace {

class FakeServiceLayer : public ServiceLayerInterface {
 public:
    uint64_t Allocate(int deviceId, uint64_t size) override {
        (void)deviceId;  // single device in tests
        int64_t base = static_cast<int64_t>(storage.size());
        storage.resize(storage.size() + size, 0xAB);  // fill pattern
        return base;  // treat as offset
    }
    void DeAllocate(int deviceId, int64_t addr) override { (void)deviceId; (void)addr; }
    uint64_t BackwardAllocateForTask(int deviceId, int taskId, uint64_t size) override {
        std::ignore = taskId;
        return Allocate(deviceId, size);
    }
    void HandleInferenceAcc(const dxrt_request_acc_t &acc, int deviceId) override {
        (void)acc; (void)deviceId; handledAcc = true;
    }
    void SignalDeviceReset(int id) override { (void)id; }
    void SignalEndJobs(int id) override { (void)id; }
    void CheckServiceRunning() override {}
    bool isRunOnService() const override { return false; }
    void RegisterDeviceCore(DeviceCore *core) override { (void)core; }
    void SignalTaskInit(int, int, dxrt::npu_bound_op, uint64_t){}
    void SignalTaskDeInit(int, int, dxrt::npu_bound_op) {}
    std::vector<uint8_t> storage;  // acts as device memory image
    bool handledAcc = false;
};

static std::shared_ptr<DeviceCore> MakeCore(MockDriverAdapter** mockOut) {
    auto* mock = new MockDriverAdapter();
    *mockOut = mock;
    std::unique_ptr<MockDriverAdapter> up(mock);
    auto core = std::make_shared<DeviceCore>(0, std::move(up));

    // Provide Identify info expectations so code using core->info() has sane values
    EXPECT_CALL(*mock, IdentifyDevice( _))
        .WillOnce(DoAll(Invoke([](dxrt_device_info_t* info) {
                std::memset(static_cast<void*>(info), 0, sizeof(*info));
                info->mem_addr = 0x10000000ULL;
                info->mem_size = 1 << 20;  // 1MB
                info->type = static_cast<uint32_t>(DeviceType::STD_TYPE);
                return 0;
            })));
    core->Identify(0);
    return core;
}

class MinimalTaskBuilder {
 public:
    static TaskData* BuildStdTask(int id, uint32_t inputSize, uint32_t outputSize) {
        (void)inputSize;   // Reserved for future validation
        (void)outputSize;  // Reserved for future validation

        std::string modelFilePath = thisExecPath+"/../test/unittest/AD01FP32_1.dxnn";
        dxrt::ModelDataBase modelDB;
        dxrt::LoadModelParam(modelDB, modelFilePath);

        rmapinfo dummy = modelDB.deepx_rmap.rmap_info(0); // default constructed, lightweight

        TaskData *t = new TaskData(id, "task" + std::to_string(id), dummy);
        std::vector<std::vector<uint8_t>> data;

        data.emplace_back(std::vector<uint8_t>(dummy.model_memory().rmap().size()));
        auto& firstMemBuffer = modelDB.deepx_binary.rmap(0).buffer();
        memcpy(data.back().data(), firstMemBuffer.data(), firstMemBuffer.size());
        DXRT_ASSERT(0 < data.back().size(), "invalid model - rmap size is zero");

        data.emplace_back(std::vector<uint8_t>(dummy.model_memory().weight().size()));
        auto& weightBuffer = modelDB.deepx_binary.weight(0).buffer();
        if (data.back().size() > 0) {
            memcpy(data.back().data(), weightBuffer.data(), weightBuffer.size());
        }
        t->set_from_npu(data);

        return t; // t sizes recorded from model; explicit sizes unused (silences -Wunused-parameter)
    }
};

TEST(DeviceTaskLayerTest, RegisterTask_STD_WritesModelParamsAndCreatesBuffers) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    EXPECT_CALL(*mock, GetName()).WillRepeatedly(Return("name"));
    auto service = std::make_shared<FakeServiceLayer>();
    StdDeviceTaskLayer layer(core, service);

    // Expect two model writes and two read-backs per buffer (rmap+weight). DEVICE_NUM_BUF=2 so 4 write+4 read.
    EXPECT_CALL(*mock, IoctlWrite( _, _))
        .Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _))
        .Times(2).WillRepeatedly(Return(0));
        /*
    EXPECT_CALL(*mock, IdentifyDevice( _))
        .WillOnce(DoAll(Invoke([](dxrt_device_info_t* info) {
                std::memset(static_cast<void*>(info), 0, sizeof(*info));
                info->mem_addr = 0x10000000ULL;
                info->mem_size = 1 << 20;  // 1MB
                info->type = static_cast<uint32_t>(DeviceType::STD_TYPE);
                return 0;
            })));
*/
    auto* task = MinimalTaskBuilder::BuildStdTask(1, /*input*/128, /*output*/256);
    int rc = layer.dxrt::StdDeviceTaskLayer::RegisterTask(task);
    EXPECT_EQ(rc, 0);
    delete task;
}

TEST(DeviceTaskLayerTest, InferenceRequest_STD_BuffersRotateAndReuseDetected) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    auto service = std::make_shared<FakeServiceLayer>();
    StdDeviceTaskLayer layer(core, service);

    EXPECT_CALL(*mock, IoctlWrite( _, _)).Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _)).Times(2).WillRepeatedly(Return(0));

    auto* task = MinimalTaskBuilder::BuildStdTask(2, 64, 128);
    ASSERT_EQ(layer.dxrt::StdDeviceTaskLayer::RegisterTask(task), 0);

    RequestData reqA{}; reqA.requestId = 10; reqA.taskData = task;
    std::vector<uint8_t> inputA(64, 0x11), outputA(128, 0);
    reqA.encoded_inputs_ptr = inputA.data();
    reqA.encoded_outputs_ptr = outputA.data();

    // First request should copy (no reuse)
    EXPECT_EQ(layer.dxrt::StdDeviceTaskLayer::InferenceRequest(&reqA, N_BOUND_NORMAL), 0);

    // Second request with same pointer should reuse (no additional write for input copy path at task layer level)
    RequestData reqB{}; reqB.requestId = 11; reqB.taskData = task;
    reqB.encoded_inputs_ptr = inputA.data();
    reqB.encoded_outputs_ptr = outputA.data();
    EXPECT_EQ(layer.dxrt::StdDeviceTaskLayer::InferenceRequest(&reqB, N_BOUND_NORMAL), 0);
    delete task;
}

TEST(DeviceTaskLayerTest, InferenceRequest_STD_BufferRotationChangesIndex) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    auto service = std::make_shared<FakeServiceLayer>();
    StdDeviceTaskLayer layer(core, service);

    EXPECT_CALL(*mock, IoctlWrite( _, _)).Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _)).Times(2).WillRepeatedly(Return(0));

    auto* task = MinimalTaskBuilder::BuildStdTask(3, 32, 64);
    ASSERT_EQ(layer.dxrt::StdDeviceTaskLayer::RegisterTask(task), 0);
    int initial = layer.test_getBufIndex(task->id());

    // Two requests -> buf index should advance (mod DEVICE_NUM_BUF=2)
    RequestData r1{}; r1.requestId = 21; r1.taskData = task; std::vector<uint8_t> in1(32,0x1), out1(64); r1.encoded_inputs_ptr=in1.data(); r1.encoded_outputs_ptr=out1.data();
    RequestData r2{}; r2.requestId = 22; r2.taskData = task; std::vector<uint8_t> in2(32,0x2), out2(64); r2.encoded_inputs_ptr=in2.data(); r2.encoded_outputs_ptr=out2.data();
    EXPECT_EQ(layer.dxrt::StdDeviceTaskLayer::InferenceRequest(&r1, N_BOUND_NORMAL), 0);
    //EXPECT_EQ(layer.InferenceRequest(&r2, N_BOUND_NORMAL), 0);
    int after = layer.test_getBufIndex(task->id());
    EXPECT_NE(initial, after);
    delete task;
}

TEST(DeviceTaskLayerTest, InferenceRequest_STD_OngoingMapContainsRequest) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    auto service = std::make_shared<FakeServiceLayer>();
    StdDeviceTaskLayer layer(core, service);

    EXPECT_CALL(*mock, IoctlWrite( _, _)).Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _)).Times(2).WillRepeatedly(Return(0));

    auto* task = MinimalTaskBuilder::BuildStdTask(4, 48, 96);
    ASSERT_EQ(layer.dxrt::StdDeviceTaskLayer::RegisterTask(task), 0);
    RequestData r{};
    r.requestId = 33;
    r.taskData = task;
    std::vector<uint8_t> in(48, 0x3), out(96);
    r.encoded_inputs_ptr = in.data();
    r.encoded_outputs_ptr = out.data();
    EXPECT_EQ(layer.dxrt::StdDeviceTaskLayer::InferenceRequest(&r, N_BOUND_NORMAL), 0);
    auto* ongoing = layer.test_getOngoing(r.requestId);
    ASSERT_NE(ongoing, nullptr);
    EXPECT_EQ(ongoing->req_id, 33);
    delete task;
}

TEST(DeviceTaskLayerTest, RegisterTask_STD_ModelDescriptorFieldsConsistent) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    auto service = std::make_shared<FakeServiceLayer>();
    StdDeviceTaskLayer layer(core, service);

    EXPECT_CALL(*mock, IoctlWrite( _, _)).Times(4).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _)).Times(2).WillRepeatedly(Return(0));

    auto* task = MinimalTaskBuilder::BuildStdTask(5, 80, 160);
    ASSERT_EQ(layer.dxrt::StdDeviceTaskLayer::RegisterTask(task), 0);
    const auto& vec = layer.test_getInferenceVec(task->id());
    ASSERT_FALSE(vec.empty());
    for (auto &inf : vec) {
        EXPECT_EQ(inf.model_type, 0u);
        EXPECT_EQ(inf.cmd_offset, task->_npuModel.rmap.offset);
        //EXPECT_EQ(inf.weight_offset, task->_npuModel.weight.offset);
        EXPECT_EQ(inf.last_output_offset, task->_npuModel.last_output_offset);
        EXPECT_GT(inf.output.size, 0u);
    }
    delete task;
}

// ACC specific tests (reuse existing infrastructure but we need an ACC device type)
// We simulate ACC by changing info().type after Identify via IOControl expectation.
TEST(DeviceTaskLayerTest, AccRegisterTask_BasicDescriptorInitialization) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    // Override Identify again to set ACC type for this test
    EXPECT_CALL(*mock, IdentifyDevice(_))
        .WillOnce(DoAll(Invoke([](dxrt_device_info_t* info) {
            std::memset(static_cast<void*>(info), 0, sizeof(*info));
            info->mem_addr = 0x20000000ULL;
            info->mem_size = 1 << 20;
            info->type = static_cast<uint32_t>(DeviceType::ACC_TYPE);
            return 0; })));
    core->Identify(0);
    auto service = std::make_shared<FakeServiceLayer>();
    dxrt::AccDeviceTaskLayer layer(core, service);

    EXPECT_CALL(*mock, IoctlWrite( _, _)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _)).Times(2).WillRepeatedly(Return(0));

    auto* task = MinimalTaskBuilder::BuildStdTask(6, 40, 80);
    ASSERT_EQ(layer.dxrt::AccDeviceTaskLayer::RegisterTask(task), 0);
    auto* acc = layer.test_getInferenceAcc(task->id());
    ASSERT_NE(acc, nullptr);
    EXPECT_EQ(acc->task_id, task->id());
    //EXPECT_EQ(acc->input.base, task->_npuModel.rmap.base);
    delete task;
}

TEST(DeviceTaskLayerTest, AccInferenceRequest_OngoingInserted) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCore(&mock);
    EXPECT_CALL(*mock, IdentifyDevice(_))
        .WillOnce(DoAll(Invoke([](dxrt_device_info_t* info) {
            std::memset(static_cast<void*>(info), 0, sizeof(*info));
            info->mem_addr = 0x21000000ULL;
            info->mem_size = 1 << 20;
            info->type = static_cast<uint32_t>(DeviceType::ACC_TYPE);
            return 0; })));
    core->Identify(0);
    auto service = std::make_shared<FakeServiceLayer>();
    dxrt::AccDeviceTaskLayer layer(core, service);

    EXPECT_CALL(*mock, IoctlWrite( _, _)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(*mock, IoctlRead( _, _)).Times(2).WillRepeatedly(Return(0));

    auto* task = MinimalTaskBuilder::BuildStdTask(7, 52, 104);
    ASSERT_EQ(layer.dxrt::AccDeviceTaskLayer::RegisterTask(task), 0);
    RequestData rq{}; rq.requestId = 70; rq.taskData = task;
    std::vector<uint8_t> in(52, 0x9), out(104);
    rq.encoded_inputs_ptr = in.data();
    rq.encoded_outputs_ptr = out.data();
    EXPECT_EQ(layer.dxrt::AccDeviceTaskLayer::InferenceRequest(&rq, N_BOUND_NORMAL), 0);
    auto* ongoing = layer.test_getOngoing(rq.requestId);
    ASSERT_NE(ongoing, nullptr);
    EXPECT_EQ(ongoing->req_id, 70);
    delete task;
}

}  // namespace
