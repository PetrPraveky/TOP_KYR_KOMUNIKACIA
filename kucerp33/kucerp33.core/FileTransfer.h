#pragma once

#include <string>
#include <map>
#include <cstdint>
#include <vector>
#include <iostream>

#include "UDPCommunication.h"

namespace UDP
{
	struct Chunk
	{
		static const uint32_t crc_padding = 0; // padding for CRC
		static const uint32_t seq_padding = 4; // padding for sequence number
		static const uint32_t command_padding = 8; // padding for command
		static const uint32_t offset_padding = 13; // padding for offset
		static const uint32_t data_padding = 17; // padding for sent data

		size_t packetSize = 0;
		//! !!! Raw data with offset and indentation !!!
		//! this way its easy to send -> just send .data()
		//! format: DATA=OFFSET+DATA
		std::vector<uint8_t> data; 
		
		// Properties
		uint32_t crc = 0xFFFFFFFF;
		uint32_t retrievedCRC = 0xFFFFFFFF;
		uint32_t seq = 0;
		uint32_t offset = 0;

		bool CheckValidity()
		{
			return data.size() <= UDP::PACKET_MAX_LENGTH;
		}

		uint32_t RetrieveCRC() {
			uint32_t toRetrieve = 0xFFFFFFFF;

			memcpy(&toRetrieve, data.data(), seq_padding);
			retrievedCRC = toRetrieve;

			return toRetrieve;
		}

		uint32_t RetrieveSeq()
		{
			uint32_t raw = 0;
			if (data.size() < seq_padding + sizeof(uint32_t))
				return 0;

			std::memcpy(&raw, data.data() + seq_padding, sizeof(uint32_t));
			seq = raw;
			return seq;
		}

		uint32_t RetrieveOffset()
		{
			uint32_t raw = 0;
			if (data.size() < offset_padding + sizeof(uint32_t))
				return 0;

			std::memcpy(&raw, data.data() + offset_padding, sizeof(uint32_t));
			offset = raw;
			return offset;
		}

		// Checks if stop command was received and returns true
		bool StopReceived()
		{
			// Check if stop is received
			if (data.size() < UDP::Chunk::command_padding + 4)
				return false;

			// Exactly 4 chars
			std::string command(reinterpret_cast<const char*>(data.data() + UDP::Chunk::command_padding), 4);

			return command == "STOP";
		}

		uint32_t ComputeCRC();
	};
	
	struct FileSession
	{
		std::string fileName = "";
		size_t totalSize = 0;

		size_t receivedBytes = 0; // To check if we were successful

		std::map<size_t, Chunk> chunks; // <sequence number, chunk>
		size_t currentSequence = 0;

		bool stopReceived = false;

		bool SetFromFile(const std::string& path);

		bool SaveToFile(const std::string& path = "");

		bool MissingBytes()
		{
			if (chunks.size() <= 0) return 0;

			return totalSize - receivedBytes;
		}

		bool IsReceived()
		{
			return stopReceived && chunks.rbegin()->first == chunks.size() - 1;
		}

	private:
		void CreateNameChunk();
		void CreateSizeChunk();
		void CreateStopChunk();
	};
}