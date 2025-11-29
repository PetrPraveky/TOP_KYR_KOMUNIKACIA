// kucerp33.sender.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "../kucerp33.core/UDPCommunication.h"

int main()
{
    std::cout << "Sender Module Online\n";
    std::cout << "Hello World!\n";

    UDP::WindowsSocketInit winsocket;
    if (!winsocket.IsOk()) return 1;

    std::string ip{ UDP::DEBUG_IP };

    UDP::Sender sender(ip, UDP::DEBUG_PORT);
    if (!sender.IsOk()) return 1;

    std::cout << "Waiting for user input\n";
    std::cin.get();

    std::string start_message = "Can you hear me?";
    std::cout << "Sending message: \"" << start_message << "\"\n";
    if (!sender.SendText(start_message))
    {
        std::cerr << "Message could not be sent :(.\n";
        return 1;
    }

    return 0;
}
