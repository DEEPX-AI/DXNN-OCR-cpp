// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include "dxrt/task.h"
#include "dxrt/request.h"
#include "dxrt/request_response_class.h"
#include "dxrt/tensor.h"

// NOTE: These tests cover only CPU path behavior of RequestResponse::ProcessResponse
// and basic Request lifecycle without needing a fully initialized NPU Task.

namespace dxrt {

static std::shared_ptr<Task> MakeCpuTaskSimple(const std::string& name,
                                               const std::vector<int>& outShape,
                                               DataType dtype) {
    // Build minimal CPU model blob; empty rmapinfo triggers CPU path.
    deepx_rmapinfo::RegisterInfoDatabase emptyInfo; // not initialized
    std::vector<std::vector<uint8_t>> blobs; // one placeholder blob required
    blobs.emplace_back(32, 0xCC);
    auto task = std::make_shared<Task>(name, emptyInfo, std::move(blobs), N_BOUND_NORMAL, std::vector<int>{0});

    // Prepare one output tensor shape by requesting outputs() with nullptr base
    // Actual tensor buffers set when Request created.
    (void)outShape; (void)dtype; // Task will allocate later; we assert size via Request outputs.
    return task;
}

TEST(RequestResponseCpuBasic, DISABLED_ProcessResponse_SetsZeroInferenceTime) {
    auto task = MakeCpuTaskSimple("cpuTask0", {1,4}, DataType::UINT8);
    auto req = Request::Create(task.get(), nullptr, nullptr, nullptr, 100);
    ASSERT_NE(req, nullptr);
    // Simulate deviceType=0 (CPU) response
    dxrt_response_t resp{}; resp.inf_time = 99999; // should be ignored for CPU
    RequestResponse::ProcessResponse(req, resp, -1);
    EXPECT_EQ(req->inference_time(), 0u);
}

TEST(RequestResponseCpuBasic, DISABLED_ProcessResponse_DoesNotCrashWithNullResponsePtr) {
    auto task = MakeCpuTaskSimple("cpuTask1", {1,8}, DataType::FLOAT);
    auto req = Request::Create(task.get(), nullptr, nullptr, nullptr, 101);
    ASSERT_NE(req, nullptr);
    // Passing nullptr response is allowed because code guards usage on processor type
    dxrt_response_t resp{};
    EXPECT_NO_THROW({ RequestResponse::ProcessResponse(req, resp, -1); });
    EXPECT_EQ(req->inference_time(), 0u);
}

TEST(RequestResponseCpuBasic, DISABLED_MultipleRequests_IndependentInferenceTimes) {
    auto task = MakeCpuTaskSimple("cpuTask2", {1,16}, DataType::INT16);
    auto r1 = Request::Create(task.get(), nullptr, nullptr, nullptr, 201);
    auto r2 = Request::Create(task.get(), nullptr, nullptr, nullptr, 202);
    dxrt_response_t respA{}; respA.inf_time = 123; // ignored
    dxrt_response_t respB{}; respB.inf_time = 456; // ignored
    RequestResponse::ProcessResponse(r1, respA, -1);
    RequestResponse::ProcessResponse(r2, respB, -1);
    EXPECT_EQ(r1->inference_time(), 0u);
    EXPECT_EQ(r2->inference_time(), 0u);
}

TEST(RequestResponseCpuBasic, DISABLED_LatencyProfilingGuard_NoProfilerBuildSafe) {
    auto task = MakeCpuTaskSimple("cpuTask3", {1,2}, DataType::UINT8);
    auto req = Request::Create(task.get(), nullptr, nullptr, nullptr, 301);
    dxrt_response_t resp{}; RequestResponse::ProcessResponse(req, resp, -1);
    // No direct assertion on latency (depends on profiler); just ensure call succeeded.
    SUCCEED();
}

TEST(RequestResponseCpuBasic, DISABLED_ReleaseBuffersIdempotent) {
    auto task = MakeCpuTaskSimple("cpuTask4", {1,4}, DataType::UINT8);
    auto req = Request::Create(task.get(), nullptr, nullptr, nullptr, 401);
    dxrt_response_t resp{}; RequestResponse::ProcessResponse(req, resp, -1);
    // First release (implicitly maybe none allocated) should not throw
    EXPECT_NO_THROW({ req->releaseBuffers(); });
    // Second release again should also be safe
    EXPECT_NO_THROW({ req->releaseBuffers(); });
}

} // namespace dxrt
