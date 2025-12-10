#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <vector>

#include "UDPCommunication.h"

namespace UDP
{
	struct Chunk
	{
		static const uint32_t crc_padding = 0; // padding for CRC
		static const uint32_t command_padding = 4; // padding for command
		static const uint32_t offset_padding = 9; // padding for offset
		static const uint32_t data_padding = 13; // 5 + 4 bytes;

		size_t packetSize = 0;
		//! !!! Raw data with offset and indentation !!!
		//! this way its easy to send -> just send .data()
		//! format: DATA=OFFSET+DATA
		std::vector<uint8_t> data; 
		
		// CRC32
		uint32_t crc = 0xFFFFFFFF;
		uint32_t retrievedCRC = 0xFFFFFFFF;

		bool CheckValidity()
		{
			return data.size() <= UDP::PACKET_MAX_LENGTH;
		}

		uint32_t RetrieveCRC() {
			uint32_t toRetrieve = 0xFFFFFFFF;

			memcpy(&toRetrieve, data.data(), command_padding);
			retrievedCRC = toRetrieve;

			return toRetrieve;
		}
		
		uint32_t ComputeCRC();
	};
	
	struct FileSession
	{
		std::string fileName = "";
		std::size_t totalSize = 0;

		std::size_t receivedBytes = 0; // To check if we were successful

		std::map<uint32_t, Chunk> chunks;

		bool SetFromFile(const std::string& path);

		bool SaveToFile(const std::string& path = "");

		bool MissingBytes()
		{
			if (chunks.size() <= 0) return 0;

			return totalSize - receivedBytes;
		}
	};
}