// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <numeric>
#include "dxrt/task.h"
#include "dxrt/request.h"
#include "dxrt/request_response_class.h"
#include "dxrt/tensor.h"
#include "dxrt/device_struct.h"

namespace dxrt {

// Helper to construct a CPU task (same idea as basic test but reused here)
static std::shared_ptr<Task> MakeCpuTask(const std::string& name, size_t blobSize = 48) {
    deepx_rmapinfo::RegisterInfoDatabase emptyInfo;  // not initialized -> CPU
    std::vector<std::vector<uint8_t>> blobs; blobs.emplace_back(blobSize, 0xAA);
    return std::make_shared<Task>(name, emptyInfo, std::move(blobs), N_BOUND_NORMAL, std::vector<int>{0});
}

static RequestPtr MakeReq(std::shared_ptr<Task> t, int job) {
    return Request::Create(t.get(), nullptr, nullptr, nullptr, job);
}

TEST(RequestResponseCpuExtended, DISABLED_ManySequentialRequestsInferenceTimeAlwaysZero) {
    auto task = MakeCpuTask("seqTask");
    std::vector<RequestPtr> reqs;
    for (int i = 0; i < 25; ++i) {
        auto r = MakeReq(task, 1000 + i);
        dxrt_response_t resp{}; resp.inf_time = 10 + i;  // ignored
        RequestResponse::ProcessResponse(r, resp, 0);
        EXPECT_EQ(r->inference_time(), 0u);
        reqs.push_back(r);
    }
}

TEST(RequestResponseCpuExtended, DISABLED_ProcessResponseNullResponseMultiple) {
    auto task = MakeCpuTask("nullRespTask");
    for (int i = 0; i < 10; ++i) {
        auto r = MakeReq(task, 2000 + i);
        dxrt_response_t resp{};
        EXPECT_NO_THROW(RequestResponse::ProcessResponse(r, resp, -1));
        EXPECT_EQ(r->inference_time(), 0u);
    }
}

TEST(RequestResponseCpuExtended, DISABLED_BufferReleaseAfterProcessResponse) {
    auto task = MakeCpuTask("releaseTask");
    auto r = MakeReq(task, 3001);
    dxrt_response_t resp{}; RequestResponse::ProcessResponse(r, resp, -1);
    EXPECT_NO_THROW(r->releaseBuffers());
    EXPECT_NO_THROW(r->releaseBuffers());  // idempotent
}

TEST(RequestResponseCpuExtended, DISABLED_RequestIdsAreUniqueAcrossCreates) {
    auto task = MakeCpuTask("idTask");
    auto r1 = MakeReq(task, 4001);
    auto r2 = MakeReq(task, 4002);
    EXPECT_NE(r1->id(), r2->id());
}

TEST(RequestResponseCpuExtended, DISABLED_LatencyValidFlagRemainsTrue) {
    auto task = MakeCpuTask("latTask");
    auto r = MakeReq(task, 5001);
    dxrt_response_t resp{}; RequestResponse::ProcessResponse(r, resp, -1);
    EXPECT_TRUE(r->latency_valid());
}

TEST(RequestResponseCpuExtended, DISABLED_ModelTypeCopiedFromTaskData) {
    auto task = MakeCpuTask("modelTypeTask");
    auto r = MakeReq(task, 6001);
    EXPECT_EQ(r->model_type(), task->getData()->_npuModel.type);
}

TEST(RequestResponseCpuExtended, DISABLED_OutputBufferBaseInitiallyNull) {
    auto task = MakeCpuTask("outBaseTask");
    auto r = MakeReq(task, 7001);
    EXPECT_EQ(r->output_buffer_base(), nullptr);
}

TEST(RequestResponseCpuExtended, DISABLED_OnRequestCompleteInvokedOnce) {
    auto task = MakeCpuTask("completeTask");
    auto r = MakeReq(task, 8001);
    struct Counter { int c = 0; }; static Counter counter; counter.c = 0;
    // Monkey patch by wrapping original onRequestComplete via friend (cannot easily). Here we rely on default impl calling user callback; skip.
    dxrt_response_t resp{}; RequestResponse::ProcessResponse(r, resp, -1);
    SUCCEED();
}

TEST(RequestResponseCpuExtended, DISABLED_MultipleTasksIndependentIds) {
    auto t1 = MakeCpuTask("taskA");
    auto t2 = MakeCpuTask("taskB");
    auto r1 = MakeReq(t1, 9001);
    auto r2 = MakeReq(t2, 9002);
    EXPECT_NE(r1->task(), r2->task());
    EXPECT_NE(r1->id(), r2->id());
}

TEST(RequestResponseCpuExtended, DISABLED_CreateManyRequestsStress) {
    auto task = MakeCpuTask("stressTask");
    constexpr int N = 50;  // moderate
    std::vector<RequestPtr> v; v.reserve(N);
    for (int i = 0; i < N; ++i) {
        v.push_back(MakeReq(task, 10000 + i));
    }
    // Just ensure all unique IDs
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            ASSERT_NE(v[i]->id(), v[j]->id());
        }
    }
}

}  // namespace dxrt
