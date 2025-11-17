/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers
 * who are supplied with DEEPX NPU (Neural Processing Unit).
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#include "gtest/gtest.h"
#include "dxrt/driver.h"
#include "dxrt/device.h"
#include "dxrt_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef __linux__
    #include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#ifdef __linux__
    #include <sys/mman.h>
    #include <sys/ioctl.h>
#endif
#include <sys/types.h>

using namespace std;
using namespace dxrt;
#ifdef __linux__
#define DEVICE_FILENAME "/dev/dxrt"
#elif _WIN32
#define DEVICE_FILENAME "\\\\.\\dxrt"
#endif

// #define NUM_DEVICES 4
#define NUM_DEVICES 1

#ifdef __linux__
int openDevice(const char *file)
{
    int fd;
    fd = open(file, O_RDWR);
    if(fd<0)
    {
        cout << "Error: Can't open " << file << endl;
    }
    return fd;
}
#elif _WIN32
HANDLE openDevice(const char* file)
{
    HANDLE hFile = CreateFile(file,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        cout << "Error: Can't open " << file << ". Error code: " << GetLastError() << endl;
    }
    return hFile;
}
#endif

TEST(driver, open)
{
#ifdef __linux__
    int fd[NUM_DEVICES];
#elif _WIN32
    HANDLE hFile[NUM_DEVICES];
#endif
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd[dev] = openDevice(devName.c_str());
        ASSERT_GT(fd[dev], 0);
        close(fd[dev]);
#elif _WIN32
        hFile[dev] = openDevice(devName.c_str());
        ASSERT_NE(hFile[dev], INVALID_HANDLE_VALUE);
        CloseHandle(hFile[dev]);
#endif
    }
}/*
TEST(driver, read)
{
#ifdef __linux__
    int fd[NUM_DEVICES];
#elif _WIN32
    HANDLE hFile[NUM_DEVICES];
#endif
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd[dev] = openDevice(devName.c_str());
        ASSERT_GT(fd[dev], 0);
#elif _WIN32
        hFile[dev] = CreateFile(devName.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        ASSERT_NE(hFile[dev], INVALID_HANDLE_VALUE);
#endif
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
        int response = 0xffaa;
#ifdef __linux__
        ssize_t bytes_read = read(fd[dev], &response, sizeof(int));
        // EXPECT_EQ(bytes_read, sizeof(response));
        EXPECT_GT(bytes_read, -1);
#elif _WIN32
        DWORD bytes_read;
        BOOL read_result = ReadFile(hFile[dev], &response, sizeof(int), &bytes_read, NULL);
        EXPECT_TRUE(read_result);
#endif
        LOG_VALUE(bytes_read);
        if(bytes_read==sizeof(int))
        {
            cout << "Response with id: " << response << endl;
        }
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
#ifdef __linux__
        close(fd[dev]);
#elif _WIN32
        CloseHandle(hFile[dev]);
#endif
    }
}
TEST(driver, write)
{
#ifdef __linux__
    int fd[NUM_DEVICES];
#elif _WIN32
    HANDLE hFile[NUM_DEVICES];
#endif
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd[dev] = openDevice(devName.c_str());
        EXPECT_GT(fd[dev], 0);
#elif _WIN32
        hFile[dev] = openDevice(devName.c_str());
        EXPECT_NE(hFile[dev], INVALID_HANDLE_VALUE);
#endif
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
        dxrt_request_t inference;
        fillStructIncreasingValues(inference);
        EXPECT_EQ(*(reinterpret_cast<uint64_t*>(&inference)), 0x706050403020100 );
        cout << inference << endl;
#ifdef __linux__
        EXPECT_EQ(write(fd[dev], &inference, sizeof(inference)), sizeof(inference));
#elif _WIN32
        DWORD bytesWritten;
        BOOL writeResult = WriteFile(hFile[dev], &inference, sizeof(inference), &bytesWritten, NULL);
        EXPECT_TRUE(writeResult);
        EXPECT_EQ(bytesWritten, sizeof(inference));
#endif
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
#ifdef __linux__
        close(fd[dev]);
#elif _WIN32
        CloseHandle(hFile[dev]);
#endif
    }
}
TEST(driver, mmap)
{
    auto& devices = dxrt::CheckDevices();
    int numDevices = devices.size();
    LOG_VALUE(numDevices);
    EXPECT_GT(numDevices, 0);
    int size = 16*1024*1024;
#ifdef __linux__
    vector<int> fd;
#elif _WIN32
    vector<HANDLE> hFile;
#endif
    cout << "mmap size: " << size << endl;
    for(int dev=0; dev<numDevices; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd.emplace_back(openDevice(devName.c_str()));
        EXPECT_GT(fd.back(), 0);
#elif _WIN32
        HANDLE handle = openDevice(devName.c_str());
        hFile.push_back(handle);
        EXPECT_NE(hFile.back(), INVALID_HANDLE_VALUE);
#endif
    }
    for(int dev=0; dev<numDevices; dev++)
    {
        void *buf;
#ifdef __linux__
        buf = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd[dev], 0);
#elif _WIN32
        HANDLE hFileMapping = CreateFileMapping(hFile[dev], NULL, PAGE_READWRITE, 0, size, NULL);
        if (hFileMapping != NULL) {
            buf = MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
            CloseHandle(hFileMapping);
        }
#endif
        if(devices[dev]->info().type==1)
        {
#ifdef __linux__
            EXPECT_NE(reinterpret_cast<int64_t>(buf), -1);
#elif _WIN32
            EXPECT_NE(buf, nullptr);
#endif
            LOG_VALUE_HEX(buf);
        }
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
#ifdef __linux__
        close(fd[dev]);
#elif _WIN32
        CloseHandle(hFile[dev]);
#endif
    }
}
TEST(driver, poll)
{
    int numDevices = 1; // devices.size();
    LOG_VALUE(numDevices);
    EXPECT_GT(numDevices, 0);
#ifdef __linux__
    vector<int> fd;
#elif _WIN32
    vector<HANDLE> hFile;
#endif
    for(int dev=0; dev<numDevices; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd.emplace_back(openDevice(devName.c_str()));
        EXPECT_GT(fd.back(), 0);
#elif _WIN32
        HANDLE handle = openDevice(devName.c_str());
        hFile.push_back(handle);
        EXPECT_NE(hFile.back(), INVALID_HANDLE_VALUE);
#endif
    }
    for(int dev=0; dev<numDevices; dev++)
    {
#ifdef __linux__
        struct pollfd pollFd;
        pollFd.fd = fd[dev];
        pollFd.events = POLLIN|POLLHUP;
        pollFd.revents = 0;

        int ret = poll(&pollFd, 1, -1);
        EXPECT_GE(ret, 0);
        if(ret<0)
        {
            cout << "poll fail" << endl;
        }
#elif _WIN32
        DWORD waitResult = WaitForSingleObject(hFile[dev], INFINITE);
        EXPECT_EQ(waitResult, WAIT_OBJECT_0);
        if (waitResult != WAIT_OBJECT_0)
        {
            cout << "WaitForSingleObject fail. Error code: " << GetLastError() << endl;
        }
#endif
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
#ifdef __linux__
        close(fd[dev]);
#elif _WIN32
        CloseHandle(hFile[dev]);
#endif
    }
}
TEST(driver, ioctl)
{
    auto& devices = dxrt::CheckDevices();
    int numDevices = devices.size();
    LOG_VALUE(numDevices);
    EXPECT_GT(numDevices, 0);
#ifdef __linux__
    vector<int> fd;
#elif _WIN32
    vector<HANDLE> hFile;
#endif
    for(int dev=0; dev<numDevices; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd.emplace_back(openDevice(devName.c_str()));
        EXPECT_GT(fd.back(), 0);
#elif _WIN32
        HANDLE handle = openDevice(devName.c_str());
        hFile.push_back(handle);
        EXPECT_NE(hFile.back(), INVALID_HANDLE_VALUE);
#endif
    }
    for(int dev=0; dev<numDevices; dev++)
    {
        for(auto &pair:ioctlTable)
        {
            for(int cmd=0; cmd<DXRT_CMD_MAX; cmd++)
            {
#ifdef __linux__
                int ret;
#elif _WIN32
                BOOL ret;
                DWORD bytesReturned;
#endif
                vector<dxrt_message_t> messages(1);
                messages[0].cmd = cmd;
                messages[0].data = nullptr;
                messages[0].size = 0;
                for(auto &msg:messages)
                {
                    cout << pair.second << ", " << cmd << endl;
#ifdef __linux__
                    ret = ioctl(fd[dev], pair.first, &msg);
                    EXPECT_EQ(ret, 0);
#elif _WIN32
                    ret = DeviceIoControl(
                        hFile[dev],
                        pair.first,
                        &msg,
                        sizeof(msg),
                        &msg,
                        sizeof(msg),
                        &bytesReturned,
                        NULL
                    );
                    EXPECT_TRUE(ret);
#endif
                }
            }
        }
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
#ifdef __linux__
        close(fd[dev]);
#elif _WIN32
        CloseHandle(hFile[dev]);
#endif
    }
}
TEST(driver, ioctl_soc_custom)
{
    int numReq = 1;

    auto& devices = dxrt::CheckDevices();
    int numDevices = devices.size();
    LOG_VALUE(numDevices);
    EXPECT_GT(numDevices, 0);
#ifdef __linux__
    vector<int> fd;
#elif _WIN32
    vector<HANDLE> hFile;
#endif
    for(int dev=0; dev<numDevices; dev++)
    {
        string devName = DEVICE_FILENAME + to_string(dev);
#ifdef __linux__
        fd.emplace_back(openDevice(devName.c_str()));
        EXPECT_GT(fd.back(), 0);
#elif _WIN32
        HANDLE handle = openDevice(devName.c_str());
        hFile.push_back(handle);
        EXPECT_NE(hFile.back(), INVALID_HANDLE_VALUE);
#endif
    }
    for(int dev=0; dev<numDevices; dev++)
    {
        for(int i=0; i<numReq; i++)
        {
            dxrt_message_t message;
            message.cmd = dxrt::dxrt_cmd_t::DXRT_CMD_SOC_CUSTOM;
            message.data = nullptr;
            message.size = 0;
#ifdef __linux__
            int ret;
            ret = ioctl(fd[dev], dxrt::dxrt_ioctl_t::DXRT_IOCTL_MESSAGE, &message);
            EXPECT_EQ(ret, 0);
#elif _WIN32
            DWORD bytesReturned;
            BOOL bRet = DeviceIoControl(
                hFile[dev],
                static_cast<DWORD>(dxrt::dxrt_ioctl_t::DXRT_IOCTL_MESSAGE),
                &message,
                sizeof(message),
                &message,
                sizeof(message),
                &bytesReturned,
                NULL
            );
            EXPECT_TRUE(bRet);
#endif
        }
    }
    for(int dev=0; dev<NUM_DEVICES; dev++)
    {
#ifdef __linux__
        close(fd[dev]);
#elif _WIN32
        CloseHandle(hFile[dev]);
#endif
    }
}*/
TEST(driver, struct)
{
    dxrt_meminfo_t meminfo;
    dxrt_request_t inference;
    dxrt_request_acc_t inferenceAcc;
    dxrt_model_t model;
    fillStructIncreasingValues(meminfo);
    fillStructIncreasingValues(inference);
    fillStructIncreasingValues(inferenceAcc);
    fillStructIncreasingValues(model);
    EXPECT_EQ(*(reinterpret_cast<uint64_t*>(&meminfo)), 0x706050403020100 );
    EXPECT_EQ(*(reinterpret_cast<uint64_t*>(&inference)), 0x706050403020100 );
    EXPECT_EQ(*(reinterpret_cast<uint64_t*>(&inferenceAcc)), 0x706050403020100 );
    EXPECT_EQ(*(reinterpret_cast<uint64_t*>(&model)), 0x706050403020100 );
    cout << meminfo << endl;
    cout << inference << endl;
    cout << inferenceAcc << endl;
    cout << model << endl;
}
