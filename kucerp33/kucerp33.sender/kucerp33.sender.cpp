// kucerp33.sender.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <limits>

#include "../kucerp33.core/UDPCommunication.h"
#include "../kucerp33.core/FileTransfer.h"

//constexpr std::string_view file = "sender.txt";
constexpr std::string_view file = "sender.png";

bool SendStopAndWait(UDP::Sender& sender, const UDP::FileSession& session)
{
    UDP::Receiver ackReceiver(UDP::RECEIVER_PORT_ACK);

    for (const auto& [seq, chunk] : session.chunks)
    {
        bool delivered = false;

        while (!delivered)
        {
            // We could not send anything
            if (!sender.SendData(chunk)) return false;

            bool isNack = false;
            bool gotResponse = ackReceiver.ReceiveAckOrNack(seq, UDP::ACK_RECEIVER_TIMEOUT, isNack);

            if (!gotResponse)
            {
                std::cout << "Timeout, sending again seq=" << seq << "\n";
                continue;
            }

            if (isNack)
            {
                std::cout << "NACK, sending again seq=" << seq << "\n";
                continue;
            }

            delivered = true;
        }
    }

    return true;
}

int main()
{
    std::cout << "Sender Module Online\n";
    std::cout << "Hello World!\n";

    UDP::WindowsSocketInit winsocket;
    if (!winsocket.IsOk()) return 1;

    std::string ip{ UDP::DEBUG_IP };

    //UDP::Sender sender(ip, UDP::RECEIVER_PORT);
    UDP::Sender sender(ip, UDP::SEND_PORT);
    if (!sender.IsOk()) return 1;

    while (true)
    {
        std::cout << "=============================\n";
        std::cout << "   Send file (UDP)\n";
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

        std::cout << "Chose file: ";
        std::string path;
        std::getline(std::cin, path);

        if (path.empty()) path = file; // Default file

        UDP::FileSession session;
        if (!session.SetFromFile(path))
        {
            std::cerr << "Error: File could not be read.\n";
            continue;
        }

        bool ok = false;
        if (choice == 1)
        {
            std::cout << "Pouzivam Stop-and-Wait...\n";
            if (!SendStopAndWait(sender, session))
            {
                std::cerr << "Error: File could not be sent.\n";
            }
        }
        else if (choice == 2)
        {
            
        }
    }


    /*
    std::cout << "Now sending file: " << file << "\n";
    std::cout << "Waiting for user input\n";
    std::cin.get();

    std::string path{ file };
    if (!sender.SendFile(path))
    {
        std::cerr << "File could not be sent :(.\n";
        return 1;
    }
    */

    return 0;
}
