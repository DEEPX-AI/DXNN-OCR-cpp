// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Additional unit tests to raise coverage for cli_internal.cpp commands.

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "dxrt/extern/cxxopts.hpp"

#include "dxrt/cli_internal.h"
#include "dxrt/device_core.h"
#include "dxrt/driver_adapter/driver_adapter.h"
#include "dxrt/filesys_support.h"
#include "dxrt/fw.h"

namespace dxrt {

// Minimal capturing DriverAdapter mock (not using gmock to avoid dependency here)
class CapturingAdapter : public DriverAdapter {
 public:
  int32_t IOControl(dxrt_cmd_t cmd, void* data, uint32_t, uint32_t sub_cmd) override {
    lastCmd = cmd;
    lastSubCmd = sub_cmd;
    lastData = data;
    ++ioctlCount;
    if (cmd == DXRT_CMD_WRITE_MEM) ++writeMemIoctlCount;
    return ioctlRet;
  }
  int32_t Write(const void*, uint32_t size) override {
    ++writeCount;
    lastWriteSize = size;
    return writeRet;
  }
  int32_t Read(void*, uint32_t) override { return 0; }
  void* MemoryMap(void*, size_t, off_t) override { return nullptr; }
  int32_t Poll() override { return 0; }
  int GetFd() const override { return -1; }
  std::string GetName() const override { return "capturing"; }

  dxrt_cmd_t lastCmd{};
  uint32_t lastSubCmd{};
  void* lastData{nullptr};
  int ioctlCount{0};
  int writeCount{0};
  int32_t ioctlRet{0};
  int32_t writeRet{0};
  uint32_t lastWriteSize{0};
  int writeMemIoctlCount{0};
};

struct CoreWithAdapter {
  std::shared_ptr<DeviceCore> core;
  CapturingAdapter* adapter;  // non-owning
};

static CoreWithAdapter MakeCore() {
  auto adapter = std::unique_ptr<CapturingAdapter>(new CapturingAdapter());
  auto raw = adapter.get();
  CoreWithAdapter cwa{ std::make_shared<DeviceCore>(0, std::move(adapter)), raw };
  return cwa;
}

class TestDevicePool : public DevicePool {
 public:
  explicit TestDevicePool(std::shared_ptr<DeviceCore> core) {
    _deviceCores.push_back(core);
    std::call_once(_coresFlag, [](){});
  }
  size_t GetDeviceCount() { return _deviceCores.size(); }
};

// Helper to build a minimal cxxopts::ParseResult with required options populated
static cxxopts::ParseResult Parse(const std::vector<std::string>& argsIn) {
  cxxopts::Options options("dxrt", "test");
  options.add_options()
    ("ddrtarget", "ddr", cxxopts::value<uint32_t>()->default_value("5600"))
    ("otp", "otp", cxxopts::value<std::string>()->default_value("GET"))
    ("setled", "led", cxxopts::value<uint32_t>()->default_value("1"))
    ("modelupload", "model", cxxopts::value<std::string>()->default_value("model_dir"))
    ("fct", "fct", cxxopts::value<uint32_t>()->default_value("0"))
    ("monitor_debug", "monitor", cxxopts::value<uint32_t>()->default_value("0"))
    ("monitor_debug_once", "once flag");
  // build argv
  std::vector<std::string> args = {"dxrt"};
  args.insert(args.end(), argsIn.begin(), argsIn.end());
  std::vector<char*> argv; argv.reserve(args.size());
  for (auto & s : args) argv.push_back(const_cast<char*>(s.c_str()));
  return options.parse(static_cast<int>(argv.size()), argv.data());
}

TEST(CLIInternalExtendedTest, DDRTargetCommand_SupportedTriggersIOCTL) {
  auto parsed = Parse({"--ddrtarget", "5800"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  DDRTargetCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_NE(out.find("Target LPDDR Frequency"), std::string::npos);
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_SET_DDR_FREQ);
}

TEST(CLIInternalExtendedTest, DDRTargetCommand_UnsupportedDoesNotTriggerIOCTL) {
  auto parsed = Parse({"--ddrtarget", "1234"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  DDRTargetCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_NE(out.find("Unsupported DDR Frequency"), std::string::npos);
  EXPECT_EQ(cap->ioctlCount, 0);
}

TEST(CLIInternalExtendedTest, OTPCommand_GetPath) {
  auto parsed = Parse({"--otp", "GET"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  OTPCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(out.empty());
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_GET_OTP);
}

TEST(CLIInternalExtendedTest, OTPCommand_SetSuccess) {
  // exactly 13 chars
  auto parsed = Parse({"--otp", "ABCDEFGHIJKLM"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  OTPCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  (void)testing::internal::GetCapturedStdout();
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_SET_OTP);
}

TEST(CLIInternalExtendedTest, OTPCommand_SetWrongLength) {
  auto parsed = Parse({"--otp", "SHORT"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  OTPCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_EQ(cap->ioctlCount, 0);  // no IOCTL executed due to validation failure
  EXPECT_NE(out.find("OTP value must be exactly"), std::string::npos);
}

TEST(CLIInternalExtendedTest, LedCommand_SetsValue) {
  auto parsed = Parse({"--setled", "7"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  LedCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  cmd.Run();
  SetTestDevicePool(nullptr);
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_SET_LED);
}

static void WriteFile(const std::string& path, const std::string& data) {
  std::ofstream ofs(path, std::ios::binary); ofs.write(data.data(), data.size());
}

TEST(CLIInternalExtendedTest, ModelUploadCommand_SuccessAllThree) {
  std::string dir = "tmp_model_ok";
  {
    int sys_rc = system(("mkdir -p " + dir).c_str());
    (void)sys_rc; // ignore return (silence warn_unused_result)
  }
  WriteFile(dir + "/rmap.bin", "RMAP");
  WriteFile(dir + "/weight.bin", "WEIGHT");
  WriteFile(dir + "/rmap.info.json", "{}");
  auto parsed = Parse({"--modelupload", dir});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  ModelUploadCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  (void)testing::internal::GetCapturedStdout();
  EXPECT_GE(cap->writeMemIoctlCount, 3);
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_UPLOAD_MODEL);
  {
    int sys_rc = system(("rm -rf " + dir).c_str());
    (void)sys_rc;
  }
}

TEST(CLIInternalExtendedTest, DISABLED_ModelUploadCommand_FailMissingRmap) {
  std::string dir = "tmp_model_fail";
  {
    int sys_rc = system(("mkdir -p " + dir).c_str());
    (void)sys_rc;
  }
  WriteFile(dir + "/weight.bin", "WEIGHT");
  WriteFile(dir + "/rmap.info.json", "{}");
  auto parsed = Parse({"--modelupload", dir});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  ModelUploadCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  EXPECT_DEATH(cmd.Run(), "failed to upload rmap.bin");
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_NE(out.find("failed to upload rmap.bin"), std::string::npos);
  EXPECT_EQ(cap->writeCount, 0);
  {
    int sys_rc = system(("rm -rf " + dir).c_str());
    (void)sys_rc;
  }
}

TEST(CLIInternalExtendedTest, StartTestCommand_InvokesInternalTestcase) {
  auto parsed = Parse({});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  StartTestCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  cmd.Run();
  SetTestDevicePool(nullptr);
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_INTERNAL_TESTCASE);
}

TEST(CLIInternalExtendedTest, FCTCommand_PrintsResult) {
  auto parsed = Parse({});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  FCTCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(out.empty());
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_GET_FCT_TESTCASE_RESULT);
}

TEST(CLIInternalExtendedTest, StartFCTTestCommand_RunsWithType) {
  auto parsed = Parse({"--fct", "2"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  StartFCTTestCommand cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  cmd.Run();
  SetTestDevicePool(nullptr);
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_CUSTOM);
  EXPECT_EQ(cap->lastSubCmd, DX_RUN_FCT_TESTCASE);
}

TEST(CLIInternalExtendedTest, DeviceMonitorDebug_OnceRunsSingleIteration) {
  // Expect exactly one status IOControl (DXRT_CMD_GET_STATUS)
  auto parsed = Parse({"--monitor_debug", "0", "--monitor_debug_once"});
  auto cwa = MakeCore();
  auto core = cwa.core; auto cap = cwa.adapter;
  DeviceMonitorDebug cmd(parsed);
  TestDevicePool pool(core);
  SetTestDevicePool(&pool);
  testing::internal::CaptureStdout();
  cmd.Run();
  SetTestDevicePool(nullptr);
  auto out = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(out.empty());
  EXPECT_EQ(cap->lastCmd, DXRT_CMD_GET_STATUS);
  EXPECT_EQ(cap->ioctlCount, 1) << "Should run only one iteration";
}

}  // namespace dxrt
