// kucerp33.reciever.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>

#include "../kucerp33.core/UDPCommunication.h"
#include "../kucerp33.core/FileTransfer.h"
#include "../kucerp33.core/SmartDebug.h"

bool ReceiveStopAndWait(UDP::Receiver& receiver, UDP::FileSession& session)
{
    constexpr uint32_t MAX_IDLE_AFTER_FINISH = 10;
    bool finished = false;
    uint32_t idle = 0;
    bool hashOk = false;

    std::string ip;
    uint16_t port;

    while (true)
    {
        UDP::Chunk data;
        bool ack;

        bool state = receiver.ReceiveData(data, ack, &ip, &port);

        if (ip.empty()) continue; // Not valid IP adress

        UDP::Sender ackSender(ip, UDP::SEND_PORT_ACK);

        // We wait few iterations
        if (!state)
        {
            if (finished)
            {
                if (++idle > MAX_IDLE_AFTER_FINISH) break;
            }
            continue;
        }
        idle = 0; // We got something

        if (!ackSender.SendAckOrNack(ack, data.seq))
        {
            std::cerr << "Error: ACK or NACK could not be sent.\n";
            continue;
        }
        
        if (!ack) continue; // We skip NACK
        
        // If we got duplicate packet we skip
        if (!session.chunks.contains(data.seq)) 
        {
            session.chunks.insert({ data.seq, data });
            PrintChunkLine(data);

            session.stopReceived |= data.StopReceived();
        }
        
        // We got everything
        if (session.IsReceived() && !finished)
        {
            finished = true;

            std::cout << "Receiver: File is complete, saving file..." << "\n";

            if (!session.ParseChunkData()) 
            {
                std::cerr << "Receiver: Chunk data could not be parsed!\n";
                return false;
            }
            
            if (!session.SaveToFile(hashOk)) 
            {
                std::cerr << "Receiver: File could not be saved!\n";
            }

        }
    }

    // We send here cuz otherwise sender do weird stuff
    // We send multiple times cuz its weird
    bool eh = session.IsReceived();
    UDP::Sender ackSender(ip, UDP::SEND_PORT_ACK);

    for (size_t i = 0; i < 5; ++i) ackSender.SendFileAckOrNack(hashOk);

    return true;
}


bool ReceiveSelectiveRepeat(UDP::Receiver& receiver, UDP::FileSession& session)
{
    return ReceiveStopAndWait(receiver, session);
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
        std::cout << "2) Selective Repeat\n";
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
            std::cout << "Started stop and wait...\n";
            if (!ReceiveStopAndWait(receiver, session))
            {
                std::cerr << "Error: File could not be received.\n";
                continue;
            }
        }
        else if (choice == 2)
        {
            std::cout << "Using Selective repeat...\n";
            if (!ReceiveSelectiveRepeat(receiver, session))
            {
                std::cerr << "Error: File could not be received.\n";
            }
        }
    }

    return 0;
}
