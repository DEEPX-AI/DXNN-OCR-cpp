/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#pragma once

#include <stdint.h>
#include <cstdint>

#include "dxrt/common.h"
#include "dxrt/driver.h"
#include "dxrt/device_struct.h"

namespace dxrt 
{

    // singleton class
    class DeviceStatus
    {
    
    public:
        static DeviceStatus& GetInstance();

    private:
        DeviceStatus(); // instanciate only by GetInstance()
        ~DeviceStatus();

        DeviceStatus(const DeviceStatus&) = delete;
        DeviceStatus& operator=(const DeviceStatus&) = delete;

    public:
        
        
        void Cleanup();
        
    };

}  // namespace dxrt