/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers
 * who are supplied with DEEPX NPU (Neural Processing Unit).
 * Unauthorized sharing or usage is strictly prohibited by law.
 *
 * This file uses Google Test (BSD 3-Clause License) - Copyright 2008, Google Inc.
 */

#include "gtest/gtest.h"
#include "dxrt/util.h"

using namespace std;
string testModelPath;
string thisExecPath;
int testNum = 1;
const char *usage =
    "DX-RT Test\n"
    "  -m    Test model path\n"
    "  -n    argument number to use in testcases\n"
    "  -h    Show help\n";
void help()
{
    cout << usage << endl;
}

static std::string GetDirNameFromPath(std::string p) {
    if (p.empty()) return ".";
    // 뒤에 붙은 '/' 또는 '\\' 제거 (디렉터리 끝에 슬래시가 올 수 있음)
    while (p.size() > 1 && (p.back() == '/' || p.back() == '\\')) {
        p.pop_back();
    }
    // 마지막 구분자 찾기
    size_t pos = p.find_last_of("/\\");
    if (pos == std::string::npos) {
        // 경로 구분자가 없으면 현재 디렉터리에서 실행된 것 (ex: "dxrt_test")
        return ".";
    }
    if (pos == 0) {
        // 루트(/something)에서 something 잘린 경우 -> "/"
        return p.substr(0, 1);
    }
    return p.substr(0, pos);
}



int main(int argc, char *argv[])
{
    thisExecPath = GetDirNameFromPath(argv[0]);
    int i = 1;
    while (i < argc){
        std::string arg(argv[i++]);
        if(arg=="-m")
        {
            testModelPath = strdup(argv[i++]);
        }
        else if(arg=="-n")
        {
            testNum = stoi(argv[i++]);
        }
        else if(arg=="-h" || arg=="--help" )
        {
            help();
            return -1;
        }
    }
    ::testing::GTEST_FLAG(output) = "xml:dxrt_test_result.xml";
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/*
 * Usage
list test suite : .\dxrt_test.exe --gtest_list_tests
list test class : .\dxrt_test.exe --gtest_list_tests | find "."
include/exclude class : --gtest_filter=POSTIVE_PATTERNS[-NEGATIVE_PATTERNS]
.\dxrt_test.exe --gtest_filter=buffer.* ; only buffer class
.\dxrt_test.exe --gtest_filter=-device.*:-driver.*:-ie.*:-request.* ; exclude device,driver,ie,request

.\dxrt_test.exe --gtest_filter=buffer.*
.\dxrt_test.exe --gtest_filter=circular_buffer.*
.\dxrt_test.exe --gtest_filter=datatype.*
.\dxrt_test.exe --gtest_filter=device.*
.\dxrt_test.exe --gtest_filter=driver.*
.\dxrt_test.exe --gtest_filter=exception_handler.*
.\dxrt_test.exe --gtest_filter=memory.*
.\dxrt_test.exe --gtest_filter=shared_ptr.*
.\dxrt_test.exe --gtest_filter=gtest.*
.\dxrt_test.exe --gtest_filter=ie.*
.\dxrt_test.exe --gtest_filter=memory.*
.\dxrt_test.exe --gtest_filter=rmap_info.*
.\dxrt_test.exe --gtest_filter=tensor.*
.\dxrt_test.exe --gtest_filter=util.*
 *
 */
