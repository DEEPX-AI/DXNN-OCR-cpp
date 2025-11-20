
#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <atomic>

#include "dxrt/ipc_wrapper/ipc_server_wrapper.h"
#include "scheduler.h"
#include "memory_manager.h"
#include "device_status.h"


void Process(dxrt::IPCClientMessage& clientMessage, dxrt::IPCServerWrapper &ipcServerWrapper)
{
    std::cout << "Client Message: " << clientMessage << std::endl;
    std::cout << "           deviceId: " << clientMessage.deviceId << std::endl;
    std::cout << "           msgType(client-process-id): " << clientMessage.msgType << std::endl;
    
    dxrt::IPCServerMessage serverMessage;
        serverMessage.code = dxrt::RESPONSE_CODE::CONFIRM_MEMORY_ALLOCATION;
        serverMessage.deviceId = clientMessage.deviceId;
        serverMessage.msgType = clientMessage.msgType; // client massage type

    ipcServerWrapper.SendToClient(serverMessage);
}

int main() 
{
    std::cout << "TEST IPC Wrapper Server (MessageQueue)" << std::endl;

    dxrt::IPCServerWrapper ipcServerWrapper(dxrt::IPC_TYPE::MESSAE_QUEUE);

    if ( ipcServerWrapper.Initialize() == 0 ) 
    {
        //ipcServerWrapper.RegisterReceiveCB(ReceiveCB, reinterpret_cast<void*>(&serverData));

        while (true)
        {
            dxrt::IPCClientMessage clientMessage;
            ipcServerWrapper.ReceiveFromClient(clientMessage);
            
            if ( clientMessage.code != dxrt::REQUEST_CODE::CLOSE ) 
            {
                Process(clientMessage, ipcServerWrapper);
            }

        }

    }
    else 
    {
        std::cerr << "Fail to start ipc message queue" << std::endl;
    }
    

    // singleton cleanup
    dxrt::Scheduler::GetInstance().Cleanup();
    dxrt::MemoryManager::GetInstance().Cleanup();
    dxrt::DeviceStatus::GetInstance().Cleanup();

    return 0;
}