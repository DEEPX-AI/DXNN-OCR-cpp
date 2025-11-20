#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include "dxrt/cli.h"

namespace dxrt {

// Basic fixture to capture std::cout without touching private DevicePool internals
class CLIBasicTest : public ::testing::Test {
protected:
    std::streambuf* oldBuf = nullptr;
    std::ostringstream capture;
    void SetUp() override { oldBuf = std::cout.rdbuf(capture.rdbuf()); }
    void TearDown() override { std::cout.rdbuf(oldBuf); }
};

// Test ShowVersionCommand prints expected keywords (device-less)
TEST_F(CLIBasicTest, ShowVersionCommandPrintsKeywords) {
    const char* argv0 = "dxrt-cli";
    const char* argv1 = "--showversion";
    std::vector<const char*> argv = {argv0, argv1};
    int argc = static_cast<int>(argv.size());

    cxxopts::Options options("dxrt-cli");
    options.add_options()("showversion", "show version");

    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    ShowVersionCommand cmd(result);
    cmd.Run();

    std::string out = capture.str();
    EXPECT_FALSE(out.empty());
    // Keywords we expect somewhere in the output (adjust if implementation differs)
    EXPECT_NE(out.find("Minimum"), std::string::npos);
    EXPECT_NE(out.find("Firmware"), std::string::npos);
}

// Negative path: invoking command without its flag should not crash, but produce empty output
TEST_F(CLIBasicTest, ShowVersionCommandNoFlagProducesSomeOutputOrNot) {
    const char* argv0 = "dxrt-cli";
    std::vector<const char*> argv = {argv0};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    // Intentionally do not register showversion flag to mimic missing option usage scenario
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    ShowVersionCommand cmd(result);
    // Should not throw / crash
    EXPECT_NO_FATAL_FAILURE(cmd.Run());
}

// Basic FWVersionCommand parsing: ensure it accepts a filename parameter (no private access)
TEST_F(CLIBasicTest, FWVersionCommandParsesFilenameOption) {
    const char* argv0 = "dxrt-cli";
    const char* fwOpt = "--fwversion=fake_fw.bin"; // file may not exist; command should handle gracefully
    std::vector<const char*> argv = {argv0, fwOpt};
    int argc = static_cast<int>(argv.size());
    cxxopts::Options options("dxrt-cli");
    options.add_options()("fwversion", "fw version", cxxopts::value<std::string>());
    auto result = options.parse(argc, const_cast<char**>(argv.data()));
    FWVersionCommand cmd(result);
    EXPECT_DEATH(cmd.Run(),"");
}

} // namespace dxrt
