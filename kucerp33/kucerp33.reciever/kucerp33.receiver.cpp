// kucerp33.reciever.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>

#include "../kucerp33.core/UDPCommunication.h"
#include "../kucerp33.core/FileTransfer.h"
#include "../kucerp33.core/SmartDebug.h"

bool ReceiveStopAndWait(UDP::Receiver& receiver, UDP::FileSession& session)
{
    while (!session.IsReceived())
    {
        std::string ip;
        uint16_t port;
        UDP::Chunk data;
        bool state = receiver.ReceiveData(data, &ip, &port);

        if (!receiver.SendAckOrNack(state, data.seq, ip, port))
        {
            std::cerr << "Error: ACK or NACK could not be sent.\n";
            continue;
        }
        
        if (state)
        {
            session.chunks.insert({ data.seq, data });
            PrintChunkLine(data);

            session.stopReceived |= data.StopReceived();
        }
    }
    //session.SaveToFile();

    return true;
}

int main(int argc, char* argv[])
{
    std::cout << "Reciever Module Online\n";
    std::cout << "Hello World!\n";

    UDP::WindowsSocketInit winsocket;
    if (!winsocket.IsOk()) return 1;

    //UDP::Receiver receiver(UDP::RECEIVER_PORT);
    UDP::Receiver receiver(UDP::RECEIVER_PORT);
    if (!receiver.IsOk()) return 1;

    // Reading communication

    while (true)
    {
        std::cout << "=============================\n";
        std::cout << "   Receive file (UDP)\n";
        std::cout << "=============================\n";
        std::cout << "1) Stop-and-Wait\n";
        //std::cout << "2) Bez Stop-and-Wait (rychle, nespolehlive)\n";
        std::cout << "0) Quit\n";
        std::cout << "Select: ";

        int choice = -1;
        if (!(std::cin >> choice))
        {
            // Wrong input, we continue
            std::cin.clear();
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            continue;
        }

        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

        if (choice == 0)
        {
            std::cout << "Program stopped.\n";
            break;
        }

        if (choice != 1 && choice != 2)
        {
            std::cout << "Invalid choice.\n";
            continue;
        }

        UDP::FileSession session;
        bool ok = false;
        if (choice == 1)
        {
            std::cout << "Stared stop and wait\n";
            if (!ReceiveStopAndWait(receiver, session))
            {
                std::cerr << "Error: File could not be received.\n";
                continue;
            }
        }
        else if (choice == 2)
        {

        }
    }

    return 0;
}
