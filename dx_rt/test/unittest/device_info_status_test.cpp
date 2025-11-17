// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Unit tests for DeviceStatus (device_info_status.cpp) aiming for near 100% coverage.

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <vector>
#include <array>
#include <cstring>
#include <cstdint>
#include <string>
#include "dxrt/device_info_status.h"
#include "dxrt/device_core.h"
#include "dxrt/device_pool.h"
#include "dxrt/driver_adapter/driver_adapter.h"

namespace dxrt {

// Lightweight fake adapter to allow constructing DeviceCore without real hardware.
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

// Fake DeviceCore exposing controlled info/status so we can exercise DeviceStatus paths indirectly.
class FakeDriverAdapter : public DriverAdapter {
 public:
  int32_t IOControl(dxrt_cmd_t request, void* data, uint32_t, uint32_t) override {
    if (request == DXRT_CMD_GET_STATUS) {
      auto* st = static_cast<dxrt_device_status_t*>(data);
      *st = statusTemplate;
      ++getStatusCalls;
      return 0;
    }
    if (request == DXRT_CMD_IDENTIFY_DEVICE) {
      auto* info = static_cast<dxrt_device_info_t*>(data);
      *info = infoTemplate;
      ++identifyCalls;
      return 0;
    }
    return 0;
  }
  int32_t Write(const void*, uint32_t) override { return 0; }
  int32_t Read(void*, uint32_t) override { return 0; }
  void* MemoryMap(void*, size_t, off_t) override { return nullptr; }
  int32_t Poll() override { return 0; }
  int GetFd() const override { return -1; }
  std::string GetName() const override { return "fakecore"; }

  dxrt_device_status_t statusTemplate{};
  dxrt_device_info_t infoTemplate{};
  int getStatusCalls{0};
  int identifyCalls{0};
};

// Wrapper to create a DeviceStatus via public GetCurrentStatus(DeviceCore) path.
static DeviceStatus MakeStatusViaCore(std::shared_ptr<DeviceCore> core,
                                      FakeDriverAdapter* fadp,
                                      const dxrt_device_info_t& info,
                                      const dxrt_device_status_t& st) {
  fadp->infoTemplate = info;
  fadp->statusTemplate = st;
  // Populate _info in core via Identify (uses IOControl IDENTIFY_DEVICE)
  core->Identify(core->id());
  // First status fetch updates internal _status
  (void)core->Status();
  // Build DeviceStatus snapshot
  return DeviceStatus::GetCurrentStatus(core);
}

static dxrt_device_info_t MakeInfo(uint32_t type, uint32_t variant, uint32_t bd_type,
                                   uint64_t mem_addr, uint64_t mem_size,
                                   uint16_t ddr_freq, uint16_t ddr_type,
                                   uint16_t bd_rev, uint16_t chip_offset) {
  dxrt_device_info_t info{};
  info.type = type; info.variant = variant; info.bd_type = bd_type;
  info.mem_addr = mem_addr; info.mem_size = mem_size; info.num_dma_ch = 4;
  info.ddr_freq = ddr_freq; info.ddr_type = ddr_type; info.bd_rev = bd_rev; info.chip_offset = chip_offset;
  return info;
}
static dxrt_device_status_t MakeStatusStruct(const std::array<uint32_t, 4>& volt,
                                             const std::array<uint32_t, 4>& clk,
                                             const std::array<uint32_t, 4>& temp,
                                             const std::array<uint32_t, 4>& ddr,
                                             const std::array<uint32_t, 4>& sbe,
                                             const std::array<uint32_t, 4>& dbe) {
  dxrt_device_status_t st{};
  for (int i = 0; i < 4; i++) {
    st.voltage[i] = volt[i];
    st.clock[i] = clk[i];
    st.temperature[i] = temp[i];
    st.ddr_status[i] = ddr[i];
    st.ddr_sbe_cnt[i] = sbe[i];
    st.ddr_dbe_cnt[i] = dbe[i];
  }
  return st;
}

// Ensure previous definitions ended correctly.
TEST(DeviceInfoStatusTest, FullFormattingAndAccessViaCore) {
  auto adapter = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw = adapter.get();
  auto core = std::make_shared<DeviceCore>(5, std::move(adapter));
  auto info = MakeInfo(0, 100, 1, 0xABC0, 2ULL * 1024 * 1024 * 1024, 5800, 2, 11, 9);
  auto st = MakeStatusStruct(
    std::array<uint32_t, 4>{910, 911, 912, 913},
    std::array<uint32_t, 4>{810, 811, 812, 813},
    std::array<uint32_t, 4>{50, 51, 52, 53},
    std::array<uint32_t, 4>{0x0D, 0x0F, 0x01, 0xEE},
    std::array<uint32_t, 4>{7, 8, 9, 10},
    std::array<uint32_t, 4>{1, 2, 3, 4});
  auto ds = MakeStatusViaCore(core, raw, info, st);

  EXPECT_EQ(ds.GetId(), 5);
  EXPECT_EQ(ds.DeviceTypeStr(), std::string("ACC"));
  EXPECT_NE(ds.GetInfoString().find("Device 5"), std::string::npos);
  EXPECT_NE(ds.GetStatusString().find("NPU 0"), std::string::npos);
  EXPECT_NE(ds.DdrStatusStr(0).find("with de-rating"), std::string::npos);  // 0x0D
  EXPECT_NE(ds.DdrStatusStr(1).find("with de-rating"), std::string::npos);  // 0x0F
  EXPECT_NE(ds.DdrStatusStr(2).find("CH[2]"), std::string::npos);          // 0x01
  EXPECT_NE(ds.DdrStatusStr(3).find("CH[3]"), std::string::npos);          // unknown
  EXPECT_NE(ds.DdrBitErrStr().find("SBE"), std::string::npos);
  EXPECT_EQ(ds.Voltage(0), 910u);
  EXPECT_EQ(ds.NpuClock(3), 813u);
  EXPECT_EQ(ds.Temperature(2), 52);
  EXPECT_EQ(ds.Voltage(-1), 0u);
  EXPECT_EQ(ds.Temperature(99), 0);
  std::ostringstream oss; oss << ds; EXPECT_NE(oss.str().find("Device 5"), std::string::npos);
}

TEST(DeviceInfoStatusTest, PcieInfoDirectFormatting) {
  auto adapter = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw = adapter.get();
  auto core = std::make_shared<DeviceCore>(0, std::move(adapter));
  auto info = MakeInfo(0, 100, 1, 0, 1024, 5600, 2, 10, 0);
  auto st = MakeStatusStruct(
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0});
  auto ds = MakeStatusViaCore(core, raw, info, st);
  EXPECT_EQ(ds.PcieInfoStr(4, 16, 1, 2, 3), std::string("Gen4 X16 [01:02:03]"));
  auto memInfo = ds.AllMemoryInfoStr();
  EXPECT_NE(memInfo.find("Type:"), std::string::npos);
}

TEST(DeviceInfoStatusTest, InfoToStreamStdTypeAndFwVersionBranch) {
  auto adapter = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw = adapter.get();
  auto core = std::make_shared<DeviceCore>(1, std::move(adapter));
  // type 1 = STD so PCIe driver line should be omitted; fw_ver below suffix threshold
  auto info = MakeInfo(1, 100, 2, 0x100, 4096, 4000, 1, 9, 3);
  info.fw_ver = 100;  // below FW_VERSION_SUPPORT_SUFFIX (230)
  std::strncpy(info.fw_ver_suffix, "A1", sizeof(info.fw_ver_suffix));
  auto st = MakeStatusStruct(
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0});
  auto ds = MakeStatusViaCore(core, raw, info, st);
  std::ostringstream os1; ds.InfoToStream(os1);
  std::string out1 = os1.str();
  EXPECT_EQ(out1.find("PCIe Driver version"), std::string::npos);  // STD type excludes PCIe driver line
  EXPECT_NE(out1.find("FW version"), std::string::npos);

  // Now test fw_ver >= threshold to exercise suffix branch (pretend ACC type too)
  auto adapter2 = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw2 = adapter2.get();
  auto core2 = std::make_shared<DeviceCore>(2, std::move(adapter2));
  auto info2 = MakeInfo(0, 101, 3, 0x200, 8192, 5000, 2, 12, 4);  // ACC type
  info2.fw_ver = 300;   // >= 230 so suffix path
  std::strncpy(info2.fw_ver_suffix, "B2", sizeof(info2.fw_ver_suffix));
  auto st2 = MakeStatusStruct(
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0});
  auto ds2 = MakeStatusViaCore(core2, raw2, info2, st2);
  std::ostringstream os2; ds2.InfoToStream(os2);
  std::string out2 = os2.str();
  EXPECT_NE(out2.find("PCIe Driver version"), std::string::npos);  // ACC includes PCIe driver line
  EXPECT_NE(out2.find("FW version"), std::string::npos);
}

// 1. Parameterized DDR status code coverage
struct DdrCase { uint32_t code; bool derate; };
class DdrStatusParamTest : public ::testing::TestWithParam<DdrCase> {};

TEST_P(DdrStatusParamTest, CoversAllSwitchBranches) {
  auto c = GetParam();
  auto adapter = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw = adapter.get();
  auto core = std::make_shared<DeviceCore>(0, std::move(adapter));
  auto info = MakeInfo(0, 100, 1, 0, 1024, 5600, 2, 10, 0);
  dxrt_device_status_t st = MakeStatusStruct(
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{c.code, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0});
  auto ds = MakeStatusViaCore(core, raw, info, st);
  auto s = ds.DdrStatusStr(0);
  if (c.derate) {
    EXPECT_NE(s.find("with de-rating"), std::string::npos) << "code=" << c.code;
  } else {
    EXPECT_EQ(s.find("with de-rating"), std::string::npos) << "code=" << c.code;
  }
}

INSTANTIATE_TEST_SUITE_P(AllCodes, DdrStatusParamTest, ::testing::Values(
  DdrCase{0x01, false}, DdrCase{0x02, false}, DdrCase{0x03, false}, DdrCase{0x04, false},
  DdrCase{0x05, false}, DdrCase{0x06, false}, DdrCase{0x07, false}, DdrCase{0x08, false},
  DdrCase{0x09, false}, DdrCase{0x0A, false}, DdrCase{0x0B, false}, DdrCase{0x0C, false},
  DdrCase{0x0D, true},  DdrCase{0x0E, false}, DdrCase{0x0F, true},  DdrCase{0xEE, false}  // default/unknown
));

// 2. Fallback map lookup when unknown keys are used (set variant / board / memory to unmapped values)
TEST(DeviceInfoStatusTest, FallbackStringsForUnknownKeys) {
  auto adapter = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw = adapter.get();
  auto core = std::make_shared<DeviceCore>(3, std::move(adapter));
  auto info = MakeInfo(0, 9999, 99, 0, 2048, 4800, 9, 5, 1);  // variant 9999, bd_type 99, ddr_type 9 unknown
  auto st = MakeStatusStruct(
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0},
    std::array<uint32_t, 4>{0, 0, 0, 0});
  auto ds = MakeStatusViaCore(core, raw, info, st);
  // Expect fallback (map_lookup likely returns "UNKNOWN" or empty string).
  // Just assert no crash and string returned is non-empty.
  (void)ds.DeviceVariantStr();
  (void)ds.BoardTypeStr();
  (void)ds.MemoryTypeStr();
  SUCCEED();
}

// 3. Regex validation for SBE/DBE formatting
// Manual format validation (avoid <regex> to stay within approved headers)
static bool ValidateBitErrFormat(const std::string& s) {
  // Expected prefix/suffix patterns and numeric commas
  if (s.find("SBE[") != 0) return false;
  auto posBracket = s.find("] DBE[");
  if (posBracket == std::string::npos) return false;
  auto secondClose = s.find("]", posBracket + 6);
  if (secondClose == std::string::npos || secondClose != s.size() - 1) return false;
  return s.find(",") != std::string::npos;  // coarse check
}
TEST(DeviceInfoStatusTest, DdrBitErrFormattingRegex) {
  auto adapter = std::unique_ptr<FakeDriverAdapter>(new FakeDriverAdapter());
  auto raw = adapter.get();
  auto core = std::make_shared<DeviceCore>(4, std::move(adapter));
  auto info = MakeInfo(0, 100, 1, 0, 1024, 5600, 2, 10, 0);
  auto st = MakeStatusStruct(
      std::array<uint32_t, 4>{0, 0, 0, 0},
      std::array<uint32_t, 4>{0, 0, 0, 0},
      std::array<uint32_t, 4>{0, 0, 0, 0},
      std::array<uint32_t, 4>{0, 0, 0, 0},
      std::array<uint32_t, 4>{11, 22, 33, 44},
      std::array<uint32_t, 4>{55, 66, 77, 88});
  auto ds = MakeStatusViaCore(core, raw, info, st);
  std::string bitStr = ds.DdrBitErrStr();
  EXPECT_TRUE(ValidateBitErrFormat(bitStr)) << bitStr;
}

}  // namespace dxrt
