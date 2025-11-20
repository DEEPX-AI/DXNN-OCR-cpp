
#ifndef __linux__	// all or dummy
#include <iostream>
#include <thread>
#include <vector>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <sys/wait.h>
#endif

#include "dxrt/ipc_wrapper/ipc_client_wrapper.h"
#include "dxrt/ipc_wrapper/ipc_message.h"

struct ClientData {
    int processIndex;
    uint32_t deviceId;
#ifdef __linux__	// all or dummy
    std::chrono::_V2::system_clock::time_point startTime;
#else
    std::chrono::steady_clock::time_point startTime;
#endif
    bool error;
};

static std::mutex gMutex;

int32_t ReceiveCB(const dxrt::IPCServerMessage& message, void* usrData)
{
    std::unique_lock<std::mutex> lock(gMutex);

    ClientData* pClientData = reinterpret_cast<ClientData*>(usrData);
    auto curTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = curTime - pClientData->startTime;
    std::cout << "Thread Index: " << pClientData->processIndex << std::endl;
    std::cout << "ReceiveCB: " << message << std::endl;
    std::cout << "           duration: " << duration.count() << std::endl;
    std::cout << "           deviceId: " << message.deviceId << std::endl;

    if ( pClientData->deviceId != message.deviceId )
    {
        std::cout << "Not matched deviceId expected=" << pClientData->deviceId << " data=" << message.deviceId << std::endl;
        pClientData->error = true;
    }

    return 0;
}

void sampleClient(int index)
{
    auto pid = getpid();
    std::cout << "TEST IPC Wrapper Client pid=" << pid << std::endl;

    ClientData clientData;
    clientData.error = false;
    clientData.processIndex = index;
    dxrt::IPCClientWrapper ipcClientWrapper(dxrt::IPC_TYPE::MESSAE_QUEUE, pid);

    if ( ipcClientWrapper.Initialize() == 0 ) 
    {
        const double SEND_DATA_GAP_MS = 1;
        const int SEND_DATA_COUNT = 100;

        ipcClientWrapper.RegisterReceiveCB(ReceiveCB, reinterpret_cast<void*>(&clientData));

        dxrt::IPCClientMessage clientMessage;
        dxrt::IPCServerMessage serverMessage;

        int count = 0;
        auto pre_send_time = std::chrono::high_resolution_clock::now();
        //auto data_transfer_start_time = std::chrono::high_resolution_clock::now();
        while (count < SEND_DATA_COUNT)
        {
            auto cur_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> duration = cur_time - pre_send_time;
            //std::cout << "Duration=" << duration.count() << std::endl;
            if ( duration.count() > SEND_DATA_GAP_MS)
            {
                clientMessage.code = dxrt::REQUEST_CODE::MEMORY_ALLOCATION_AND_TRANSFER_MODEL;
                clientMessage.deviceId = count;
                clientMessage.data = 0;
                clientData.deviceId = clientMessage.deviceId;
            
                //data_transfer_start_time = std::chrono::high_resolution_clock::now();
                clientData.startTime = std::chrono::high_resolution_clock::now();
                ipcClientWrapper.SendToServer(clientMessage);
                //auto end = std::chrono::high_resolution_clock::now();

                //ipcClientWrapper.RegisterReceiveCB(nullptr);
                pre_send_time = cur_time;

                count++;

                //break;
            }

            //if ( ipcClientWrapper.RegisterReceive(serverMessage) )
#ifdef __linux__	// all or dummy
            usleep(1000);
#else
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
#endif


        }

        ipcClientWrapper.Close();
        std::cout << "Close index=" << index << std::endl;

        if ( clientData.error )
        {
            std::cerr << "[ERROR] some data not matched thread-index=" << clientData.processIndex << std::endl;
        }
        else 
        {
            std::cout << "[SUCCESS] all data matched data-count=" << SEND_DATA_COUNT << " process-index=" << clientData.processIndex << std::endl;
        }

    }
    else 
    {
        std::cerr << "[ERROR] fail to init (message queue) thread-id=" << clientData.processIndex << std::endl;
    }
}


int main(int argc, char* argv[])
{
    const int DEFAULT_PROCESS_COUNT = 1;
    std::vector<std::thread> threads;

    int process_count = DEFAULT_PROCESS_COUNT;
    if ( argc > 1 ) 
    {
        process_count = std::stoi(argv[1]);
    }

    std::cout << "Process count=" << process_count << std::endl;

    //clock_t start = clock(); // for performance test
    auto start = std::chrono::high_resolution_clock::now();
    
#ifdef __linux__
    for(int i = 0; i < process_count; i++)
    {
        auto pid = fork();

        if ( pid == 0 ) // child process
        {
            std::cout << "Child process: index=" << i << std::endl;
            sampleClient(i);
            std::exit(0);
        }
        else if ( pid < 0 )
        {
            std::cerr << "Fork failed" << std:: endl;
            std::exit(-1);
        }

    }
    for(int i = 0; i < process_count; i++)
    {
        int status;
        wait(&status);
    }
#else
    sampleClient(0);
#endif

    //clock_t end = clock(); // for performance test
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    //auto resultTime = (double)(end - start) / CLOCKS_PER_SEC;
    std::cout << "Running Time: " << duration.count() << "ms" << std::endl;
    std::cout << "Process count: " << process_count << std::endl;

    return 0;
}

#else
#include <stdio.h>
int main()
{
	printf("Not implemented in Windows.\n") ;
    return 0;
}


#endif
