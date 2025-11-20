/*
 * Copyright (C) 2018- DEEPX Ltd.
 * All rights reserved.
 *
 * This software is the property of DEEPX and is provided exclusively to customers 
 * who are supplied with DEEPX NPU (Neural Processing Unit). 
 * Unauthorized sharing or usage is strictly prohibited by law.
 */

#include "scheduler.h"

using namespace dxrt;

Scheduler& Scheduler::GetInstance()
{
    static Scheduler instance;    
    return instance;
}

Scheduler::Scheduler()
{

}

Scheduler::~Scheduler()
{

}

void Scheduler::Cleanup()
{

}