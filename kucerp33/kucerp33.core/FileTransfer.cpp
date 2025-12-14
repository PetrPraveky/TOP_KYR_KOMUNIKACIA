#include "FileTransfer.h"
#include "SmartDebug.h"

#include "crc.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace UDP;
namespace fs = std::filesystem;

uint32_t Chunk::ComputeCRC() {
	boost::crc_32_type result;

	result.process_bytes(data.data() + Chunk::seq_padding, packetSize - Chunk::seq_padding);
	crc = result.checksum();

	return result.checksum();
}

/// <summary>
/// Opens file and fills its self with data
/// </summary>
/// <param name="path"></param>
/// <returns></returns>
bool FileSession::SetFromFile(const std::string& path)
{
	this->chunks.clear();

	// Try to open the file
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		ERR("File could not be opened: " << path);
		return false;
	}

	std::streamsize size = file.tellg();
	if (size < 0)
	{
		ERR("Size of file could not be received: " << path);
		return false;
	}
	file.seekg(0, std::ios::beg);
	
	fs::path p(path);
	// Get name
	std::string name = p.filename().string();

	this->fileName = name;
	this->totalSize = size;
	// todo hash
	
	// Create sequences
	CreateNameChunk();
	CreateSizeChunk();

	// Start to read file
	std::vector<unsigned char> buffer(UDP::PACKET_MAX_LENGTH);
	uint32_t offset = 0;
	while (file)
	{
		Chunk fileChunk{};

		uint32_t offsetNet = htonl(offset);
		std::memcpy(buffer.data() + Chunk::seq_padding, &currentSequence, 4);
		std::memcpy(buffer.data() + Chunk::command_padding, "DATA=", 5);
		std::memcpy(buffer.data() + Chunk::offset_padding, &offsetNet, sizeof(offsetNet));

		// Pointer arithmetic -> 'skip' the first 13 bytes -> CRC+COMMAND+OFFSET
		file.read(
			reinterpret_cast<char*>(buffer.data() + Chunk::data_padding), 
			UDP::PACKET_MAX_LENGTH - Chunk::data_padding);

		std::streamsize bytesRead = file.gcount();
		if (bytesRead <= 0) break; // We're on the EOF

		int packetSize = Chunk::data_padding + static_cast<int>(bytesRead); // Max 1024 Bytes

		fileChunk.data.resize(packetSize); // Resize so we don't waste space on our prescius RAM
		std::memcpy(fileChunk.data.data(), buffer.data(), packetSize);
		fileChunk.packetSize = packetSize;

		// Do CRC
		uint32_t crc = fileChunk.ComputeCRC();
		std::memcpy(fileChunk.data.data(), &crc, Chunk::seq_padding);

		this->chunks.insert({ currentSequence, fileChunk });

		offset += static_cast<uint32_t>(bytesRead);

		++currentSequence;
	}

	CreateStopChunk();

	return true;
}


bool FileSession::SaveToFile(const std::string& path)
{


	/*
	if (this->fileName.size() == 0)
	{
		ERR("Invalid file name!");
		return false;
	}

	std::ofstream out(path + this->fileName, std::ios::binary);
	if (!out)
	{
		ERR("Could not create file in specified path: \"" + path + "\"");
		return false;
	}

	// We create correct size
	out.seekp(static_cast<std::streamoff>(totalSize - 1), std::ios::beg);
	out.write("", 1);
	out.seekp(0, std::ios::beg);

	for (auto& [offset, data] : chunks)
	{
		out.seekp(static_cast<std::streamoff>(offset), std::ios::beg);

		out.write(reinterpret_cast<const char*>(data.data.data() + data.data_padding),
			static_cast<std::streamsize>(data.packetSize - data.data_padding));
	}
	*/

	return true;
}


void FileSession::CreateNameChunk()
{
	// Name
	Chunk nameChunk;
	std::string nameCommand = "NAME=";
	nameChunk.packetSize = Chunk::data_padding + fileName.size();
	nameChunk.data.resize(nameChunk.packetSize); // Cuz we dont have the memory yet

	memcpy(nameChunk.data.data() + Chunk::data_padding, &fileName, fileName.size());
	memcpy(nameChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(nameChunk.data.data() + Chunk::command_padding, nameCommand.c_str(), 5);
	uint32_t nameCRC = nameChunk.ComputeCRC();
	memcpy(nameChunk.data.data() + Chunk::crc_padding, &nameCRC, 4);

	chunks.insert({ currentSequence, nameChunk });

	++currentSequence;
}


void FileSession::CreateSizeChunk()
{
	// Name
	Chunk sizeChunk;
	std::string sizeCommand = "SIZE=";
	
	std::stringstream ss;
	ss << totalSize;
	std::string stringSize = ss.str();

	sizeChunk.packetSize = Chunk::data_padding + stringSize.size();
	sizeChunk.data.resize(sizeChunk.packetSize); // Cuz we dont have the memory yet

	memcpy(sizeChunk.data.data() + Chunk::data_padding, &stringSize, stringSize.size());
	memcpy(sizeChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(sizeChunk.data.data() + Chunk::command_padding, sizeCommand.c_str(), 5);
	uint32_t sizeCRC = sizeChunk.ComputeCRC();
	memcpy(sizeChunk.data.data() + Chunk::crc_padding, &sizeCRC, 4);

	chunks.insert({ currentSequence, sizeChunk });

	++currentSequence;
}


void FileSession::CreateStopChunk()
{
	// Name
	Chunk stopChunk;
	std::string stopCommand = "STOP";
	
	stopChunk.packetSize = Chunk::data_padding;
	stopChunk.data.resize(stopChunk.packetSize); // Cuz we dont have the memory yet

	memcpy(stopChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(stopChunk.data.data() + Chunk::command_padding, stopCommand.c_str(), 4);
	uint32_t stopCRC = stopChunk.ComputeCRC();
	memcpy(stopChunk.data.data() + Chunk::crc_padding, &stopCRC, 4);

	chunks.insert({ currentSequence, stopChunk });

	++currentSequence;
}