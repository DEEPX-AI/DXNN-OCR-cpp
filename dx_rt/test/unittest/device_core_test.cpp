#include <gtest/gtest.h>
#include <gmock/gmock.h>

#ifndef DXRT_USB_NETWORK_DRIVER
#define DXRT_USB_NETWORK_DRIVER 0
#endif

#include "dxrt/device_core.h"
#include "dxrt/device_struct.h"
#include "dxrt/driver_adapter/driver_adapter.h"
#include "mocks/mock_driver_adapter.h"
#include <cstring>

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Sequence;

using namespace dxrt;

static std::unique_ptr<DeviceCore> MakeCoreWithMock(MockDriverAdapter*& outMock, int id = 0) {
    auto* mock = new MockDriverAdapter();
    outMock = mock;
    std::unique_ptr<DriverAdapter> up(mock);
    return std::make_unique<DeviceCore>(id, std::move(up));
}

TEST(DeviceCoreTest, Process_DelegatesToIOControl) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCoreWithMock(mock, /*id=*/1);

    EXPECT_CALL(*mock, IOControl_Other(dxrt_cmd_t::DXRT_CMD_RESET, _, _, _))
        .WillOnce(Return(0));

    int rc = core->Process(dxrt_cmd_t::DXRT_CMD_RESET, nullptr);
    EXPECT_EQ(rc, 0);
}

TEST(DeviceCoreTest, Status_FillsStatusViaGetStatus) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCoreWithMock(mock);

    EXPECT_CALL(*mock, IOControl_Other(dxrt_cmd_t::DXRT_CMD_GET_STATUS, _, _, _))
        .WillOnce(DoAll(
            Invoke([](dxrt_cmd_t, void* data, uint32_t, uint32_t) {
                auto* s = static_cast<dxrt_device_status_t*>(data);
                std::memset(s, 0, sizeof(*s));
                s->ddr_sbe_cnt[0] = 7;
                return 0;
            })
        ));

    auto st = core->Status();
    EXPECT_EQ(st.ddr_sbe_cnt[0], 7u);
}

TEST(DeviceCoreTest, Write_RoundRobinChannelsAndMeminfoPacked) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCoreWithMock(mock);

    dxrt_meminfo_t mi{};
    uint8_t buf[64] = {};
    mi.data   = reinterpret_cast<uint64_t>(buf);
    mi.base   = 0x1000;
    mi.offset = 0x20;
    mi.size   = sizeof(buf);

    Sequence seq;

    EXPECT_CALL(*mock, IoctlWrite( _, _))
        .InSequence(seq)
        .WillOnce(DoAll(Invoke([&](void* p, uint32_t) {
            auto* req = static_cast<dxrt_req_meminfo_t*>(p);
            EXPECT_EQ(req->base, mi.base);
            EXPECT_EQ(req->offset, mi.offset);
            EXPECT_EQ(req->size, mi.size);
            EXPECT_EQ(req->ch, 0); // first write uses ch=0
            return 0;
        })));

    EXPECT_CALL(*mock, IoctlWrite(_, _))
        .InSequence(seq)
        .WillOnce(DoAll(Invoke([&](void* p, uint32_t) {
            auto* req = static_cast<dxrt_req_meminfo_t*>(p);
            EXPECT_EQ(req->ch, 1); // second write uses ch=1
            return 0;
        })));

    EXPECT_EQ(core->Write(mi), 0);
    EXPECT_EQ(core->Write(mi), 0);
}

TEST(DeviceCoreTest, Read_RoundRobinChannelsAndMeminfoPacked) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCoreWithMock(mock);

    dxrt_meminfo_t mi{};
    uint8_t buf[32] = {};
    mi.data   = reinterpret_cast<uint64_t>(buf);
    mi.base   = 0x2000;
    mi.offset = 0x40;
    mi.size   = sizeof(buf);

    Sequence seq;

    EXPECT_CALL(*mock, IoctlRead(_, _))
        .InSequence(seq)
        .WillOnce(DoAll(Invoke([&](void* p, uint32_t) {
            auto* req = static_cast<dxrt_req_meminfo_t*>(p);
            EXPECT_EQ(req->base, mi.base);
            EXPECT_EQ(req->offset, mi.offset);
            EXPECT_EQ(req->size, mi.size);
            EXPECT_EQ(req->ch, 0); // first read uses ch=0
            return 0;
        })));

    EXPECT_CALL(*mock, IoctlRead(_, _))
        .InSequence(seq)
        .WillOnce(DoAll(Invoke([&](void* p, uint32_t) {
            auto* req = static_cast<dxrt_req_meminfo_t*>(p);
            EXPECT_EQ(req->ch, 1); // second read uses ch=1
            return 0;
        })));

    EXPECT_EQ(core->Read(mi), 0);
    EXPECT_EQ(core->Read(mi), 0);
}

TEST(DeviceCoreTest, Poll_DelegatesToAdapter) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCoreWithMock(mock);

    EXPECT_CALL(*mock, Poll()).WillOnce(Return(0));
    EXPECT_EQ(core->Poll(), 0);
}

TEST(DeviceCoreTest, Identify_PopulatesInfo) {
    MockDriverAdapter* mock = nullptr;
    auto core = MakeCoreWithMock(mock);

    EXPECT_CALL(*mock, IdentifyDevice(_))
        .WillOnce(DoAll(Invoke([](dxrt_device_info_t* info) {
            std::memset(static_cast<void*>(info), 0, sizeof(*info));
            info->mem_addr = 0x1000000;
            info->mem_size = 4096;
            info->type = static_cast<uint32_t>(DeviceType::ACC_TYPE);
            info->variant = 1;
            return 0;
        })));

    core->Identify(/*id=*/0, /*subCmd=*/0);
    EXPECT_EQ(core->info().mem_size, 4096u);
    EXPECT_EQ(static_cast<uint32_t>(core->GetDeviceType()), static_cast<uint32_t>(DeviceType::ACC_TYPE));
}
