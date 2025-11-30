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
		static const uint32_t padding = 9; // 5 + 4 bytes;

		size_t packetSize = 0;
		//! !!! Raw data with offset and indentation !!!
		//! this way its easy to send -> just send .data()
		//! format: DATA=OFFSET+DATA
		std::vector<uint8_t> data; 

		bool CheckValidity()
		{
			return data.size() <= UDP::PACKET_MAX_LENGTH;
		}
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