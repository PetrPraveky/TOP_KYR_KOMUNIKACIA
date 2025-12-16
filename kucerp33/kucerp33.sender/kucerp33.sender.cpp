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

    // Wait for NACK or ACK from receiver if file was received correctly or not.
    constexpr size_t fileCheckTries = 10;
    size_t tries = 0;

    while (tries < fileCheckTries) 
    {
        ++tries;
        bool isNack = false;
        bool gotResponse = ackReceiver.ReceiveFileAckOrNack(UDP::ACK_RECEIVER_TIMEOUT, isNack);

        if (!gotResponse) {
            std::cout << "Got uknown response, retrying...\n";
            continue;
        }

        if (isNack) {
            std::cout << "FNACK, file receive failed...\n";
            return false;
        }
        else {
            std::cout << "FACK, file receive succesful...\n";
            return true;
        }
    }

    return true;
}

bool SendSelectiveRepeat(UDP::Sender& sender, const UDP::FileSession& session, int window) {
    UDP::Receiver ackReceiver(UDP::RECEIVER_PORT_ACK);
    if (!ackReceiver.IsOk()) 
    {
        std::cerr << "Sender: ACK receiver could not be initialized!" << "\n";
        return false;
    }

    if (session.chunks.empty()) return false;

    // Max sequence number
    size_t maxSeq = session.chunks.rbegin()->first;
    size_t totalChunks = maxSeq + 1;

    std::vector<bool> ackedChunks(totalChunks, false);

    // Properties for selective repeat
    size_t baseSeq = 0;
    size_t nextSeq = 0;

    // We cycle through all chunks
    while (baseSeq < totalChunks) 
    {
        // We move window if needed
        while (nextSeq < totalChunks && nextSeq < baseSeq + static_cast<size_t>(window)) 
        {
            // We check if packet with this sequence exists
            if (session.chunks.contains(nextSeq)) 
            {
                // We cant send packet
                if (!sender.SendData(session.chunks.at(nextSeq))) return false; 

                std::cout << "Sender: Sent packet with sequence " << nextSeq << "\n";
            }
            ++nextSeq;
        }
        // We wait for ACK/NACK
        uint32_t ackSeq = 0;
        bool isNack = false;
        bool gotResponse = ackReceiver.ReceiveAnyAckOrNack(ackSeq, UDP::ACK_RECEIVER_TIMEOUT, isNack);

        // timeout -> we send all NACK packets
        if (!gotResponse) 
        {
            std::cout << "Timeout, sending packets again" << "\n";

            for (size_t seq = baseSeq; seq < nextSeq; ++seq) 
            {
                if (seq < totalChunks && !ackedChunks[seq] && session.chunks.contains(seq))
                {
                    // We send packet
                    if (!sender.SendData(session.chunks.at(seq))) return false;

                    std::cout << "Sender: Resent packet with sequence " << seq << "\n";
                }
            }

            continue;
        }
        
        // We git incorrect sequence number
        if (ackSeq >= totalChunks || !session.chunks.contains(ackSeq)) 
        {
            std::cout << "Sender: Received incorrect ACK/NACK sequence: " << ackSeq << "\n";
            continue;
        }

        // We got nack -> resent packet with sequence number
        if (isNack) 
        {
            if (!sender.SendData(session.chunks.at(ackSeq))) return false;

            std::cout << "Sender: NACK received. Resent packet with sequence " << ackSeq << "\n";
        }

        if (!isNack && !ackedChunks[ackSeq]) 
        {
            ackedChunks[ackSeq] = true;

            std::cout << "Sender: ACK received:" << ackSeq << "\n";
        }

        // We move the window forward if possible
        while (baseSeq < totalChunks && ackedChunks[baseSeq]) ++baseSeq;
    }

    // Wait for NACK or ACK from receiver if file was received correctly or not.
    constexpr size_t fileCheckTries = 10;
    size_t tries = 0;

    while (tries < fileCheckTries)
    {
        ++tries;
        bool isNack = false;
        bool gotResponse = ackReceiver.ReceiveFileAckOrNack(UDP::ACK_RECEIVER_TIMEOUT, isNack);

        if (!gotResponse) {
            std::cout << "Got uknown response, retrying...\n";
            continue;
        }

        if (isNack) {
            std::cout << "FNACK, file receive failed...\n";
            return false;
        }
        else {
            std::cout << "FACK, file receive succesful...\n";
            return true;
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
            std::cout << "Using Stop-and-Wait...\n";
            if (!SendStopAndWait(sender, session))
            {
                std::cerr << "Error: File could not be sent.\n";
            }
        }
        else if (choice == 2)
        {
            std::cout << "Using Selective repeat with window 4\n";
            //todo add option to select window size
            if (!SendSelectiveRepeat(sender, session, 4)) 
            {
                std::cerr << "Error: File could not be sent.\n";
            }
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
