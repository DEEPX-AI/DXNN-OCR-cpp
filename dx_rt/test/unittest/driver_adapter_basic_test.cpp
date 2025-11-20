// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Basic unit tests for DriverAdapter::getDeviceType and ::getDeviceStatus
// Rewritten minimal version using an in-file mock to ensure we actually
// exercise the virtual IOControl path (previous flakes suggested stale object).

#include <gtest/gtest.h>
#include <cstdio>
#include "dxrt/driver_adapter/driver_adapter.h"
#include "dxrt/driver.h"

namespace dxrt {

// Compile-time markers to prove this TU is compiled
static_assert(DXRT_CMD_IDENTIFY_DEVICE == 0, "IDENT ENUM VALUE CHANGED");
static_assert(DXRT_CMD_GET_STATUS == 1, "STATUS ENUM VALUE CHANGED");

class SeededMockAdapter final : public DriverAdapter {
 public:
  int32_t IOControl(dxrt_cmd_t request, void* data, uint32_t size = 0, uint32_t sub_cmd = 0) override {
    (void)size; (void)sub_cmd;
    ++totalCalls;
    if (request == DXRT_CMD_IDENTIFY_DEVICE) {
      ++identifyCalls;
      if (data) {
        auto* info = static_cast<dxrt_device_info_t*>(data);
        info->type = DEVICE_TYPE_STANDALONE;
      }
      return 0;
    }
    if (request == DXRT_CMD_GET_STATUS) {
      ++statusCalls;
      if (data) {
        auto* st = static_cast<dxrt_device_status_t*>(data);
        for (int i = 0; i < 4; ++i) {
          st->temperature[i] = 10 + i;
          st->clock[i]       = 100 + i;
        }
      }
      return 0;
    }
    return 0;
  }
  int32_t Write(const void*, uint32_t) override { return -1; }
  int32_t Read(void*, uint32_t) override { return -1; }
  void*   MemoryMap(void*, size_t, off_t) override { return nullptr; }
  int32_t Poll() override { return 0; }
  int     GetFd() const override { return -1; }
  std::string GetName() const override { return "seeded-mock"; }

  int totalCalls{0};
  int identifyCalls{0};
  int statusCalls{0};
};

TEST(DriverAdapterBasicTest, GetDeviceTypeReturnsTypeField) {
  SeededMockAdapter adapter;
  EXPECT_EQ(adapter.identifyCalls, 0);
  auto devType = adapter.getDeviceType();
  EXPECT_EQ(adapter.identifyCalls, 1) << "IOControl(DXRT_CMD_IDENTIFY_DEVICE) not invoked";
  EXPECT_EQ(static_cast<uint32_t>(devType), static_cast<uint32_t>(DEVICE_TYPE_STANDALONE));
}

TEST(DriverAdapterBasicTest, GetDeviceStatusCopiesStatusStruct) {
  SeededMockAdapter adapter;
  EXPECT_EQ(adapter.statusCalls, 0);
  auto status = adapter.getDeviceStatus();
  EXPECT_EQ(adapter.statusCalls, 1) << "IOControl(DXRT_CMD_GET_STATUS) not invoked";
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(status.temperature[i], 10u + static_cast<uint32_t>(i));
    EXPECT_EQ(status.clock[i],       100u + static_cast<uint32_t>(i));
  }
}

} // namespace dxrt
