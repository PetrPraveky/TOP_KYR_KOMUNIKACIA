// kucerp33.sender.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <limits>
#include <algorithm>
#include <unordered_set>

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

bool SendSelectiveRepeatOld(UDP::Sender& sender, const UDP::FileSession& session, int window) {
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

    // New sequence stuff
    while (baseSeq < totalChunks) 
    {
        // we send window of packets
        // -> we wait for 8? iterations of receive ACK for each sequence and 
        // we check if sequence number is same as expected (or in expected array?)
        // if not/timeout/nack, we mark as false and then we send another window
        // of packets.

        std::vector<UDP::Chunk> package;
        package.reserve(window);

    }



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
        
        // We skip incorrect sequence number
        // if its outside of range of sent packets then its wrong -> theoretically it can still mess up things,
        // but idk how else to repair it
        if (ackSeq >= totalChunks || 
            ackSeq < baseSeq ||
            ackSeq >= nextSeq || 
            !session.chunks.contains(ackSeq))
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

    return true;
}


bool SendSelectiveRepeat(UDP::Sender& sender, const UDP::FileSession& session, int window)
{
    UDP::Receiver ackReceiver(UDP::RECEIVER_PORT_ACK);
    if (!ackReceiver.IsOk())
    {
        std::cerr << "Sender: ACK receiver could not be initialized!\n";
        return false;
    }

    if (session.chunks.empty()) return false;

    // Biggest sequence number
    size_t maxSeq = session.chunks.rbegin()->first;
    size_t totalChunks = maxSeq + 1;

    // Mask of "ACKs"
    std::vector<bool> delivered(totalChunks, false);
    size_t deliveredCount = 0;

    size_t nextSeq = 0;

    // Buffer of unsucessful packets
    std::vector<size_t> pendingResend;

    while (deliveredCount < totalChunks)
    {
        // We create a window of packets and reserve it
        std::vector<size_t> windowSeqs;
        windowSeqs.reserve(static_cast<size_t>(window));

        // We refill the window with unsucessful packets
        // -> length of pedingResend will always be <= window
        for (size_t seq : pendingResend)
        {
            if (windowSeqs.size() >= static_cast<size_t>(window))
                break;

            if (seq < totalChunks && !delivered[seq] && session.chunks.contains(seq))
            {
                windowSeqs.push_back(seq);
            }
        }
        pendingResend.clear();

        // We fill the rest with new sequences
        while (windowSeqs.size() < static_cast<size_t>(window) && nextSeq < totalChunks)
        {
            if (!session.chunks.contains(nextSeq))
            {
                ++nextSeq;
                continue;
            }

            if (!delivered[nextSeq])
            {
                windowSeqs.push_back(nextSeq);
            }

            ++nextSeq;
        }

        if (windowSeqs.empty()) break;

        // We send the whole window
        std::cout << "Sender: SENDING NEW BATCH!\n";
        for (size_t seq : windowSeqs)
        {
            if (!session.chunks.contains(seq))
                continue;

            if (!sender.SendData(session.chunks.at(seq)))
            {
                std::cerr << "Sender: SendData failed for seq=" << seq << "\n";
                return false;
            }

            std::cout << "Sender: Sent packet with sequence " << seq << "\n";
        }

        // Waiting set cuz some packets are just eh
        std::unordered_set<size_t> waiting(windowSeqs.begin(), windowSeqs.end());

        // We send all the packets in window
        for (size_t seq : windowSeqs)
        {
            if (seq >= totalChunks || delivered[seq]) continue;

            uint32_t ackSeq = 0;
            bool isNack = false;

            bool gotResponse = ackReceiver.ReceiveAnyAckOrNack(
                ackSeq,
                UDP::ACK_RECEIVER_TIMEOUT,
                isNack
            );

            // Timeout
            if (!gotResponse)
            {
                std::cout << "Sender: Timeout or CRC error for seq=" << seq << ", will resend in next window\n";
                continue;
            }

            // Fallback if something goes wrong
            if (ackSeq >= totalChunks)
            {
                std::cout << "Sender: ACK/NACK for out-of-range seq=" << ackSeq << " ignored.\n";
                continue;
            }

            // Just nack, sefl explanatory
            if (isNack)
            {
                std::cout << "Sender: NACK for seq=" << ackSeq << ", will resend in next window\n";
                pendingResend.push_back(ackSeq);
            }

            // we correctly got ACK!
            if (!isNack && !delivered[ackSeq])
            {
                delivered[ackSeq] = true;
                ++deliveredCount;
                std::cout << "Sender: ACK received for seq=" << ackSeq << "\n";
            }

            // We erase from waiting
            auto it = waiting.find(ackSeq);
            if (it != waiting.end())
            {
                waiting.erase(it);
            }
        }

        // We refill pending
        for (size_t seq : waiting)
        {
            if (seq < totalChunks && !delivered[seq])
            {
                std::cout << "Sender: Timeout for seq=" << seq
                    << ", will resend in next window\n";
                pendingResend.push_back(seq);
            }
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

            int window = 4;
            std::cout << "Chose packet window: ";
            while (!(std::cin >> window))
            {
                std::cout << "Not valid option, try again...\n";
            }
            std::cout << "Using Selective repeat with window "<< window << "\n";
            
            if (!SendSelectiveRepeat(sender, session, window)) 
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
