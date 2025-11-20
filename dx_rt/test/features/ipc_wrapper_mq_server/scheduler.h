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

class IPCClientMessage;
class IPCServerMessage;
namespace dxrt 
{

    // singleton class
    class Scheduler
    {
    
    public:
        static Scheduler& GetInstance();

    private:
        Scheduler(); // instanciate only by GetInstance()
        ~Scheduler();

        Scheduler(const Scheduler&) = delete;
        Scheduler& operator=(const Scheduler&) = delete;

    public:
        
        int32_t Request(IPCServerMessage& responseMessage, IPCClientMessage& requestMessage);
        
        void Cleanup();
        
    };

}  // namespace dxrt