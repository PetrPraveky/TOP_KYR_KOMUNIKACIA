// kucerp33.reciever.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>

#include "../kucerp33.core/UDPCommunication.h"
#include "../kucerp33.core/FileTransfer.h"

int main(int argc, char* argv[])
{
    std::cout << "Reciever Module Online\n";
    std::cout << "Hello World!\n";

    UDP::WindowsSocketInit winsocket;
    if (!winsocket.IsOk()) return 1;

    UDP::Receiver receiver(UDP::DEBUG_PORT);
    if (!receiver.IsOk()) return 1;

    // Reading communication

    UDP::FileSession session{};
    uint32_t counter = 0;
    while (true)
    {
        std::string msg;
        std::string fromIp;
        uint16_t fromPort = 0;

        if (!receiver.ReceiveText(msg, &fromIp, &fromPort))
        {
            std::cerr << "Error while receiving!\n";
            break;
        }
        
        // todo check package, if its wrong, send message that it's wrong lol

        std::cout << ++counter << "Received packet from: " << fromIp << ":" << fromPort << "\n";
        
        // File "creation"
        // Name detection
        if (msg.rfind("NAME=", 0) == 0)
        {
            if (session.fileName.size() > 0) std::cerr << "Filename already in use, overwriting! \n";
            
            session.fileName = msg.substr(5);
            continue;
        }
        // Size detection
        if (msg.rfind("SIZE=", 0) == 0)
        {
            if (session.totalSize > 0) std::cerr << "Total size already defined, overwriting! \n";

            session.totalSize = std::stoull(msg.substr(5));
            continue;
        }
        // Chunk reading start
        if (msg.rfind("START") == 0)
        {
            continue;
        }
        // Chunk reading 
        if (msg.rfind("DATA=") == 0)
        {
            UDP::Chunk data{};
            uint32_t offset = receiver.ParseFileData(msg, &data);

            // todo log if packet is already used

            session.chunks.insert({ offset, data });
            session.receivedBytes += data.packetSize - data.padding; // So we have correct size of file idk.

            continue;
        }
        // Chunk reading stop
        if (msg.rfind("STOP") == 0)
        {
            if (session.MissingBytes() <= 0)
            {
                session.SaveToFile();
            }
            else
            {
                std::cerr << "Received file is not the same size!";
            }

            session = {};
            continue;
        }
    }

    return 0;
}
