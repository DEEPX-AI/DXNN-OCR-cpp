/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 * 
 * This file uses cxxopts (MIT License) - Copyright (c) 2014 Jarryd Beck.
 */

#ifdef __linux__
#include <getopt.h>
#endif
#include <iostream>
#include <vector>
#include "dxrt/dxrt_api.h"
#include "dxrt/cli.h"
#include "dxrt/cli_internal.h"
#include "dxrt/extern/cxxopts.hpp"
#include "dxrt/exception/exception.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

int main(int argc, char *argv[])
{
    cxxopts::Options options("dxrt-cli", "DXRT " DXRT_VERSION " CLI");
    options.add_options()
        ("s, status", "Get device status")
        ("i, info", "Get device info")
        ("m, monitor_debug", "Monitoring device status every [arg] seconds", cxxopts::value<uint32_t>() )
        ("r, reset", "Reset device(0: reset only NPU, 1: reset entire device)", cxxopts::value<int>()->default_value("0"))
        ("d, device", "Device ID (if not specified, CLI commands will be sent to all devices.)", cxxopts::value<int>()->default_value("-1"))
        ("u, fwupdate", "Update firmware with deepx firmware file.\nsub-option : [force:force update, unreset:device unreset(default:reset)]", cxxopts::value<std::vector<std::string>>())
        ("w, fwupload", "Upload firmware with deepx firmware file.[2nd_boot/rtos]", cxxopts::value<std::vector<std::string>>() )
        ("g, fwversion", "Get firmware version with deepx firmware file", cxxopts::value<string>())
        ("p, dump", "Dump device internals to a file", cxxopts::value<string>() )
        ("c, fwconfig", "Update firmware settings from list of parameters", cxxopts::value<vector<uint32_t>>() )
        ("t, ddrtarget", "Update firmware ddr target freqeuncy", cxxopts::value<uint32_t>() )
        ("o, otp", "Handling OTP data. Usage:\n"
            "  --otp GET         (fetch OTP data)\n"
            "  --otp <value>     (set OTP data with <value>)",
            cxxopts::value<std::string>())
        ("l, fwlog", "Extract firmware logs to a file", cxxopts::value<string>() )
        ("f, setled", "Set led if bitmatch failed (value: 0~7)\n"
            "  if bitmatch failed -> use value 6 (red)\n",
            cxxopts::value<uint32_t>() )
        ("U, modelupload", "Upload model(rmap, weight, rmap_info)", cxxopts::value<string>() )
        ("S, starttest", "Start Internal Testcase")
        ("errorstat", "show internal error status")
        ("ddrerror", "show ddr error count")
        ("fct", "FCT (Factory Test) - run test with type (1: full test, 2: simple test)", cxxopts::value<uint32_t>())
        ("fct-result", "Get FCT test results")
        ("h, help", "Print usage");

    try
    {

        auto cmd = options.parse(argc, argv);
        if (cmd.count("help"))
        {
            cout << "DXRT " DXRT_VERSION << endl;
            cout << options.help() << endl;
            exit(0);
        }
        else if (cmd.count("status"))
        {
            dxrt::DeviceStatusCLICommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("info"))
        {
            dxrt::DeviceInfoCLICommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("monitor_debug"))
        {
            dxrt::DeviceMonitorDebug cli(cmd);
            cli.Run();
        }
        else if (cmd.count("reset"))
        {
            dxrt::DeviceResetCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("fwupdate"))
        {
            dxrt::FWUpdateCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("fwupload"))
        {
            dxrt::FWUploadCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("fwversion"))
        {
            dxrt::FWVersionCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("dump"))
        {
            dxrt::DeviceDumpCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("fwconfig"))
        {
            dxrt::FWConfigCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("ddrtarget"))
        {
            dxrt::DDRTargetCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("fwlog"))
        {
            dxrt::FWLogCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("otp"))
        {
            dxrt::OTPCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("setled"))
        {
            dxrt::LedCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("modelupload"))
        {
            dxrt::ModelUploadCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("starttest"))
        {
            dxrt::StartTestCommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("errorstat"))
        {
            dxrt::PcieStatusCLICommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("ddrerror"))
        {
            dxrt::DDRErrorCLICommand cli(cmd);
            cli.Run();
        }
        else if (cmd.count("fct"))
        {
            uint32_t fct_type = cmd["fct"].as<uint32_t>();
            if (fct_type == 1 || fct_type == 2)
            {
                dxrt::StartFCTTestCommand cli(cmd);
                cli.Run();
            }
            else
            {
                std::cerr << "Invalid FCT test type: " << fct_type << std::endl;
                std::cerr << "Valid types: 1 (full test), 2 (simple test)" << std::endl;
                return 1;
            }
        }
        else if (cmd.count("fct-result"))
        {
            dxrt::FCTCommand cli(cmd);
            cli.Run();
        }

        return 0;
    }
    catch(cxxopts::exceptions::exception& e)
    {
        cout << e.what() << endl;
    }
    catch(const dxrt::Exception& e)
    {
        cout << e.what() << endl;
    }
    catch(const std::exception& e)
    {
        cout << e.what() << endl;
    }
    

    return 1;
}