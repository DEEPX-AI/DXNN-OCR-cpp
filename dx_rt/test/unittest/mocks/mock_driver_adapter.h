#include "dxrt/driver_adapter/driver_adapter.h"
#include "dxrt/device_struct.h"         // enum 정의 포함
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <iostream>
#include <sstream>


namespace dxrt {

// gMock가 사람이 읽을 수 있게 출력하도록 헬퍼
inline void PrintTo(const dxrt_cmd_t& cmd, std::ostream* os) {
    switch (cmd) {
    case dxrt_cmd_t::DXRT_CMD_IDENTIFY_DEVICE: *os << "DXRT_CMD_IDENTIFY_DEVICE"; break;
    case dxrt_cmd_t::DXRT_CMD_GET_STATUS:      *os << "DXRT_CMD_GET_STATUS"; break;
    case dxrt_cmd_t::DXRT_CMD_WRITE_MEM:       *os << "DXRT_CMD_WRITE_MEM"; break;
    case dxrt_cmd_t::DXRT_CMD_READ_MEM:        *os << "DXRT_CMD_READ_MEM"; break;
    case dxrt_cmd_t::DXRT_CMD_RESET:           *os << "DXRT_CMD_RESET"; break;
    case dxrt_cmd_t::DXRT_CMD_TERMINATE_EVENT: *os << "DXRT_CMD_TERMINATE_EVENT"; break;
    default: *os << "dxrt_cmd_t(" << static_cast<int>(cmd) << ")"; break;
    }
}

class MockDriverAdapter : public DriverAdapter {
public:
    MOCK_METHOD(int32_t, IOControl_Other,
                (dxrt_cmd_t request, void *data, uint32_t size, uint32_t sub_cmd));
    MOCK_METHOD(int32_t, NetControl,
                (dxrt_cmd_t request, void *data, uint32_t size, uint32_t sub_cmd,
                 uint64_t address, bool ctrlCmd));
    MOCK_METHOD(int32_t, Write, (const void *buffer, uint32_t size));
    MOCK_METHOD(int32_t, Read, (void *buffer, uint32_t size));
    MOCK_METHOD(void*, MemoryMap, (void* __addr, size_t __len, off_t __offset));
    MOCK_METHOD(int32_t, Poll, ());
    MOCK_METHOD(int, GetFd, (), (const));
    MOCK_METHOD(std::string, GetName, (), (const));

    ~MockDriverAdapter() override = default;

    MOCK_METHOD(int32_t, IdentifyDevice, (dxrt_device_info_t* data));
    MOCK_METHOD(int32_t, IoctlRead, (void* data, uint32_t size));
    MOCK_METHOD(int32_t, IoctlWrite, (void* data, uint32_t size));


    int32_t IOControl(dxrt_cmd_t request, void *data, uint32_t size, uint32_t sub_cmd) override
    {
        switch(request)
        {
            case dxrt_cmd_t::DXRT_CMD_IDENTIFY_DEVICE:
                return IdentifyDevice(static_cast<dxrt_device_info_t*>(data));
            case dxrt_cmd_t::DXRT_CMD_READ_MEM:
                return IoctlRead(data, size);
            case dxrt_cmd_t::DXRT_CMD_WRITE_MEM:
                return IoctlWrite(data, size);
            case dxrt_cmd_t::DXRT_CMD_DRV_INFO:
            {
                if (sub_cmd == dxrt::dxrt_drvinfo_sub_cmd_t::DRVINFO_CMD_GET_RT_INFO)
                {
                    static_cast<uint32_t*>(data)[0] = 1700; // driver version
                    return 0;
                }
                else if (sub_cmd == dxrt::dxrt_drvinfo_sub_cmd_t::DRVINFO_CMD_GET_RT_INFO_V2)
                {
                    auto *ver = static_cast<dxrt_rt_drv_version_t*>(data);
                    ver->driver_version = 1701;
                    const char suffix[] = "TEST"; // test marker
                    // Use snprintf to ensure null-termination without overflow
                    std::snprintf(ver->driver_version_suffix,
                                  sizeof(ver->driver_version_suffix), "%s", suffix);
                    return 0;
                }
                else if (sub_cmd == dxrt::dxrt_drvinfo_sub_cmd_t::DRVINFO_CMD_GET_PCIE_INFO)
                {
                    memset(data, 0, sizeof(deepx_pcie_info));
                    return 0;
                }
                else
                {
                    return -1;
                }
            }
            default:
                return IOControl_Other(request, data, size, sub_cmd);
        }
    }


    MockDriverAdapter() {
        using ::testing::Return;

        EXPECT_CALL(*this, GetName()).Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Return("mock-driver"));

        ON_CALL(*this, NetControl).WillByDefault([](dxrt_cmd_t, void*, uint32_t, uint32_t,
                                                    uint64_t, bool){
            return 0;
        });
        ON_CALL(*this, Poll()).WillByDefault(Return(0));
    }
};

} // namespace dxrt
