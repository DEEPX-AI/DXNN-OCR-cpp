// Simplified NFHLayer tests (Plan A)

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

#include "dxrt/nfh_layer.h"
#include "dxrt/device_task_layer.h"
#include "dxrt/device_core.h"
#include "dxrt/driver_adapter/driver_adapter.h"
#include "dxrt/request.h"
#include "dxrt/request_response_class.h"  // for RequestData
#include "dxrt/task_data.h"               // TaskData base

namespace dxrt {

// Minimal dummy DriverAdapter
class DummyAdapter : public DriverAdapter {
public:
    int32_t IOControl(dxrt_cmd_t, void*, uint32_t, uint32_t) override { return 0; }
    int32_t Write(const void*, uint32_t) override { return 0; }
    int32_t Read(void*, uint32_t) override { return 0; }
    void* MemoryMap(void*, size_t, off_t) override { return nullptr; }
    int32_t Poll() override { return 0; }
    int GetFd() const override { return -1; }
    std::string GetName() const override { return "dummy"; }
};

// Dummy TaskData (필수 최소 필드만 사용; 실제 구조에 따라 필요시 확장)
class DummyTaskData : public TaskData {
public:
    DummyTaskData() : TaskData(1, "DummyTask", rmapinfo()) {
        // 최소 설정: _npuModel.type 등 필요 시 0으로 초기 상태
        _npuModel.type = 0;
    }
};

static DummyTaskData g_dummyTask; // 모든 테스트 공유 (읽기 전용 용도)

class SimpleDeviceTaskLayer : public DeviceTaskLayer {
public:
    SimpleDeviceTaskLayer(std::shared_ptr<DeviceCore> core)
        : DeviceTaskLayer(core, nullptr) {}
    int InferenceRequest(RequestData* req, npu_bound_op) override {
        // simulate success; req->taskData 존재 가정
        std::ignore = req;
        return 0;
    }
    int RegisterTask(TaskData*) override { return 0; }
    int Release(TaskData*) override { return 0; }
    void StartThread() override {}
    int getFullLoad() const override { return 1; }
    void ProcessResponseFromService(const dxrt_response_t&) override {}
    std::vector<Tensors> inputs(int) override { return {}; }
};

// Helper: 초기화된 Request 생성
static RequestPtr MakeReq(int id) {
    auto r = std::make_shared<Request>(id);
    auto *d = r->getData();
    d->taskData = &g_dummyTask; // 핵심: NFHLayer encode 경로 접근시 nullptr 회피
    // 빈 입력/출력 텐서는 허용되도록 가정 (EncodeInputs가 빈 목록 허용하지 않으면 훅 필요)
    return r;
}

class NFHLayerTest : public ::testing::Test {
protected:
    std::shared_ptr<DeviceCore> core;
    std::shared_ptr<SimpleDeviceTaskLayer> taskLayer;
    std::unique_ptr<NFHLayer> syncLayer;   // _isDynamic=false
    std::unique_ptr<NFHLayer> asyncLayer;  // _isDynamic=true

    void SetUp() override {
        core = std::make_shared<DeviceCore>(0, std::unique_ptr<DriverAdapter>(new DummyAdapter()));
        taskLayer = std::make_shared<SimpleDeviceTaskLayer>(core);
        syncLayer.reset(new NFHLayer(taskLayer, false));
        asyncLayer.reset(new NFHLayer(taskLayer, true));
        // Override default callback (which assumes fully initialized Task/TaskData) with no-op for tests
        auto noop = [](int, const dxrt_response_t&, int){};
        syncLayer->SetResponseCallback(noop);
        asyncLayer->SetResponseCallback(noop);
    }
};

TEST_F(NFHLayerTest, InferenceRequest_InvalidDeviceId) {
    auto r = MakeReq(1);
    EXPECT_EQ(-1, syncLayer->InferenceRequest(99, r, npu_bound_op{}));
}

TEST_F(NFHLayerTest, InferenceRequest_Sync_Success) {
    auto r = MakeReq(2);
    EXPECT_EQ(0, syncLayer->InferenceRequest(0, r, npu_bound_op{}));
}

TEST_F(NFHLayerTest, InferenceRequest_Async_Queued) {
    auto r = MakeReq(3);
    EXPECT_EQ(0, asyncLayer->InferenceRequest(0, r, npu_bound_op{}));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(NFHLayerTest, ProcessResponse_InvalidDeviceId) {
    dxrt_response_t resp{};
    EXPECT_EQ(-1, syncLayer->ProcessResponse(99, 1, &resp));
}

TEST_F(NFHLayerTest, ProcessResponse_NullResponse) {
    EXPECT_EQ(-1, syncLayer->ProcessResponse(0, 5, nullptr));
}

TEST_F(NFHLayerTest, ProcessResponse_Sync_NoRegisteredRequest_NoCrash) {
    dxrt_response_t resp{};
    // Ensure pooled request id 10 has valid taskData to avoid crash inside DecodeOutputs path
    if (auto pooled = Request::GetById(10)) {
        pooled->getData()->taskData = &g_dummyTask;
        pooled->model_type() = 0; // so DecodeOutputs branch executes safely
    }
    EXPECT_EQ(0, syncLayer->ProcessResponse(0, 10, &resp));
}

TEST_F(NFHLayerTest, ProcessResponse_Async_NoRegisteredRequest_NoCrash) {
    dxrt_response_t resp{};
    if (auto pooled = Request::GetById(11)) {
        pooled->getData()->taskData = &g_dummyTask;
        pooled->model_type() = 0;
    }
    EXPECT_EQ(0, asyncLayer->ProcessResponse(0, 11, &resp));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(NFHLayerTest, Async_ManyRequests_Stability) {
    const int N = 50;
    for (int i = 0; i < N; ++i) {
        auto r = MakeReq(100 + i);
        ASSERT_EQ(0, asyncLayer->InferenceRequest(0, r, npu_bound_op{}));
    }
    dxrt_response_t resp{};
    for (int i = 0; i < N; ++i) {
        if (auto pooled = Request::GetById(100 + i)) {
            pooled->getData()->taskData = &g_dummyTask;
            pooled->model_type() = 0;
        }
        ASSERT_EQ(0, asyncLayer->ProcessResponse(0, 100 + i, &resp));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

} // namespace dxrt
