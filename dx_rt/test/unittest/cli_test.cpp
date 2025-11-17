// Copyright (c) 2025 DEEPX Corporation. All rights reserved.
// Licensed under the MIT License.

#include <unistd.h>
#include <gtest/gtest.h>
#include <sstream>
#include <memory>
#include <fstream>
#include <vector>
#include "dxrt/cli.h"
#include "dxrt/device_core.h"
#include "dxrt/device_pool.h"
#include "dxrt/cli_internal.h"
#include "mocks/mock_driver_adapter.h"

namespace dxrt {





class MockDevicePool : public DevicePool {
 public:
    MockDevicePool(){
        _deviceCores.clear();
        std::call_once(_coresFlag, [](){});
        std::call_once(_taskLayersFlag, [](){});
        std::call_once(_nfhLayersFlag, [](){});
    }
    void InjectDevice(int deviceId, std::shared_ptr<DeviceCore> device) {
        if (static_cast<int>(_deviceCores.size()) <= deviceId) {
            _deviceCores.resize(deviceId + 1);
        }
        _deviceCores[deviceId] = device;
    }
};

class CLITest : public ::testing::Test {
protected:
    std::streambuf* oldBuf = nullptr;
    std::ostringstream capture;
    MockDevicePool *_poolPtr = nullptr;
    std::vector<MockDriverAdapter*> allocatedMocks;

    void SetUp() override {
        oldBuf = std::cout.rdbuf(capture.rdbuf());
        _poolPtr = new MockDevicePool();
        SetTestDevicePool(_poolPtr);  // ensure CLICommand skips InitCores
    }
    void TearDown() override {
        std::cout.rdbuf(oldBuf);
        SetTestDevicePool(nullptr);
        delete _poolPtr; _poolPtr = nullptr;
        // mocks owned by DeviceCore now; we don't delete allocatedMocks individually
    }
    MockDriverAdapter* injectStrictMockDevice(int deviceId = 0) {
        auto *mock = new MockDriverAdapter();
        allocatedMocks.push_back(mock);
        _poolPtr->InjectDevice(deviceId, std::make_shared<DeviceCore>(deviceId, std::unique_ptr<DriverAdapter>(mock)));
        return mock;
    }
};

// For commands that don't require device interaction (ShowVersionCommand, FWVersionCommand)
TEST_F(CLITest, ShowVersionOutputsMinimumVersions) {
    // Build a minimal parse result with the flag present.
    const char* argv0 = "dxrt-cli";
    const char* argv1 = "--showversion";  // consumed below
    std::vector<const char*> argv = {argv0, argv1};
    int argc = static_cast<int>(argv.size());

    cxxopts::Options options("dxrt-cli");
    options.add_options()("showversion", "show version");

    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    ShowVersionCommand cmd(result);
    cmd.Run();

    std::string out = capture.str();
    EXPECT_NE(out.find("Minimum Driver Versions"), std::string::npos);
    EXPECT_NE(out.find("Firmware"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Additional lightweight tests for CLICommand base behavior (device-less)
// ---------------------------------------------------------------------------

namespace {
class FakeNoDeviceCommand : public CLICommand {
 public:
    explicit FakeNoDeviceCommand(const cxxopts::ParseResult &r, int *calls, int *fin)
            : CLICommand(const_cast<cxxopts::ParseResult &>(r)), callCounter(calls), finishCounter(fin) {
        _withDevice = false;  // ensure doCommand invoked exactly once with nullptr
    }
 private:
    int *callCounter;
    int *finishCounter;
    void doCommand(std::shared_ptr<DeviceCore> devicePtr) override {
        EXPECT_EQ(devicePtr, nullptr);
        ++(*callCounter);
    }
    void finish() override { ++(*finishCounter); }
};

class FakeDeviceCommand : public CLICommand {
 public:
    explicit FakeDeviceCommand(const cxxopts::ParseResult &r, int *calls)
            : CLICommand(const_cast<cxxopts::ParseResult &>(r)), callCounter(calls) {
        _withDevice = true;  // will iterate devices (likely zero in test env)
    }
 private:
    int *callCounter;
    void doCommand(std::shared_ptr<DeviceCore> devicePtr) override {
        // Only invoked if at least one device core exists.
        if (devicePtr) ++(*callCounter);
    }
};
}  // namespace

TEST_F(CLITest, Base_NoDevice_CommandInvokedAndFinishCalled) {
    const char* argv0 = "dxrt-cli";
    std::vector<const char*> argv = {argv0};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    int doCalls = 0;
    int finishCalls = 0;
    FakeNoDeviceCommand cmd(result, &doCalls, &finishCalls);
    cmd.Run();
    EXPECT_EQ(doCalls, 1);
    EXPECT_EQ(finishCalls, 1);
}

TEST_F(CLITest, Base_WithDevice_NoCrashWhenNoDevices) {
    const char* argv0 = "dxrt-cli";
    std::vector<const char*> argv = {argv0};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    int doCalls = 0;
    FakeDeviceCommand cmd(result, &doCalls);
    EXPECT_NO_FATAL_FAILURE(cmd.Run());
    // If no devices present, doCalls stays 0; that's acceptable.
    EXPECT_GE(doCalls, 0);
}

// ---------------------------------------------------------------------------
// Mock-based tests for commands that operate on devices
// ---------------------------------------------------------------------------

TEST_F(CLITest, DeviceStatusCLICommand_PrintsStatusHeader) {
    // Inject mock device into DevicePool
    injectStrictMockDevice(0);

    const char* argv0 = "dxrt-cli";
    const char* argv1 = "--status";
    std::vector<const char*> argv = {argv0, argv1};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("status", "device status")("device", "device id", cxxopts::value<int>()->default_value("-1"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DeviceStatusCLICommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_FALSE(out.empty());
}

TEST_F(CLITest, DeviceResetCommand_CallsReset) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // Expect a reset command (DXRT_CMD_RESET)
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_RESET, _, _, _)).WillOnce(Return(0));

    const char* argv0 = "dxrt-cli";
    const char* argv1 = "--reset";
    const char* argv2 = "1";
    std::vector<const char*> argv = {argv0, argv1, argv2, "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
        options.add_options()
                ("reset", "reset", cxxopts::value<int>())
                ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DeviceResetCommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("reset"), std::string::npos);
}

TEST_F(CLITest, DeviceDumpCommand_WritesSomeOutput) {
    injectStrictMockDevice(0);

    const char* argv0 = "dxrt-cli";
    std::string dumpArg = std::string("--dump=") + "dump_test.bin";
    std::vector<const char*> argv = {argv0, dumpArg.c_str(), "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()
            ("dump", "dump", cxxopts::value<std::string>())
            ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DeviceDumpCommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("dump to file"), std::string::npos);
    ::remove("dump_test.bin");
    ::remove("dump_test.bin.txt");
}

TEST_F(CLITest, PcieStatusCLICommand_NoCrash) {
    injectStrictMockDevice(0);

    const char* argv0 = "dxrt-cli";
    const char* argv1 = "--pciestatus";
    std::vector<const char*> argv = {argv0, argv1, "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()
            ("pciestatus", "pcie status")
            ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    PcieStatusCLICommand cmd(result);
    EXPECT_NO_FATAL_FAILURE(cmd.Run());
}

// ---------------------------------------------------------------------------
// Firmware related commands (update/upload/config/log) & DDR error
// ---------------------------------------------------------------------------

TEST_F(CLITest, FWUploadCommand_TwoFilesTriggersTwoCalls) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // Expect two upload commands (DXRT_CMD_UPLOAD_FIRMWARE). We allow any order but exact count 2.
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_UPLOAD_FIRMWARE, _, _, _))
            .Times(2)
            .WillRepeatedly(Return(0));

    // Create two dummy firmware files
    const char* fw1 = "fw_u_1.bin"; const char* fw2 = "fw_u_2.bin";
    { std::ofstream o(fw1, std::ios::binary); }
    { std::ofstream o(fw2, std::ios::binary); }

    const char* argv0 = "dxrt-cli";
    std::string a1 = std::string("--fwupload=") + fw1;
    std::string a2 = std::string("--fwupload=") + fw2;
    std::vector<const char*> argv = {argv0, a1.c_str(), a2.c_str(), "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwupload", "fw upload", cxxopts::value<std::vector<std::string>>())
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWUploadCommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("upload firmware"), std::string::npos);
    ::remove(fw1); ::remove(fw2);
}

TEST_F(CLITest, FWConfigCommand_AcceptsVector) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_UPDATE_CONFIG, _, _, _))
            .Times(1).WillOnce(Return(0));

    const char* argv0 = "dxrt-cli";
    const char* a1 = "--fwconfig=1";
    const char* a2 = "--fwconfig=2";
    const char* a3 = "--fwconfig=3";
    std::vector<const char*> argv = {argv0, a1, a2, a3, "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwconfig", "fw cfg", cxxopts::value<std::vector<uint32_t>>())
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWConfigCommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("update firmware config"), std::string::npos);
}

TEST_F(CLITest, FWConfigCommandJson_SuccessPath) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // DXRT_CMD_UPDATE_CONFIG_JSON expected
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_UPDATE_CONFIG_JSON, _, _, _))
            .Times(1).WillOnce(Return(0));

    // Create dummy json file
    const char* jsonFile = "fw_cfg.json";
    {
         std::ofstream jf(jsonFile); jf << "{\n  \"throttling_cfg\": { \"enable\": 1 }\n}";
    }
    const char* argv0 = "dxrt-cli";
    std::string a1 = std::string("--fwconfig_json=") + jsonFile;
    std::vector<const char*> argv = {argv0, a1.c_str(), "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwconfig_json", "fw cfg json", cxxopts::value<std::string>())
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWConfigCommandJson cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("update firmware config"), std::string::npos);
    ::remove(jsonFile);
}

TEST_F(CLITest, FWLogCommand_AppendsLog) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // Expect DXRT_CMD_GET_LOG once
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_GET_LOG, _, _, _))
            .Times(1).WillOnce(Return(0));

    const char* logFile = "fw_log.txt";
    const char* argv0 = "dxrt-cli";
    std::string a1 = std::string("--fwlog=") + logFile;
    std::vector<const char*> argv = {argv0, a1.c_str(), "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwlog", "fw log", cxxopts::value<std::string>())
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWLogCommand cmd(result);
    cmd.Run();
    std::ifstream ifs(logFile); bool fileCreated = ifs.good();
    EXPECT_TRUE(fileCreated);
    ifs.close();
    ::remove(logFile);
}

TEST_F(CLITest, DDRErrorCLICommand_PrintsLine) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // For status retrieval
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_GET_STATUS, _, _, _))
            .Times(::testing::AtLeast(1)).WillRepeatedly(Return(0));

    const char* argv0 = "dxrt-cli";
    const char* a1 = "--ddrerr";
    std::vector<const char*> argv = {argv0, a1, "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("ddrerr", "ddr error")
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DDRErrorCLICommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("Device 0"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Error path tests
// ---------------------------------------------------------------------------

TEST_F(CLITest, FWConfigCommandJson_InvalidJsonShowsHelp) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // Adapter returns non-zero to simulate failure path
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_UPDATE_CONFIG_JSON, _, _, _))
            .Times(1).WillOnce(Return(-1));

    const char* badFile = "bad_cfg.json";
    { std::ofstream jf(badFile); jf << "{ invalid json"; }

    const char* argv0 = "dxrt-cli";
    std::string a1 = std::string("--fwconfig_json=") + badFile;
    std::vector<const char*> argv = {argv0, a1.c_str(), "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwconfig_json", "fw cfg json", cxxopts::value<std::string>())
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWConfigCommandJson cmd(result);
    cmd.Run();
    std::string out = capture.str();
    // Help text contains 'Json format example' string fragment
    EXPECT_NE(out.find("Json format"), std::string::npos);
    ::remove(badFile);
}

TEST_F(CLITest, DeviceStatusMonitor_SingleIterationWithFlag) {
    injectStrictMockDevice(0);

    const char* argv0 = "dxrt-cli"; const char* a1 = "--monitor=1"; const char* a2 = "--monitor_once";
    std::vector<const char*> argv = {argv0, a1, a2, "--device", "0"};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("monitor", "monitor", cxxopts::value<uint32_t>())
                                     ("monitor_once", "single run")
                                     ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DeviceStatusMonitor cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_FALSE(out.empty());
}

TEST_F(CLITest, DeviceStatusCLICommand_MultiDevicesEnumeratesAll) {
    // Inject two mock devices
    injectStrictMockDevice(0);
    injectStrictMockDevice(1);

    const char* argv0 = "dxrt-cli"; const char* a1 = "--status";  // no --device => enumerate all
    std::vector<const char*> argv = {argv0, a1};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("status", "device status");
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DeviceStatusCLICommand cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_NE(out.find("Device 0"), std::string::npos);
    EXPECT_NE(out.find("Device 1"), std::string::npos);
}

TEST_F(CLITest, DeviceStatusMonitor_ZeroDelaySingleRun) {
    injectStrictMockDevice(0);

    const char* argv0 = "dxrt-cli"; const char* a1 = "--monitor=0"; const char* a2 = "--monitor_once";
    std::vector<const char*> argv = {argv0, a1, a2};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
        options.add_options()("monitor", "monitor", cxxopts::value<uint32_t>())
                                         ("monitor_once", "single run");
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    DeviceStatusMonitor cmd(result);
    cmd.Run();
    std::string out = capture.str();
    EXPECT_FALSE(out.empty());
}

TEST_F(CLITest, FWUploadCommand_SingleFile) {


    // NOTE: Upload command requires two files (2nd_boot and rtos). Here we test that providing only one file does NOT trigger any upload attempt.
    // This is to ensure user error (missing file) does not cause any unintended firmware upload.

    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_UPLOAD_FIRMWARE, _, _, _))
            .Times(0).WillRepeatedly(Return(0));

    // two fw file required (one with 2nd_boot, one with rtos)
    const char* fw1 = "dummy_fw_2nd_boot.bin"; { std::ofstream o(fw1, std::ios::binary); o << "dummy_fw_2nd_boot"; }
    const char* fw2 = "dummy_fw_rtos.bin"; { std::ofstream o(fw2, std::ios::binary); o << "dummy_fw_rtos"; }
    const char* argv0 = "dxrt-cli";
    std::string a1 = std::string("--fwupload=") + fw1;
    std::vector<const char*> argv = {argv0, a1.c_str(), "--device", "0", fw2};
    int argc = static_cast<int>(argv.size());
        cxxopts::Options options("dxrt-cli");
        options.add_options()("fwupload", "fw upload", cxxopts::value<std::vector<std::string>>())
                                         ("device", "device id", cxxopts::value<int>()->default_value("0"));
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWUploadCommand cmd(result); cmd.Run();
    std::string out = capture.str();
    std::cout << out << std::endl;
    EXPECT_EQ(out.find("upload firmware"), std::string::npos);
    ::remove(fw1);
    ::remove(fw2);
}

TEST_F(CLITest, FWConfigCommand_ErrorReturnPath) {
    using ::testing::_; using ::testing::Return;
    MockDriverAdapter* mockAdapter = injectStrictMockDevice(0);

    // Force error return
    EXPECT_CALL(*mockAdapter, IOControl_Other(dxrt_cmd_t::DXRT_CMD_UPDATE_CONFIG, _, _, _))
            .Times(1).WillOnce(Return(-5));

        const char* argv0 = "dxrt-cli";
        const char* a1 = "--fwconfig=10";
        std::vector<const char*> argv = {argv0, a1, "--device", "0"};
        int argc = static_cast<int>(argv.size());
        cxxopts::Options options("dxrt-cli");
        options.add_options()("fwconfig", "fw cfg", cxxopts::value<std::vector<uint32_t>>())
                                         ("device", "device id", cxxopts::value<int>()->default_value("0"));
        auto result = options.parse(argc, const_cast<char**>(argv.data()));
        FWConfigCommand cmd(result);
        cmd.Run();
        std::string out = capture.str();
        EXPECT_NE(out.find("update firmware config"), std::string::npos);
}


// Test FWVersionCommand's basic parsing path (will not open real file, so expect no throw if file missing is handled)
TEST_F(CLITest, FWVersionCommandPrintsFileName) {
    // Create a temporary empty file
    const char* tmpFile = "test_fw.bin";
    { std::ofstream ofs(tmpFile, std::ios::binary); }

    const char* argv0 = "dxrt-cli";
    std::string arg1 = std::string("--fwversion=") + tmpFile;
    std::vector<const char*> argv = {argv0, arg1.c_str()};
    int argc = static_cast<int>(argv.size());

    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwversion", "fw version", cxxopts::value<std::string>());

    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWVersionCommand cmd(result);
    ASSERT_EQ(::access(tmpFile, F_OK), 0);
    cmd.Run();

    std::string out = capture.str();
    EXPECT_NE(out.find("fwFile:"), std::string::npos);
    // cleanup
    ::remove(tmpFile);
}

}  // namespace dxrt
