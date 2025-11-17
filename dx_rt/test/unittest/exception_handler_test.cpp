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
#include "dxrt/device.h"
#include "dxrt_test.h"
#include <csignal>
#include <cstdlib>
#include <cstdio>
#ifdef __linux__
    #include <execinfo.h>
#elif _WIN32
    #include <Windows.h>
    #include <DbgHelp.h>
    #pragma comment(lib, "DbgHelp.lib")
#endif

using namespace std;
using namespace dxrt;

#ifdef _WIN32
void PrintStackTrace() {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    SymInitialize(process, NULL, TRUE);

    STACKFRAME64 frame;
    memset(&frame, 0, sizeof(STACKFRAME64));
#ifdef _M_IX86
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    frame.AddrPC.Offset = context.StIIP;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.IntSp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrBStore.Offset = context.RsBSP;
    frame.AddrBStore.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.IntSp;
    frame.AddrStack.Mode = AddrModeFlat;
#endif

    for (ULONG frame_number = 0; ; frame_number++) {
        BOOL result = StackWalk64(
#ifdef _M_IX86
            IMAGE_FILE_MACHINE_I386,
#elif _M_X64
            IMAGE_FILE_MACHINE_AMD64,
#elif _M_IA64
            IMAGE_FILE_MACHINE_IA64,
#endif
            process,
            thread,
            &frame,
            &context,
            NULL,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            NULL
        );

        if (!result) break;

        printf("Frame %lu\n", frame_number);
        printf("  PC = 0x%016llX\n", frame.AddrPC.Offset);
        printf("  Frame = 0x%016llX\n", frame.AddrFrame.Offset);
        printf("  Stack = 0x%016llX\n", frame.AddrStack.Offset);
    }

    SymCleanup(process);
}
#else
void PrintStackTrace() {
    void* array[10];
    size_t size;
    char** strings;
    size_t i;

    size = backtrace(array, 10);
    strings = backtrace_symbols(array, size);

    printf("Obtained %zd stack frames.\n", size);

    for (i = 0; i < size; i++)
        printf("%s\n", strings[i]);

    free(strings);
}
#endif

void FaultFunction()
{
    int *ptr = nullptr;
    *ptr = 1;
}

TEST(exception_handler, basic)
{
    // inject segmentation fault
    try {
        //FaultFunction();
    }
    catch (...) {
        PrintStackTrace();
        throw;
    }
}
