#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include "FileTransfer.h"

#define ERR(msg) \
    do { \
        std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " \
                  << msg << std::endl; \
    } while (0)



static void PrintChunkLine(const UDP::Chunk & chunk)
{
    if (chunk.data.size() < UDP::Chunk::data_padding)
    {
        std::cout << "PACKET_TOO_SHORT size=" << chunk.data.size() << "\n";
        return;
    }

    // CRC
    uint32_t crcRaw = 0;
    std::memcpy(&crcRaw,
        chunk.data.data() + UDP::Chunk::crc_padding,
        sizeof(uint32_t));
    // pokud CRC posíláš v network order, odkomentuj:
    // crcRaw = ntohl(crcRaw);

    // SEQ
    uint32_t seqRaw = 0;
    std::memcpy(&seqRaw,
        chunk.data.data() + UDP::Chunk::seq_padding,
        sizeof(uint32_t));
    // seqRaw = ntohl(seqRaw);

    // COMMAND (5 znakù)
    unsigned char cmd[6]{};
    std::memcpy(cmd,
        chunk.data.data() + UDP::Chunk::command_padding,
        5);
    cmd[5] = '\0';

    // OFFSET
    uint32_t offRaw = 0;
    std::memcpy(&offRaw,
        chunk.data.data() + UDP::Chunk::offset_padding,
        sizeof(uint32_t));
    // offRaw = ntohl(offRaw);

    // jeden radek, oddeleny mezerami
    // CRC dávám v hex (0x....), zbytek v dec
    std::cout
        << "CRC=0x" << std::hex << std::setw(8) << std::setfill('0') << crcRaw
        << " SEQ=" << std::dec << seqRaw
        << " CMD=" << cmd
        << " OFFSET=" << offRaw
        << "\n";
}