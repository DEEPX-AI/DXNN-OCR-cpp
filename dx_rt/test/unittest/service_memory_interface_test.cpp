// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Licensed under the MIT License.

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <memory>
#include "dxrt/memory_interface.h"
#include "dxrt/memory_interface_singleprocess.h"
#include "dxrt/memory_interface_multiprocess.h"
#include "dxrt/service_abstract_layer.h"
#include "dxrt/device_struct.h"

namespace dxrt {

// NOTE: MemoryInterface base is currently commented out in memory_interface.h, so
// custom observer with overrides is removed to avoid override compile errors.

// TEST(ServiceMemoryInterface, SingleProcess_BasicAllocateDeallocate) {
//   NoMultiprocessMemory mem;
//   auto addr = mem.Allocate(0, 4096);
//   EXPECT_NE(addr, 0u);
//   mem.Deallocate(0, addr);
// }

// TEST(ServiceMemoryInterface, SingleProcess_BackwardAllocate) {
//   NoMultiprocessMemory mem;
//   auto addr = mem.BackwardAllocate(0, 2048);
//   EXPECT_NE(addr, 0u);
// }


// Minimal fake DeviceCore for ServiceLayer registration
class FakeDeviceCore : public DeviceCore {
 public:
  FakeDeviceCore(): DeviceCore(0, nullptr) {}
};

// TEST(ServiceMemoryInterface, ServiceLayer_NoService_RunFlags) {
//   NoServiceLayer noSvc;
//   EXPECT_FALSE(noSvc.isRunOnService());
// }

// TEST(ServiceMemoryInterface, ServiceLayer_ServiceStubSignals) {
//   auto mp = std::make_shared<MultiprocessMemory>();
//   ServiceLayer svc(mp);
//   auto a = svc.Allocate(0, 1024);
//   auto b = svc.BackwardAllocateForTask(0, 1, 512);
//   EXPECT_NE(a, 0u);
//   EXPECT_NE(b, 0u);
// }

}  // namespace dxrt
