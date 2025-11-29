// kucerp33.reciever.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "../kucerp33.core/UDPCommunication.h"

int main(int argc, char* argv[])
{
    std::cout << "Reciever Module Online\n";
    std::cout << "Hello World!\n";

    UDP::WindowsSocketInit winsocket;
    if (!winsocket.IsOk()) return 1;

    UDP::Reciever reciever(UDP::DEBUG_PORT);
    if (!reciever.IsOk()) return 1;

    while (true)
    {
        std::string msg;
        std::string fromIp;
        uint16_t fromPort = 0;

        if (!reciever.RecieveText(msg, &fromIp, &fromPort))
        {
            std::cerr << "Error while receiving!\n";
            break;
        }

        std::cout << "Received from: " << fromIp << ":" << fromPort << " -> \"" << msg << "\"\n";
    }

    return 0;
}
